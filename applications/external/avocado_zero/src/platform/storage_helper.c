#include "include/platform/storage_helper.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <stdio.h>
#include <storage/storage.h>

#define APPS_DATA_DIR           "/ext/apps_data/avocado_zero"
#define STORAGE_PATH            APPS_DATA_DIR "/data.bin"
#define ONBOARDING_DONE_PATH    APPS_DATA_DIR "/onboarding_done"
#define STORAGE_BACKUP_PATH_MAX 64

#define AVOCADO_SAVE_MAGIC 0x41564143u

typedef struct {
    uint32_t magic;
    uint32_t last_timestamp;
    uint8_t dirty_level;
    uint8_t roots_length;
    uint8_t victory_seen;
    uint8_t reserved;
} AvocadoDataFile;

// Set once a load finds an unreadable save that can't be quarantined (no free .bad slot, or
// the rename itself fails): saving would overwrite the only copy left, so it's blocked for
// the rest of this session.
static bool s_save_blocked = false;

static void avocado_data_apply_defaults(AvocadoData* data) {
    data->last_timestamp = furi_hal_rtc_get_timestamp();
    data->dirty_level = 0;
    data->roots_length = 0;
    data->victory_seen = 0;
}

static void backup_path_for_slot(int slot, char* out, size_t out_size) {
    const char digit[2] = {slot > 0 ? (char)('0' + slot) : '\0', '\0'};
    snprintf(out, out_size, "%s.bad%s", STORAGE_PATH, digit);
}

static bool path_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

// Moves the unreadable save file aside to the first free ".bad" slot so a later save never
// overwrites it. Returns false (file left in place) if every slot is taken or the rename fails.
static bool quarantine_unreadable(Storage* storage) {
    bool taken[AVOCADO_STORE_BACKUP_SLOTS];
    char candidate[STORAGE_BACKUP_PATH_MAX];
    for(int i = 0; i < AVOCADO_STORE_BACKUP_SLOTS; i++) {
        backup_path_for_slot(i, candidate, sizeof(candidate));
        taken[i] = path_exists(storage, candidate);
    }

    int slot = avocado_store_backup_slot(taken);
    if(slot < 0) {
        return false;
    }
    backup_path_for_slot(slot, candidate, sizeof(candidate));
    return storage_common_rename(storage, STORAGE_PATH, candidate) == FSE_OK;
}

AvocadoStoreLoadStatus avocado_data_load(AvocadoData* data) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    AvocadoStoreLoadStatus status;

    if(storage_file_open(file, STORAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[sizeof(AvocadoDataFile)];
        size_t read = storage_file_read(file, buf, sizeof(buf));
        bool io_ok = read == sizeof(buf) && storage_file_get_error(file) == FSE_OK;
        storage_file_close(file);

        if(!io_ok) {
            status = AvocadoStoreLoadCorrupt;
        } else {
            const AvocadoDataFile* f = (const AvocadoDataFile*)buf;
            if(f->magic == AVOCADO_SAVE_MAGIC) {
                data->last_timestamp = f->last_timestamp;
                data->dirty_level = f->dirty_level;
                data->roots_length = f->roots_length;
                data->victory_seen = f->victory_seen;
                status = AvocadoStoreLoadOk;
            } else {
                status = AvocadoStoreLoadCorrupt;
            }
        }
    } else {
        // Only FSE_NOT_EXIST means "never saved"; any other error means the file is there
        // but couldn't be opened, which must not be silently overwritten either.
        FS_Error err = storage_file_get_error(file);
        storage_file_close(file);
        status = (err == FSE_NOT_EXIST) ? AvocadoStoreLoadMissing : AvocadoStoreLoadCorrupt;
    }

    if(status == AvocadoStoreLoadCorrupt) {
        s_save_blocked = !quarantine_unreadable(storage);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(status != AvocadoStoreLoadOk) {
        avocado_data_apply_defaults(data);
    }
    return status;
}

bool avocado_onboarding_should_show(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    const bool exists =
        storage_file_open(file, ONBOARDING_DONE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return !exists;
}

void avocado_onboarding_complete(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    storage_common_mkdir(storage, APPS_DATA_DIR);
    if(storage_file_open(file, ONBOARDING_DONE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const uint8_t marker = 1;
        storage_file_write(file, &marker, sizeof(marker));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void avocado_data_save(const AvocadoData* data) {
    if(s_save_blocked) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    storage_common_mkdir(storage, APPS_DATA_DIR);
    if(storage_file_open(file, STORAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        AvocadoDataFile f = {
            .magic = AVOCADO_SAVE_MAGIC,
            .last_timestamp = data->last_timestamp,
            .dirty_level = data->dirty_level,
            .roots_length = data->roots_length,
            .victory_seen = data->victory_seen,
            .reserved = 0,
        };
        storage_file_write(file, &f, sizeof(f));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
