// helpers/protopirate_settings.c
#include "protopirate_settings.h"
#include "protopirate_storage.h"
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include "../defines.h"
#include "../protocols/protocols_common.h"

#define TAG "ProtoPirateSettings"

#define SETTINGS_FILE_HEADER  "ProtoPirate Settings"
#define SETTINGS_FILE_VERSION 1

void protopirate_settings_set_defaults(ProtoPirateSettings* settings) {
    settings->frequency = 433920000;
    settings->preset_index = 0;
    settings->tx_power = 0;
    settings->auto_save = false;
    settings->hopping_enabled = false;
    settings->emulate_feature_enabled = false;
    settings->check_saved = false;
}

void protopirate_settings_load(ProtoPirateSettings* settings) {
    // Set defaults first
    protopirate_settings_set_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_existing(ff, PROTOPIRATE_SETTINGS_FILE)) {
            FURI_LOG_I(TAG, "Settings file not found, using defaults");
            break;
        }

        FuriString* header = furi_string_alloc();
        uint32_t version = 0;

        if(!flipper_format_read_header(ff, header, &version)) {
            FURI_LOG_W(TAG, "Failed to read settings header");
            furi_string_free(header);
            break;
        }

        if(furi_string_cmp_str(header, SETTINGS_FILE_HEADER) != 0) {
            FURI_LOG_W(TAG, "Invalid settings file header");
            furi_string_free(header);
            break;
        }

        furi_string_free(header);

        if(version != SETTINGS_FILE_VERSION) {
            FURI_LOG_I(
                TAG,
                "Migrating settings from version %lu to %u",
                (unsigned long)version,
                SETTINGS_FILE_VERSION);
        }

        // Read frequency
        if(!flipper_format_read_uint32(ff, FF_FREQUENCY, &settings->frequency, 1)) {
            FURI_LOG_W(TAG, "Failed to read frequency, using default");
            settings->frequency = 433920000;
        }

        // Read preset index
        uint32_t preset_temp = 0;
        if(!flipper_format_read_uint32(ff, "PresetIndex", &preset_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read preset index, using default");
            preset_temp = 0;
        }
        settings->preset_index = (uint8_t)preset_temp;

        // Read auto-save
        uint32_t auto_save_temp = 0;
        if(!flipper_format_read_uint32(ff, "AutoSave", &auto_save_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read auto-save, using default");
            auto_save_temp = 0;
        }
        settings->auto_save = (auto_save_temp == 1);

        // Read tx-power
        uint32_t tx_power_temp = 0;
        if(!flipper_format_read_uint32(ff, "TXPower", &tx_power_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read TXPower, using default");
            tx_power_temp = 0;
        }

        if(tx_power_temp > PROTOPIRATE_TX_POWER_MAX_INDEX) {
            FURI_LOG_W(TAG, "TXPower %lu out of range, clamping", (unsigned long)tx_power_temp);
            tx_power_temp = PROTOPIRATE_TX_POWER_MAX_INDEX;
        }
        settings->tx_power = (uint8_t)tx_power_temp;

        // Read hopping
        uint32_t hopping_temp = 0;
        if(!flipper_format_read_uint32(ff, "Hopping", &hopping_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read hopping, using default");
            hopping_temp = 0;
        }
        settings->hopping_enabled = (hopping_temp == 1);

#ifdef ENABLE_EMULATE_FEATURE
        uint32_t emulate_temp = 0;
        if(!flipper_format_read_uint32(ff, "EmulateFeature", &emulate_temp, 1)) {
            FURI_LOG_I(TAG, "EmulateFeature key missing, defaulting to disabled");
            emulate_temp = 0;
        }
        settings->emulate_feature_enabled = (emulate_temp == 1);
#endif

        uint32_t check_saved_temp = 0;
        if(!flipper_format_read_uint32(ff, "CheckSaved", &check_saved_temp, 1)) {
            check_saved_temp = 0;
        }
        settings->check_saved = (check_saved_temp == 1);

        FURI_LOG_I(
            TAG,
            "Settings loaded: freq=%lu, preset=%u, auto_save=%d, hopping=%d, emulate=%d, check_saved=%d",
            settings->frequency,
            settings->preset_index,
            settings->auto_save,
            settings->hopping_enabled,
            settings->emulate_feature_enabled,
            settings->check_saved);

    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void protopirate_settings_save(ProtoPirateSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    if(!storage_simply_mkdir(storage, PROTOPIRATE_SETTINGS_DIR)) {
        FURI_LOG_W(TAG, "Settings directory could not be created");
    }

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool write_ok = false;

    const char* tmp_path = PROTOPIRATE_SETTINGS_FILE ".tmp";

    do {
        if(!flipper_format_file_open_always(ff, tmp_path)) {
            FURI_LOG_E(TAG, "Failed to open settings file for writing");
            break;
        }

        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILE_HEADER, SETTINGS_FILE_VERSION)) {
            FURI_LOG_E(TAG, "Failed to write settings header");
            break;
        }

        if(!flipper_format_write_uint32(ff, FF_FREQUENCY, &settings->frequency, 1)) {
            FURI_LOG_E(TAG, "Failed to write frequency");
            break;
        }

        uint32_t preset_temp = settings->preset_index;
        if(!flipper_format_write_uint32(ff, "PresetIndex", &preset_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write preset index");
            break;
        }

        uint32_t auto_save_temp = settings->auto_save ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "AutoSave", &auto_save_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write auto-save");
            break;
        }

        uint32_t tx_power_temp = settings->tx_power;
        if(!flipper_format_write_uint32(ff, "TXPower", &tx_power_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write TX Power");
            break;
        }

        uint32_t hopping_temp = settings->hopping_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "Hopping", &hopping_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write hopping");
            break;
        }

#ifdef ENABLE_EMULATE_FEATURE
        uint32_t emulate_temp = settings->emulate_feature_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "EmulateFeature", &emulate_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write emulate feature flag");
            break;
        }
#endif

        uint32_t check_saved_temp = settings->check_saved ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "CheckSaved", &check_saved_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write check saved");
            break;
        }

        write_ok = true;

        FURI_LOG_I(
            TAG,
            "Settings saved: freq=%lu, preset=%u, auto_save=%d, hopping=%d, emulate=%d, check_saved=%d",
            settings->frequency,
            settings->preset_index,
            settings->auto_save,
            settings->hopping_enabled,
            settings->emulate_feature_enabled,
            settings->check_saved);

    } while(false);

    flipper_format_free(ff);

    if(write_ok) {
        if(!protopirate_storage_commit_temp_file(storage, tmp_path, PROTOPIRATE_SETTINGS_FILE)) {
            FURI_LOG_E(TAG, "Failed to commit settings file");
        }
    } else if(storage_file_exists(storage, tmp_path)) {
        storage_simply_remove(storage, tmp_path);
    }

    furi_record_close(RECORD_STORAGE);
}
