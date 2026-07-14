#include "keyboard.h"

uint16_t get_key(char c) {
    if(c >= 'A' && c <= 'Z') {
        return HID_KEYBOARD_A + (c - 'A');
    }

    if(c >= 'a' && c <= 'z') {
        return HID_KEYBOARD_A + (c - 'a');
    }

    switch(c) {
    case '0':
        return HID_KEYBOARD_0;
    case '1':
        return HID_KEYBOARD_1;
    case '2':
        return HID_KEYBOARD_2;
    case '3':
        return HID_KEYBOARD_3;
    case '4':
        return HID_KEYBOARD_4;
    case '5':
        return HID_KEYBOARD_5;
    case '6':
        return HID_KEYBOARD_6;
    case '7':
        return HID_KEYBOARD_7;
    case '8':
        return HID_KEYBOARD_8;
    case '9':
        return HID_KEYBOARD_9;
    case ' ':
        return HID_KEYBOARD_SPACEBAR;
    case '\n':
        return HID_KEYBOARD_RETURN;
    }

    return 0;
}

bool is_uppercase(char c) {
    return (c >= 'A' && c <= 'Z');
}

void capslock() {
    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(KB_PRESS_DELAY);
}

void numlock() {
    furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(KB_PRESS_DELAY);
}

void write_char(char c) {
    uint16_t key = get_key(c);
    bool uppercase = is_uppercase(c);

    if(c == 0) {
        return;
    }

    if(uppercase) {
        capslock();
    }

    furi_hal_hid_kb_press(key);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(KB_PRESS_DELAY);

    if(uppercase) {
        capslock();
    }
}

void write_string(const char* str, size_t len) {
    for(size_t i = 0; i < len; i++) {
        write_char(str[i]);
    }
}
