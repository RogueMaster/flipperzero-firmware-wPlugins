#include "bv_hid.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#define TAG "BioVaultHid"

#define HID_ENUM_DELAY_MS     500 // host re-enumerate after mode switch
#define HID_ATTACH_TIMEOUT_MS 3000 // wait for host to attach HID
#define HID_SETTLE_MS         500 // let host HID driver come up before typing
#define HID_KEY_DELAY_MS      8 // press/release dwell per key
#define HID_DRAIN_MS          50 // flush last report before restoring USB

// Press then release one key (modifier bits in the keycode high byte).
static void bv_hid_tap(uint16_t key) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(HID_KEY_DELAY_MS);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(HID_KEY_DELAY_MS);
}

BvHidResult bv_hid_type(const char* text) {
    FuriHalUsbInterface* prev = furi_hal_usb_get_config();

    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        FURI_LOG_E(TAG, "USB mode switch locked");
        return BvHidBusy;
    }

    // Mode switch is async: let the host re-enumerate before polling.
    furi_delay_ms(HID_ENUM_DELAY_MS);

    uint32_t waited = 0;
    while(!furi_hal_hid_is_connected() && waited < HID_ATTACH_TIMEOUT_MS) {
        furi_delay_ms(50);
        waited += 50;
    }

    BvHidResult result = BvHidOk;
    if(!furi_hal_hid_is_connected()) {
        result = BvHidNoUsb;
    } else {
        // Interface configured != driver ready; wait or early chars drop.
        furi_delay_ms(HID_SETTLE_MS);
        for(const char* p = text; *p; p++) {
            uint16_t key = HID_ASCII_TO_KEY(*p);
            if(key == HID_KEYBOARD_NONE) continue; // skip unmappable chars
            bv_hid_tap(key);
        }
        furi_hal_hid_kb_release_all();
        furi_delay_ms(HID_DRAIN_MS);
    }

    // Restore previous USB mode (typically CDC / serial CLI).
    furi_hal_usb_set_config(prev, NULL);
    FURI_LOG_I(TAG, "type done: result=%d waited=%lu", result, (unsigned long)waited);
    return result;
}
