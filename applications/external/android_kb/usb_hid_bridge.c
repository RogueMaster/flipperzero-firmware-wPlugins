#include "usb_hid_bridge.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

static void* usb_mode_prev = NULL;

static uint16_t akb_pack_button(uint8_t mods, uint8_t keycode) {
    return ((uint16_t)mods << 8) | keycode;
}

void usb_hid_bridge_start(void) {
    if(furi_hal_usb_is_locked()) {
        return;
    }
    usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_set_config(NULL, NULL);
    furi_hal_usb_set_config(&usb_hid, NULL);
}

void usb_hid_bridge_stop(void) {
    // Only release keys while the host is still there — after unplug,
    // hid_send_report can sit on the IN semaphore until timeout.
    if(furi_hal_hid_is_connected()) {
        usb_hid_panic_release();
    }
    if(usb_mode_prev) {
        furi_hal_usb_set_config(usb_mode_prev, NULL);
        usb_mode_prev = NULL;
    }
}

bool usb_hid_bridge_is_usb_connected(void) {
    return furi_hal_hid_is_connected();
}

void usb_hid_panic_release(void) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_kb_release_all();
    furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT | HID_MOUSE_BTN_RIGHT | HID_MOUSE_BTN_WHEEL);
}

void usb_hid_key_down(uint8_t mods, uint8_t keycode) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_kb_press(akb_pack_button(mods, keycode));
}

void usb_hid_key_up(uint8_t mods, uint8_t keycode) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_kb_release(akb_pack_button(mods, keycode));
}

void usb_hid_mouse_move(int8_t dx, int8_t dy) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_mouse_move(dx, dy);
}

void usb_hid_mouse_button_down(uint8_t button) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_mouse_press(button);
}

void usb_hid_mouse_button_up(uint8_t button) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_mouse_release(button);
}

void usb_hid_mouse_scroll(int8_t delta) {
    if(!furi_hal_hid_is_connected()) {
        return;
    }
    furi_hal_hid_mouse_scroll(delta);
}
