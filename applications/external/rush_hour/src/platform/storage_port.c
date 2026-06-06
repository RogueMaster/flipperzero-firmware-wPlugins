#include "../../include/platform/storage_port.h"
#include <furi.h>
#include <storage/storage.h>

#define TUTU_DIR  "/ext/apps_data/rush_hour"
#define TUTU_FILE TUTU_DIR "/progress.bin"

bool tutu_storage_load_progress(TutuProgress* p) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, TUTU_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t read = storage_file_read(file, p, sizeof(TutuProgress));
        ok = (read == sizeof(TutuProgress));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tutu_storage_save_progress(const TutuProgress* p) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, TUTU_DIR);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, TUTU_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t wrote = storage_file_write(file, p, sizeof(TutuProgress));
        ok = (wrote == sizeof(TutuProgress));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
