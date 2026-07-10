#include "include/persistence/best_store.h"

#include "include/domain/record.h"

#include <furi.h>
#include <storage/storage.h>

// App-scoped data file storing the serialized best-distance record (see record.h for the
// byte layout). APP_DATA_PATH resolves this to the FAP's private data folder on-device.
#define BEST_STORE_FILE_NAME "best.dat"
#define BEST_STORE_FILE_PATH APP_DATA_PATH(BEST_STORE_FILE_NAME)

int32_t best_store_load(void) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    int32_t best = 0;
    if (storage_file_open(file, BEST_STORE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[RECORD_BYTES];
        size_t bytes_read = storage_file_read(file, buf, RECORD_BYTES);
        if (bytes_read == RECORD_BYTES) {
            best = record_parse(buf, RECORD_BYTES);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return best;
}

void best_store_save(int32_t best) {
    uint8_t buf[RECORD_BYTES];
    size_t written = record_serialize(best, buf, RECORD_BYTES);
    if (written != RECORD_BYTES) {
        // Codec refused to write (should not happen with a fixed-size stack buffer); nothing
        // to persist.
        return;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    if (storage_file_open(file, BEST_STORE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, buf, RECORD_BYTES);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
