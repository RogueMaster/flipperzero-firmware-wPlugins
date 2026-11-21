#include "rfs_diag.h"

#include <furi_hal_rtc.h>
#include <stdarg.h>

#define RFS_LOG_MAX_SIZE (64 * 1024)

struct RfsDiag {
    Storage* storage;
    FuriMutex* mutex;
    bool logging;
    char errors[RFS_DIAG_ERROR_SLOTS][RFS_DIAG_ERROR_LEN];
    uint8_t next;
    uint8_t count;
};

RfsDiag* rfs_diag_alloc(Storage* storage) {
    RfsDiag* diag = malloc(sizeof(RfsDiag));
    memset(diag, 0, sizeof(RfsDiag));
    diag->storage = storage;
    diag->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return diag;
}

void rfs_diag_free(RfsDiag* diag) {
    furi_mutex_free(diag->mutex);
    free(diag);
}

void rfs_diag_set_logging(RfsDiag* diag, bool enabled) {
    furi_mutex_acquire(diag->mutex, FuriWaitForever);
    diag->logging = enabled;
    furi_mutex_release(diag->mutex);
}

/* Caller holds the mutex. */
static void rfs_diag_write_log(RfsDiag* diag, const char* level, const char* text) {
    if(!diag->logging) return;
    Storage* storage = diag->storage;
    if(storage_sd_status(storage) != FSE_OK) return;

    FileInfo info;
    if(storage_common_stat(storage, RFS_LOG_PATH, &info) == FSE_OK &&
       info.size > RFS_LOG_MAX_SIZE) {
        storage_common_remove(storage, RFS_LOG_PATH ".old");
        storage_common_rename(storage, RFS_LOG_PATH, RFS_LOG_PATH ".old");
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    FuriString* line = furi_string_alloc_printf(
        "%04u-%02u-%02u %02u:%02u:%02u [%s] %s\n",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        level,
        text);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RFS_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(line);
}

void rfs_diag_error(RfsDiag* diag, const char* fmt, ...) {
    char text[RFS_DIAG_ERROR_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    FURI_LOG_E(RFS_TAG, "%s", text);

    furi_mutex_acquire(diag->mutex, FuriWaitForever);
    strlcpy(diag->errors[diag->next], text, RFS_DIAG_ERROR_LEN);
    diag->next = (diag->next + 1) % RFS_DIAG_ERROR_SLOTS;
    if(diag->count < RFS_DIAG_ERROR_SLOTS) diag->count++;
    rfs_diag_write_log(diag, "E", text);
    furi_mutex_release(diag->mutex);
}

void rfs_diag_info(RfsDiag* diag, const char* fmt, ...) {
    char text[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    FURI_LOG_I(RFS_TAG, "%s", text);

    furi_mutex_acquire(diag->mutex, FuriWaitForever);
    rfs_diag_write_log(diag, "I", text);
    furi_mutex_release(diag->mutex);
}

void rfs_diag_errors_to_string(RfsDiag* diag, FuriString* out) {
    furi_mutex_acquire(diag->mutex, FuriWaitForever);
    if(diag->count == 0) {
        furi_string_cat_str(out, " (none)\n");
    } else {
        for(uint8_t i = 0; i < diag->count; i++) {
            uint8_t slot = (diag->next + RFS_DIAG_ERROR_SLOTS - 1 - i) % RFS_DIAG_ERROR_SLOTS;
            furi_string_cat_printf(out, " %s\n", diag->errors[slot]);
        }
    }
    furi_mutex_release(diag->mutex);
}
