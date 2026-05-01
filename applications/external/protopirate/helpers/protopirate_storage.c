// helpers/protopirate_storage.c
#include "protopirate_storage.h"

#define TAG "ProtoPirateStorage"

bool protopirate_storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void protopirate_storage_wipe_history_cache(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_dir_exists(storage, PROTOPIRATE_HISTORY_FOLDER)) {
        storage_simply_remove_recursive(storage, PROTOPIRATE_HISTORY_FOLDER);
        FURI_LOG_I(TAG, "Wiped history cache");
    }
    furi_record_close(RECORD_STORAGE);
}

void protopirate_storage_purge_temp_history_at_startup(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_dir_exists(storage, PROTOPIRATE_HISTORY_FOLDER)) {
        storage_simply_remove_recursive(storage, PROTOPIRATE_HISTORY_FOLDER);
    }
    furi_record_close(RECORD_STORAGE);
}

bool protopirate_storage_ensure_history_folder(void) {
    if(!protopirate_storage_init()) {
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROTOPIRATE_CACHE_FOLDER);
    bool ok = storage_simply_mkdir(storage, PROTOPIRATE_HISTORY_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool protopirate_storage_save_history_capture(
    FlipperFormat* flipper_format,
    uint32_t seq,
    FuriString* out_path) {
    furi_check(flipper_format);
    furi_check(out_path);

    if(!protopirate_storage_ensure_history_folder()) {
        FURI_LOG_E(TAG, "History folder missing");
        return false;
    }

    furi_string_printf(
        out_path,
        "%s/hist_%08lu%s",
        PROTOPIRATE_HISTORY_FOLDER,
        (unsigned long)seq,
        PROTOPIRATE_APP_EXTENSION);

    return protopirate_storage_save_capture_to_path(
        flipper_format, furi_string_get_cstr(out_path));
}

static void sanitize_filename(const char* input, char* output, size_t output_size) {
    if(!output || output_size == 0) return;
    if(!input) {
        output[0] = '\0';
        return;
    }
    size_t i = 0;
    size_t j = 0;
    while(input[i] != '\0' && j < output_size - 1) {
        char c = input[i];
        if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
           c == '>' || c == '|' || c == ' ') {
            output[j] = '_';
        } else {
            output[j] = c;
        }
        i++;
        j++;
    }
    output[j] = '\0';
}

bool protopirate_storage_get_next_filename(const char* protocol_name, FuriString* out_filename) {
    if(!protocol_name || !out_filename) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* temp_path = furi_string_alloc();
    uint32_t index = 0;
    bool found = false;

    char safe_name[64];
    sanitize_filename(protocol_name, safe_name, sizeof(safe_name));

    while(!found && index <= 999) {
        furi_string_printf(
            temp_path,
            "%s/%s_%03lu%s",
            PROTOPIRATE_APP_FOLDER,
            safe_name,
            (unsigned long)index,
            PROTOPIRATE_APP_EXTENSION);

        if(!storage_file_exists(storage, furi_string_get_cstr(temp_path))) {
            furi_string_set(out_filename, temp_path);
            found = true;
        } else {
            index++;
        }
    }

    furi_string_free(temp_path);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static bool protopirate_storage_write_capture_data(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format) {
    furi_check(save_file);
    furi_check(flipper_format);

    bool status = true;

    FuriString* string_value = furi_string_alloc();
    if(!string_value) {
        FURI_LOG_E("ProtoPirate", "Failed to alloc string_value");
        return false;
    }

    uint32_t uint32_value = 0;
    uint32_t uint32_array_size = 0;

    /* Protocol */
    PROTOPIRATE_COPY_STRING_OPTIONAL("Protocol");

    /* Bit */
    PROTOPIRATE_COPY_U32_OPTIONAL("Bit");

    /* Key (string OR u32 array) */
    flipper_format_rewind(flipper_format);
    if(flipper_format_read_string(flipper_format, "Key", string_value)) {
        if(!flipper_format_write_string(save_file, "Key", string_value)) {
            PROTOPIRATE_FAIL_WRITE("Key");
        }
    } else {
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(flipper_format, "Key", &uint32_array_size) &&
           uint32_array_size > 0) {
            if(uint32_array_size >= 1024) {
                FURI_LOG_E("ProtoPirate", "Key too large: %lu", (unsigned long)uint32_array_size);
                status = false;
                goto cleanup;
            }

            uint32_t* uint32_array = malloc(sizeof(uint32_t) * uint32_array_size);
            if(!uint32_array) {
                FURI_LOG_E(
                    "ProtoPirate",
                    "Malloc failed: Key (%lu u32)",
                    (unsigned long)uint32_array_size);
                status = false;
                goto cleanup;
            }

            flipper_format_rewind(flipper_format);
            if(!flipper_format_read_uint32(
                   flipper_format, "Key", uint32_array, uint32_array_size)) {
                free(uint32_array);
                PROTOPIRATE_FAIL_READ("Key");
            }

            if(!flipper_format_write_uint32(save_file, "Key", uint32_array, uint32_array_size)) {
                free(uint32_array);
                PROTOPIRATE_FAIL_WRITE("Key");
            }

            free(uint32_array);
        } else {
            uint8_t key_hex[8];
            flipper_format_rewind(flipper_format);
            if(flipper_format_read_hex(flipper_format, "Key", key_hex, sizeof(key_hex))) {
                if(!flipper_format_write_hex(save_file, "Key", key_hex, sizeof(key_hex))) {
                    PROTOPIRATE_FAIL_WRITE("Key");
                }
            }
        }
    }

    /* Frequency */
    PROTOPIRATE_COPY_U32_OPTIONAL("Frequency");

    /* Preset */
    PROTOPIRATE_COPY_STRING_OPTIONAL("Preset");

    /* Custom_preset_module (only if present) */
    PROTOPIRATE_COPY_STRING_IF_PRESENT("Custom_preset_module");

    /* Custom_preset_data (only if present) */
    flipper_format_rewind(flipper_format);
    if(flipper_format_get_value_count(flipper_format, "Custom_preset_data", &uint32_array_size) &&
       uint32_array_size > 0) {
        if(uint32_array_size >= 1024) {
            FURI_LOG_E(
                "ProtoPirate",
                "Custom_preset_data too large: %lu",
                (unsigned long)uint32_array_size);
            status = false;
            goto cleanup;
        }

        uint8_t* custom_data = malloc(uint32_array_size);
        if(!custom_data) {
            FURI_LOG_E(
                "ProtoPirate",
                "Malloc failed: Custom_preset_data (%lu bytes)",
                (unsigned long)uint32_array_size);
            status = false;
            goto cleanup;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_hex(
               flipper_format, "Custom_preset_data", custom_data, uint32_array_size)) {
            free(custom_data);
            PROTOPIRATE_FAIL_READ("Custom_preset_data");
        }

        if(!flipper_format_write_hex(
               save_file, "Custom_preset_data", custom_data, uint32_array_size)) {
            free(custom_data);
            PROTOPIRATE_FAIL_WRITE("Custom_preset_data");
        }

        free(custom_data);
    }

    /* TE / Serial / Btn / Cnt / Checksum / CRC / Type */
    PROTOPIRATE_COPY_U32_OPTIONAL("TE");
    PROTOPIRATE_COPY_U32_OPTIONAL("Serial");
    PROTOPIRATE_COPY_U32_OPTIONAL("Btn");
    PROTOPIRATE_COPY_U32_OPTIONAL("Cnt");
    PROTOPIRATE_COPY_U32_OPTIONAL("Extra");
    PROTOPIRATE_COPY_U32_OPTIONAL("Extra_bits");
    PROTOPIRATE_COPY_U32_OPTIONAL("Checksum");
    PROTOPIRATE_COPY_U32_OPTIONAL("CRC");
    PROTOPIRATE_COPY_U32_OPTIONAL("Type");

    /* Key2 (VAG) */
    uint8_t key2_buf[8];
    PROTOPIRATE_COPY_HEX_FIXED_OPTIONAL("Key2", key2_buf, 8);

    /* KeyIdx / Seed */
    PROTOPIRATE_COPY_U32_OPTIONAL("KeyIdx");
    PROTOPIRATE_COPY_U32_OPTIONAL("Seed");

    /* ValidationField (hex[2] OR u32) */
    flipper_format_rewind(flipper_format);
    uint8_t val_field[2];
    if(flipper_format_read_hex(flipper_format, "ValidationField", val_field, 2)) {
        if(!flipper_format_write_hex(save_file, "ValidationField", val_field, 2)) {
            PROTOPIRATE_FAIL_WRITE("ValidationField");
        }
    } else {
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "ValidationField", &uint32_value, 1)) {
            if(!flipper_format_write_uint32(save_file, "ValidationField", &uint32_value, 1)) {
                PROTOPIRATE_FAIL_WRITE("ValidationField");
            }
        }
    }

    /* Key_2 */
    do {
        uint8_t key_2_hex[8];
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_hex(flipper_format, "Key_2", key_2_hex, sizeof(key_2_hex))) {
            if(!flipper_format_write_hex(save_file, "Key_2", key_2_hex, sizeof(key_2_hex))) {
                PROTOPIRATE_FAIL_WRITE("Key_2");
            }
        } else {
            PROTOPIRATE_COPY_STRING_OPTIONAL("Key_2");
            PROTOPIRATE_COPY_U32_OPTIONAL("Key_2");
        }
    } while(0);

    do {
        uint8_t key_3_hex[4];
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_hex(flipper_format, "Key_3", key_3_hex, sizeof(key_3_hex))) {
            if(!flipper_format_write_hex(save_file, "Key_3", key_3_hex, sizeof(key_3_hex))) {
                PROTOPIRATE_FAIL_WRITE("Key_3");
            }
        } else {
            PROTOPIRATE_COPY_U32_OPTIONAL("Key_3");
        }
    } while(0);
    PROTOPIRATE_COPY_U32_OPTIONAL("Key_4");
    PROTOPIRATE_COPY_U32_OPTIONAL("Fx");

    /* Key1 */
    uint8_t key1_buf[8];
    PROTOPIRATE_COPY_HEX_FIXED_OPTIONAL("Key1", key1_buf, 8);

    /* Check */
    PROTOPIRATE_COPY_U32_OPTIONAL("Check");

    /* RAW_Data (u32 array, only if present) */
    flipper_format_rewind(flipper_format);
    if(flipper_format_get_value_count(flipper_format, "RAW_Data", &uint32_array_size) &&
       uint32_array_size > 0) {
        if(uint32_array_size >= 4096) {
            FURI_LOG_E("ProtoPirate", "RAW_Data too large: %lu", (unsigned long)uint32_array_size);
            status = false;
            goto cleanup;
        }

        uint32_t* raw_array = malloc(sizeof(uint32_t) * uint32_array_size);
        if(!raw_array) {
            FURI_LOG_E(
                "ProtoPirate",
                "Malloc failed: RAW_Data (%lu u32)",
                (unsigned long)uint32_array_size);
            status = false;
            goto cleanup;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "RAW_Data", raw_array, uint32_array_size)) {
            free(raw_array);
            PROTOPIRATE_FAIL_READ("RAW_Data");
        }

        if(!flipper_format_write_uint32(save_file, "RAW_Data", raw_array, uint32_array_size)) {
            free(raw_array);
            PROTOPIRATE_FAIL_WRITE("RAW_Data");
        }

        free(raw_array);
    }

    /* DataHi / DataLo / RawCnt / Encrypted / Decrypted / KIAVersion / Checksum */
    PROTOPIRATE_COPY_U32_OPTIONAL("DataHi");
    PROTOPIRATE_COPY_U32_OPTIONAL("DataLo");
    PROTOPIRATE_COPY_U32_OPTIONAL("RawCnt");
    PROTOPIRATE_COPY_U32_OPTIONAL("Encrypted");
    PROTOPIRATE_COPY_U32_OPTIONAL("Decrypted");
    PROTOPIRATE_COPY_U32_OPTIONAL("KIAVersion");
    PROTOPIRATE_COPY_U32_OPTIONAL("Checksum");

    /* Manufacture */
    PROTOPIRATE_COPY_STRING_OPTIONAL("Manufacture");

cleanup:
    furi_string_free(string_value);

    return status;
}

bool protopirate_storage_save_capture_to_path(FlipperFormat* flipper_format, const char* full_path) {
    furi_check(flipper_format);
    furi_check(full_path);

    if(!protopirate_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        // Remove if it already exists (overwrite)
        if(storage_file_exists(storage, full_path)) {
            storage_simply_remove(storage, full_path);
        }

        if(!flipper_format_file_open_new(save_file, full_path)) {
            FURI_LOG_E(TAG, "Failed to create file: %s", full_path);
            break;
        }

        if(!flipper_format_write_header_cstr(save_file, "Flipper SubGhz Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        if(!protopirate_storage_write_capture_data(save_file, flipper_format)) {
            FURI_LOG_E(TAG, "Failed to write capture data");
            break;
        }

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", full_path);

    } while(false);

    flipper_format_free(save_file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void protopirate_storage_delete_temp(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, PROTOPIRATE_TEMP_FILE)) {
        storage_simply_remove(storage, PROTOPIRATE_TEMP_FILE);
        FURI_LOG_I(TAG, "Deleted temp file");
    }
    furi_record_close(RECORD_STORAGE);
}

bool protopirate_storage_save_capture(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    FuriString* out_path) {
    furi_check(flipper_format);
    furi_check(protocol_name);
    furi_check(out_path);

    if(!protopirate_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    FuriString* file_path = furi_string_alloc();

    if(!protopirate_storage_get_next_filename(protocol_name, file_path)) {
        FURI_LOG_E(TAG, "Failed to get next filename");
        furi_string_free(file_path);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        if(!flipper_format_file_open_new(save_file, furi_string_get_cstr(file_path))) {
            FURI_LOG_E(TAG, "Failed to create file");
            break;
        }

        if(!flipper_format_write_header_cstr(save_file, "Flipper SubGhz Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        if(!protopirate_storage_write_capture_data(save_file, flipper_format)) {
            FURI_LOG_E(TAG, "Failed to write capture data");
            break;
        }

        if(out_path) furi_string_set(out_path, file_path);

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", furi_string_get_cstr(file_path));

    } while(false);

    flipper_format_free(save_file);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool protopirate_storage_delete_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Delete file %s: %s", file_path, result ? "OK" : "FAILED");
    return result;
}
