//! USB HID — **safe public API**, `unsafe` only inside this module.
//!
//! Callers never write `unsafe` to type or move the mouse. Firmware FFI is
//! confined here with explicit `SAFETY` comments.

use core::ptr::{self, NonNull};

use flipperzero_sys as sys;

/// Mouse button bitmasks (firmware `furi_hal_usb_hid.h`).
pub const MOUSE_BTN_LEFT: u8 = 1 << 0;
pub const MOUSE_BTN_RIGHT: u8 = 1 << 1;
pub const MOUSE_BTN_WHEEL: u8 = 1 << 2;

/// Owns the previous USB config so Drop / [`UsbHid::stop`] can restore it.
pub struct UsbHid {
    prev: Option<NonNull<sys::FuriHalUsbInterface>>,
    active: bool,
}

impl UsbHid {
    pub const fn new() -> Self {
        Self {
            prev: None,
            active: false,
        }
    }

    /// Switch Flipper USB into stock HID keyboard/mouse mode.
    pub fn start(&mut self) {
        // SAFETY: firmware USB HAL; no aliasing of the saved config pointer.
        unsafe {
            if sys::furi_hal_usb_is_locked() {
                return;
            }
            let prev = sys::furi_hal_usb_get_config();
            self.prev = NonNull::new(prev);
            sys::furi_hal_usb_set_config(ptr::null_mut(), ptr::null_mut());
            sys::furi_hal_usb_set_config(ptr::addr_of_mut!(sys::usb_hid), ptr::null_mut());
            self.active = true;
        }
    }

    pub fn stop(&mut self) {
        self.panic_release();
        if let Some(prev) = self.prev.take() {
            // SAFETY: `prev` was returned by `furi_hal_usb_get_config` earlier.
            unsafe {
                sys::furi_hal_usb_set_config(prev.as_ptr(), ptr::null_mut());
            }
        }
        self.active = false;
    }

    pub fn is_connected(&self) -> bool {
        // SAFETY: pure query into USB HID state.
        unsafe { sys::furi_hal_hid_is_connected() }
    }

    pub fn panic_release(&self) {
        // SAFETY: release-all is idempotent on the HID stack.
        unsafe {
            let _ = sys::furi_hal_hid_kb_release_all();
            let _ = sys::furi_hal_hid_mouse_release(
                MOUSE_BTN_LEFT | MOUSE_BTN_RIGHT | MOUSE_BTN_WHEEL,
            );
        }
    }

    #[inline]
    fn pack(mods: u8, keycode: u8) -> u16 {
        ((mods as u16) << 8) | (keycode as u16)
    }

    pub fn key_down(&self, mods: u8, keycode: u8) {
        if !self.is_connected() {
            return;
        }
        // SAFETY: host accepted HID; button encoding matches firmware convention.
        unsafe {
            let _ = sys::furi_hal_hid_kb_press(Self::pack(mods, keycode));
        }
    }

    pub fn key_up(&self, mods: u8, keycode: u8) {
        if !self.is_connected() {
            return;
        }
        unsafe {
            let _ = sys::furi_hal_hid_kb_release(Self::pack(mods, keycode));
        }
    }

    pub fn mouse_move(&self, dx: i8, dy: i8) {
        if !self.is_connected() {
            return;
        }
        unsafe {
            let _ = sys::furi_hal_hid_mouse_move(dx, dy);
        }
    }

    pub fn mouse_button_down(&self, button: u8) {
        if !self.is_connected() {
            return;
        }
        unsafe {
            let _ = sys::furi_hal_hid_mouse_press(button);
        }
    }

    pub fn mouse_button_up(&self, button: u8) {
        if !self.is_connected() {
            return;
        }
        unsafe {
            let _ = sys::furi_hal_hid_mouse_release(button);
        }
    }

    pub fn mouse_scroll(&self, delta: i8) {
        if !self.is_connected() {
            return;
        }
        unsafe {
            let _ = sys::furi_hal_hid_mouse_scroll(delta);
        }
    }
}

impl Default for UsbHid {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for UsbHid {
    fn drop(&mut self) {
        if self.active {
            self.stop();
        }
    }
}
