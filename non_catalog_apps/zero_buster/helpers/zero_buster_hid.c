#include "zero_buster_hid.h"

const FuriHalUsbHidConfig spoof_configs[] = {
    [Spoof_Logitech_K120] = {
        .vid = 0x046D,
        .pid = 0xC31C,
        .manuf = "Logitech",
        .product = "Keyboard K120",
    },
    [Spoof_Dell_QuietKey] = {
        .vid = 0x413C,
        .pid = 0x2003,
        .manuf = "Dell",
        .product = "Dell QuietKey Keyboard",
    },
    [Spoof_Apple_Magic] = {
        .vid = 0x05AC,
        .pid = 0x0220,
        .manuf = "Apple Inc.",
        .product = "Magic Keyboard",
    },
    [Spoof_Flipper_Default] = {
        .vid = 0x0483,
        .pid = 0x5740,
        .manuf = "Flipper Devices Inc.",
        .product = "Flipper Zero HID",
    },
};

const char* spoof_names[] = {
    [Spoof_Logitech_K120] = "Logitech K120",
    [Spoof_Dell_QuietKey] = "Dell QuietKey",
    [Spoof_Apple_Magic] = "Apple Magic",
    [Spoof_Flipper_Default] = "Flipper Default",
    [Spoof_Custom] = "Custom (SD)",
};

const char* custom_mfg_presets[] = {
    "Custom Corp",
    "Logitech",
    "Dell",
    "Apple Inc.",
    "Microsoft",
    "HP",
    "Lenovo",
};

const char* custom_prod_presets[] = {
    "Custom Keyboard",
    "Keyboard K120",
    "QuietKey Keyboard",
    "Magic Keyboard",
    "USB Keyboard",
};

void zero_buster_type_char(char c, const uint16_t* keymap, uint32_t delay_ms) {
    uint16_t key = HID_KEYBOARD_NONE;
    if((uint8_t)c < LAYOUT_KEYS) {
        key = keymap ? keymap[(uint8_t)c] : HID_ASCII_TO_KEY(c);
    }

    if(key != HID_KEYBOARD_NONE) {
        furi_hal_hid_kb_press(key);
        furi_delay_ms(14);
        furi_hal_hid_kb_release(key);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(delay_ms);
    }
}

void zero_buster_type_string(
    const char* str,
    const uint16_t* keymap,
    uint32_t delay_ms,
    volatile bool* stop_flag) {
    while(*str && (!stop_flag || !*stop_flag)) {
        char c = *str++;
        if(c == '\r') continue;
        if(c == '\n') {
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(20);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
            furi_hal_hid_kb_release_all();
            furi_delay_ms(delay_ms * 2);
        } else {
            zero_buster_type_char(c, keymap, delay_ms);
        }
    }
}

uint8_t get_hex_nibble(uint32_t val, uint8_t nibble_idx) {
    uint8_t shift = (3 - (nibble_idx % 4)) * 4;
    return (val >> shift) & 0xF;
}

uint32_t set_hex_nibble(uint32_t val, uint8_t nibble_idx, uint8_t new_nibble) {
    uint8_t shift = (3 - (nibble_idx % 4)) * 4;
    uint32_t mask = ~((uint32_t)0xF << shift);
    return (val & mask) | (((uint32_t)(new_nibble & 0xF)) << shift);
}
