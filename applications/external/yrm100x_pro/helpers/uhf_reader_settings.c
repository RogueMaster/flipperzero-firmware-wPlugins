#include "uhf_reader_settings.h"

#include "../app.h"
#include <flipper_format/flipper_format.h>

#define UHF_READER_SETTINGS_PATH      APP_DATA_PATH("ReaderSettings.ff")
#define UHF_READER_SETTINGS_TEMP_PATH APP_DATA_PATH("ReaderSettings.tmp")
#define UHF_READER_SETTINGS_FILE_TYPE "YRM100X_PRO Settings"
#define UHF_READER_SETTINGS_VERSION   11U

static bool uhf_reader_settings_valid(const UHFReaderSettings* settings) {
    return settings->save_on_write_index <= 1U && settings->region_index <= 4U &&
           settings->power_dbm >= UHF_READER_MIN_POWER_DBM &&
           settings->power_dbm <= UHF_READER_MAX_POWER_DBM && settings->session_index <= 3U &&
           settings->target_index <= 1U && settings->multi_full_dump_index <= 1U &&
           settings->auto_save_multi_index <= 1U &&
           settings->full_dump_attempts >= UHF_READER_FULL_DUMP_ATTEMPTS_MIN &&
           settings->full_dump_attempts <= UHF_READER_FULL_DUMP_ATTEMPTS_MAX &&
           settings->clone_attempts >= UHF_READER_CLONE_ATTEMPTS_MIN &&
           settings->clone_attempts <= UHF_READER_CLONE_ATTEMPTS_MAX &&
           settings->forever_delay_seconds >= UHF_READER_FOREVER_DELAY_MIN_SEC &&
           settings->forever_delay_seconds <= UHF_READER_FOREVER_DELAY_MAX_SEC &&
           settings->tag_profile_index < UHF_READER_TAG_PROFILE_COUNT;
}

void uhf_reader_settings_set_defaults(UHFReaderSettings* settings) {
    furi_assert(settings);
    settings->reader_profile_initialized = false;
    settings->save_on_write_index = 0;
    settings->region_index = USA_REGION;
    settings->power_dbm = UHF_READER_MAX_POWER_DBM;
    settings->session_index = 2;
    settings->target_index = 0;
    settings->default_access_password = 0;
    settings->multi_full_dump_index = 1;
    settings->auto_save_multi_index = 1;
    settings->full_dump_attempts = UHF_READER_FULL_DUMP_ATTEMPTS_DEFAULT;
    settings->clone_attempts = UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    settings->forever_delay_seconds = UHF_READER_FOREVER_DELAY_DEFAULT_SEC;
    settings->sound_enabled = true;
    settings->vibration_enabled = true;
    settings->tag_profile_index = UHF_READER_TAG_PROFILE_GENERIC;
}

bool uhf_reader_settings_load(Storage* storage, UHFReaderSettings* settings) {
    furi_assert(storage);
    furi_assert(settings);

    UHFReaderSettings loaded;
    uhf_reader_settings_set_defaults(&loaded);

    FlipperFormat* format = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t value = 0;
    uint8_t access_password[4] = {0};
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(format, UHF_READER_SETTINGS_PATH)) break;
        if(!flipper_format_read_header(format, file_type, &version)) break;
        if(version < 1U || version > UHF_READER_SETTINGS_VERSION) break;

        bool known_file_type = furi_string_equal_str(file_type, UHF_READER_SETTINGS_FILE_TYPE) ||
                               furi_string_equal_str(file_type, "UHF RFID Reader Settings");
        if(!known_file_type) break;

        if(!flipper_format_read_bool(
               format, "Reader Profile Initialized", &loaded.reader_profile_initialized, 1))
            break;

        if(!flipper_format_read_uint32(format, "Save On Write", &value, 1)) break;
        loaded.save_on_write_index = (uint8_t)value;

        if(!flipper_format_read_uint32(format, "Region", &value, 1)) break;
        loaded.region_index = (uint8_t)value;

        if(!flipper_format_read_uint32(format, "Power dBm", &value, 1)) break;
        loaded.power_dbm = (uint8_t)value;

        if(!flipper_format_read_uint32(format, "Session", &value, 1)) break;
        loaded.session_index = (uint8_t)value;

        if(!flipper_format_read_uint32(format, "Target", &value, 1)) break;
        loaded.target_index = (uint8_t)value;

        if(!flipper_format_read_hex(format, "Default Access Password", access_password, 4)) break;
        loaded.default_access_password = ((uint32_t)access_password[0] << 24) |
                                         ((uint32_t)access_password[1] << 16) |
                                         ((uint32_t)access_password[2] << 8) | access_password[3];

        // Version 1 predates the two multi-dump preferences; keep their defaults.
        if(version >= 2U) {
            if(!flipper_format_read_uint32(format, "Multi Full Dump", &value, 1)) break;
            loaded.multi_full_dump_index = (uint8_t)value;

            if(!flipper_format_read_uint32(format, "Auto Save Multi", &value, 1)) break;
            loaded.auto_save_multi_index = (uint8_t)value;
        }

        // Version 3 adds the number of whole-card FULL acquisition attempts.
        // Older settings files automatically inherit the default (4).
        if(version >= 3U) {
            if(!flipper_format_read_uint32(format, "Full Dump Attempts", &value, 1)) break;
            loaded.full_dump_attempts = (uint8_t)value;
        }

        if(version >= 4U) {
            if(!flipper_format_read_bool(format, "Sound Enabled", &loaded.sound_enabled, 1)) break;
            if(!flipper_format_read_bool(format, "Vibration Enabled", &loaded.vibration_enabled, 1))
                break;
        }

        // v5: software interpretation profile for manufacturer-specific layouts.
        if(version >= 5U) {
            if(!flipper_format_read_uint32(format, "Tag Profile", &value, 1)) break;
            loaded.tag_profile_index = (uint8_t)value;
        }

        // v6: delay between completed Read Forever captures.
        // Older settings files inherit the 2-second default.
        if(version >= 6U) {
            if(!flipper_format_read_uint32(format, "Forever Delay Seconds", &value, 1)) break;
            loaded.forever_delay_seconds = (uint8_t)value;
        }

        // v9: bounded attempts for each retryable Clone read/write stage.
        // Older settings files inherit the production default (5).
        if(version >= 9U) {
            if(!flipper_format_read_uint32(format, "Clone Attempts", &value, 1)) break;
            loaded.clone_attempts = (uint8_t)value;
        }

        /* v11 removes file logging. Extra Log Level keys in v7/v10 are ignored. */

        if(!uhf_reader_settings_valid(&loaded)) break;

        *settings = loaded;
        success = true;
    } while(false);

    flipper_format_file_close(format);
    furi_string_free(file_type);
    flipper_format_free(format);
    return success;
}

bool uhf_reader_settings_save(Storage* storage, const UHFReaderSettings* settings) {
    furi_assert(storage);
    furi_assert(settings);
    if(!uhf_reader_settings_valid(settings)) return false;

    FlipperFormat* format = flipper_format_file_alloc(storage);
    uint32_t value = 0;
    uint8_t access_password[4] = {
        (uint8_t)(settings->default_access_password >> 24),
        (uint8_t)(settings->default_access_password >> 16),
        (uint8_t)(settings->default_access_password >> 8),
        (uint8_t)settings->default_access_password,
    };
    bool success = false;

    storage_common_remove(storage, UHF_READER_SETTINGS_TEMP_PATH);
    do {
        if(!flipper_format_file_open_new(format, UHF_READER_SETTINGS_TEMP_PATH)) break;
        if(!flipper_format_write_header_cstr(
               format, UHF_READER_SETTINGS_FILE_TYPE, UHF_READER_SETTINGS_VERSION)) {
            break;
        }
        if(!flipper_format_write_bool(
               format, "Reader Profile Initialized", &settings->reader_profile_initialized, 1)) {
            break;
        }
        value = settings->save_on_write_index;
        if(!flipper_format_write_uint32(format, "Save On Write", &value, 1)) break;
        value = settings->region_index;
        if(!flipper_format_write_uint32(format, "Region", &value, 1)) break;
        value = settings->power_dbm;
        if(!flipper_format_write_uint32(format, "Power dBm", &value, 1)) break;
        value = settings->session_index;
        if(!flipper_format_write_uint32(format, "Session", &value, 1)) break;
        value = settings->target_index;
        if(!flipper_format_write_uint32(format, "Target", &value, 1)) break;
        if(!flipper_format_write_hex(
               format, "Default Access Password", access_password, sizeof(access_password))) {
            break;
        }

        value = settings->multi_full_dump_index;
        if(!flipper_format_write_uint32(format, "Multi Full Dump", &value, 1)) break;

        value = settings->auto_save_multi_index;
        if(!flipper_format_write_uint32(format, "Auto Save Multi", &value, 1)) break;

        value = settings->full_dump_attempts;
        if(!flipper_format_write_uint32(format, "Full Dump Attempts", &value, 1)) break;

        if(!flipper_format_write_bool(format, "Sound Enabled", &settings->sound_enabled, 1)) break;
        if(!flipper_format_write_bool(format, "Vibration Enabled", &settings->vibration_enabled, 1))
            break;

        value = settings->tag_profile_index;
        if(!flipper_format_write_uint32(format, "Tag Profile", &value, 1)) break;

        value = settings->forever_delay_seconds;
        if(!flipper_format_write_uint32(format, "Forever Delay Seconds", &value, 1)) break;

        value = settings->clone_attempts;
        if(!flipper_format_write_uint32(format, "Clone Attempts", &value, 1)) break;

        success = true;
    } while(false);

    flipper_format_file_close(format);
    flipper_format_free(format);
    if(!success) {
        storage_common_remove(storage, UHF_READER_SETTINGS_TEMP_PATH);
        return false;
    }
    if(storage_common_rename(storage, UHF_READER_SETTINGS_TEMP_PATH, UHF_READER_SETTINGS_PATH) !=
       FSE_OK) {
        storage_common_remove(storage, UHF_READER_SETTINGS_TEMP_PATH);
        return false;
    }
    return true;
}
