// helpers/protopirate_storage.c
#include "protopirate_storage.h"
#include "../defines.h"
#include "../protocols/protocol_items.h"
#include "../protocols/protocols_common.h"
#include <lib/flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>
#include <string.h>

#define TAG "ProtoPirateStorage"

bool protopirate_storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool protopirate_storage_commit_temp_file(
    Storage* storage,
    const char* tmp_path,
    const char* final_path) {
    furi_check(storage);
    furi_check(tmp_path);
    furi_check(final_path);

    FuriString* backup_path = furi_string_alloc();
    furi_string_printf(backup_path, "%s.bak", final_path);
    const char* backup_cstr = furi_string_get_cstr(backup_path);

    if(storage_file_exists(storage, backup_cstr)) {
        storage_simply_remove(storage, backup_cstr);
    }

    if(storage_file_exists(storage, final_path)) {
        if(storage_common_rename(storage, final_path, backup_cstr) != FSE_OK) {
            FURI_LOG_E(TAG, "Failed to stage backup for %s", final_path);
            furi_string_free(backup_path);
            return false;
        }
    }

    if(storage_common_rename(storage, tmp_path, final_path) != FSE_OK) {
        FURI_LOG_E(TAG, "Failed to commit %s", final_path);
        if(storage_file_exists(storage, backup_cstr)) {
            if(storage_common_rename(storage, backup_cstr, final_path) != FSE_OK) {
                FURI_LOG_E(TAG, "Failed to restore backup for %s", final_path);
            }
        }
        if(storage_file_exists(storage, tmp_path)) {
            storage_simply_remove(storage, tmp_path);
        }
        furi_string_free(backup_path);
        return false;
    }

    if(storage_file_exists(storage, backup_cstr)) {
        storage_simply_remove(storage, backup_cstr);
    }

    furi_string_free(backup_path);
    return true;
}

static void protopirate_storage_remove_history_folder(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_dir_exists(storage, PROTOPIRATE_HISTORY_FOLDER)) {
        storage_simply_remove_recursive(storage, PROTOPIRATE_HISTORY_FOLDER);
    }
    furi_record_close(RECORD_STORAGE);
}

void protopirate_storage_wipe_history_cache(void) {
    protopirate_storage_remove_history_folder();
    FURI_LOG_I(TAG, "Wiped history cache");
}

void protopirate_storage_purge_temp_history_at_startup(void) {
    protopirate_storage_remove_history_folder();
}

bool protopirate_storage_ensure_history_folder(void) {
    if(!protopirate_storage_init()) {
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_mkdir(storage, PROTOPIRATE_CACHE_FOLDER) &&
              storage_simply_mkdir(storage, PROTOPIRATE_HISTORY_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void protopirate_storage_build_history_path(uint32_t seq, FuriString* out) {
    furi_check(out);
    furi_string_printf(
        out,
        "%s/hist_%08lu%s",
        PROTOPIRATE_HISTORY_FOLDER,
        (unsigned long)seq,
        PROTOPIRATE_APP_EXTENSION);
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

    protopirate_storage_build_history_path(seq, out_path);

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

bool protopirate_storage_get_capture_display_protocol(
    FlipperFormat* flipper_format,
    FuriString* protocol_name) {
    furi_check(flipper_format);
    furi_check(protocol_name);

    FuriString* raw_protocol = furi_string_alloc();
    bool have_protocol = false;
    uint32_t protocol_type = 0U;

    flipper_format_rewind(flipper_format);
    have_protocol = flipper_format_read_string(flipper_format, FF_PROTOCOL, raw_protocol);
    if(!have_protocol) {
        furi_string_set(raw_protocol, "Unknown");
    }

    flipper_format_rewind(flipper_format);
    flipper_format_read_uint32(flipper_format, FF_TYPE, &protocol_type, 1);

    const char* display_name = protopirate_protocol_catalog_display_name(
        furi_string_get_cstr(raw_protocol), protocol_type);
    furi_string_set(protocol_name, display_name ? display_name : "Unknown");

    furi_string_free(raw_protocol);
    return have_protocol;
}

static const char* const protopirate_storage_base_u32_fields[] = {
    "TE",
    FF_SERIAL,
    FF_BTN,
    "BtnSig",
    FF_CNT,
    "Extra",
    "ExtraBit",
    "Extra_bits",
    "Tail",
    "Checksum",
    "CRC",
    FF_TYPE,
};

static const char* const protopirate_storage_tail_u32_fields[] = {
    "DataHi",
    "DataLo",
    "RawCnt",
    "Rolling",
    "Encrypted",
    "Decrypted",
    "KIAVersion",
    "Checksum",
};

static bool protopirate_storage_fail(const char* action, const char* key) {
    UNUSED(action);
    UNUSED(key);
    FURI_LOG_E(TAG, "%s failed: %s", action, key);
    return false;
}

static bool
    protopirate_storage_get_count(FlipperFormat* flipper_format, const char* key, uint32_t* count) {
    *count = 0;
    flipper_format_rewind(flipper_format);
    return flipper_format_get_value_count(flipper_format, key, count) && (*count > 0);
}

static bool protopirate_storage_stream_read_char(Stream* stream, char* out) {
    uint8_t value = 0;
    if(stream_read(stream, &value, 1U) != 1U) return false;
    *out = (char)value;
    return true;
}

static bool protopirate_storage_stream_write_char(Stream* stream, char value) {
    return stream_write_char(stream, value) == 1U;
}

static bool protopirate_storage_copy_raw_value_line(
    Stream* out_stream,
    Stream* in_stream,
    const char* key) {
    const size_t key_len = strlen(key);
    if(!key_len || !stream_rewind(in_stream) || !stream_seek(out_stream, 0, StreamOffsetFromEnd)) {
        return protopirate_storage_fail("Stream", key);
    }

    bool copied = false;

    while(!stream_eof(in_stream)) {
        bool line_match = true;
        bool line_ended = false;

        for(size_t i = 0; i < key_len; i++) {
            char c = '\0';
            if(!protopirate_storage_stream_read_char(in_stream, &c)) {
                return protopirate_storage_fail("Read", key);
            }
            if(c == '\n') {
                line_match = false;
                line_ended = true;
                break;
            }
            if(c != key[i]) {
                line_match = false;
            }
        }

        if(line_ended) continue;

        char c = '\0';
        if(!protopirate_storage_stream_read_char(in_stream, &c)) {
            return protopirate_storage_fail("Read", key);
        }

        if(c != ':') {
            line_match = false;
        }

        if(line_match) {
            if(stream_write(out_stream, (const uint8_t*)key, key_len) != key_len ||
               !protopirate_storage_stream_write_char(out_stream, ':')) {
                return protopirate_storage_fail("Write", key);
            }

            bool wrote_newline = false;
            while(protopirate_storage_stream_read_char(in_stream, &c)) {
                if(!protopirate_storage_stream_write_char(out_stream, c)) {
                    return protopirate_storage_fail("Write", key);
                }
                if(c == '\n') {
                    wrote_newline = true;
                    break;
                }
            }

            if(!wrote_newline && !protopirate_storage_stream_write_char(out_stream, '\n')) {
                return protopirate_storage_fail("Write", key);
            }
            copied = true;
            continue;
        }

        while(c != '\n' && protopirate_storage_stream_read_char(in_stream, &c)) {
        }
    }

    return copied ? true : protopirate_storage_fail("Read", key);
}

static bool protopirate_storage_copy_string_optional(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    FuriString* value) {
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_string(flipper_format, key, value)) {
        return true;
    }
    if(!flipper_format_write_string(save_file, key, value)) {
        return protopirate_storage_fail("Write", key);
    }
    return true;
}

static bool protopirate_storage_copy_string_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    FuriString* value) {
    uint32_t count = 0;
    if(!protopirate_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    if(!flipper_format_read_string(flipper_format, key, value)) {
        return protopirate_storage_fail("Read", key);
    }
    if(!flipper_format_write_string(save_file, key, value)) {
        return protopirate_storage_fail("Write", key);
    }
    return true;
}

static bool protopirate_storage_copy_u32_optional(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key) {
    uint32_t value = 0;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_uint32(flipper_format, key, &value, 1)) {
        return true;
    }
    if(!flipper_format_write_uint32(save_file, key, &value, 1)) {
        return protopirate_storage_fail("Write", key);
    }
    return true;
}

static bool protopirate_storage_copy_u32_fields(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* const* fields,
    size_t field_count) {
    for(size_t i = 0; i < field_count; i++) {
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, fields[i])) {
            return false;
        }
    }
    return true;
}

static bool protopirate_storage_copy_hex_fixed(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    size_t len,
    bool* copied) {
    uint8_t data[8];
    furi_check(len <= sizeof(data));
    if(copied) {
        *copied = false;
    }

    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_hex(flipper_format, key, data, len)) {
        return true;
    }
    if(copied) {
        *copied = true;
    }
    if(!flipper_format_write_hex(save_file, key, data, len)) {
        return protopirate_storage_fail("Write", key);
    }
    return true;
}

static bool protopirate_storage_copy_u32_array(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t count,
    uint32_t max_count) {
    if(count > max_count) {
        FURI_LOG_E(TAG, "%s too large: %lu", key, (unsigned long)count);
        return false;
    }

    Stream* in_stream = flipper_format_get_raw_stream(flipper_format);
    Stream* out_stream = flipper_format_get_raw_stream(save_file);
    if(!in_stream || !out_stream) {
        FURI_LOG_E(TAG, "Raw stream missing: %s", key);
        return false;
    }

    return protopirate_storage_copy_raw_value_line(out_stream, in_stream, key);
}

static bool protopirate_storage_copy_u32_array_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t max_count) {
    uint32_t count = 0;
    if(!protopirate_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    return protopirate_storage_copy_u32_array(save_file, flipper_format, key, count, max_count);
}

static bool protopirate_storage_copy_hex_array_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t max_count) {
    uint32_t count = 0;
    if(!protopirate_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    if(count > max_count) {
        FURI_LOG_E(TAG, "%s too large: %lu", key, (unsigned long)count);
        return false;
    }

    Stream* in_stream = flipper_format_get_raw_stream(flipper_format);
    Stream* out_stream = flipper_format_get_raw_stream(save_file);
    if(!in_stream || !out_stream) {
        FURI_LOG_E(TAG, "Raw stream missing: %s", key);
        return false;
    }

    return protopirate_storage_copy_raw_value_line(out_stream, in_stream, key);
}

static bool protopirate_storage_copy_key(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    uint32_t count = 0;

    flipper_format_rewind(flipper_format);
    if(flipper_format_read_string(flipper_format, FF_KEY, value)) {
        if(!flipper_format_write_string(save_file, FF_KEY, value)) {
            return protopirate_storage_fail("Write", FF_KEY);
        }
        return true;
    }

    if(protopirate_storage_get_count(flipper_format, FF_KEY, &count)) {
        return protopirate_storage_copy_u32_array(save_file, flipper_format, FF_KEY, count, 1024);
    }

    return protopirate_storage_copy_hex_fixed(save_file, flipper_format, FF_KEY, 8, NULL);
}

static bool protopirate_storage_copy_hex_or_u32(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    size_t hex_len) {
    bool copied = false;
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, key, hex_len, &copied)) {
        return false;
    }
    return copied || protopirate_storage_copy_u32_optional(save_file, flipper_format, key);
}

static bool protopirate_storage_copy_key_2(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    bool copied = false;
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key_2", 8, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }
    return protopirate_storage_copy_string_optional(save_file, flipper_format, "Key_2", value) &&
           protopirate_storage_copy_u32_optional(save_file, flipper_format, "Key_2");
}

static bool
    protopirate_storage_copy_key2(FlipperFormat* save_file, FlipperFormat* flipper_format) {
    bool copied = false;
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key2", 8, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }

    return protopirate_storage_copy_hex_or_u32(save_file, flipper_format, "Key2", 4);
}

static bool protopirate_storage_write_capture_data(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format) {
    furi_check(save_file);
    furi_check(flipper_format);

    FuriString* string_value = furi_string_alloc();
    if(!string_value) {
        FURI_LOG_E(TAG, "Failed to alloc string_value");
        return false;
    }

    bool status = false;
    do {
        if(!protopirate_storage_copy_string_optional(
               save_file, flipper_format, FF_PROTOCOL, string_value))
            break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, FF_BIT)) break;
        if(!protopirate_storage_copy_key(save_file, flipper_format, string_value)) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, FF_FREQUENCY)) break;
        if(!protopirate_storage_copy_string_optional(
               save_file, flipper_format, FF_PRESET, string_value))
            break;
        if(!protopirate_storage_copy_string_if_present(
               save_file, flipper_format, "Custom_preset_module", string_value))
            break;
        if(!protopirate_storage_copy_hex_array_if_present(
               save_file, flipper_format, "Custom_preset_data", 1024))
            break;
        if(!protopirate_storage_copy_u32_fields(
               save_file,
               flipper_format,
               protopirate_storage_base_u32_fields,
               COUNT_OF(protopirate_storage_base_u32_fields)))
            break;
        if(!protopirate_storage_copy_key2(save_file, flipper_format)) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "KeyIdx")) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "Seed")) break;
        if(!protopirate_storage_copy_hex_or_u32(save_file, flipper_format, "ValidationField", 2))
            break;
        if(!protopirate_storage_copy_key_2(save_file, flipper_format, string_value)) break;
        if(!protopirate_storage_copy_hex_or_u32(save_file, flipper_format, "Key_3", 4)) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "Key_4")) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "Fx")) break;
        if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key1", 8, NULL)) break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "Check")) break;
        if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Hitag2 Key", 6, NULL))
            break;
        if(!protopirate_storage_copy_u32_optional(save_file, flipper_format, "Hitag2 Epoch"))
            break;
        if(!protopirate_storage_copy_u32_array_if_present(
               save_file, flipper_format, "RAW_Data", 4096))
            break;
        if(!protopirate_storage_copy_u32_fields(
               save_file,
               flipper_format,
               protopirate_storage_tail_u32_fields,
               COUNT_OF(protopirate_storage_tail_u32_fields)))
            break;
        if(!protopirate_storage_copy_string_optional(
               save_file, flipper_format, FF_MANUFACTURE, string_value))
            break;
        status = true;
    } while(false);

    furi_string_free(string_value);

    return status;
}

static bool protopirate_storage_write_capture_file(
    Storage* storage,
    FlipperFormat* flipper_format,
    const char* path) {
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_new(save_file, path)) {
            FURI_LOG_E(TAG, "Failed to create file: %s", path);
            break;
        }

        if(!flipper_format_write_header_cstr(
               save_file, "Flipper SubGhz Key File", PROTOPIRATE_APP_FILE_VERSION)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        if(!protopirate_storage_write_capture_data(save_file, flipper_format)) {
            FURI_LOG_E(TAG, "Failed to write capture data");
            break;
        }

        ok = true;
    } while(false);

    flipper_format_free(save_file);
    return ok;
}

static bool protopirate_storage_save_capture_atomic(
    Storage* storage,
    FlipperFormat* flipper_format,
    const char* full_path) {
    FuriString* tmp_path = furi_string_alloc();
    furi_check(tmp_path);
    furi_string_printf(tmp_path, "%s.tmp", full_path);
    const char* tmp_cstr = furi_string_get_cstr(tmp_path);
    bool ok = false;
    bool write_ok = false;

    do {
        if(storage_file_exists(storage, tmp_cstr)) {
            storage_simply_remove(storage, tmp_cstr);
        }

        if(!protopirate_storage_write_capture_file(storage, flipper_format, tmp_cstr)) {
            break;
        }
        write_ok = true;

        if(!protopirate_storage_commit_temp_file(storage, tmp_cstr, full_path)) {
            break;
        }

        ok = true;
    } while(false);

    if(!write_ok && storage_file_exists(storage, tmp_cstr)) {
        storage_simply_remove(storage, tmp_cstr);
    }

    furi_string_free(tmp_path);
    return ok;
}

bool protopirate_storage_save_capture_to_path(FlipperFormat* flipper_format, const char* full_path) {
    furi_check(flipper_format);
    furi_check(full_path);

    if(!protopirate_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = protopirate_storage_save_capture_atomic(storage, flipper_format, full_path);
    furi_record_close(RECORD_STORAGE);

    if(result) {
        FURI_LOG_I(TAG, "Saved capture to %s", full_path);
    }
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
    bool result = protopirate_storage_save_capture_atomic(
        storage, flipper_format, furi_string_get_cstr(file_path));
    furi_record_close(RECORD_STORAGE);

    if(result) {
        if(out_path) furi_string_set(out_path, file_path);
        FURI_LOG_I(TAG, "Saved capture to %s", furi_string_get_cstr(file_path));
    }

    furi_string_free(file_path);
    return result;
}

bool protopirate_storage_delete_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Delete file %s: %s", file_path, result ? "OK" : "FAILED");
    return result;
}
