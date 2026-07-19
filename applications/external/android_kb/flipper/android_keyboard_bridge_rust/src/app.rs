//! Application orchestration — **mostly safe Rust**.
//!
//! # Safe vs unsafe split
//!
//! | Layer | Safety | Responsibility |
//! |-------|--------|----------------|
//! | `protocol` | 100% safe | byte stream → [`HidCmd`] |
//! | `shared` | safe | UI state behind [`Mutex`] |
//! | `usb_hid` / `ble` / `viewport` | safe API | FFI hidden inside |
//! | `app` main loop | safe | queue pump, gaps, reclaim |
//! | `extern "C"` callbacks below | unsafe boundary | only convert pointers, then call safe methods |
//!
//! Flipper has no choice but to invoke C function pointers. We keep those
//! shims tiny: validate pointers, then jump into safe `App` methods.

use core::ffi::c_void;
use core::ptr;
use core::time::Duration;

use alloc::boxed::Box;
use alloc::format;

use flipperzero::furi::message_queue::MessageQueue;
use flipperzero::furi::sync::Mutex;
use flipperzero::furi::thread::sleep;
use flipperzero::furi::time::FuriDuration;
use flipperzero::notification::{NotificationApp, backlight};
use flipperzero_sys as sys;
use flipperzero_sys::furi::UnsafeRecord;

use crate::ble::{self, BleSerial};
use crate::protocol::{HidCmd, HidCmdKind};
use crate::shared::{ExitFlag, Shared, SharedCell};
use crate::usb_hid::UsbHid;
use crate::viewport::FullscreenView;

const HID_QUEUE_SIZE: usize = 64;
const HID_EVENT_GAP_MS: u32 = 25;
const RECORD_INPUT: &core::ffi::CStr = c"input_events";

/// Top-level FAP state. Heap-allocated so C callbacks keep a stable `*mut App`.
pub struct App {
    shared: SharedCell,
    exit: ExitFlag,
    queue: MessageQueue<HidCmd>,
    usb: UsbHid,
    ble: BleSerial,
    notifications: NotificationApp,
    view: Option<FullscreenView>,
    input_events: UnsafeRecord<sys::FuriPubSub>,
    input_subscription: *mut sys::FuriPubSubSubscription,
}

impl App {
    /// Construct services and register callbacks. Returns a boxed app.
    pub fn new() -> Box<Self> {
        let mut app = Box::new(Self {
            shared: Mutex::new(Shared::new()),
            exit: ExitFlag::new(),
            queue: MessageQueue::new(HID_QUEUE_SIZE),
            usb: UsbHid::new(),
            ble: BleSerial::open(),
            notifications: NotificationApp::open(),
            view: None,
            input_events: unsafe { UnsafeRecord::open(RECORD_INPUT) },
            input_subscription: ptr::null_mut(),
        });

        let ctx = app.as_mut() as *mut App as *mut c_void;
        app.view = Some(FullscreenView::new(draw_callback, ctx));

        // SAFETY: input pubsub + App lifetime; unsubscribed in Drop.
        unsafe {
            app.input_subscription = sys::furi_pubsub_subscribe(
                app.input_events.as_ptr(),
                Some(input_events_callback),
                ctx,
            );
        }

        app
    }

    fn view(&self) -> &FullscreenView {
        self.view.as_ref().expect("viewport installed in App::new")
    }

    /// Main loop — entirely safe aside from what callees encapsulate.
    pub fn run(&mut self) {
        self.usb.start();

        let ctx = self as *mut App as *mut c_void;
        match self.ble.start(bt_status_changed, serial_callback, ctx) {
            Ok(()) => {
                let mut g = self.shared.lock();
                g.phone_connected = false;
                g.status.set("Waiting for phone...");
            }
            Err(msg) => {
                self.shared.lock().status.set(msg);
            }
        }
        self.view().update();

        while !self.exit.is_set() {
            {
                let mut g = self.shared.lock();
                g.usb_connected = self.usb.is_connected();

                if g.reclaim_ticks > 0 {
                    self.ble.claim_rx(serial_callback, ctx);
                    g.reclaim_ticks -= 1;
                }

                if g.need_buffer_notify && self.ble.has_profile() {
                    g.need_buffer_notify = false;
                    drop(g);
                    self.ble.notify_buffer_empty();
                }
            }

            if self.exit.is_set() {
                break;
            }

            match self.queue.get(FuriDuration::from_millis(0)) {
                Ok(cmd) => {
                    let needs_gap =
                        cmd.kind == HidCmdKind::KeyDown || cmd.kind == HidCmdKind::KeyUp;
                    self.apply_hid_cmd(&cmd);
                    if needs_gap {
                        for _ in 0..HID_EVENT_GAP_MS {
                            if self.exit.is_set() {
                                break;
                            }
                            sleep(Duration::from_millis(1));
                        }
                    }
                }
                Err(_) => sleep(Duration::from_millis(5)),
            }

            self.view().update();
        }

        self.ble.stop();
        self.usb.stop();
    }

    /// Apply one HID command — safe path using [`UsbHid`].
    fn apply_hid_cmd(&mut self, cmd: &HidCmd) {
        match cmd.kind {
            HidCmdKind::KeyDown => self.usb.key_down(cmd.mods, cmd.keycode),
            HidCmdKind::KeyUp => self.usb.key_up(cmd.mods, cmd.keycode),
            HidCmdKind::MouseMove => self.usb.mouse_move(cmd.dx, cmd.dy),
            HidCmdKind::MouseButtonDown => self.usb.mouse_button_down(cmd.mouse_button),
            HidCmdKind::MouseButtonUp => self.usb.mouse_button_up(cmd.mouse_button),
            HidCmdKind::MouseScroll => self.usb.mouse_scroll(cmd.scroll),
            HidCmdKind::Panic => self.usb.panic_release(),
        }

        let mut g = self.shared.lock();
        g.hid_applied = g.hid_applied.wrapping_add(1);
        match cmd.kind {
            HidCmdKind::KeyDown => g.status.set(&format!("HID down key={:02X}", cmd.keycode)),
            HidCmdKind::KeyUp => g.status.set(&format!("HID up key={:02X}", cmd.keycode)),
            HidCmdKind::MouseMove => g.status.set(&format!("Mouse {:+},{}", cmd.dx, cmd.dy)),
            HidCmdKind::MouseButtonDown | HidCmdKind::MouseButtonUp => {
                g.status.set(&format!("Mouse btn {:02X}", cmd.mouse_button));
            }
            HidCmdKind::MouseScroll => g.status.set(&format!("Scroll {:+}", cmd.scroll)),
            HidCmdKind::Panic => g.status.set("HID panic release"),
        }
    }

    fn set_backlight_forced(&mut self, forced: bool) {
        {
            let mut g = self.shared.lock();
            if g.backlight_forced == forced {
                return;
            }
            g.backlight_forced = forced;
        }
        if forced {
            self.notifications
                .notify(&backlight::DISPLAY_BACKLIGHT_ENFORCE_ON);
        } else {
            self.notifications
                .notify(&backlight::DISPLAY_BACKLIGHT_ENFORCE_AUTO);
        }
    }

    // --- safe methods invoked from C shims ---------------------------------

    /// BLE Serial data arrived — parse (safe) and enqueue (safe MessageQueue).
    fn on_serial_bytes(&mut self, data: &[u8]) {
        // Disjoint borrows: queue vs shared mutex contents.
        let queue = &self.queue;
        let mut g = self.shared.lock();
        g.phone_connected = true;
        g.need_buffer_notify = true;

        let frames = g.parser.feed(data, |cmd| {
            queue.put(cmd, FuriDuration::from_millis(0)).is_ok()
        });
        g.frames_received = g.frames_received.wrapping_add(frames as u32);
        g.status.set(&format!("Last RX: {} bytes", data.len()));
    }

    fn on_bt_status(&mut self, status: sys::BtStatus) {
        let ctx = self as *mut App as *mut c_void;
        if status == sys::BtStatusConnected {
            self.ble.claim_rx(serial_callback, ctx);
            self.shared.lock().reclaim_ticks = 20;
        }

        let mut g = self.shared.lock();
        if status == sys::BtStatusConnected {
            g.phone_connected = true;
            g.status.set("Phone linked — type now");
        } else if status == sys::BtStatusAdvertising {
            g.phone_connected = false;
            g.status.set("Waiting for phone...");
        } else {
            g.phone_connected = false;
            g.status.set(&format!("BLE status: {}", status.0));
        }
        drop(g);
        self.view().update();
    }

    fn on_input(&mut self, event: &sys::InputEvent) {
        if event.key == sys::InputKeyBack
            && (event.type_ == sys::InputTypeShort
                || event.type_ == sys::InputTypeLong
                || event.type_ == sys::InputTypePress)
        {
            self.exit.request();
            return;
        }
        if event.key == sys::InputKeyUp && event.type_ == sys::InputTypeShort {
            let next = !self.shared.lock().backlight_forced;
            self.set_backlight_forced(next);
        }
    }

    fn draw(&self, canvas: *mut sys::Canvas) {
        let g = self.shared.lock();
        // SAFETY: canvas pointer comes from GUI draw callback for this frame.
        unsafe {
            sys::canvas_clear(canvas);
            sys::canvas_set_font(canvas, sys::FontPrimary);
            sys::canvas_draw_str_aligned(
                canvas,
                64,
                2,
                sys::AlignCenter,
                sys::AlignTop,
                c"Android KB Bridge (Rust)".as_ptr(),
            );
            sys::canvas_set_font(canvas, sys::FontSecondary);
            sys::canvas_draw_str(canvas, 2, 18, g.status.as_ptr());

            let usb = if g.usb_connected {
                c"USB: connected"
            } else {
                c"USB: waiting"
            };
            sys::canvas_draw_str(canvas, 2, 30, usb.as_ptr());

            let phone = if g.phone_connected {
                c"Phone: connected"
            } else {
                c"Phone: waiting"
            };
            sys::canvas_draw_str(canvas, 2, 42, phone.as_ptr());
        }

        let counter = format!("Frames:{} HID:{}", g.frames_received, g.hid_applied);
        let mut counter_buf = [0u8; 32];
        let n = counter.as_bytes().len().min(counter_buf.len() - 1);
        counter_buf[..n].copy_from_slice(&counter.as_bytes()[..n]);

        let hint = if g.backlight_forced {
            c"Up=light* Back=exit"
        } else {
            c"Up=light  Back=exit"
        };

        unsafe {
            sys::canvas_draw_str(canvas, 2, 54, counter_buf.as_ptr().cast());
            sys::canvas_draw_str(canvas, 2, 62, hint.as_ptr());
        }
    }
}

impl Drop for App {
    fn drop(&mut self) {
        self.set_backlight_forced(false);
        if !self.input_subscription.is_null() {
            // SAFETY: pair with subscribe in `new`.
            unsafe {
                sys::furi_pubsub_unsubscribe(self.input_events.as_ptr(), self.input_subscription);
            }
            self.input_subscription = ptr::null_mut();
        }
        // view / ble / usb / queue / records drop via their own Drop impls.
    }
}

// =============================================================================
// Unsafe C boundary — intentionally tiny
// =============================================================================

unsafe fn app_from_ctx<'a>(context: *mut c_void) -> Option<&'a mut App> {
    if context.is_null() {
        None
    } else {
        // SAFETY: context is Box<App> pinned for the FAP lifetime.
        Some(unsafe { &mut *(context as *mut App) })
    }
}

unsafe extern "C" fn draw_callback(canvas: *mut sys::Canvas, context: *mut c_void) {
    if canvas.is_null() {
        return;
    }
    if let Some(app) = unsafe { app_from_ctx(context) } {
        app.draw(canvas);
    }
}

unsafe extern "C" fn input_events_callback(value: *const c_void, context: *mut c_void) {
    if value.is_null() {
        return;
    }
    let Some(app) = (unsafe { app_from_ctx(context) }) else {
        return;
    };
    // SAFETY: pubsub delivers a live InputEvent for the duration of the callback.
    let event = unsafe { &*(value as *const sys::InputEvent) };
    app.on_input(event);
}

unsafe extern "C" fn serial_callback(
    event: sys::SerialServiceEvent,
    context: *mut c_void,
) -> u16 {
    let Some(app) = (unsafe { app_from_ctx(context) }) else {
        return ble::SERIAL_BUFFER_SIZE;
    };

    if event.event == sys::SerialServiceEventTypeDataReceived {
        let size = event.data.size as usize;
        // SAFETY: buffer valid for this callback only; we parse before returning.
        let slice = if event.data.buffer.is_null() || size == 0 {
            &[][..]
        } else {
            unsafe { core::slice::from_raw_parts(event.data.buffer, size) }
        };
        app.on_serial_bytes(slice);
    }

    ble::SERIAL_BUFFER_SIZE
}

unsafe extern "C" fn bt_status_changed(status: sys::BtStatus, context: *mut c_void) {
    if let Some(app) = unsafe { app_from_ctx(context) } {
        app.on_bt_status(status);
    }
}
