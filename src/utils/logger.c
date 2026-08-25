#include "logger.h"

#include <storage/storage.h>

/** Resolved once by logger_init; logger_log only appends. */
static FuriString* logger_path = NULL;

void logger_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(logger_path == NULL) {
        logger_path = furi_string_alloc();
    }
    furi_string_set_str(logger_path, APP_DATA_PATH(LOGGER_FILENAME));
    storage_common_resolve_path_and_ensure_app_directory(storage, logger_path);

    furi_record_close(RECORD_STORAGE);
}

void logger_log(const char* message) {
    furi_check(message);

    if(logger_path == NULL) {
        logger_init();
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FuriString* line = NULL;

    do {
        if(!storage_file_open(
               file, furi_string_get_cstr(logger_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
            break;
        }

        line = furi_string_alloc_printf("[%lums] %s\n", (unsigned long)furi_get_tick(), message);
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));

        // Truncate the log when it grows too large
        if(storage_file_size(file) >= LOGGER_MAX_SIZE) {
            storage_file_close(file);
            if(!storage_file_open(
                   file, furi_string_get_cstr(logger_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                break;
            }
        }
    } while(false);

    if(line != NULL) {
        furi_string_free(line);
    }
    storage_file_close(file); // Safe even when the open failed
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
