//! Android KB Bridge — **Rust** FAP focused on *safe* Rust structure.
//!
//! Flipper firmware is C. Some `unsafe` is unavoidable at the FFI boundary.
//! Everything else — protocol parsing, queues, shared state, main loop —
//! is written as ordinary safe Rust. See `app.rs` for the layering table.
//!
//! Build:
//!
//! ```bash
//! make flipper-rust-build
//! ```

#![no_main]
#![no_std]

extern crate flipperzero_rt;
extern crate flipperzero_alloc;
extern crate alloc;

mod app;
mod ble;
mod protocol;
mod shared;
mod usb_hid;
mod viewport;

use core::ffi::CStr;

use flipperzero_rt::{entry, manifest};

use app::App;

// Metadata embedded in the `.fap` (shown in Flipper's app list).
manifest!(
    name = "Android KB Bridge RS",
    app_version = 1,
    has_icon = false,
);

entry!(main);

fn main(_args: Option<&CStr>) -> i32 {
    // Safe construction + safe run loop; Drop cleans up.
    let mut app = App::new();
    app.run();
    0
}
