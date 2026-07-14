#include "nfc_login_settings.h"
#include "../storage/nfc_login_card_storage.h"
#include "../nfc_login_app.h"
#include "../hid/nfc_login_hid.h"

void app_load_keyboard_layout(App* app) {
    for(int i = 0; i < 128; i++) {
        app->layout[i] = HID_KEYBOARD_NONE;
    }

    for(char c = 'a'; c <= 'z'; c++) {
        app->layout[(unsigned char)c] = HID_KEYBOARD_A + (c - 'a');
    }
    for(char c = 'A'; c <= 'Z'; c++) {
        app->layout[(unsigned char)c] = (KEY_MOD_LEFT_SHIFT << 8) | (HID_KEYBOARD_A + (c - 'A'));
    }
    for(char c = '1'; c <= '9'; c++) {
        app->layout[(unsigned char)c] = HID_KEYBOARD_1 + (c - '1');
    }
    app->layout['0'] = HID_KEYBOARD_0;
    app->layout[' '] = HID_KEYBOARD_SPACEBAR;
    app->layout['\n'] = HID_KEYBOARD_RETURN;
    app->layout['\r'] = HID_KEYBOARD_RETURN;
    app->layout['\t'] = HID_KEYBOARD_TAB;

    app->layout['!'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_1;
    app->layout['@'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_2;
    app->layout['#'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_3;
    app->layout['$'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_4;
    app->layout['%'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_5;
    app->layout['^'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_6;
    app->layout['&'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_7;
    app->layout['*'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_8;
    app->layout['('] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_9;
    app->layout[')'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_0;

    app->layout['-'] = HID_KEYBOARD_MINUS;
    app->layout['_'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_MINUS;
    app->layout['='] = HID_KEYBOARD_EQUAL_SIGN;
    app->layout['+'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_EQUAL_SIGN;
    app->layout['['] = HID_KEYBOARD_OPEN_BRACKET;
    app->layout['{'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_OPEN_BRACKET;
    app->layout[']'] = HID_KEYBOARD_CLOSE_BRACKET;
    app->layout['}'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_CLOSE_BRACKET;
    app->layout['\\'] = HID_KEYBOARD_BACKSLASH;
    app->layout['|'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_BACKSLASH;
    app->layout[';'] = HID_KEYBOARD_SEMICOLON;
    app->layout[':'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_SEMICOLON;
    app->layout['\''] = HID_KEYBOARD_APOSTROPHE;
    app->layout['"'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_APOSTROPHE;
    app->layout['`'] = HID_KEYBOARD_GRAVE_ACCENT;
    app->layout['~'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_GRAVE_ACCENT;
    app->layout[','] = HID_KEYBOARD_COMMA;
    app->layout['<'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_COMMA;
    app->layout['.'] = HID_KEYBOARD_DOT;
    app->layout['>'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_DOT;
    app->layout['/'] = HID_KEYBOARD_SLASH;
    app->layout['?'] = (KEY_MOD_LEFT_SHIFT << 8) | HID_KEYBOARD_SLASH;

    app->layout_loaded = true;

    if(app->keyboard_layout[0] != '\0') {
        char layout_path[96];
        snprintf(
            layout_path, sizeof(layout_path), "%s/%s", BADUSB_LAYOUTS_DIR, app->keyboard_layout);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(storage) {
            File* file = storage_file_alloc(storage);
            if(file) {
                if(storage_file_open(file, layout_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                    storage_file_read(file, app->layout, sizeof(app->layout));
                    storage_file_close(file);
                }
                storage_file_free(file);
            }
            furi_record_close(RECORD_STORAGE);
        }
    }
}

void app_save_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    app_ensure_data_dir(storage);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, NFC_SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[128];
        snprintf(line, sizeof(line), "append_enter=%d\n", app->append_enter ? 1 : 0);
        storage_file_write(file, line, strlen(line));
        snprintf(line, sizeof(line), "input_delay=%d\n", app->input_delay_ms);
        storage_file_write(file, line, strlen(line));
        snprintf(line, sizeof(line), "keyboard_layout=%s\n", app->keyboard_layout);
        storage_file_write(file, line, strlen(line));
        snprintf(line, sizeof(line), "hid_mode=%d\n", (int)app->hid_mode);
        FURI_LOG_D(TAG, "app_save_settings: Writing hid_mode=%d", (int)app->hid_mode);
        storage_file_write(file, line, strlen(line));
        if(app->has_active_selection && app->active_card_index < app->card_count) {
            snprintf(line, sizeof(line), "active_card_index=%zu\n", app->active_card_index);
            storage_file_write(file, line, strlen(line));
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    app_load_keyboard_layout(app);
}

void app_load_settings(App* app) {
    app->append_enter = true;
    app->input_delay_ms = 10;
    strncpy(app->keyboard_layout, "en-US.kl", sizeof(app->keyboard_layout) - 1);
    app->keyboard_layout[sizeof(app->keyboard_layout) - 1] = '\0';
    app->selecting_keyboard_layout = false;
    app->layout_loaded = false;
    app->has_active_selection = false;
    app->active_card_index = 0;
// passcode_disabled is now stored encrypted in cards.enc header, not in settings.txt
// Default to USB, or force USB if BLE HID not available
#if HAS_BLE_HID_API
    app->hid_mode = HidModeUsb;
#else
    app->hid_mode = HidModeUsb; // BLE not available, force USB
#endif

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        app_load_keyboard_layout(app);
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        app_load_keyboard_layout(app);
        return;
    }

    if(storage_file_open(file, NFC_SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_D(TAG, "app_load_settings: Opened settings file");
        char line[128];
        size_t len = 0;
        uint8_t ch = 0;
        bool found_hid_mode = false;

        while(true) {
            len = 0;
            bool eof_reached = false;
            while(len < sizeof(line) - 1) {
                uint16_t rd = storage_file_read(file, &ch, 1);
                if(rd == 0) {
                    // End of file reached
                    eof_reached = true;
                    break;
                }
                if(ch == '\n') break;
                // Skip carriage return characters
                if(ch != '\r') {
                    line[len++] = (char)ch;
                }
            }
            if(len == 0) {
                // No more data to read
                if(eof_reached) break;
                // Check if we're at end of file
                if(storage_file_tell(file) >= storage_file_size(file)) break;
                continue;
            }
            line[len] = '\0';

            if(strncmp(line, "append_enter=", 13) == 0) {
                const char* value_str = line + 13;
                // Skip whitespace
                while(*value_str == ' ' || *value_str == '\t')
                    value_str++;
                int value = atoi(value_str);
                app->append_enter = (value != 0);
            } else if(strncmp(line, "input_delay=", 12) == 0) {
                const char* value_str = line + 12;
                // Skip whitespace
                while(*value_str == ' ' || *value_str == '\t')
                    value_str++;
                int value = atoi(value_str);
                if(value == 10 || value == 50 || value == 100 || value == 200) {
                    app->input_delay_ms = (uint16_t)value;
                }
            } else if(strncmp(line, "keyboard_delay=", 15) == 0) {
                const char* value_str = line + 15;
                // Skip whitespace
                while(*value_str == ' ' || *value_str == '\t')
                    value_str++;
                int value = atoi(value_str);
                if(value == 0 || value == 10) {
                    app->input_delay_ms = 10;
                } else if(value == 50) {
                    app->input_delay_ms = 50;
                } else if(value == 100) {
                    app->input_delay_ms = 100;
                } else if(value >= 200) {
                    app->input_delay_ms = 200;
                }
            } else if(strncmp(line, "keyboard_layout=", 16) == 0) {
                const char* layout_name = line + 16;
                // Skip leading whitespace
                while(*layout_name == ' ' || *layout_name == '\t') {
                    layout_name++;
                }
                // Copy the layout name, stopping at whitespace, newline, or null
                size_t i = 0;
                const char* src = layout_name;
                while(i < sizeof(app->keyboard_layout) - 1 && *src != '\0' && *src != ' ' &&
                      *src != '\t' && *src != '\n' && *src != '\r') {
                    app->keyboard_layout[i++] = *src++;
                }
                app->keyboard_layout[i] = '\0';
                // If we got an empty string, use default
                if(i == 0) {
                    strncpy(app->keyboard_layout, "en-US.kl", sizeof(app->keyboard_layout) - 1);
                    app->keyboard_layout[sizeof(app->keyboard_layout) - 1] = '\0';
                }
            } else if(strncmp(line, "active_card_index=", 18) == 0) {
                size_t index = (size_t)atoi(line + 18);
                app->active_card_index = index;
            } else if(strncmp(line, "passcode_disabled=", 18) == 0) {
                // passcode_disabled is now stored encrypted in cards.enc header, ignore this line
            } else if(strncmp(line, "hid_mode=", 9) == 0) {
                found_hid_mode = true;
                const char* value_str = line + 9;
                // Skip whitespace
                while(*value_str == ' ' || *value_str == '\t')
                    value_str++;
                // Parse value, stopping at whitespace, newline, or null
                char value_buf[16];
                size_t i = 0;
                while(i < sizeof(value_buf) - 1 && value_str[i] != '\0' && value_str[i] != ' ' &&
                      value_str[i] != '\t' && value_str[i] != '\n' && value_str[i] != '\r') {
                    value_buf[i] = value_str[i];
                    i++;
                }
                value_buf[i] = '\0';
                FURI_LOG_D(
                    TAG,
                    "app_load_settings: Found hid_mode line, value_buf='%s', i=%zu",
                    value_buf,
                    i);
#if HAS_BLE_HID_API
                FURI_LOG_W(TAG, "app_load_settings: HAS_BLE_HID_API is 1, entering parsing block");
                if(i > 0) { // Only parse if we got a value
                    // Parse explicitly - check for "0" or "1" first
                    bool value_set = false;
                    if(i == 1) {
                        if(value_buf[0] == '0') {
                            app->hid_mode = HidModeUsb;
                            value_set = true;
                            FURI_LOG_W(
                                TAG, "app_load_settings: Set hid_mode to USB (explicit '0')");
                        } else if(value_buf[0] == '1') {
                            app->hid_mode = HidModeBle;
                            value_set = true;
                            FURI_LOG_W(
                                TAG, "app_load_settings: Set hid_mode to BLE (explicit '1')");
                        }
                    }

                    if(!value_set) {
                        // Try atoi as fallback
                        int value = atoi(value_buf);
                        FURI_LOG_W(
                            TAG,
                            "app_load_settings: Using atoi fallback, parsed value: %d",
                            value);
                        if(value == 0) {
                            app->hid_mode = HidModeUsb;
                            FURI_LOG_W(TAG, "app_load_settings: Set hid_mode to USB (atoi=0)");
                        } else if(value == 1) {
                            app->hid_mode = HidModeBle;
                            FURI_LOG_W(TAG, "app_load_settings: Set hid_mode to BLE (atoi=1)");
                        } else {
                            // Invalid value - log and keep default
                            FURI_LOG_W(
                                TAG,
                                "app_load_settings: Invalid hid_mode value: %d, keeping default USB",
                                value);
                        }
                    }
                } else {
                    // No value found - log and keep default
                    FURI_LOG_W(
                        TAG,
                        "app_load_settings: No hid_mode value found in line (i=0), keeping default USB");
                }
#else
                // BLE HID not available - force USB mode
                FURI_LOG_W(TAG, "app_load_settings: HAS_BLE_HID_API is 0, forcing USB mode");
                app->hid_mode = HidModeUsb;
#endif
            }

            // Check if we've reached end of file
            if(eof_reached) break;
            if(storage_file_tell(file) >= storage_file_size(file)) break;
        }

        storage_file_close(file);
#if HAS_BLE_HID_API
        if(!found_hid_mode) {
            FURI_LOG_W(
                TAG,
                "app_load_settings: hid_mode line not found in settings file, using default USB");
        }
        FURI_LOG_W(TAG, "app_load_settings: Final hid_mode value: %d", (int)app->hid_mode);
#else
        (void)found_hid_mode; // Suppress unused variable warning when BLE not available
#endif
    } else {
        FURI_LOG_W(TAG, "app_load_settings: Failed to open settings file, using defaults");
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    app_load_keyboard_layout(app);
}
