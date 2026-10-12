//! The system clipboard (clipboard.device, FTXT) for text.

use crate::core::clipboard::Kind;
use crate::ffi;

/// The AROS clipboard: text goes through clipboard.device as FTXT.
#[derive(Debug, Default)]
pub struct Clipboard;

impl crate::core::Clipboard for Clipboard {
    fn read(&self, _kind: Kind) -> Option<String> {
        ffi::clipboard_read_text()
    }

    fn write(&mut self, _kind: Kind, contents: String) {
        if !ffi::clipboard_write_text(&contents) {
            log::warn!("could not write the clipboard");
        }
    }
}
