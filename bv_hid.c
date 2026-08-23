#include "bv_hid.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#define TAG "BioVaultHid"

#define HID_ENUM_DELAY_MS     500 // let the host re-enumerate after the mode switch
#define HID_ATTACH_TIMEOUT_MS 3000 // how long to wait for the host to attach HID
#define HID_SETTLE_MS         500 // let the host's HID driver come up before typing
#define HID_KEY_DELAY_MS      8 // press/release dwell per key
#define HID_DRAIN_MS          50 // let the last report flush before restoring USB

// Press then release one key (modifier bits are carried in the high byte of the
// keycode, matching hid_asciimap's encoding).
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

    // The mode switch is asynchronous: give the host time to re-enumerate the
    // device as an HID keyboard before we start polling for the connection.
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
        // is_connected only means the interface is configured; the host's HID
        // driver still needs a moment before it routes keystrokes, or the first
        // characters (or the whole short string) are dropped.
        furi_delay_ms(HID_SETTLE_MS);
        for(const char* p = text; *p; p++) {
            uint16_t key = HID_ASCII_TO_KEY(*p);
            if(key == HID_KEYBOARD_NONE) continue; // skip unmappable chars
            bv_hid_tap(key);
        }
        furi_hal_hid_kb_release_all();
        furi_delay_ms(HID_DRAIN_MS); // flush the last report before tearing down
    }

    // Restore the previous USB mode (typically CDC / the serial CLI).
    furi_hal_usb_set_config(prev, NULL);
    FURI_LOG_I(TAG, "type done: result=%d waited=%lu", result, (unsigned long)waited);
    return result;
}
