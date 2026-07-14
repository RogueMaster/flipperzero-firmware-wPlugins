#include "badusb.h"

void initialize_hid(void) {
    // Stop other USB modes that might be active
    furi_hal_usb_unlock();

    // Set USB to HID keyboard mode
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL));

    // Give system time to enumerate device
    furi_delay_ms(100);
}

// Release all keys (important after sending key combinations)
void release_all_keys(void) {
    furi_hal_hid_kb_release_all();
}

// Type a string
void type_string(const char* str) {
    for(size_t i = 0; i < strlen(str); i++) {
        furi_hal_hid_kb_press(HID_ASCII_TO_KEY(str[i]));
        furi_hal_hid_kb_release(HID_ASCII_TO_KEY(str[i]));
        furi_delay_ms(10); // Small delay between keypresses
    }
}

// Press a simple key
void press_key(uint8_t key) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(50);
    furi_hal_hid_kb_release(key);
}

// Press a key combination (e.g., CTRL+ALT+DELETE)
// void press_key_combo(uint16_t mod, uint8_t key) {
//     furi_hal_hid_kb_press_modifier(mod);
//     furi_hal_hid_kb_press(key);
//     furi_delay_ms(50);
//     furi_hal_hid_kb_release(key);
//     furi_hal_hid_kb_release_modifier(mod);
// }
