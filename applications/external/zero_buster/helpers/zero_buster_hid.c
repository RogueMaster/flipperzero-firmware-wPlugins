#include "zero_buster_hid.h"

const char* layout_names[] = {
    [Layout_ES] = "ES (Espanol)",
    [Layout_US] = "US (English)",
};

const FuriHalUsbHidConfig spoof_configs[] = {
    [Spoof_Logitech_K120] =
        {
            .vid = 0x046D,
            .pid = 0xC31C,
            .manuf = "Logitech",
            .product = "Keyboard K120",
        },
    [Spoof_Dell_QuietKey] =
        {
            .vid = 0x413C,
            .pid = 0x2003,
            .manuf = "Dell",
            .product = "Dell QuietKey Keyboard",
        },
    [Spoof_Apple_Magic] =
        {
            .vid = 0x05AC,
            .pid = 0x0220,
            .manuf = "Apple Inc.",
            .product = "Magic Keyboard",
        },
    [Spoof_Flipper_Default] =
        {
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

/* Spanish ISO Keyboard ASCII Scancode Translation Table */
static const uint16_t hid_asciimap_es[128] = {
    HID_KEYBOARD_NONE, // 0x00 NUL
    HID_KEYBOARD_NONE, // 0x01 SOH
    HID_KEYBOARD_NONE, // 0x02 STX
    HID_KEYBOARD_NONE, // 0x03 ETX
    HID_KEYBOARD_NONE, // 0x04 EOT
    HID_KEYBOARD_NONE, // 0x05 ENQ
    HID_KEYBOARD_NONE, // 0x06 ACK
    HID_KEYBOARD_NONE, // 0x07 BEL
    HID_KEYBOARD_DELETE, // 0x08 BS
    HID_KEYBOARD_TAB, // 0x09 TAB
    HID_KEYBOARD_RETURN, // 0x0A LF
    HID_KEYBOARD_NONE, // 0x0B VT
    HID_KEYBOARD_NONE, // 0x0C FF
    HID_KEYBOARD_NONE, // 0x0D CR
    HID_KEYBOARD_NONE, // 0x0E SO
    HID_KEYBOARD_NONE, // 0x0F SI
    HID_KEYBOARD_NONE, // 0x10 DEL
    HID_KEYBOARD_NONE, // 0x11 DC1
    HID_KEYBOARD_NONE, // 0x12 DC2
    HID_KEYBOARD_NONE, // 0x13 DC3
    HID_KEYBOARD_NONE, // 0x14 DC4
    HID_KEYBOARD_NONE, // 0x15 NAK
    HID_KEYBOARD_NONE, // 0x16 SYN
    HID_KEYBOARD_NONE, // 0x17 ETB
    HID_KEYBOARD_NONE, // 0x18 CAN
    HID_KEYBOARD_NONE, // 0x19 EM
    HID_KEYBOARD_NONE, // 0x1A SUB
    HID_KEYBOARD_NONE, // 0x1B ESC
    HID_KEYBOARD_NONE, // 0x1C FS
    HID_KEYBOARD_NONE, // 0x1D GS
    HID_KEYBOARD_NONE, // 0x1E RS
    HID_KEYBOARD_NONE, // 0x1F US
    HID_KEYBOARD_SPACEBAR, // 0x20 ' '
    HID_KEYBOARD_1 | KEY_MOD_LEFT_SHIFT, // 0x21 '!'
    HID_KEYBOARD_2 | KEY_MOD_LEFT_SHIFT, // 0x22 '"'
    HID_KEYBOARD_3 | KEY_MOD_RIGHT_ALT, // 0x23 '#' (AltGr + 3)
    HID_KEYBOARD_4 | KEY_MOD_LEFT_SHIFT, // 0x24 '$'
    HID_KEYBOARD_5 | KEY_MOD_LEFT_SHIFT, // 0x25 '%'
    HID_KEYBOARD_6 | KEY_MOD_LEFT_SHIFT, // 0x26 '&'
    HID_KEYBOARD_MINUS, // 0x27 '\'' (key next to 0)
    HID_KEYBOARD_8 | KEY_MOD_LEFT_SHIFT, // 0x28 '('
    HID_KEYBOARD_9 | KEY_MOD_LEFT_SHIFT, // 0x29 ')'
    HID_KEYBOARD_CLOSE_BRACKET | KEY_MOD_LEFT_SHIFT, // 0x2A '*'
    HID_KEYBOARD_CLOSE_BRACKET, // 0x2B '+'
    HID_KEYBOARD_COMMA, // 0x2C ','
    HID_KEYBOARD_SLASH, // 0x2D '-' (key next to right shift)
    HID_KEYBOARD_DOT, // 0x2E '.'
    HID_KEYBOARD_7 | KEY_MOD_LEFT_SHIFT, // 0x2F '/' (Shift + 7)
    HID_KEYBOARD_0, // 0x30 '0'
    HID_KEYBOARD_1, // 0x31 '1'
    HID_KEYBOARD_2, // 0x32 '2'
    HID_KEYBOARD_3, // 0x33 '3'
    HID_KEYBOARD_4, // 0x34 '4'
    HID_KEYBOARD_5, // 0x35 '5'
    HID_KEYBOARD_6, // 0x36 '6'
    HID_KEYBOARD_7, // 0x37 '7'
    HID_KEYBOARD_8, // 0x38 '8'
    HID_KEYBOARD_9, // 0x39 '9'
    HID_KEYBOARD_DOT | KEY_MOD_LEFT_SHIFT, // 0x3A ':' (Shift + .)
    HID_KEYBOARD_COMMA | KEY_MOD_LEFT_SHIFT, // 0x3B ';' (Shift + ,)
    0x64, // 0x3C '<' (key next to left shift)
    HID_KEYBOARD_0 | KEY_MOD_LEFT_SHIFT, // 0x3D '=' (Shift + 0)
    0x64 | KEY_MOD_LEFT_SHIFT, // 0x3E '>' (Shift + <)
    HID_KEYBOARD_MINUS | KEY_MOD_LEFT_SHIFT, // 0x3F '?' (Shift + ')
    HID_KEYBOARD_2 | KEY_MOD_RIGHT_ALT, // 0x40 '@' (AltGr + 2)
    HID_KEYBOARD_A | KEY_MOD_LEFT_SHIFT, // A
    HID_KEYBOARD_B | KEY_MOD_LEFT_SHIFT, // B
    HID_KEYBOARD_C | KEY_MOD_LEFT_SHIFT, // C
    HID_KEYBOARD_D | KEY_MOD_LEFT_SHIFT, // D
    HID_KEYBOARD_E | KEY_MOD_LEFT_SHIFT, // E
    HID_KEYBOARD_F | KEY_MOD_LEFT_SHIFT, // F
    HID_KEYBOARD_G | KEY_MOD_LEFT_SHIFT, // G
    HID_KEYBOARD_H | KEY_MOD_LEFT_SHIFT, // H
    HID_KEYBOARD_I | KEY_MOD_LEFT_SHIFT, // I
    HID_KEYBOARD_J | KEY_MOD_LEFT_SHIFT, // J
    HID_KEYBOARD_K | KEY_MOD_LEFT_SHIFT, // K
    HID_KEYBOARD_L | KEY_MOD_LEFT_SHIFT, // L
    HID_KEYBOARD_M | KEY_MOD_LEFT_SHIFT, // M
    HID_KEYBOARD_N | KEY_MOD_LEFT_SHIFT, // N
    HID_KEYBOARD_O | KEY_MOD_LEFT_SHIFT, // O
    HID_KEYBOARD_P | KEY_MOD_LEFT_SHIFT, // P
    HID_KEYBOARD_Q | KEY_MOD_LEFT_SHIFT, // Q
    HID_KEYBOARD_R | KEY_MOD_LEFT_SHIFT, // R
    HID_KEYBOARD_S | KEY_MOD_LEFT_SHIFT, // S
    HID_KEYBOARD_T | KEY_MOD_LEFT_SHIFT, // T
    HID_KEYBOARD_U | KEY_MOD_LEFT_SHIFT, // U
    HID_KEYBOARD_V | KEY_MOD_LEFT_SHIFT, // V
    HID_KEYBOARD_W | KEY_MOD_LEFT_SHIFT, // W
    HID_KEYBOARD_X | KEY_MOD_LEFT_SHIFT, // X
    HID_KEYBOARD_Y | KEY_MOD_LEFT_SHIFT, // Y
    HID_KEYBOARD_Z | KEY_MOD_LEFT_SHIFT, // Z
    HID_KEYBOARD_OPEN_BRACKET | KEY_MOD_RIGHT_ALT, // 0x5B '['
    HID_KEYBOARD_GRAVE_ACCENT | KEY_MOD_RIGHT_ALT, // 0x5C '\\' (AltGr + º)
    HID_KEYBOARD_CLOSE_BRACKET | KEY_MOD_RIGHT_ALT, // 0x5D ']'
    HID_KEYBOARD_OPEN_BRACKET | KEY_MOD_LEFT_SHIFT, // 0x5E '^'
    HID_KEYBOARD_SLASH | KEY_MOD_LEFT_SHIFT, // 0x5F '_' (Shift + -)
    HID_KEYBOARD_OPEN_BRACKET, // 0x60 '`'
    HID_KEYBOARD_A, // a
    HID_KEYBOARD_B, // b
    HID_KEYBOARD_C, // c
    HID_KEYBOARD_D, // d
    HID_KEYBOARD_E, // e
    HID_KEYBOARD_F, // f
    HID_KEYBOARD_G, // g
    HID_KEYBOARD_H, // h
    HID_KEYBOARD_I, // i
    HID_KEYBOARD_J, // j
    HID_KEYBOARD_K, // k
    HID_KEYBOARD_L, // l
    HID_KEYBOARD_M, // m
    HID_KEYBOARD_N, // n
    HID_KEYBOARD_O, // o
    HID_KEYBOARD_P, // p
    HID_KEYBOARD_Q, // q
    HID_KEYBOARD_R, // r
    HID_KEYBOARD_S, // s
    HID_KEYBOARD_T, // t
    HID_KEYBOARD_U, // u
    HID_KEYBOARD_V, // v
    HID_KEYBOARD_W, // w
    HID_KEYBOARD_X, // x
    HID_KEYBOARD_Y, // y
    HID_KEYBOARD_Z, // z
    HID_KEYBOARD_APOSTROPHE | KEY_MOD_RIGHT_ALT, // 0x7B '{'
    HID_KEYBOARD_1 | KEY_MOD_RIGHT_ALT, // 0x7C '|' (AltGr + 1)
    HID_KEYBOARD_BACKSLASH | KEY_MOD_RIGHT_ALT, // 0x7D '}'
    HID_KEYBOARD_4 | KEY_MOD_RIGHT_ALT, // 0x7E '~'
    HID_KEYBOARD_NONE, // 0x7F DEL
};

/* Type a single character taking the selected layout into account */
void zero_buster_type_char(char c, KeyboardLayout layout, uint32_t delay_ms) {
    uint16_t key = HID_KEYBOARD_NONE;
    if((uint8_t)c < 128) {
        if(layout == Layout_ES) {
            key = hid_asciimap_es[(uint8_t)c];
        } else {
            key = hid_asciimap[(uint8_t)c];
        }
    }

    if(key != HID_KEYBOARD_NONE) {
        furi_hal_hid_kb_press(key);
        furi_delay_ms(14);
        furi_hal_hid_kb_release(key);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(delay_ms);
    }
}

/* Type an entire string through HID */
void zero_buster_type_string(
    const char* str,
    KeyboardLayout layout,
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
            zero_buster_type_char(c, layout, delay_ms);
        }
    }
}

/* Hex nibble helpers for in-app HID editor */
uint8_t get_hex_nibble(uint32_t val, uint8_t nibble_idx) {
    uint8_t shift = (3 - (nibble_idx % 4)) * 4;
    return (val >> shift) & 0xF;
}

uint32_t set_hex_nibble(uint32_t val, uint8_t nibble_idx, uint8_t new_nibble) {
    uint8_t shift = (3 - (nibble_idx % 4)) * 4;
    uint32_t mask = ~((uint32_t)0xF << shift);
    return (val & mask) | (((uint32_t)(new_nibble & 0xF)) << shift);
}
