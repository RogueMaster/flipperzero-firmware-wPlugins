#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi_hal_usb_hid.h>

static inline bool clipboard_hid_release_all(void) {
    // Attempt both even if disconnected: the HAL clears its persistent report state before sending.
    bool keyboard = furi_hal_hid_kb_release_all();
    bool mouse = furi_hal_hid_mouse_release(UINT8_MAX);
    return keyboard && mouse;
}

static inline bool clipboard_hid_mouse_move(int8_t dx, int8_t dy) {
    // Move Mouse never means drag; do not move if the button-up report could not be sent.
    return furi_hal_hid_mouse_release(UINT8_MAX) && furi_hal_hid_mouse_move(dx, dy);
}
