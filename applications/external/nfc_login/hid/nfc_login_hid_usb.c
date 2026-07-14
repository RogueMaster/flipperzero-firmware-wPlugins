#include "nfc_login_hid_usb.h"
#include "../settings/nfc_login_settings.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <usb_hid.h>
#include <furi.h>

static FuriHalUsbInterface* g_usb_previous_config = NULL;

bool usb_hid_init(void) {
    g_usb_previous_config = furi_hal_usb_get_config();
    furi_hal_usb_unlock();

    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        return false;
    }

    // Wait for USB connection
    uint8_t retries = HID_CONNECT_TIMEOUT_MS / HID_CONNECT_RETRY_MS;
    for(uint8_t i = 0; i < retries; i++) {
        if(furi_hal_hid_is_connected()) {
            return true;
        }
        furi_delay_ms(HID_CONNECT_RETRY_MS);
    }

    return false;
}

static void usb_hid_deinit_common(FuriHalUsbInterface* config) {
    furi_hal_hid_kb_release_all();
    furi_delay_ms(HID_INIT_DELAY_MS);

    if(config) {
        furi_hal_usb_set_config(config, NULL);
    } else {
        furi_hal_usb_unlock();
    }

    furi_delay_ms(HID_SETTLE_DELAY_MS);
}

void usb_hid_deinit(void) {
    FuriHalUsbInterface* config = g_usb_previous_config;
    g_usb_previous_config = NULL;
    usb_hid_deinit_common(config);
}

void usb_hid_deinit_with_restore(FuriHalUsbInterface* previous_config) {
    usb_hid_deinit_common(previous_config);
}

void usb_hid_release_all_keys(void) {
    furi_hal_hid_kb_release_all();
}

void usb_hid_press_key(uint16_t keycode) {
    furi_hal_hid_kb_press(keycode);
}

void usb_hid_release_key(uint16_t keycode) {
    furi_hal_hid_kb_release(keycode);
}

uint32_t usb_hid_type_password(App* app, const char* password) {
    if(!password || !app) return 0;

    if(!app->layout_loaded) {
        app_load_keyboard_layout(app);
    }

    uint32_t approx_typed_ms = 0;

    // Type each character using USB ONLY
    for(size_t i = 0; password[i] != '\0'; i++) {
        unsigned char uc = (unsigned char)password[i];
        if(uc >= 128) continue;

        uint16_t full_keycode = app->layout[uc];
        if(full_keycode != HID_KEYBOARD_NONE) {
            usb_hid_press_key(full_keycode);
            furi_delay_ms(KEY_PRESS_DELAY_MS);
            usb_hid_release_key(full_keycode);
            furi_delay_ms(KEY_RELEASE_DELAY_MS);
            approx_typed_ms += KEY_PRESS_DELAY_MS + KEY_RELEASE_DELAY_MS;

            furi_delay_ms(app->input_delay_ms);
            approx_typed_ms += app->input_delay_ms;
        }
    }

    // Press Enter if needed
    if(app->append_enter) {
        usb_hid_press_key(HID_KEYBOARD_RETURN);
        furi_delay_ms(ENTER_PRESS_DELAY_MS);
        usb_hid_release_key(HID_KEYBOARD_RETURN);
        furi_delay_ms(ENTER_RELEASE_DELAY_MS);
        approx_typed_ms += ENTER_PRESS_DELAY_MS + ENTER_RELEASE_DELAY_MS;
    }

    return approx_typed_ms;
}
