#include "nfc_login_hid.h"
#include "nfc_login_hid_usb.h"
#include "nfc_login_hid_ble.h"

// ============================================================================
// PUBLIC API - MODE-AWARE WRAPPERS
// ============================================================================

bool is_ble_hid_ready(void) {
#if HAS_BLE_HID_API
    return ble_hid_is_ready();
#else
    return false;
#endif
}

void release_all_keys_with_mode(HidMode mode) {
    if(mode == HidModeBle) {
        ble_hid_release_all_keys();
    } else {
        usb_hid_release_all_keys();
    }
}

bool initialize_hid_and_wait_with_mode(HidMode mode) {
    // CRITICAL: Route to correct implementation based on mode
    // BLE and USB are completely separate
    if(mode == HidModeBle) {
#ifdef BtIconHid
        furi_hal_bt_set_app_icon(BtIconHid);
#endif
        return ble_hid_init();
    } else {
        return usb_hid_init();
    }
}

void deinitialize_hid_with_restore_and_mode(FuriHalUsbInterface* previous_config, HidMode mode) {
    if(mode == HidModeBle) {
        release_all_keys_with_mode(HidModeBle);
        furi_delay_ms(HID_INIT_DELAY_MS);
        // Don't deinit BLE - keep it running for future scans
    } else {
        // Use USB module's deinit function
        usb_hid_deinit_with_restore(previous_config);
    }
}

uint32_t app_type_password(App* app, const char* password) {
    if(!password || !app) return 0;

    // CRITICAL: Check mode FIRST and route to correct implementation
    // BLE and USB are completely separate - no cross-contamination
    if(app->hid_mode == HidModeBle) {
        return ble_hid_type_password(app, password);
    } else {
        return usb_hid_type_password(app, password);
    }
}

void app_start_ble_advertising(void) {
    ble_hid_start_advertising();
}

void app_stop_ble_advertising(void) {
    ble_hid_stop_advertising();
}
