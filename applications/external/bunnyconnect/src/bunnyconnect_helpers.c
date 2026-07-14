#include "../lib/bunnyconnect_helpers.h"
#include <furi.h>
#include <furi_hal_usb_hid.h>

void bunnyconnect_send_key_press(uint16_t key) {
    if(furi_hal_hid_is_connected()) {
        furi_hal_hid_kb_press(key);
    }
}

void bunnyconnect_send_key_release(uint16_t key) {
    if(furi_hal_hid_is_connected()) {
        furi_hal_hid_kb_release(key);
    }
}

void bunnyconnect_send_string(const char* string) {
    if(!string || !furi_hal_hid_is_connected()) return;

    size_t len = strlen(string);
    for(size_t i = 0; i < len; i++) {
        uint16_t key = bunnyconnect_char_to_hid_key(string[i]);
        if(key != HID_KEYBOARD_NONE) {
            bunnyconnect_send_key_press(key);
            furi_delay_ms(10);
            bunnyconnect_send_key_release(key);
            furi_delay_ms(10);
        }
    }
}

void bunnyconnect_send_enter(void) {
    if(furi_hal_hid_is_connected()) {
        bunnyconnect_send_key_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(10);
        bunnyconnect_send_key_release(HID_KEYBOARD_RETURN);
    }
}

void bunnyconnect_send_backspace(void) {
    if(furi_hal_hid_is_connected()) {
        bunnyconnect_send_key_press(HID_KEYBOARD_DELETE);
        furi_delay_ms(10);
        bunnyconnect_send_key_release(HID_KEYBOARD_DELETE);
    }
}

uint16_t bunnyconnect_char_to_hid_key(char c) {
    if((uint8_t)c < 128) {
        return hid_asciimap[(uint8_t)c];
    }
    return HID_KEYBOARD_NONE;
}
