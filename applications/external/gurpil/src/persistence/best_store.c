#include "include/persistence/best_store.h"

#include "include/domain/record.h"

#include <furi.h>
#include <storage/storage.h>

// App-scoped data file storing the serialized best-distance record (see record.h for the
// byte layout). APP_DATA_PATH resolves this to the FAP's private data folder on-device.
#define BEST_STORE_FILE_NAME       "best.dat"
#define BEST_STORE_FILE_PATH       APP_DATA_PATH(BEST_STORE_FILE_NAME)
#define BEST_STORE_BACKUP_PATH_MAX 48

// Once a load finds an unreadable file that can't be quarantined (no free .bad slot, or the
// rename itself fails), saving would overwrite the only copy left: block it for this session.
static bool s_save_blocked = false;

static void backup_path_for_slot(int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", BEST_STORE_FILE_PATH, digit);
}

static bool path_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

// Moves the unreadable best-score file aside to the first free ".bad" slot so a later save
// never overwrites it. Returns false (and leaves the file where it is) if every slot is
// taken or the rename fails.
static bool quarantine_unreadable(Storage* storage) {
    bool taken[RECORD_BACKUP_SLOTS];
    char candidate[BEST_STORE_BACKUP_PATH_MAX];
    for(int i = 0; i < RECORD_BACKUP_SLOTS; i++) {
        backup_path_for_slot(i, candidate, sizeof(candidate));
        taken[i] = path_exists(storage, candidate);
    }

    int slot = record_backup_slot(taken);
    if(slot < 0) {
        return false;
    }
    backup_path_for_slot(slot, candidate, sizeof(candidate));
    return storage_common_rename(storage, BEST_STORE_FILE_PATH, candidate) == FSE_OK;
}

int32_t best_store_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    int32_t best = 0;
    if(storage_file_open(file, BEST_STORE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[RECORD_BYTES];
        size_t bytes_read = storage_file_read(file, buf, RECORD_BYTES);
        bool io_ok = bytes_read == RECORD_BYTES && storage_file_get_error(file) == FSE_OK;
        storage_file_close(file);
        if(io_ok && record_is_valid(buf, bytes_read)) {
            best = record_parse(buf, bytes_read);
        } else {
            s_save_blocked = !quarantine_unreadable(storage);
        }
    } else {
        FS_Error err = storage_file_get_error(file);
        storage_file_close(file);
        // Only FSE_NOT_EXIST means "never saved"; any other error means the file is there
        // but couldn't be opened, which must not be silently overwritten either.
        if(err != FSE_NOT_EXIST) {
            s_save_blocked = !quarantine_unreadable(storage);
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return best;
}

void best_store_save(int32_t best) {
    if(s_save_blocked) {
        return;
    }

    uint8_t buf[RECORD_BYTES];
    size_t written = record_serialize(best, buf, RECORD_BYTES);
    if(written != RECORD_BYTES) {
        // Codec refused to write (should not happen with a fixed-size stack buffer); nothing
        // to persist.
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BEST_STORE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, buf, RECORD_BYTES);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
