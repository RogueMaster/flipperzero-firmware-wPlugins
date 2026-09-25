#include "../../include/platform/storage_port.h"
#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>

#define TUTU_DIR             "/ext/apps_data/tutu"
#define TUTU_FILE            TUTU_DIR "/progress.bin"
#define TUTU_BACKUP_PATH_MAX 48

// Set once a load finds an unreadable save that can't be quarantined (no free .bad slot, or
// the rename itself fails): saving would overwrite the only copy left, so it's blocked for
// the rest of this session.
static bool s_save_blocked = false;

static void backup_path_for_slot(int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", TUTU_FILE, digit);
}

static bool path_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

// Moves the unreadable save file aside to the first free ".bad" slot so a later save never
// overwrites it. Returns false (file left in place) if every slot is taken or the rename fails.
static bool quarantine_unreadable(Storage* storage) {
    bool taken[TUTU_PROGRESS_BACKUP_SLOTS];
    char candidate[TUTU_BACKUP_PATH_MAX];
    for(int i = 0; i < TUTU_PROGRESS_BACKUP_SLOTS; i++) {
        backup_path_for_slot(i, candidate, sizeof(candidate));
        taken[i] = path_exists(storage, candidate);
    }

    int slot = tutu_progress_backup_slot(taken);
    if(slot < 0) {
        return false;
    }
    backup_path_for_slot(slot, candidate, sizeof(candidate));
    return storage_common_rename(storage, TUTU_FILE, candidate) == FSE_OK;
}

bool tutu_storage_load_progress(TutuProgress* p) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, TUTU_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t read = storage_file_read(file, p, sizeof(TutuProgress));
        bool io_ok = (read == sizeof(TutuProgress)) && storage_file_get_error(file) == FSE_OK;
        storage_file_close(file);
        if(io_ok) {
            ok = true;
        } else {
            s_save_blocked = !quarantine_unreadable(storage);
        }
    } else {
        // Only FSE_NOT_EXIST means "never saved"; any other error means the file is there
        // but couldn't be opened, which must not be silently overwritten either.
        FS_Error err = storage_file_get_error(file);
        storage_file_close(file);
        if(err != FSE_NOT_EXIST) {
            s_save_blocked = !quarantine_unreadable(storage);
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool tutu_storage_save_progress(const TutuProgress* p) {
    if(s_save_blocked) {
        return false;
    }

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
