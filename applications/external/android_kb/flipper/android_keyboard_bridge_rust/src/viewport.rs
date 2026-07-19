//! Fullscreen viewport — RAII, safe methods, `unsafe` only for GUI FFI.

use core::ffi::c_void;
use core::ptr::{self, NonNull};

use flipperzero::gui::Gui;
use flipperzero_sys as sys;

pub type DrawCallback = unsafe extern "C" fn(*mut sys::Canvas, *mut c_void);

pub struct FullscreenView {
    gui: Gui,
    view_port: NonNull<sys::ViewPort>,
}

impl FullscreenView {
    pub fn new(draw: DrawCallback, ctx: *mut c_void) -> Self {
        let gui = Gui::open();
        // SAFETY: `view_port_alloc` always returns a valid port.
        let view_port = unsafe { NonNull::new_unchecked(sys::view_port_alloc()) };

        unsafe {
            sys::view_port_draw_callback_set(view_port.as_ptr(), Some(draw), ctx);
            sys::gui_add_view_port(gui.as_ptr(), view_port.as_ptr(), sys::GuiLayerFullscreen);
            sys::view_port_enabled_set(view_port.as_ptr(), true);
        }

        Self { gui, view_port }
    }

    pub fn update(&self) {
        // SAFETY: port is alive while `self` exists.
        unsafe {
            sys::view_port_update(self.view_port.as_ptr());
        }
    }
}

impl Drop for FullscreenView {
    fn drop(&mut self) {
        unsafe {
            sys::view_port_enabled_set(self.view_port.as_ptr(), false);
            sys::gui_remove_view_port(self.gui.as_ptr(), self.view_port.as_ptr());
            sys::view_port_free(self.view_port.as_ptr());
        }
        // Gui record closes via Drop.
        let _ = ptr::addr_of!(self.gui);
    }
}
