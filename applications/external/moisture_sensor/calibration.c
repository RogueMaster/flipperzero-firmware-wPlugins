#include "calibration.h"

bool calibration_load(MoistureSensorApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    FuriString* filetype = furi_string_alloc();
    if(!filetype) {
        flipper_format_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = false;

    do {
        if(!flipper_format_file_open_existing(file, CALIBRATION_FILE_PATH)) break;

        uint32_t version;
        if(!flipper_format_read_header(file, filetype, &version)) break;
        if(furi_string_cmp_str(filetype, CALIBRATION_FILE_TYPE) != 0) break;
        if(version != CALIBRATION_FILE_VERSION) break;

        uint32_t dry_val, wet_val;
        if(!flipper_format_read_uint32(file, "Dry", &dry_val, 1)) break;
        if(!flipper_format_read_uint32(file, "Wet", &wet_val, 1)) break;

        // Validate: dry > wet, both within valid ADC range
        if(dry_val > wet_val && dry_val <= ADC_MAX_VALUE && wet_val >= SENSOR_MIN_THRESHOLD) {
            app->cal_dry_value = (uint16_t)dry_val;
            app->cal_wet_value = (uint16_t)wet_val;
            success = true;
        }
    } while(false);

    furi_string_free(filetype);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool calibration_save(MoistureSensorApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure app data directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = false;

    do {
        if(!flipper_format_file_open_always(file, CALIBRATION_FILE_PATH)) break;
        if(!flipper_format_write_header_cstr(file, CALIBRATION_FILE_TYPE, CALIBRATION_FILE_VERSION))
            break;

        uint32_t dry_val = app->cal_dry_value;
        uint32_t wet_val = app->cal_wet_value;
        if(!flipper_format_write_uint32(file, "Dry", &dry_val, 1)) break;
        if(!flipper_format_write_uint32(file, "Wet", &wet_val, 1)) break;

        success = true;
    } while(false);

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}
