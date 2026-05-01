// helpers/protopirate_storage.h
#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define PROTOPIRATE_APP_FOLDER       APP_DATA_PATH("saved")
#define PROTOPIRATE_APP_EXTENSION    ".psf"
#define PROTOPIRATE_APP_FILE_VERSION 1
#define PROTOPIRATE_TEMP_FILE        APP_DATA_PATH("saved/.temp.psf")
#define PROTOPIRATE_CACHE_FOLDER     APP_DATA_PATH("cache")
#define PROTOPIRATE_HISTORY_FOLDER   APP_DATA_PATH("cache/history")

// Helper: failed read
#define PROTOPIRATE_FAIL_READ(_k)                           \
    do {                                                    \
        FURI_LOG_E("ProtoPirate", "Read failed: %s", (_k)); \
        status = false;                                     \
        goto cleanup;                                       \
    } while(0)

// Helper: failed write
#define PROTOPIRATE_FAIL_WRITE(_k)                           \
    do {                                                     \
        FURI_LOG_E("ProtoPirate", "Write failed: %s", (_k)); \
        status = false;                                      \
        goto cleanup;                                        \
    } while(0)

// Helper: read and write optional string
#define PROTOPIRATE_COPY_STRING_OPTIONAL(_k)                                  \
    do {                                                                      \
        flipper_format_rewind(flipper_format);                                \
        if(flipper_format_read_string(flipper_format, (_k), string_value)) {  \
            if(!flipper_format_write_string(save_file, (_k), string_value)) { \
                PROTOPIRATE_FAIL_WRITE((_k));                                 \
            }                                                                 \
        }                                                                     \
    } while(0)

// Helper: read and write optional u32
#define PROTOPIRATE_COPY_U32_OPTIONAL(_k)                                         \
    do {                                                                          \
        flipper_format_rewind(flipper_format);                                    \
        if(flipper_format_read_uint32(flipper_format, (_k), &uint32_value, 1)) {  \
            if(!flipper_format_write_uint32(save_file, (_k), &uint32_value, 1)) { \
                PROTOPIRATE_FAIL_WRITE((_k));                                     \
            }                                                                     \
        }                                                                         \
    } while(0)

// Helper: read and write string if present
#define PROTOPIRATE_COPY_STRING_IF_PRESENT(_k)                                         \
    do {                                                                               \
        flipper_format_rewind(flipper_format);                                         \
        if(flipper_format_get_value_count(flipper_format, (_k), &uint32_array_size) && \
           uint32_array_size > 0) {                                                    \
            if(!flipper_format_read_string(flipper_format, (_k), string_value)) {      \
                PROTOPIRATE_FAIL_READ((_k));                                           \
            }                                                                          \
            if(!flipper_format_write_string(save_file, (_k), string_value)) {          \
                PROTOPIRATE_FAIL_WRITE((_k));                                          \
            }                                                                          \
        }                                                                              \
    } while(0)

// Helper: read and write optional hex array
#define PROTOPIRATE_COPY_HEX_FIXED_OPTIONAL(_k, _buf, _len)                  \
    do {                                                                     \
        flipper_format_rewind(flipper_format);                               \
        if(flipper_format_read_hex(flipper_format, (_k), (_buf), (_len))) {  \
            if(!flipper_format_write_hex(save_file, (_k), (_buf), (_len))) { \
                PROTOPIRATE_FAIL_WRITE((_k));                                \
            }                                                                \
        }                                                                    \
    } while(0)

// Initialize storage (create folder if needed)
bool protopirate_storage_init(void);

// Save a capture to a new file (auto-generated name)
bool protopirate_storage_save_capture(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    FuriString* out_path);

// Save a capture to a specific file path (user-chosen name)
bool protopirate_storage_save_capture_to_path(FlipperFormat* flipper_format, const char* full_path);

// Save to temp file for emulation
bool protopirate_storage_save_temp(FlipperFormat* flipper_format);

// Delete temp file
void protopirate_storage_delete_temp(void);

// Get next available filename for a protocol
bool protopirate_storage_get_next_filename(const char* protocol_name, FuriString* out_filename);

// Delete a file
bool protopirate_storage_delete_file(const char* file_path);

// Load a file (caller must close with protopirate_storage_close_file)
FlipperFormat* protopirate_storage_load_file(const char* file_path);

// Close a loaded file (by protopirate_storage_load_file only)
void protopirate_storage_close_file(FlipperFormat* flipper_format);

// Check if file exists
bool protopirate_storage_file_exists(const char* file_path);

bool protopirate_storage_ensure_history_folder(void);

void protopirate_storage_purge_temp_history_at_startup(void);

void protopirate_storage_wipe_history_cache(void);

bool protopirate_storage_save_history_capture(
    FlipperFormat* flipper_format,
    uint32_t seq,
    FuriString* out_path);
