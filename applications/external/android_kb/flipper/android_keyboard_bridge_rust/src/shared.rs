//! Shared UI / connection state guarded by a **safe** [`Mutex`].
//!
//! Flipper C callbacks may interrupt the main loop, so anything they touch
//! lives here. The protocol parser also lives here: only the Serial RX path
//! feeds it (under the same lock).

use core::sync::atomic::{AtomicBool, Ordering};

use flipperzero::furi::sync::Mutex;

use crate::protocol::ProtocolParser;

/// NUL-terminated status string for the canvas (no heap).
#[derive(Clone, Copy)]
pub struct StatusLine {
    buf: [u8; 48],
}

impl StatusLine {
    pub const fn empty() -> Self {
        Self { buf: [0; 48] }
    }

    pub fn set(&mut self, text: &str) {
        self.buf.fill(0);
        let bytes = text.as_bytes();
        let n = bytes.len().min(self.buf.len() - 1);
        self.buf[..n].copy_from_slice(&bytes[..n]);
    }

    /// Pointer suitable for `canvas_draw_str` (always NUL-terminated).
    pub fn as_ptr(&self) -> *const core::ffi::c_char {
        self.buf.as_ptr().cast()
    }
}

/// Mutable snapshot drawn on screen / updated from BLE callbacks.
pub struct Shared {
    pub status: StatusLine,
    pub usb_connected: bool,
    pub phone_connected: bool,
    pub need_buffer_notify: bool,
    pub reclaim_ticks: u8,
    pub frames_received: u32,
    pub hid_applied: u32,
    pub backlight_forced: bool,
    /// Stream reassembly — only touched while holding this mutex from RX.
    pub parser: ProtocolParser,
}

impl Shared {
    pub fn new() -> Self {
        let mut status = StatusLine::empty();
        status.set("Starting...");
        Self {
            status,
            usb_connected: false,
            phone_connected: false,
            need_buffer_notify: false,
            reclaim_ticks: 0,
            frames_received: 0,
            hid_applied: 0,
            backlight_forced: false,
            parser: ProtocolParser::new(),
        }
    }
}

/// App-wide shared cell.
pub type SharedCell = Mutex<Shared>;

/// Exit flag is separate so Back can set it without taking the UI mutex
/// (avoids lock ordering surprises during HID gaps).
pub struct ExitFlag(AtomicBool);

impl ExitFlag {
    pub const fn new() -> Self {
        Self(AtomicBool::new(false))
    }

    pub fn request(&self) {
        self.0.store(true, Ordering::Relaxed);
    }

    pub fn is_set(&self) -> bool {
        self.0.load(Ordering::Relaxed)
    }
}
