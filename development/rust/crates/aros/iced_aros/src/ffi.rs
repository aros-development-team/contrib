#![allow(dead_code)]
//! The C side of the shell (`c/aros_shell.c`), wrapped for the Rust event loop: the window and
//! its events, the frame buffer blit, the timer-backed wait, ASL requesters, the clipboard.
//!
//! `pc_event` and the event kinds mirror the C definitions; keep them in sync.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};

pub const EV_CLOSE: i32 = 1;
pub const EV_RESIZE: i32 = 2;
pub const EV_MOUSE_MOVE: i32 = 3;
pub const EV_BUTTON: i32 = 4;
pub const EV_KEY: i32 = 5;
pub const EV_WHEEL: i32 = 6;
pub const EV_FOCUS: i32 = 7;

/// `IEQUALIFIER_*` bits (devices/inputevent.h).
pub const Q_LSHIFT: u32 = 1 << 0;
pub const Q_RSHIFT: u32 = 1 << 1;
pub const Q_CAPSLOCK: u32 = 1 << 2;
pub const Q_CONTROL: u32 = 1 << 3;
pub const Q_LALT: u32 = 1 << 4;
pub const Q_RALT: u32 = 1 << 5;
pub const Q_LCOMMAND: u32 = 1 << 6;
pub const Q_RCOMMAND: u32 = 1 << 7;
pub const Q_REPEAT: u32 = 1 << 9;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PcEvent {
    pub kind: i32,
    pub x: i32,
    pub y: i32,
    pub button: i32,
    pub down: i32,
    pub rawkey: i32,
    pub qualifier: u32,
    pub wheel_x: i32,
    pub wheel_y: i32,
    pub width: i32,
    pub height: i32,
    pub focus: i32,
    pub text: [u8; 16],
    pub plain: [u8; 8],
}

impl PcEvent {
    fn zeroed() -> Self {
        Self {
            kind: 0,
            x: 0,
            y: 0,
            button: 0,
            down: 0,
            rawkey: 0,
            qualifier: 0,
            wheel_x: 0,
            wheel_y: 0,
            width: 0,
            height: 0,
            focus: 0,
            text: [0; 16],
            plain: [0; 8],
        }
    }

    /// The typed characters, converted from the system charset (ISO 8859-1 on AROS) to UTF-8.
    pub fn text(&self) -> String {
        latin1(&self.text)
    }

    /// The key with no qualifiers held, for naming the logical key.
    pub fn plain(&self) -> String {
        latin1(&self.plain)
    }
}

fn latin1(bytes: &[u8]) -> String {
    bytes.iter().take_while(|&&b| b != 0).map(|&b| b as char).collect()
}

#[repr(C)]
pub struct PcWindow {
    _private: [u8; 0],
}

unsafe extern "C" {
    fn pc_screen_size(width: *mut i32, height: *mut i32);
    fn pc_window_open(width: i32, height: i32, title: *const c_char) -> *mut PcWindow;
    fn pc_window_close(w: *mut PcWindow);
    fn pc_window_inner_size(w: *mut PcWindow, width: *mut i32, height: *mut i32);
    fn pc_window_wait(w: *mut PcWindow, timeout_ms: i32) -> i32;
    fn pc_window_wake(w: *mut PcWindow);
    fn pc_window_poll(w: *mut PcWindow, ev: *mut PcEvent) -> i32;
    fn pc_window_qualifier(w: *mut PcWindow) -> u32;
    fn pc_window_blit(w: *mut PcWindow, rgba: *const u8, width: i32, height: i32, stride: i32);
    fn pc_window_set_busy(w: *mut PcWindow, busy: i32);
    fn pc_window_set_title(w: *mut PcWindow, title: *const c_char);
    fn pc_window_activate(w: *mut PcWindow);
    fn pc_beep();
    fn pc_file_request(
        w: *mut PcWindow,
        save: i32,
        multi: i32,
        title: *const c_char,
        dir: *const c_char,
        file: *const c_char,
        pattern: *const c_char,
        out: *mut c_char,
        outlen: i32,
    ) -> i32;
    fn pc_clip_write_text(text: *const c_char, len: i32) -> i32;
    fn pc_clip_read_text(out: *mut c_char, outlen: i32) -> i32;
    fn pc_open_url(url: *const c_char) -> c_int;
    fn pc_window_set_priority(w: *mut PcWindow, pri: i32);
}

fn cstring(s: &str) -> CString {
    // Interior NULs cannot be passed through; the text is cut at the first one.
    CString::new(s.split('\0').next().unwrap_or("")).unwrap_or_default()
}

/// Converts UTF-8 to the system charset (ISO 8859-1), replacing what it cannot represent.
fn to_latin1(s: &str) -> Vec<u8> {
    s.chars().map(|c| if (c as u32) < 256 { c as u32 as u8 } else { b'?' }).collect()
}

/// The screen's size in pixels.
pub fn screen_size() -> (i32, i32) {
    let (mut w, mut h) = (0, 0);
    // SAFETY: plain out-parameters.
    unsafe { pc_screen_size(&mut w, &mut h) };
    (w, h)
}

/// The shell's window. Lives on the main thread; only [`Waker`] crosses threads.
pub struct Window {
    raw: *mut PcWindow,
}

/// Wakes the event loop from another thread (egui's repaint requests from workers).
#[derive(Clone, Copy)]
pub struct Waker(*mut PcWindow);

// SAFETY: pc_window_wake only raises an exec signal for the main task, which is safe from any
// task; the pointer is never dereferenced on the other thread beyond that call, and the window
// outlives every worker that holds a waker (the app, and its workers, are dropped before it).
unsafe impl Send for Waker {}
unsafe impl Sync for Waker {}

impl Waker {
    pub fn wake(&self) {
        // SAFETY: see the Send impl.
        unsafe { pc_window_wake(self.0) }
    }
}

impl Window {
    pub fn open(width: i32, height: i32, title: &str) -> Option<Self> {
        let title = cstring(title);
        // SAFETY: the title outlives the call (the C side copies it).
        let raw = unsafe { pc_window_open(width, height, title.as_ptr()) };
        if raw.is_null() { None } else { Some(Self { raw }) }
    }

    pub fn waker(&self) -> Waker {
        Waker(self.raw)
    }

    pub fn inner_size(&self) -> (i32, i32) {
        let (mut w, mut h) = (0, 0);
        // SAFETY: valid window, plain out-parameters.
        unsafe { pc_window_inner_size(self.raw, &mut w, &mut h) };
        (w.max(1), h.max(1))
    }

    /// Sleeps until an event or a wake-up, or for `timeout_ms` (negative: no timeout).
    pub fn wait(&self, timeout_ms: i32) -> bool {
        // SAFETY: valid window.
        unsafe { pc_window_wait(self.raw, timeout_ms) != 0 }
    }

    pub fn poll(&self) -> Option<PcEvent> {
        let mut ev = PcEvent::zeroed();
        // SAFETY: valid window and a properly laid out event record.
        let got = unsafe { pc_window_poll(self.raw, &mut ev) };
        (got != 0).then_some(ev)
    }

    pub fn qualifier(&self) -> u32 {
        // SAFETY: valid window.
        unsafe { pc_window_qualifier(self.raw) }
    }

    /// Copies an RGBA frame buffer (`stride` bytes per row) into the window.
    pub fn blit(&self, rgba: &[u8], width: usize, height: usize, stride: usize) {
        if rgba.len() < stride * height || width == 0 || height == 0 {
            return;
        }
        // SAFETY: the buffer holds `height` rows of `stride` bytes, checked above.
        unsafe { pc_window_blit(self.raw, rgba.as_ptr(), width as i32, height as i32, stride as i32) };
    }

    pub fn set_busy(&self, busy: bool) {
        // SAFETY: valid window.
        unsafe { pc_window_set_busy(self.raw, busy as i32) }
    }

    /// The event loop's task priority (see the C side).
    pub fn set_priority(&self, pri: i32) {
        // SAFETY: valid window.
        unsafe { pc_window_set_priority(self.raw, pri) }
    }

    /// Brings the window to the front.
    pub fn activate(&self) {
        // SAFETY: valid window.
        unsafe { pc_window_activate(self.raw) }
    }

    pub fn set_title(&self, title: &str) {
        let title = cstring(title);
        // SAFETY: the C side copies the string before returning.
        unsafe { pc_window_set_title(self.raw, title.as_ptr()) }
    }

    /// Shows an ASL file requester (modal) and returns the chosen paths.
    pub fn file_request(&self, save: bool, multi: bool, title: &str, dir: &str, file: &str, pattern: &str) -> Vec<String> {
        let (title, dir, file, pattern) = (cstring(title), cstring(dir), cstring(file), cstring(pattern));
        let mut out = vec![0u8; 16 * 1024];
        // SAFETY: all strings are NUL-terminated and outlive the call; `out` is `outlen` bytes.
        let n = unsafe {
            pc_file_request(
                self.raw,
                save as i32,
                multi as i32,
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
        let text = latin1(&out);
        text.split('\n').filter(|p| !p.is_empty()).map(str::to_string).collect()
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        // SAFETY: closed exactly once.
        unsafe { pc_window_close(self.raw) }
    }
}

pub fn beep() {
    // SAFETY: no arguments.
    unsafe { pc_beep() }
}

pub fn clipboard_write_text(text: &str) -> bool {
    let bytes = to_latin1(text);
    // SAFETY: the buffer and its length are passed together.
    unsafe { pc_clip_write_text(bytes.as_ptr() as *const c_char, bytes.len() as i32) != 0 }
}

pub fn clipboard_read_text() -> Option<String> {
    let mut out = vec![0u8; 1 << 20];
    // SAFETY: `out` is `outlen` bytes and is NUL-terminated by the C side.
    let n = unsafe { pc_clip_read_text(out.as_mut_ptr() as *mut c_char, out.len() as i32) };
    (n > 0).then(|| latin1(&out[..n as usize]))
}

pub fn open_url(url: &str) -> bool {
    let url = cstring(url);
    // SAFETY: NUL-terminated string.
    unsafe { pc_open_url(url.as_ptr()) != 0 }
}

/// The last path element of an AmigaDOS or POSIX path.
pub fn file_part(path: &str) -> &str {
    path.rsplit(['/', ':']).next().unwrap_or(path)
}

/// The directory part of an AmigaDOS or POSIX path (`Work:Images/` for `Work:Images/a.png`).
pub fn dir_part(path: &str) -> &str {
    match path.rfind(['/', ':']) {
        Some(i) => &path[..=i],
        None => "",
    }
}

#[allow(dead_code)]
pub fn c_str_to_string(p: *const c_char) -> String {
    if p.is_null() {
        return String::new();
    }
    // SAFETY: the caller passes a NUL-terminated string.
    unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

