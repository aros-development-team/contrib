//! An AROS shell for iced: one Intuition window, events from IDCMP, the tiny-skia renderer
//! drawing into a frame buffer that cybergraphics blits (`c/aros_shell.c`). It takes the
//! place of `iced_winit` on AROS, where neither winit nor wgpu exist.
//!
//! The loop follows `iced_winit`'s single-window flow: window events become `iced_core`
//! events, the user interface is rebuilt from the program's view and updated with them, the
//! program's `update` runs for the messages that produced, its tasks and subscriptions run on
//! the program's executor and deliver their actions through a [`Proxy`] that wakes the
//! window's task, and a redraw renders the interface (through the renderer's headless path)
//! into the window.
pub use iced_program as program;
pub use program::core;
pub use program::graphics;
pub use program::runtime;
pub use runtime::futures;

pub mod clipboard;
pub mod conversion;
mod error;
mod ffi;
mod proxy;

pub use clipboard::Clipboard;
pub use error::Error;
pub use proxy::Proxy;

use crate::core::mouse;
use crate::core::renderer;
use crate::core::theme;
use crate::core::time::Instant;
use crate::core::window;
use crate::core::Size;
use crate::futures::futures::StreamExt;
use crate::futures::subscription;
use crate::futures::{Executor, Runtime};
use crate::runtime::user_interface::{self, UserInterface};
use crate::runtime::{Action, Task};
use program::Program;

use std::borrow::Cow;
use std::time::Duration;

/// Why the window could not be opened.
#[derive(Debug, Clone)]
pub struct WindowError(pub String);

impl std::fmt::Display for WindowError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for WindowError {}

/// How long the loop sleeps when nothing asks for a frame: tasks and subscriptions wake it
/// through the proxy, so this only bounds the latency of anything that does not.
const IDLE_POLL: Duration = Duration::from_millis(250);

/// The stack of the thread that runs the program: a big iced view, built and laid out in one
/// go, takes more than the few megabytes an AROS process starts with.
const LOOP_STACK: usize = 64 << 20;

/// Runs an iced program in an Intuition window.
pub fn run<P>(program: P) -> Result<(), Error>
where
    P: Program + 'static,
    P::Theme: theme::Base,
    P::Renderer: renderer::Headless,
{
    // The whole loop (window, executor, interface) lives on one thread of its own, so that
    // it has the stack it needs and the window's signals belong to the task that waits on them.
    // The program need not be `Send`: it is handed to the thread and never touched again
    // here, which is what `Send` guarantees for the types that have it.
    struct Handoff<T>(T);
    // SAFETY: see above; the main task only joins the thread.
    unsafe impl<T> Send for Handoff<T> {}
    let handoff = Handoff(program);
    let thread = std::thread::Builder::new()
        .name("iced-aros".into())
        .stack_size(LOOP_STACK)
        .spawn(move || {
            let handoff = handoff;
            run_loop(handoff.0)
        });
    let result = match thread {
        Ok(handle) => handle.join().unwrap_or_else(|_| Err(Error::WindowCreationFailed(WindowError("the program's thread panicked".into())))),
        Err(e) => Err(Error::WindowCreationFailed(WindowError(format!("cannot start the program's thread: {e}")))),
    };
    result
}

fn run_loop<P>(program: P) -> Result<(), Error>
where
    P: Program + 'static,
    P::Theme: theme::Base,
    P::Renderer: renderer::Headless,
{
    let settings = program.settings();
    let window_settings = program.window().unwrap_or_default();
    let executor = P::Executor::new().map_err(Error::ExecutorCreationFailed)?;

    let scale = 1.0f32;
    let (screen_w, screen_h) = ffi::screen_size();
    let want_w = (window_settings.size.width * scale) as i32;
    let want_h = (window_settings.size.height * scale) as i32;
    let width = want_w.clamp(320, (screen_w - 16).max(320));
    let height = want_h.clamp(240, (screen_h - 48).max(240));
    let window = ffi::Window::open(width, height, P::name())
        .ok_or_else(|| Error::WindowCreationFailed(WindowError("intuition.library could not open the window".into())))?;
    let (proxy, mut actions_rx) = Proxy::<P::Message>::new(window.waker());

    let mut runtime: Runtime<P::Executor, Proxy<P::Message>, Action<P::Message>> = Runtime::new(executor, proxy.clone());
    let (mut instance, boot) = runtime.enter(|| program::Instance::new(program));
    if let Some(stream) = runtime::task::into_stream(boot) {
        runtime.run(stream);
    }

    // The renderer: tiny-skia, through the renderer's headless path (no compositor on AROS).
    let mut renderer = runtime
        .block_on(<P::Renderer as renderer::Headless>::new(settings.default_font, settings.default_text_size, Some("tiny-skia")))
        .ok_or_else(|| Error::WindowCreationFailed(WindowError("the tiny-skia renderer could not be created".into())))?;
    setup_fonts();
    for font in &settings.fonts {
        load_font(font.clone());
    }

    let id = window::Id::unique();
    let mut title = instance.title(id);
    window.set_title(&title);
    let (mut width, mut height) = window.inner_size();
    let mut logical = conversion::logical_size(width, height, scale);

    let mut input = conversion::Input::default();
    let mut clipboard = Clipboard;
    let mut cache = Some(user_interface::Cache::default());
    let mut events: Vec<core::Event> = vec![core::Event::Window(window::Event::Opened { position: None, size: logical })];
    let mut messages: Vec<P::Message> = Vec::new();
    let mut redraw_at: Option<Instant> = Some(Instant::now());
    let mut mouse_interaction = mouse::Interaction::default();
    let mut busy = false;

    track_subscription(&mut runtime, &instance);
    let mut screenshot = Screenshot::from_env();
    let mut autoclicks = AutoClick::from_env();
    let trace = std::env::var_os("ICED_AROS_TRACE").is_some();

    'main: loop {
        // Sleep until input, an action or the next frame.
        let mut timeout = match redraw_at {
            Some(at) => at.saturating_duration_since(Instant::now()).min(IDLE_POLL),
            None => IDLE_POLL,
        };
        if let Some(at) = autoclicks.next_at() {
            timeout = timeout.min(at.saturating_duration_since(Instant::now()).max(Duration::from_millis(1)));
        }
        if screenshot.path.is_some() && !screenshot.taken {
            timeout = timeout.min(screenshot.at.saturating_duration_since(Instant::now()).max(Duration::from_millis(1)));
            if redraw_at.is_none() && Instant::now() >= screenshot.at {
                redraw_at = Some(Instant::now());
            }
        }
        if events.is_empty() && messages.is_empty() {
            if trace {
                log::debug!("loop: wait {} ms (redraw {:?})", timeout.as_millis(), redraw_at.map(|at| at.saturating_duration_since(Instant::now())));
            }
            window.wait(timeout.as_millis() as i32);
        }
        while let Some(ev) = window.poll() {
            input.push(&ev, scale);
        }
        if let Some((w, h)) = input.resized.take() {
            width = w.max(1);
            height = h.max(1);
            logical = conversion::logical_size(width, height, scale);
            events.push(core::Event::Window(window::Event::Resized(logical)));
            redraw_at = Some(Instant::now());
        }
        let _ = input.focus.take();
        if std::mem::take(&mut input.close_requested) {
            if window_settings.exit_on_close_request {
                break 'main;
            }
            events.push(core::Event::Window(window::Event::CloseRequested));
        }
        events.extend(input.take_events());
        if let Some(position) = autoclicks.due() {
            log::info!("autoclick at {position:?}");
            input.cursor = Some(position);
            events.push(core::Event::Mouse(mouse::Event::CursorMoved { position }));
            events.push(core::Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left)));
            events.push(core::Event::Mouse(mouse::Event::ButtonReleased(mouse::Button::Left)));
        }

        // Actions from tasks and subscriptions.
        let mut exit = false;
        let mut reload = false;
        let mut operations: Vec<Box<dyn core::widget::Operation>> = Vec::new();
        while let Ok(action) = actions_rx.try_recv() {
            run_action(action, &mut messages, &mut operations, &mut clipboard, &window, logical, scale, &mut exit, &mut reload, &mut redraw_at);
        }
        if exit {
            break 'main;
        }

        let cursor = match input.cursor {
            Some(p) => mouse::Cursor::Available(p),
            None => mouse::Cursor::Unavailable,
        };
        let theme = instance.theme(id).unwrap_or_else(|| <P::Theme as theme::Base>::default(theme::Mode::None));
        let style = instance.style(&theme);

        // Lay the interface out and feed it this pass's events.
        let mut ui = UserInterface::build(instance.view(id), logical, cache.take().unwrap_or_default(), &mut renderer);
        for operation in operations.iter_mut() {
            ui.operate(&renderer, operation.as_mut());
        }
        if !events.is_empty() {
            let (state, statuses) = ui.update(&events, cursor, &mut renderer, &mut clipboard, &mut messages);
            for (event, status) in events.drain(..).zip(statuses) {
                runtime.broadcast(subscription::Event::Interaction { window: id, event, status });
            }
            if let user_interface::State::Updated { redraw_request, .. } = state {
                schedule(&mut redraw_at, redraw_request);
            } else {
                redraw_at = Some(Instant::now());
            }
            // Input usually changes what is shown.
            if redraw_at.is_none() {
                redraw_at = Some(Instant::now());
            }
        }

        if !messages.is_empty() || reload {
            cache = Some(ui.into_cache());
            let actions = update(&mut instance, &mut runtime, &mut messages);
            track_subscription(&mut runtime, &instance);
            for action in actions {
                match action {
                    Action::Output(message) => messages.push(message),
                    other => proxy.send_action(other),
                }
            }
            let new_title = instance.title(id);
            if new_title != title {
                title = new_title;
                window.set_title(&title);
            }
            redraw_at = Some(Instant::now());
            continue 'main;
        }

        // Draw when a frame is due.
        let due = redraw_at.is_some_and(|at| at <= Instant::now());
        if due {
            let now = Instant::now();
            let redraw_event = core::Event::Window(window::Event::RedrawRequested(now));
            let (state, _) = ui.update(std::slice::from_ref(&redraw_event), cursor, &mut renderer, &mut clipboard, &mut messages);
            runtime.broadcast(subscription::Event::Interaction { window: id, event: redraw_event, status: core::event::Status::Ignored });
            ui.draw(&mut renderer, &theme, &renderer::Style { text_color: style.text_color }, cursor);
            redraw_at = None;
            if let user_interface::State::Updated { redraw_request, mouse_interaction: interaction, .. } = state {
                schedule(&mut redraw_at, redraw_request);
                if interaction != mouse_interaction {
                    mouse_interaction = interaction;
                    let wants_busy = matches!(interaction, mouse::Interaction::Wait | mouse::Interaction::Progress);
                    if wants_busy != busy {
                        busy = wants_busy;
                        window.set_busy(busy);
                    }
                }
            }
            // The layers hold weak references to the widgets' paragraphs, so they are
            // rendered while the interface (and its element tree) is still alive.
            let pixels = renderer::Headless::screenshot(&mut renderer, Size::new(width as u32, height as u32), scale, style.background_color);
            window.blit(&pixels, width as usize, height as usize, width as usize * 4);
            cache = Some(ui.into_cache());
            if screenshot.take_due() {
                screenshot.write(&pixels, width as u32, height as u32);
                // The program then quits the way it does when the window is closed (its own
                // shutdown of the engine and its threads): on AROS, exit() waits for every
                // pthread to finish, so a program with running threads cannot just exit.
                events.push(core::Event::Window(window::Event::CloseRequested));
                redraw_at = Some(Instant::now());
            }
            if !messages.is_empty() {
                redraw_at = Some(Instant::now());
            }
        } else {
            cache = Some(ui.into_cache());
        }
    }

    Ok(())
}

/// Makes the generic families resolve on a system without the usual font directories: the
/// system's TrueType fonts (`FONTS:TrueType`) are loaded, and the generic families that
/// then still match nothing are pointed at the bundled Fira Sans, so that text shaped
/// without fallback (iced's basic shaping) has a font.
fn setup_fonts() {
    let mut system = match graphics::text::font_system().write() {
        Ok(s) => s,
        Err(poisoned) => poisoned.into_inner(),
    };
    let db = system.raw().db_mut();
    // fontdb maps font files into memory, which AROS cannot do; the files are read instead.
    for dir in ["FONTS:TrueType", "SYS:Fonts/TrueType"] {
        let Ok(entries) = std::fs::read_dir(dir) else { continue };
        let mut loaded = 0usize;
        for entry in entries.flatten() {
            let path = entry.path();
            let is_font = path
                .extension()
                .and_then(|e| e.to_str())
                .is_some_and(|e| matches!(e.to_ascii_lowercase().as_str(), "ttf" | "otf" | "ttc"));
            if !is_font {
                continue;
            }
            match std::fs::read(&path) {
                Ok(bytes) => {
                    db.load_font_data(bytes);
                    loaded += 1;
                }
                Err(e) => log::debug!("font {}: {e}", path.display()),
            }
        }
        if loaded > 0 {
            log::info!("fonts: {loaded} files from {dir}");
            break;
        }
    }
    let has_family = |db: &graphics::text::cosmic_text::fontdb::Database, name: &str| {
        db.faces().any(|f| f.families.iter().any(|(n, _)| n == name))
    };
    let pick = |db: &graphics::text::cosmic_text::fontdb::Database, wanted: &[&str]| -> String {
        wanted.iter().find(|n| has_family(db, n)).map(|n| n.to_string()).unwrap_or_else(|| "Fira Sans".to_string())
    };
    let sans = pick(db, &["DejaVu Sans", "Noto Sans", "Liberation Sans", "Fira Sans"]);
    let serif = pick(db, &["DejaVu Serif", "Noto Serif", "Liberation Serif"]);
    let mono = pick(db, &["DejaVu Sans Mono", "Noto Sans Mono", "Liberation Mono"]);
    log::info!("fonts: {} faces; sans-serif = {sans}, serif = {serif}, monospace = {mono}", db.len());
    db.set_sans_serif_family(sans);
    db.set_serif_family(serif);
    db.set_monospace_family(mono.clone());
    db.set_cursive_family("Fira Sans");
    db.set_fantasy_family("Fira Sans");
}

/// Registers a font with the renderers' shared font system.
fn load_font(bytes: Cow<'static, [u8]>) {
    match graphics::text::font_system().write() {
        Ok(mut system) => system.load_font(bytes),
        Err(poisoned) => poisoned.into_inner().load_font(bytes),
    }
}

/// A frame dump for testing without a display to look at: with `ICED_AROS_SCREENSHOT=<file>`
/// the shell writes the window's pixels to that PNG once `ICED_AROS_SCREENSHOT_AFTER` (ms,
/// 3000 by default) have passed, then exits.
struct Screenshot {
    path: Option<std::path::PathBuf>,
    at: Instant,
    taken: bool,
}

impl Screenshot {
    fn from_env() -> Self {
        let path = std::env::var_os("ICED_AROS_SCREENSHOT").map(std::path::PathBuf::from);
        let after = std::env::var("ICED_AROS_SCREENSHOT_AFTER").ok().and_then(|v| v.parse::<u64>().ok()).unwrap_or(3000);
        Self { path, at: Instant::now() + Duration::from_millis(after), taken: false }
    }

    /// Whether the shot is due now; it is taken once.
    fn take_due(&mut self) -> bool {
        if !self.taken && self.path.is_some() && Instant::now() >= self.at {
            self.taken = true;
            return true;
        }
        false
    }

    fn write(&self, rgba: &[u8], width: u32, height: u32) {
        let Some(path) = &self.path else { return };
        let Some(pixmap) = tiny_skia::Pixmap::from_vec(rgba.to_vec(), tiny_skia::IntSize::from_wh(width, height).unwrap_or_else(|| tiny_skia::IntSize::from_wh(1, 1).expect("size"))) else {
            log::error!("screenshot: the frame does not fit {width}x{height}");
            return;
        };
        match pixmap.save_png(path) {
            Ok(()) => log::info!("screenshot written to {}", path.display()),
            Err(e) => log::error!("screenshot: {e}"),
        }
    }
}

/// Scripted left clicks for testing without a hand on the mouse:
/// `ICED_AROS_AUTOCLICK=<ms>:<x>,<y>+<ms>:<x>,<y>...`, times from start, positions in points.
struct AutoClick {
    clicks: std::collections::VecDeque<(Instant, core::Point)>,
}

impl AutoClick {
    fn from_env() -> Self {
        let start = Instant::now();
        let mut clicks = std::collections::VecDeque::new();
        if let Ok(spec) = std::env::var("ICED_AROS_AUTOCLICK") {
            // `+` as well as `;`: the AmigaDOS shell takes `;` for a comment.
            for item in spec.split(|c| c == ';' || c == '+') {
                let Some((ms, xy)) = item.split_once(':') else { continue };
                let Some((x, y)) = xy.split_once(',') else { continue };
                if let (Ok(ms), Ok(x), Ok(y)) = (ms.trim().parse::<u64>(), x.trim().parse::<f32>(), y.trim().parse::<f32>()) {
                    clicks.push_back((start + Duration::from_millis(ms), core::Point::new(x, y)));
                }
            }
        }
        Self { clicks }
    }

    fn next_at(&self) -> Option<Instant> {
        self.clicks.front().map(|(at, _)| *at)
    }

    fn due(&mut self) -> Option<core::Point> {
        match self.clicks.front() {
            Some((at, _)) if *at <= Instant::now() => self.clicks.pop_front().map(|(_, p)| p),
            _ => None,
        }
    }
}

/// When the next frame is due, from what the interface asked for.
fn schedule(redraw_at: &mut Option<Instant>, request: window::RedrawRequest) {
    let at = match request {
        window::RedrawRequest::NextFrame => Some(Instant::now()),
        window::RedrawRequest::At(at) => Some(at),
        window::RedrawRequest::Wait => None,
    };
    *redraw_at = match (*redraw_at, at) {
        (Some(a), Some(b)) => Some(a.min(b)),
        (a, b) => a.or(b),
    };
}

fn track_subscription<P>(runtime: &mut Runtime<P::Executor, Proxy<P::Message>, Action<P::Message>>, instance: &program::Instance<P>)
where
    P: Program + 'static,
    P::Theme: theme::Base,
{
    let subscription = runtime.enter(|| instance.subscription());
    runtime.track(subscription::into_recipes(subscription.map(Action::Output)));
}

/// Runs the program's `update` for every message, returning the actions of the tasks that
/// complete at once; the rest keep running on the executor.
fn update<P>(
    instance: &mut program::Instance<P>,
    runtime: &mut Runtime<P::Executor, Proxy<P::Message>, Action<P::Message>>,
    messages: &mut Vec<P::Message>,
) -> Vec<Action<P::Message>>
where
    P: Program + 'static,
    P::Theme: theme::Base,
{
    use crate::futures::futures;
    let mut actions = Vec::new();
    for message in messages.drain(..) {
        let task: Task<P::Message> = runtime.enter(|| instance.update(message));
        if let Some(mut stream) = runtime::task::into_stream(task) {
            let waker = futures::task::noop_waker_ref();
            let mut context = futures::task::Context::from_waker(waker);
            loop {
                match runtime.enter(|| stream.poll_next_unpin(&mut context)) {
                    futures::task::Poll::Ready(Some(action)) => actions.push(action),
                    futures::task::Poll::Ready(None) => break,
                    futures::task::Poll::Pending => {
                        runtime.run(stream);
                        break;
                    }
                }
            }
        }
    }
    actions
}

#[allow(clippy::too_many_arguments)]
fn run_action<M>(
    action: Action<M>,
    messages: &mut Vec<M>,
    operations: &mut Vec<Box<dyn core::widget::Operation>>,
    clipboard: &mut Clipboard,
    window: &ffi::Window,
    logical: Size,
    scale: f32,
    exit: &mut bool,
    reload: &mut bool,
    redraw_at: &mut Option<Instant>,
) {
    use crate::core::Clipboard as _;
    use crate::runtime::clipboard;
    use crate::runtime::system;
    use crate::runtime::window as win;
    match action {
        Action::Output(message) => messages.push(message),
        Action::Clipboard(action) => match action {
            clipboard::Action::Read { target, channel } => {
                let _ = channel.send(clipboard.read(target));
            }
            clipboard::Action::Write { target, contents } => clipboard.write(target, contents),
        },
        Action::Window(action) => match action {
            win::Action::Open(_, _, channel) => {
                // One window: a request for another is answered with it.
                log::warn!("a second window was requested; the AROS shell has one window");
                let _ = channel;
            }
            win::Action::Close(_) => *exit = true,
            win::Action::GetOldest(channel) | win::Action::GetLatest(channel) => {
                let _ = channel.send(None);
            }
            win::Action::GetSize(_, channel) => {
                let _ = channel.send(logical);
            }
            win::Action::GetScaleFactor(_, channel) => {
                let _ = channel.send(scale);
            }
            win::Action::GetMaximized(_, channel) => {
                let _ = channel.send(false);
            }
            win::Action::GetMinimized(_, channel) => {
                let _ = channel.send(None);
            }
            win::Action::GetPosition(_, channel) => {
                let _ = channel.send(None);
            }
            win::Action::GetMode(_, channel) => {
                let _ = channel.send(window::Mode::Windowed);
            }
            win::Action::GetRawId(_, channel) => {
                let _ = channel.send(0);
            }
            win::Action::GetMonitorSize(_, channel) => {
                let (w, h) = ffi::screen_size();
                let _ = channel.send(Some(Size::new(w as f32 / scale, h as f32 / scale)));
            }
            win::Action::Screenshot(_, _channel) => {
                log::info!("window screenshots are not supported by the AROS shell");
            }
            win::Action::RedrawAll | win::Action::RelayoutAll => {
                *redraw_at = Some(Instant::now());
            }
            win::Action::GainFocus(_) => window.activate(),
            _ => {}
        },
        Action::LoadFont { bytes, channel } => {
            load_font(bytes);
            let _ = channel.send(Ok(()));
        }
        Action::Widget(operation) => operations.push(operation),
        Action::System(action) => match action {
            system::Action::GetTheme(channel) => {
                let _ = channel.send(theme::Mode::None);
            }
            system::Action::NotifyTheme(_) => {}
            system::Action::GetInformation(_channel) => {
                log::info!("system information is not available from the AROS shell");
            }
        },
        Action::Image(_) => {}
        Action::Reload => *reload = true,
        Action::Exit => *exit = true,
    }
}
