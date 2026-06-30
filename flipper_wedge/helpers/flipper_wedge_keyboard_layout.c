#include "flipper_wedge_keyboard_layout.h"
#include <flipper_format/flipper_format.h>
#include <lib/toolbox/path.h>

#define TAG "FlipperWedgeKeyboardLayout"

#define LAYOUT_FILE_TYPE "Flipper Wedge Keyboard Layout"
#define LAYOUT_FILE_VERSION 1

// NumPad keycodes from hid_usage_keyboard.h
// NOTE: Digits 0-9 (0x62, 0x59-0x61) are standard HID numpad keycodes.
// Hex letters A-F (0xBC-0xC1) are NON-STANDARD extended keycodes.
// Standard USB HID does not define numpad A-F keys, so these may not
// work on all operating systems or applications. NumPad mode is primarily
// useful for typing digits 0-9 on systems where the number row layout differs.
#define HID_KEYPAD_NUMLOCK 0x53
#define HID_KEYPAD_0       0x62
#define HID_KEYPAD_1       0x59
#define HID_KEYPAD_2       0x5A
#define HID_KEYPAD_3       0x5B
#define HID_KEYPAD_4       0x5C
#define HID_KEYPAD_5       0x5D
#define HID_KEYPAD_6       0x5E
#define HID_KEYPAD_7       0x5F
#define HID_KEYPAD_8       0x60
#define HID_KEYPAD_9       0x61
#define HID_KEYPAD_A       0xBC  // Non-standard
#define HID_KEYPAD_B       0xBD  // Non-standard
#define HID_KEYPAD_C       0xBE  // Non-standard
#define HID_KEYPAD_D       0xBF  // Non-standard
#define HID_KEYPAD_E       0xC0  // Non-standard
#define HID_KEYPAD_F       0xC1  // Non-standard

static const char* layout_type_names[] = {
    [FlipperWedgeLayoutDefault] = "Default (QWERTY)",
    [FlipperWedgeLayoutNumPad] = "NumPad",
    [FlipperWedgeLayoutCustom] = "Custom",
};

FlipperWedgeKeyboardLayout* flipper_wedge_keyboard_layout_alloc(void) {
    FlipperWedgeKeyboardLayout* layout = malloc(sizeof(FlipperWedgeKeyboardLayout));
    flipper_wedge_keyboard_layout_set_default(layout);
    return layout;
}

void flipper_wedge_keyboard_layout_free(FlipperWedgeKeyboardLayout* layout) {
    furi_assert(layout);
    free(layout);
}

void flipper_wedge_keyboard_layout_set_default(FlipperWedgeKeyboardLayout* layout) {
    furi_assert(layout);

    memset(layout, 0, sizeof(FlipperWedgeKeyboardLayout));
    strncpy(layout->name, "Default (QWERTY)", FLIPPER_WEDGE_LAYOUT_NAME_MAX - 1);
    layout->type = FlipperWedgeLayoutDefault;
    // All mappings left as undefined - will use firmware default
}

void flipper_wedge_keyboard_layout_set_numpad(FlipperWedgeKeyboardLayout* layout) {
    furi_assert(layout);

    memset(layout, 0, sizeof(FlipperWedgeKeyboardLayout));
    strncpy(layout->name, "NumPad", FLIPPER_WEDGE_LAYOUT_NAME_MAX - 1);
    layout->type = FlipperWedgeLayoutNumPad;

    // Map digits 0-9 to numpad keycodes
    layout->map['0'].keycode = HID_KEYPAD_0;
    layout->map['0'].defined = true;
    layout->map['1'].keycode = HID_KEYPAD_1;
    layout->map['1'].defined = true;
    layout->map['2'].keycode = HID_KEYPAD_2;
    layout->map['2'].defined = true;
    layout->map['3'].keycode = HID_KEYPAD_3;
    layout->map['3'].defined = true;
    layout->map['4'].keycode = HID_KEYPAD_4;
    layout->map['4'].defined = true;
    layout->map['5'].keycode = HID_KEYPAD_5;
    layout->map['5'].defined = true;
    layout->map['6'].keycode = HID_KEYPAD_6;
    layout->map['6'].defined = true;
    layout->map['7'].keycode = HID_KEYPAD_7;
    layout->map['7'].defined = true;
    layout->map['8'].keycode = HID_KEYPAD_8;
    layout->map['8'].defined = true;
    layout->map['9'].keycode = HID_KEYPAD_9;
    layout->map['9'].defined = true;

    // Map hex letters A-F (uppercase)
    layout->map['A'].keycode = HID_KEYPAD_A;
    layout->map['A'].defined = true;
    layout->map['B'].keycode = HID_KEYPAD_B;
    layout->map['B'].defined = true;
    layout->map['C'].keycode = HID_KEYPAD_C;
    layout->map['C'].defined = true;
    layout->map['D'].keycode = HID_KEYPAD_D;
    layout->map['D'].defined = true;
    layout->map['E'].keycode = HID_KEYPAD_E;
    layout->map['E'].defined = true;
    layout->map['F'].keycode = HID_KEYPAD_F;
    layout->map['F'].defined = true;

    // Map hex letters a-f (lowercase) - same keycodes, no shift
    layout->map['a'].keycode = HID_KEYPAD_A;
    layout->map['a'].defined = true;
    layout->map['b'].keycode = HID_KEYPAD_B;
    layout->map['b'].defined = true;
    layout->map['c'].keycode = HID_KEYPAD_C;
    layout->map['c'].defined = true;
    layout->map['d'].keycode = HID_KEYPAD_D;
    layout->map['d'].defined = true;
    layout->map['e'].keycode = HID_KEYPAD_E;
    layout->map['e'].defined = true;
    layout->map['f'].keycode = HID_KEYPAD_F;
    layout->map['f'].defined = true;
}

bool flipper_wedge_keyboard_layout_load(FlipperWedgeKeyboardLayout* layout, const char* path) {
    furi_assert(layout);
    furi_assert(path);

    FURI_LOG_I(TAG, "Loading layout from: %s", path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(file, path)) {
            FURI_LOG_E(TAG, "Failed to open file: %s", path);
            break;
        }

        // Check file type and version
        FuriString* file_type = furi_string_alloc();
        uint32_t version = 0;

        if(!flipper_format_read_header(file, file_type, &version)) {
            FURI_LOG_E(TAG, "Failed to read header");
            furi_string_free(file_type);
            break;
        }

        if(furi_string_cmp_str(file_type, LAYOUT_FILE_TYPE) != 0) {
            FURI_LOG_E(TAG, "Invalid file type: %s", furi_string_get_cstr(file_type));
            furi_string_free(file_type);
            break;
        }

        if(version > LAYOUT_FILE_VERSION) {
            FURI_LOG_E(TAG, "Unsupported version: %lu", version);
            furi_string_free(file_type);
            break;
        }

        furi_string_free(file_type);

        // Clear layout and set type
        memset(layout->map, 0, sizeof(layout->map));
        layout->type = FlipperWedgeLayoutCustom;
        strncpy(layout->file_path, path, FLIPPER_WEDGE_LAYOUT_PATH_MAX - 1);

        // Read layout name
        FuriString* name = furi_string_alloc();
        if(flipper_format_read_string(file, "Name", name)) {
            strncpy(layout->name, furi_string_get_cstr(name), FLIPPER_WEDGE_LAYOUT_NAME_MAX - 1);
        } else {
            // Use filename as fallback name
            FuriString* filename = furi_string_alloc();
            path_extract_filename_no_ext(path, filename);
            strncpy(layout->name, furi_string_get_cstr(filename), FLIPPER_WEDGE_LAYOUT_NAME_MAX - 1);
            furi_string_free(filename);
            // Rewind to continue reading mappings
            flipper_format_rewind(file);
            // Skip header again
            uint32_t dummy_ver;
            FuriString* dummy_type = furi_string_alloc();
            flipper_format_read_header(file, dummy_type, &dummy_ver);
            furi_string_free(dummy_type);
        }
        furi_string_free(name);

        // Read character mappings
        // Format: single character key, value is "keycode" or "keycode SHIFT"
        FuriString* value = furi_string_alloc();

        // Only lookup characters we actually need for hex UIDs and delimiters
        // (27 lookups instead of 95 - ~3.5x faster)
        static const char* chars_to_lookup = "0123456789ABCDEFabcdef-:,.;";
        for(size_t i = 0; chars_to_lookup[i] != '\0'; i++) {
            char c = chars_to_lookup[i];
            char key[2] = {c, '\0'};

            if(flipper_format_read_string(file, key, value)) {
                // Parse value: "keycode" or "keycode SHIFT"
                const char* val_str = furi_string_get_cstr(value);
                uint32_t keycode = 0;
                bool has_shift = false;

                // Check for SHIFT modifier
                if(strstr(val_str, "SHIFT") != NULL || strstr(val_str, "shift") != NULL) {
                    has_shift = true;
                }

                // Parse keycode (decimal or hex)
                if(val_str[0] == '0' && (val_str[1] == 'x' || val_str[1] == 'X')) {
                    keycode = strtoul(val_str, NULL, 16);
                } else {
                    keycode = strtoul(val_str, NULL, 10);
                }

                if(keycode > 0 && keycode < 256) {
                    layout->map[(uint8_t)c].keycode = keycode;
                    if(has_shift) {
                        layout->map[(uint8_t)c].keycode |= KEY_MOD_LEFT_SHIFT;
                    }
                    layout->map[(uint8_t)c].defined = true;
                    FURI_LOG_D(TAG, "Mapped '%c' -> 0x%04X", c, layout->map[(uint8_t)c].keycode);
                }
            }

            // Rewind after each read attempt to try next key
            flipper_format_rewind(file);
            // Skip header
            uint32_t dummy_ver;
            FuriString* dummy_type = furi_string_alloc();
            flipper_format_read_header(file, dummy_type, &dummy_ver);
            furi_string_free(dummy_type);
            // Skip Name if present
            FuriString* dummy_name = furi_string_alloc();
            flipper_format_read_string(file, "Name", dummy_name);
            furi_string_free(dummy_name);
        }

        furi_string_free(value);
        success = true;

        FURI_LOG_I(TAG, "Loaded layout: %s", layout->name);

    } while(false);

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

uint16_t flipper_wedge_keyboard_layout_get_keycode(FlipperWedgeKeyboardLayout* layout, char c) {
    furi_assert(layout);

    uint8_t index = (uint8_t)c;
    if(index >= 128) {
        return HID_KEYBOARD_NONE;
    }

    // If character is mapped in layout, use that
    if(layout->map[index].defined) {
        return layout->map[index].keycode;
    }

    // Otherwise fall back to firmware default
    return HID_ASCII_TO_KEY(c);
}

const char* flipper_wedge_keyboard_layout_type_name(FlipperWedgeLayoutType type) {
    if(type >= FlipperWedgeLayoutCount) {
        return "Unknown";
    }
    return layout_type_names[type];
}

size_t flipper_wedge_keyboard_layout_list(
    Storage* storage,
    FuriString** names,
    FuriString** paths,
    size_t max_count) {
    furi_assert(storage);
    furi_assert(names);
    furi_assert(paths);

    size_t count = 0;

    // Ensure directory exists
    if(!storage_dir_exists(storage, FLIPPER_WEDGE_LAYOUTS_DIRECTORY)) {
        FURI_LOG_I(TAG, "Creating layouts directory");
        storage_simply_mkdir(storage, FLIPPER_WEDGE_LAYOUTS_DIRECTORY);
        return 0;
    }

    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, FLIPPER_WEDGE_LAYOUTS_DIRECTORY)) {
        FURI_LOG_E(TAG, "Failed to open layouts directory");
        storage_file_free(dir);
        return 0;
    }

    FileInfo file_info;
    char filename[256];

    while(count < max_count && storage_dir_read(dir, &file_info, filename, sizeof(filename))) {
        // Skip directories
        if(file_info.flags & FSF_DIRECTORY) {
            continue;
        }

        // Only process .txt files
        size_t len = strlen(filename);
        if(len < 4 || strcmp(filename + len - 4, ".txt") != 0) {
            continue;
        }

        // Build full path
        FuriString* full_path = furi_string_alloc();
        furi_string_printf(full_path, "%s/%s", FLIPPER_WEDGE_LAYOUTS_DIRECTORY, filename);

        // Try to read layout name from file
        FlipperFormat* file = flipper_format_file_alloc(storage);
        FuriString* layout_name = furi_string_alloc();
        bool got_name = false;

        if(flipper_format_file_open_existing(file, furi_string_get_cstr(full_path))) {
            FuriString* file_type = furi_string_alloc();
            uint32_t version = 0;

            if(flipper_format_read_header(file, file_type, &version)) {
                if(furi_string_cmp_str(file_type, LAYOUT_FILE_TYPE) == 0) {
                    if(flipper_format_read_string(file, "Name", layout_name)) {
                        got_name = true;
                    }
                }
            }
            furi_string_free(file_type);
        }

        flipper_format_file_close(file);
        flipper_format_free(file);

        // Use filename without extension as fallback name
        if(!got_name) {
            furi_string_set_str(layout_name, filename);
            furi_string_left(layout_name, len - 4);  // Remove .txt
        }

        // Store name and path
        names[count] = layout_name;
        paths[count] = full_path;
        count++;

        FURI_LOG_D(TAG, "Found layout: %s at %s",
            furi_string_get_cstr(layout_name),
            furi_string_get_cstr(full_path));
    }

    storage_dir_close(dir);
    storage_file_free(dir);

    FURI_LOG_I(TAG, "Found %zu custom layouts", count);
    return count;
}
