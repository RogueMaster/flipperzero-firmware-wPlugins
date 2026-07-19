//! Thin RAII + safe methods around Flipper BLE Serial.
//!
//! All `unsafe` for `bt_*` / `ble_profile_serial_*` stays inside this file.
//! Callers use ordinary methods without `unsafe` blocks.

use core::ffi::c_void;
use core::ptr::{self, NonNull};
use core::time::Duration;

use flipperzero::furi::thread::sleep;
use flipperzero_sys as sys;
use flipperzero_sys::furi::UnsafeRecord;

const RECORD_BT: &core::ffi::CStr = c"bt";

pub const SERIAL_BUFFER_SIZE: u16 = 128;

pub type SerialCallback = unsafe extern "C" fn(sys::SerialServiceEvent, *mut c_void) -> u16;
pub type StatusCallback = unsafe extern "C" fn(sys::BtStatus, *mut c_void);

/// Owns the BT record and optional active Serial profile.
pub struct BleSerial {
    bt: UnsafeRecord<sys::Bt>,
    profile: Option<NonNull<sys::FuriHalBleProfileBase>>,
}

impl BleSerial {
    pub fn open() -> Self {
        Self {
            // SAFETY: `bt` record name is stable in firmware.
            bt: unsafe { UnsafeRecord::open(RECORD_BT) },
            profile: None,
        }
    }

    pub fn is_radio_on() -> bool {
        // SAFETY: query only.
        unsafe { sys::furi_hal_bt_is_active() }
    }

    pub fn set_status_callback(&self, cb: Option<StatusCallback>, ctx: *mut c_void) {
        // SAFETY: callback must outlive the profile session; App guarantees that.
        unsafe {
            sys::bt_set_status_changed_callback(self.bt.as_ptr(), cb, ctx);
        }
    }

    /// Start Serial profile, claim RX, advertise.
    pub fn start(
        &mut self,
        status_cb: StatusCallback,
        serial_cb: SerialCallback,
        ctx: *mut c_void,
    ) -> Result<(), &'static str> {
        if !Self::is_radio_on() {
            return Err("Enable Bluetooth");
        }

        self.set_status_callback(Some(status_cb), ctx);
        // SAFETY: disconnect before profile swap (same as C FAP).
        unsafe {
            sys::bt_disconnect(self.bt.as_ptr());
        }
        sleep(Duration::from_millis(200));

        // SAFETY: `ble_profile_serial` is a static firmware template.
        let profile = unsafe {
            sys::bt_profile_start(self.bt.as_ptr(), sys::ble_profile_serial, ptr::null_mut())
        };
        let profile = NonNull::new(profile).ok_or("BLE profile failed")?;
        self.profile = Some(profile);

        self.claim_rx(serial_cb, ctx);
        // SAFETY: advertising after profile start.
        unsafe {
            sys::furi_hal_bt_start_advertising();
        }
        Ok(())
    }

    pub fn claim_rx(&self, serial_cb: SerialCallback, ctx: *mut c_void) {
        let Some(profile) = self.profile else {
            return;
        };
        // SAFETY: profile is live for this session; callback/ctx owned by App.
        unsafe {
            sys::ble_profile_serial_set_event_callback(
                profile.as_ptr(),
                SERIAL_BUFFER_SIZE,
                Some(serial_cb),
                ctx,
            );
            sys::ble_profile_serial_set_rpc_active(profile.as_ptr(), true);
            sys::ble_profile_serial_notify_buffer_is_empty(profile.as_ptr());
        }
    }

    /// Safe to call from the **main loop** only (not from Serial RX).
    pub fn notify_buffer_empty(&self) {
        let Some(profile) = self.profile else {
            return;
        };
        // SAFETY: must not re-enter buff_size_mtx from inside RX callback.
        unsafe {
            sys::ble_profile_serial_notify_buffer_is_empty(profile.as_ptr());
        }
    }

    pub fn stop(&mut self) {
        self.set_status_callback(None, ptr::null_mut());
        if let Some(profile) = self.profile.take() {
            // SAFETY: clear callbacks before restoring default profile.
            unsafe {
                sys::ble_profile_serial_set_rpc_active(profile.as_ptr(), false);
                sys::ble_profile_serial_set_event_callback(
                    profile.as_ptr(),
                    0,
                    None,
                    ptr::null_mut(),
                );
                let _ = sys::bt_profile_restore_default(self.bt.as_ptr());
            }
        }
    }

    pub fn has_profile(&self) -> bool {
        self.profile.is_some()
    }
}

impl Drop for BleSerial {
    fn drop(&mut self) {
        self.stop();
    }
}
