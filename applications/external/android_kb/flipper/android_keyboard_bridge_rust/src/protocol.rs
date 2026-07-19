//! Binary frame protocol shared with the Android app (and the C FAP).
//!
//! # Wire format
//!
//! Every frame is exactly **6 bytes**:
//!
//! ```text
//!  [0] 0xFB   magic
//!  [1] 0x4B   magic ("FK" hint: Flipper Keyboard)
//!  [2] 0x03   payload length (always 3 for current protocol)
//!  [3] event  see EventKind
//!  [4] a      meaning depends on event
//!  [5] b      meaning depends on event
//! ```
//!
//! BLE delivers a byte stream, not message boundaries — so we keep a small
//! reassembly buffer and resynchronize on the magic pair when bytes are lost
//! or glued together.
//!
//! # Threading note
//!
//! `ProtocolParser::feed` is called from the BLE Serial RX callback.
//! It must not block and must not call into code that takes the same mutex
//! the serial service already holds (see main app comments about notify).

/// First magic byte (`0xFB`).
pub const MAGIC_0: u8 = 0xFB;
/// Second magic byte (`0x4B`).
pub const MAGIC_1: u8 = 0x4B;
/// Payload length after magic+len header field — always 3 today.
pub const FRAME_PAYLOAD_LEN: u8 = 3;
/// Total on-wire size: 2 magic + 1 len + 3 payload.
pub const FRAME_TOTAL_LEN: usize = 6;

/// Event byte values (frame[3]).
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EventKind {
    KeyDown = 0x01,
    KeyUp = 0x02,
    MouseMove = 0x10,
    MouseDown = 0x11,
    MouseUp = 0x12,
    MouseScroll = 0x13,
}

impl EventKind {
    /// Parse a raw event byte; unknown values become `None`.
    pub fn from_u8(value: u8) -> Option<Self> {
        match value {
            0x01 => Some(Self::KeyDown),
            0x02 => Some(Self::KeyUp),
            0x10 => Some(Self::MouseMove),
            0x11 => Some(Self::MouseDown),
            0x12 => Some(Self::MouseUp),
            0x13 => Some(Self::MouseScroll),
            _ => None,
        }
    }
}

/// Commands enqueued for the main loop to apply on USB HID.
///
/// Kept `repr(C)` so we can push/pop them through Flipper's
/// `FuriMessageQueue` as opaque blobs (same size producer/consumer).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct HidCmd {
    pub kind: HidCmdKind,
    pub mods: u8,
    pub keycode: u8,
    pub dx: i8,
    pub dy: i8,
    pub mouse_button: u8,
    pub scroll: i8,
}

/// Discriminant for [`HidCmd`].
#[repr(u8)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum HidCmdKind {
    #[default]
    KeyDown = 0,
    KeyUp = 1,
    MouseMove = 2,
    MouseButtonDown = 3,
    MouseButtonUp = 4,
    MouseScroll = 5,
    /// Reserved for forced release-all (main loop / disconnect path).
    #[allow(dead_code)]
    Panic = 6,
}

/// Incremental stream parser.
pub struct ProtocolParser {
    /// Bytes waiting for a complete frame (or garbage before next magic).
    buffer: [u8; 256],
    /// How many valid bytes are currently in [`Self::buffer`].
    length: usize,
}

impl ProtocolParser {
    pub const fn new() -> Self {
        Self {
            buffer: [0; 256],
            length: 0,
        }
    }

    /// Feed raw BLE bytes. Returns how many frames were successfully decoded.
    ///
    /// Decoded commands are passed to `enqueue` (typically a message-queue put).
    /// If `enqueue` returns `false` (queue full), the frame is counted as dropped
    /// but we still consume it from the stream so we do not stall forever.
    pub fn feed<F>(&mut self, data: &[u8], mut enqueue: F) -> usize
    where
        F: FnMut(HidCmd) -> bool,
    {
        let mut frames = 0usize;

        for &byte in data {
            // Hard reset if somehow overrun — should not happen with 6-byte frames.
            if self.length >= self.buffer.len() {
                self.length = 0;
            }
            self.buffer[self.length] = byte;
            self.length += 1;

            // Try to pull as many complete frames as currently buffered.
            while self.length >= 3 {
                // --- resync: find magic FB 4B ---
                let mut sync = 0usize;
                while sync + 1 < self.length
                    && !(self.buffer[sync] == MAGIC_0 && self.buffer[sync + 1] == MAGIC_1)
                {
                    sync += 1;
                }
                if sync > 0 {
                    // Drop leading junk.
                    self.buffer.copy_within(sync..self.length, 0);
                    self.length -= sync;
                }
                if self.length < 3 {
                    break;
                }

                let payload_len = self.buffer[2];
                if payload_len != FRAME_PAYLOAD_LEN {
                    // Bad length — slide one byte and try again.
                    self.buffer.copy_within(1..self.length, 0);
                    self.length -= 1;
                    continue;
                }

                let total = 3 + payload_len as usize;
                if self.length < total {
                    // Need more BLE bytes for this frame.
                    break;
                }

                // We have a candidate frame in buffer[0..total].
                if let Some(cmd) = Self::dispatch_frame(&self.buffer[..total]) {
                    let _ = enqueue(cmd);
                    frames += 1;
                }

                // Consume the frame regardless of enqueue success / unknown event.
                self.buffer.copy_within(total..self.length, 0);
                self.length -= total;
            }
        }

        frames
    }

    /// Interpret one complete 6-byte frame into a HID command.
    fn dispatch_frame(frame: &[u8]) -> Option<HidCmd> {
        if frame.len() < FRAME_TOTAL_LEN {
            return None;
        }
        if frame[0] != MAGIC_0 || frame[1] != MAGIC_1 || frame[2] != FRAME_PAYLOAD_LEN {
            return None;
        }

        let event = EventKind::from_u8(frame[3])?;
        let mut cmd = HidCmd::default();

        match event {
            EventKind::KeyDown => {
                cmd.kind = HidCmdKind::KeyDown;
                cmd.mods = frame[4];
                cmd.keycode = frame[5];
            }
            EventKind::KeyUp => {
                cmd.kind = HidCmdKind::KeyUp;
                cmd.mods = frame[4];
                cmd.keycode = frame[5];
            }
            EventKind::MouseMove => {
                cmd.kind = HidCmdKind::MouseMove;
                cmd.dx = frame[4] as i8;
                cmd.dy = frame[5] as i8;
            }
            EventKind::MouseDown => {
                cmd.kind = HidCmdKind::MouseButtonDown;
                cmd.mouse_button = frame[4];
            }
            EventKind::MouseUp => {
                cmd.kind = HidCmdKind::MouseButtonUp;
                cmd.mouse_button = frame[4];
            }
            EventKind::MouseScroll => {
                cmd.kind = HidCmdKind::MouseScroll;
                cmd.scroll = frame[4] as i8;
            }
        }

        Some(cmd)
    }
}

impl Default for ProtocolParser {
    fn default() -> Self {
        Self::new()
    }
}
