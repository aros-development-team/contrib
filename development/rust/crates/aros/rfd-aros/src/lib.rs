//! The parts of `rfd` Maolan uses, on AROS: `AsyncFileDialog`, `FileDialog` and `FileHandle`,
//! backed by asl.library's file requester (`c/asl_glue.c`). The requester is modal in the
//! task that shows it; the async dialog runs it on a thread of its own so the caller's
//! executor keeps going.

use std::ffi::{CString, c_char};
use std::future::Future;
use std::path::{Path, PathBuf};

extern "C" {
    fn rfd_asl_request(
        save: i32,
        multi: i32,
        drawers: i32,
        title: *const c_char,
        dir: *const c_char,
        file: *const c_char,
        pattern: *const c_char,
        out: *mut c_char,
        outlen: i32,
    ) -> i32;
}

/// A chosen file or drawer.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct FileHandle(PathBuf);

impl FileHandle {
    pub fn wrap(path: PathBuf) -> Self {
        Self(path)
    }

    pub fn path(&self) -> &Path {
        &self.0
    }

    pub fn file_name(&self) -> String {
        self.0.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default()
    }

    pub async fn read(&self) -> Vec<u8> {
        std::fs::read(&self.0).unwrap_or_default()
    }

    pub async fn write(&self, data: &[u8]) -> std::io::Result<()> {
        std::fs::write(&self.0, data)
    }

    pub fn inner(&self) -> &PathBuf {
        &self.0
    }
}

impl From<PathBuf> for FileHandle {
    fn from(path: PathBuf) -> Self {
        Self(path)
    }
}

impl From<FileHandle> for PathBuf {
    fn from(handle: FileHandle) -> Self {
        handle.0
    }
}

#[derive(Clone, Debug, Default)]
struct Request {
    title: Option<String>,
    directory: Option<PathBuf>,
    file_name: Option<String>,
    extensions: Vec<String>,
}

fn cstring(s: &str) -> CString {
    // The system charset is Latin-1 like; characters outside it become '?'.
    let bytes: Vec<u8> = s.chars().map(|c| if (c as u32) < 256 { c as u32 as u8 } else { b'?' }).filter(|b| *b != 0).collect();
    CString::new(bytes).unwrap_or_default()
}

fn latin1(bytes: &[u8]) -> String {
    bytes.iter().take_while(|b| **b != 0).map(|b| *b as char).collect()
}

impl Request {
    /// The AmigaDOS pattern for the filters: `#?.(wav|mid)`; none when there are no filters.
    fn pattern(&self) -> String {
        if self.extensions.is_empty() {
            return String::new();
        }
        let mut exts: Vec<&str> = self.extensions.iter().map(String::as_str).filter(|e| !e.is_empty() && *e != "*").collect();
        exts.sort_unstable();
        exts.dedup();
        if exts.is_empty() {
            return String::new();
        }
        format!("#?.({})", exts.join("|"))
    }

    fn show(&self, save: bool, multi: bool, drawers: bool) -> Vec<PathBuf> {
        let dir = self
            .directory
            .as_deref()
            .filter(|d| d.is_dir())
            .map(|d| d.to_string_lossy().into_owned())
            .unwrap_or_default();
        let (title, dir, file, pattern) = (
            cstring(self.title.as_deref().unwrap_or("")),
            cstring(&dir),
            cstring(self.file_name.as_deref().unwrap_or("")),
            cstring(&self.pattern()),
        );
        let mut out = vec![0u8; 16 * 1024];
        // SAFETY: all strings are NUL-terminated and outlive the call; `out` is `outlen` bytes.
        let n = unsafe {
            rfd_asl_request(
                save as i32,
                multi as i32,
                drawers as i32,
                title.as_ptr(),
                dir.as_ptr(),
                file.as_ptr(),
                pattern.as_ptr(),
                out.as_mut_ptr() as *mut c_char,
                out.len() as i32,
            )
        };
        if n <= 0 {
            return Vec::new();
        }
        latin1(&out).split('\n').filter(|p| !p.is_empty()).map(PathBuf::from).collect()
    }
}

/// A file requester shown from a blocking call.
#[derive(Clone, Debug, Default)]
pub struct FileDialog {
    request: Request,
}

impl FileDialog {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_filter(mut self, _name: impl Into<String>, extensions: &[impl ToString]) -> Self {
        self.request.extensions.extend(extensions.iter().map(ToString::to_string));
        self
    }

    pub fn set_directory<P: AsRef<Path>>(mut self, path: P) -> Self {
        self.request.directory = Some(path.as_ref().to_path_buf());
        self
    }

    pub fn set_file_name(mut self, file_name: impl Into<String>) -> Self {
        self.request.file_name = Some(file_name.into());
        self
    }

    pub fn set_title(mut self, title: impl Into<String>) -> Self {
        self.request.title = Some(title.into());
        self
    }

    pub fn set_can_create_directories(self, _can: bool) -> Self {
        self
    }

    pub fn pick_file(self) -> Option<PathBuf> {
        self.request.show(false, false, false).into_iter().next()
    }

    pub fn pick_files(self) -> Option<Vec<PathBuf>> {
        let paths = self.request.show(false, true, false);
        (!paths.is_empty()).then_some(paths)
    }

    pub fn pick_folder(self) -> Option<PathBuf> {
        self.request.show(false, false, true).into_iter().next()
    }

    pub fn pick_folders(self) -> Option<Vec<PathBuf>> {
        let paths = self.request.show(false, false, true);
        (!paths.is_empty()).then_some(paths)
    }

    pub fn save_file(self) -> Option<PathBuf> {
        self.request.show(true, false, false).into_iter().next()
    }
}

/// A file requester shown from a future: the requester runs on its own thread and the
/// future resolves when it closes.
#[derive(Clone, Debug, Default)]
pub struct AsyncFileDialog {
    request: Request,
}

impl AsyncFileDialog {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_filter(mut self, _name: impl Into<String>, extensions: &[impl ToString]) -> Self {
        self.request.extensions.extend(extensions.iter().map(ToString::to_string));
        self
    }

    pub fn set_directory<P: AsRef<Path>>(mut self, path: P) -> Self {
        self.request.directory = Some(path.as_ref().to_path_buf());
        self
    }

    pub fn set_file_name(mut self, file_name: impl Into<String>) -> Self {
        self.request.file_name = Some(file_name.into());
        self
    }

    pub fn set_title(mut self, title: impl Into<String>) -> Self {
        self.request.title = Some(title.into());
        self
    }

    pub fn set_can_create_directories(self, _can: bool) -> Self {
        self
    }

    fn run(self, save: bool, multi: bool, drawers: bool) -> impl Future<Output = Vec<PathBuf>> + Send {
        let request = self.request;
        Blocking::spawn(move || request.show(save, multi, drawers))
    }

    pub fn pick_file(self) -> impl Future<Output = Option<FileHandle>> + Send {
        let fut = self.run(false, false, false);
        async move { fut.await.into_iter().next().map(FileHandle) }
    }

    pub fn pick_files(self) -> impl Future<Output = Option<Vec<FileHandle>>> + Send {
        let fut = self.run(false, true, false);
        async move {
            let paths = fut.await;
            (!paths.is_empty()).then(|| paths.into_iter().map(FileHandle).collect())
        }
    }

    pub fn pick_folder(self) -> impl Future<Output = Option<FileHandle>> + Send {
        let fut = self.run(false, false, true);
        async move { fut.await.into_iter().next().map(FileHandle) }
    }

    pub fn pick_folders(self) -> impl Future<Output = Option<Vec<FileHandle>>> + Send {
        let fut = self.run(false, false, true);
        async move {
            let paths = fut.await;
            (!paths.is_empty()).then(|| paths.into_iter().map(FileHandle).collect())
        }
    }

    pub fn save_file(self) -> impl Future<Output = Option<FileHandle>> + Send {
        let fut = self.run(true, false, false);
        async move { fut.await.into_iter().next().map(FileHandle) }
    }
}

/// A future for a closure run on a thread of its own.
struct Blocking<T> {
    state: std::sync::Arc<std::sync::Mutex<BlockingState<T>>>,
}

struct BlockingState<T> {
    result: Option<T>,
    waker: Option<std::task::Waker>,
}

impl<T: Send + 'static> Blocking<T> {
    fn spawn(f: impl FnOnce() -> T + Send + 'static) -> Self {
        let state = std::sync::Arc::new(std::sync::Mutex::new(BlockingState { result: None, waker: None }));
        let shared = state.clone();
        let spawned = std::thread::Builder::new().name("asl-requester".into()).spawn(move || {
            let result = f();
            let waker = {
                let mut s = shared.lock().unwrap_or_else(|e| e.into_inner());
                s.result = Some(result);
                s.waker.take()
            };
            if let Some(w) = waker {
                w.wake();
            }
        });
        if spawned.is_err() {
            // No thread: run it here, modally.
            let mut s = state.lock().unwrap_or_else(|e| e.into_inner());
            s.result = None;
            drop(s);
        }
        Self { state }
    }
}

impl<T> Future for Blocking<T> {
    type Output = T;

    fn poll(self: std::pin::Pin<&mut Self>, cx: &mut std::task::Context<'_>) -> std::task::Poll<T> {
        let mut s = self.state.lock().unwrap_or_else(|e| e.into_inner());
        if let Some(r) = s.result.take() {
            return std::task::Poll::Ready(r);
        }
        s.waker = Some(cx.waker().clone());
        std::task::Poll::Pending
    }
}

impl<T> Unpin for Blocking<T> {}

extern "C" {
    fn rfd_message_request(title: *const c_char, text: *const c_char, gadgets: *const c_char) -> i32;
}

/// The severity of a message requester; AROS requesters look the same for all of them.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum MessageLevel {
    #[default]
    Info,
    Warning,
    Error,
}

/// The gadgets of a message requester.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub enum MessageButtons {
    #[default]
    Ok,
    OkCancel,
    YesNo,
    YesNoCancel,
    OkCustom(String),
    OkCancelCustom(String, String),
    YesNoCancelCustom(String, String, String),
}

/// What the user chose in a message requester.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum MessageDialogResult {
    Yes,
    No,
    Ok,
    Cancel,
    Custom(String),
}

#[derive(Clone, Debug, Default)]
struct MessageRequest {
    title: String,
    description: String,
    buttons: MessageButtons,
}

impl MessageRequest {
    /// The gadget texts, left to right, with the result each one stands for.
    fn gadgets(&self) -> Vec<(String, MessageDialogResult)> {
        use MessageDialogResult as R;
        match &self.buttons {
            MessageButtons::Ok => vec![("OK".into(), R::Ok)],
            MessageButtons::OkCancel => vec![("OK".into(), R::Ok), ("Cancel".into(), R::Cancel)],
            MessageButtons::YesNo => vec![("Yes".into(), R::Yes), ("No".into(), R::No)],
            MessageButtons::YesNoCancel => vec![("Yes".into(), R::Yes), ("No".into(), R::No), ("Cancel".into(), R::Cancel)],
            MessageButtons::OkCustom(ok) => vec![(ok.clone(), R::Custom(ok.clone()))],
            MessageButtons::OkCancelCustom(ok, cancel) => vec![(ok.clone(), R::Custom(ok.clone())), (cancel.clone(), R::Custom(cancel.clone()))],
            MessageButtons::YesNoCancelCustom(yes, no, cancel) => vec![
                (yes.clone(), R::Custom(yes.clone())),
                (no.clone(), R::Custom(no.clone())),
                (cancel.clone(), R::Custom(cancel.clone())),
            ],
        }
    }

    fn show(&self) -> MessageDialogResult {
        let gadgets = self.gadgets();
        let text = gadgets.iter().map(|(t, _)| t.replace('|', "/")).collect::<Vec<_>>().join("|");
        let (title, body, text) = (cstring(&self.title), cstring(&self.description), cstring(&text));
        // SAFETY: NUL-terminated strings that outlive the call.
        let n = unsafe { rfd_message_request(title.as_ptr(), body.as_ptr(), text.as_ptr()) };
        // EasyRequest: 1.. for the gadgets from the left, 0 for the rightmost (the escape one).
        let index = match n {
            i if i < 0 => gadgets.len() - 1,
            0 => gadgets.len() - 1,
            i => (i as usize - 1).min(gadgets.len() - 1),
        };
        gadgets[index].1.clone()
    }
}

/// A message requester shown from a blocking call.
#[derive(Clone, Debug, Default)]
pub struct MessageDialog {
    request: MessageRequest,
}

impl MessageDialog {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_level(self, _level: MessageLevel) -> Self {
        self
    }

    pub fn set_title(mut self, title: impl Into<String>) -> Self {
        self.request.title = title.into();
        self
    }

    pub fn set_description(mut self, text: impl Into<String>) -> Self {
        self.request.description = text.into();
        self
    }

    pub fn set_buttons(mut self, buttons: MessageButtons) -> Self {
        self.request.buttons = buttons;
        self
    }

    pub fn show(self) -> MessageDialogResult {
        self.request.show()
    }
}

/// A message requester shown from a future (on a thread of its own).
#[derive(Clone, Debug, Default)]
pub struct AsyncMessageDialog {
    request: MessageRequest,
}

impl AsyncMessageDialog {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_level(self, _level: MessageLevel) -> Self {
        self
    }

    pub fn set_title(mut self, title: impl Into<String>) -> Self {
        self.request.title = title.into();
        self
    }

    pub fn set_description(mut self, text: impl Into<String>) -> Self {
        self.request.description = text.into();
        self
    }

    pub fn set_buttons(mut self, buttons: MessageButtons) -> Self {
        self.request.buttons = buttons;
        self
    }

    pub fn show(self) -> impl Future<Output = MessageDialogResult> + Send {
        let request = self.request;
        Blocking::spawn(move || request.show())
    }
}
