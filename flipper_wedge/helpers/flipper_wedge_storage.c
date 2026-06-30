#include "flipper_wedge_storage.h"
#include "flipper_wedge_keyboard_layout.h"

static Storage* flipper_wedge_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void flipper_wedge_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

static void flipper_wedge_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

void flipper_wedge_save_settings(void* context) {
    FlipperWedge* app = context;

    FURI_LOG_D(TAG, "Saving Settings");
    Storage* storage = flipper_wedge_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    // Overwrite wont work, so delete first
    if(storage_file_exists(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH)) {
        storage_simply_remove(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH);
    }

    // Open File, create if not exists
    if(!storage_common_stat(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH, NULL) == FSE_OK) {
        FURI_LOG_D(
            TAG, "Config file %s is not found. Will create new.", FLIPPER_WEDGE_SETTINGS_SAVE_PATH);
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            FURI_LOG_D(
                TAG, "Directory %s doesn't exist. Will create new.", CONFIG_FILE_DIRECTORY_PATH);
            if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
                FURI_LOG_E(TAG, "Error creating directory %s", CONFIG_FILE_DIRECTORY_PATH);
            }
        }
    }

    if(!flipper_format_file_open_new(fff_file, FLIPPER_WEDGE_SETTINGS_SAVE_PATH)) {
        //totp_close_config_file(fff_file);
        FURI_LOG_E(TAG, "Error creating new file %s", FLIPPER_WEDGE_SETTINGS_SAVE_PATH);
        flipper_wedge_close_storage();
        return;
    }

    // Store Settings - Track errors but write all settings
    bool save_success = true;

    if(!flipper_format_write_header_cstr(
        fff_file, FLIPPER_WEDGE_SETTINGS_HEADER, FLIPPER_WEDGE_SETTINGS_FILE_VERSION)) {
        FURI_LOG_E(TAG, "Failed to write header");
        save_success = false;
    }
    if(!flipper_format_write_string_cstr(
        fff_file, FLIPPER_WEDGE_SETTINGS_KEY_DELIMITER, app->delimiter)) {
        FURI_LOG_E(TAG, "Failed to write delimiter");
        save_success = false;
    }
    if(!flipper_format_write_bool(
        fff_file, FLIPPER_WEDGE_SETTINGS_KEY_APPEND_ENTER, &app->append_enter, 1)) {
        FURI_LOG_E(TAG, "Failed to write append_enter");
        save_success = false;
    }
    uint32_t mode = app->mode;
    if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_MODE, &mode, 1)) {
        FURI_LOG_E(TAG, "Failed to write mode");
        save_success = false;
    }
    uint32_t mode_startup = app->mode_startup_behavior;
    if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_MODE_STARTUP, &mode_startup, 1)) {
        FURI_LOG_E(TAG, "Failed to write mode_startup");
        save_success = false;
    }
    uint32_t output_mode = app->output_mode;
    if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_OUTPUT_MODE, &output_mode, 1)) {
        FURI_LOG_E(TAG, "Failed to write output_mode");
        save_success = false;
    }
    // Note: USB Debug Mode no longer saved (deprecated in favor of Output selector)
    uint32_t vibration = app->vibration_level;
    if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_VIBRATION, &vibration, 1)) {
        FURI_LOG_E(TAG, "Failed to write vibration");
        save_success = false;
    }
    uint32_t ndef_max_len = app->ndef_max_len;
    FURI_LOG_I(TAG, "Saving NDEF max len: %lu", ndef_max_len);
    if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_NDEF_MAX_LEN, &ndef_max_len, 1)) {
        FURI_LOG_E(TAG, "Failed to write ndef_max_len");
        save_success = false;
    }
    if(!flipper_format_write_bool(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LOG_TO_SD, &app->log_to_sd, 1)) {
        FURI_LOG_E(TAG, "Failed to write log_to_sd");
        save_success = false;
    }
    // Keyboard layout settings
    if(app->keyboard_layout) {
        uint32_t layout_type = app->keyboard_layout->type;
        if(!flipper_format_write_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_TYPE, &layout_type, 1)) {
            FURI_LOG_E(TAG, "Failed to write layout_type");
            save_success = false;
        }
        // Only save file path for custom layouts
        if(app->keyboard_layout->type == FlipperWedgeLayoutCustom) {
            if(!flipper_format_write_string_cstr(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_FILE, app->keyboard_layout->file_path)) {
                FURI_LOG_E(TAG, "Failed to write layout_file");
                save_success = false;
            }
        }
    }

    if(!flipper_format_rewind(fff_file)) {
        FURI_LOG_E(TAG, "Rewind error");
        save_success = false;
    }

    flipper_wedge_close_config_file(fff_file);
    flipper_wedge_close_storage();

    if(save_success) {
        FURI_LOG_I(TAG, "Settings saved successfully");
    } else {
        FURI_LOG_E(TAG, "Failed to save one or more settings!");
    }
}

void flipper_wedge_read_settings(void* context) {
    FlipperWedge* app = context;
    Storage* storage = flipper_wedge_open_storage();
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);

    // Migrate old settings file from hid_device path if it exists and new one doesn't
    if(storage_common_stat(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH_OLD, NULL) == FSE_OK &&
       storage_common_stat(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        FURI_LOG_I(TAG, "Migrating settings from old path");
        // Ensure new directory exists
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH);
        }
        // Copy old file to new location
        if(storage_common_copy(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH_OLD, FLIPPER_WEDGE_SETTINGS_SAVE_PATH) == FSE_OK) {
            FURI_LOG_I(TAG, "Settings migrated successfully");
        } else {
            FURI_LOG_E(TAG, "Failed to migrate settings");
        }
    }

    if(storage_common_stat(storage, FLIPPER_WEDGE_SETTINGS_SAVE_PATH, NULL) != FSE_OK) {
        flipper_wedge_close_config_file(fff_file);
        flipper_wedge_close_storage();
        return;
    }
    uint32_t file_version;
    FuriString* temp_str = furi_string_alloc();

    if(!flipper_format_file_open_existing(fff_file, FLIPPER_WEDGE_SETTINGS_SAVE_PATH)) {
        FURI_LOG_E(TAG, "Cannot open file %s", FLIPPER_WEDGE_SETTINGS_SAVE_PATH);
        flipper_wedge_close_config_file(fff_file);
        flipper_wedge_close_storage();
        furi_string_free(temp_str);
        return;
    }

    if(!flipper_format_read_header(fff_file, temp_str, &file_version)) {
        FURI_LOG_E(TAG, "Missing Header Data");
        flipper_wedge_close_config_file(fff_file);
        flipper_wedge_close_storage();
        furi_string_free(temp_str);
        return;
    }

    FURI_LOG_I(TAG, "Config file version: %lu (current: %d)", file_version, FLIPPER_WEDGE_SETTINGS_FILE_VERSION);
    furi_string_free(temp_str);

    if(file_version < FLIPPER_WEDGE_SETTINGS_FILE_VERSION) {
        FURI_LOG_W(TAG, "Old config version %lu (expected %d), settings will not be loaded", file_version, FLIPPER_WEDGE_SETTINGS_FILE_VERSION);
        flipper_wedge_close_config_file(fff_file);
        flipper_wedge_close_storage();
        return;
    }

    FuriString* delimiter_str = furi_string_alloc();
    if(flipper_format_read_string(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_DELIMITER, delimiter_str)) {
        strncpy(app->delimiter, furi_string_get_cstr(delimiter_str), FLIPPER_WEDGE_DELIMITER_MAX_LEN - 1);
        app->delimiter[FLIPPER_WEDGE_DELIMITER_MAX_LEN - 1] = '\0';
    }
    furi_string_free(delimiter_str);

    flipper_format_read_bool(
        fff_file, FLIPPER_WEDGE_SETTINGS_KEY_APPEND_ENTER, &app->append_enter, 1);

    uint32_t saved_mode = FlipperWedgeModeNfc;  // Default to NFC mode
    flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_MODE, &saved_mode, 1);

    // Read mode startup behavior (default to Remember)
    uint32_t mode_startup = FlipperWedgeModeStartupRemember;
    if(flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_MODE_STARTUP, &mode_startup, 1)) {
        // Validate mode startup is within valid range
        if(mode_startup < FlipperWedgeModeStartupCount) {
            app->mode_startup_behavior = (FlipperWedgeModeStartup)mode_startup;
        }
    }

    // Apply mode startup behavior logic
    if(app->mode_startup_behavior == FlipperWedgeModeStartupRemember) {
        // Use saved mode (if valid)
        if(saved_mode < FlipperWedgeModeCount) {
            app->mode = (FlipperWedgeMode)saved_mode;
        }
    } else {
        // Use default mode based on startup behavior
        switch(app->mode_startup_behavior) {
            case FlipperWedgeModeStartupDefaultNfc:
                app->mode = FlipperWedgeModeNfc;
                break;
            case FlipperWedgeModeStartupDefaultRfid:
                app->mode = FlipperWedgeModeRfid;
                break;
            case FlipperWedgeModeStartupDefaultNdef:
                app->mode = FlipperWedgeModeNdef;
                break;
            case FlipperWedgeModeStartupDefaultNfcRfid:
                app->mode = FlipperWedgeModeNfcThenRfid;
                break;
            case FlipperWedgeModeStartupDefaultRfidNfc:
                app->mode = FlipperWedgeModeRfidThenNfc;
                break;
            default:
                app->mode = FlipperWedgeModeNfc;
                break;
        }
    }

    // Read output mode setting (default to USB)
    // Note: Old file versions (< 5) are rejected above, so no migration needed
    // Current format: 0 = USB, 1 = BLE
    uint32_t output_mode = FlipperWedgeOutputUsb;  // Default to USB
    if(flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_OUTPUT_MODE, &output_mode, 1)) {
        // Validate output_mode is within valid range
        if(output_mode < FlipperWedgeOutputCount) {
            app->output_mode = (FlipperWedgeOutput)output_mode;
        } else {
            // Invalid value - default to USB
            FURI_LOG_W(TAG, "Invalid output mode %lu, defaulting to USB", output_mode);
            app->output_mode = FlipperWedgeOutputUsb;
        }
    }

    // Note: USB Debug Mode is deprecated and no longer read (removed to prevent breaking sequential reads)
    // Old file versions (< 5) with UsbDebug are rejected at line 113-117

    // Read vibration level setting (default to Medium for backward compatibility)
    uint32_t vibration = FlipperWedgeVibrationMedium;
    if(flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_VIBRATION, &vibration, 1)) {
        // Validate vibration level is within valid range
        if(vibration < FlipperWedgeVibrationCount) {
            app->vibration_level = (FlipperWedgeVibration)vibration;
        }
    }

    // Read NDEF max length setting (default to 250 chars for fast typing)
    uint32_t ndef_max_len = FlipperWedgeNdefMaxLen250;
    if(flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_NDEF_MAX_LEN, &ndef_max_len, 1)) {
        FURI_LOG_I(TAG, "Loaded NDEF max len from file: %lu", ndef_max_len);
        // Validate NDEF max length is within valid range
        // Migrate old "unlimited" value (2) to new max value (2)
        if(ndef_max_len < FlipperWedgeNdefMaxLenCount) {
            app->ndef_max_len = (FlipperWedgeNdefMaxLen)ndef_max_len;
            FURI_LOG_I(TAG, "Set NDEF max len to: %d", app->ndef_max_len);
        } else {
            FURI_LOG_E(TAG, "Invalid NDEF max len %lu (max %d), using default", ndef_max_len, FlipperWedgeNdefMaxLenCount);
        }
    } else {
        FURI_LOG_W(TAG, "NDEF max len not found in file, using default 250");
    }

    // Read log to SD setting (default to OFF for privacy/performance)
    flipper_format_read_bool(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LOG_TO_SD, &app->log_to_sd, 1);

    // Read keyboard layout settings (default to QWERTY)
    if(app->keyboard_layout) {
        uint32_t layout_type = FlipperWedgeLayoutDefault;
        if(flipper_format_read_uint32(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_TYPE, &layout_type, 1)) {
            if(layout_type < FlipperWedgeLayoutCount) {
                switch(layout_type) {
                    case FlipperWedgeLayoutDefault:
                        flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
                        break;
                    case FlipperWedgeLayoutNumPad:
                        flipper_wedge_keyboard_layout_set_numpad(app->keyboard_layout);
                        break;
                    case FlipperWedgeLayoutCustom: {
                        FuriString* layout_path = furi_string_alloc();
                        if(flipper_format_read_string(fff_file, FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_FILE, layout_path)) {
                            if(!flipper_wedge_keyboard_layout_load(app->keyboard_layout, furi_string_get_cstr(layout_path))) {
                                FURI_LOG_W(TAG, "Failed to load layout file, using default");
                                flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
                            }
                        } else {
                            FURI_LOG_W(TAG, "Layout file path not found, using default");
                            flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
                        }
                        furi_string_free(layout_path);
                        break;
                    }
                    default:
                        flipper_wedge_keyboard_layout_set_default(app->keyboard_layout);
                        break;
                }
            }
        }
    }

    flipper_format_rewind(fff_file);

    flipper_wedge_close_config_file(fff_file);
    flipper_wedge_close_storage();
}
