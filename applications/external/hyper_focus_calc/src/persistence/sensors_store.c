#include "include/persistence/sensors_store.h"
#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

#define APPS_DATA_DIR "/ext/apps_data/hyperfocuscalc"
#define SENSORS_PATH  APPS_DATA_DIR "/sensors.bin"

typedef struct {
    Storage* storage;
} FsCtx;

static StoreStat fs_stat(void* ctx, const char* path) {
    Storage* storage = ((FsCtx*)ctx)->storage;
    FileInfo info;
    switch(storage_common_stat(storage, path, &info)) {
    case FSE_OK:
        return StoreStatOk;
    case FSE_NOT_EXIST:
        return StoreStatMissing;
    default:
        return StoreStatError;
    }
}

static bool fs_exists(void* ctx, const char* path) {
    return storage_common_exists(((FsCtx*)ctx)->storage, path);
}

static ReadResult
    fs_read_all(void* ctx, const char* path, uint8_t* buf, size_t buf_size, size_t* out_len) {
    Storage* storage = ((FsCtx*)ctx)->storage;
    File* file = storage_file_alloc(storage);
    ReadResult result = ReadError;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const uint64_t file_size = storage_file_size(file);
        size_t total = 0;
        bool io_error = false;
        while(total < buf_size) {
            const size_t r = storage_file_read(file, buf + total, buf_size - total);
            // The firmware can return a partial count alongside an error (storage_ext.c):
            // a nonzero `r` must not be mistaken for a legitimate short read.
            if(storage_file_get_error(file) != FSE_OK) {
                io_error = true;
                break;
            }
            if(r == 0) {
                break;
            }
            total += r;
        }
        if(!io_error && total < buf_size && (uint64_t)total < file_size) {
            // Stopped short of both the buffer and the file's own size: not EOF, not a
            // too-large file — a read error the firmware didn't flag on its own.
            io_error = true;
        }
        if(!io_error) {
            uint8_t probe;
            const size_t probed = storage_file_read(file, &probe, 1);
            if(storage_file_get_error(file) != FSE_OK) {
                io_error = true;
            } else if(probed == 0) {
                result = ReadOk;
                if(out_len) {
                    *out_len = total;
                }
            } else {
                result = ReadTooLarge; // filled the buffer and there's still more
            }
        }
        if(io_error) {
            result = ReadError;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return result;
}

static bool fs_write_all(void* ctx, const char* path, const uint8_t* buf, size_t len) {
    Storage* storage = ((FsCtx*)ctx)->storage;
    storage_common_mkdir(storage, APPS_DATA_DIR);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, buf, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, path);
    }
    return ok;
}

static bool fs_rename(void* ctx, const char* src, const char* dst) {
    return storage_common_rename(((FsCtx*)ctx)->storage, src, dst) == FSE_OK;
}

static StoragePort fs_port(FsCtx* ctx) {
    return (StoragePort){
        .ctx = ctx,
        .stat = fs_stat,
        .exists = fs_exists,
        .read_all = fs_read_all,
        .write_all = fs_write_all,
        .rename = fs_rename,
    };
}

void sensors_store_init(SensorsStore* store) {
    furi_assert(store);
    memset(store, 0, sizeof(*store));
}

static void notice_for(const SensorsRecoveryResult* result, char* notice, size_t notice_size) {
    if(!notice || notice_size == 0) {
        return;
    }
    notice[0] = '\0';
    if(result->status == StoreLoadCorrupt && result->kept_path[0] != '\0') {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved sensors.\nA copy was kept as:\n%s",
            result->kept_path);
    } else if(result->status == StoreLoadCorrupt && result->no_free_backup_slot) {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved sensors, and every backup slot is full.\n"
            "Changes won't be saved until it's cleared.\nIt's still at:\n%s",
            SENSORS_PATH);
    } else if(result->status == StoreLoadCorrupt) {
        snprintf(
            notice,
            notice_size,
            "Couldn't read your saved sensors, and the unreadable copy couldn't be set "
            "aside.\nChanges won't be saved until this is fixed.");
    } else if(result->status == StoreLoadIoError) {
        snprintf(
            notice,
            notice_size,
            "Couldn't check your saved sensors (storage error).\n"
            "Changes won't be saved until this is fixed.");
    }
}

StoreLoadStatus sensors_store_load(SensorsStore* store, char* notice, size_t notice_size) {
    furi_assert(store);
    sensors_store_init(store);

    FsCtx ctx = {.storage = furi_record_open(RECORD_STORAGE)};
    const StoragePort port = fs_port(&ctx);

    const SensorsRecoveryResult result =
        sensors_recovery_load(&port, SENSORS_PATH, &store->record);
    store->save_blocked = result.save_blocked;

    notice_for(&result, notice, notice_size);

    furi_record_close(RECORD_STORAGE);
    return result.status;
}

// Shared by every mutator below: saves `rec` (a trial copy, not yet committed to `store`)
// and, on a blocked outcome, marks `store` blocked for good.
static SensorsSaveOutcome save_record(SensorsStore* store, const SensorsRecord* rec) {
    FsCtx ctx = {.storage = furi_record_open(RECORD_STORAGE)};
    const StoragePort port = fs_port(&ctx);
    const SensorsSaveOutcome outcome =
        sensors_recovery_save(&port, SENSORS_PATH, rec, store->save_blocked);
    if(outcome == SensorsSaveBlocked) {
        store->save_blocked = true;
    }
    furi_record_close(RECORD_STORAGE);
    return outcome;
}

bool sensors_store_save(SensorsStore* store) {
    furi_assert(store);
    return save_record(store, &store->record) == SensorsSaveOk;
}

bool sensors_store_set_active(SensorsStore* store, uint32_t index) {
    furi_assert(store);
    if(store->save_blocked || index >= store->record.count) {
        return false;
    }
    SensorsRecord trial = store->record;
    trial.active_index = index;
    if(save_record(store, &trial) != SensorsSaveOk) {
        return false;
    }
    store->record = trial;
    return true;
}

const SensorData* sensors_store_active(const SensorsStore* store) {
    furi_assert(store);
    if(store->record.count == 0 || store->record.active_index >= store->record.count) {
        return NULL;
    }
    return &store->record.sensors[store->record.active_index];
}

bool sensors_store_add(SensorsStore* store, const SensorData* sensor) {
    furi_assert(store && sensor);
    if(store->save_blocked || store->record.count >= SENSOR_MAX_COUNT) {
        return false;
    }
    if(!sensor_data_valid(sensor)) {
        return false;
    }
    SensorsRecord trial = store->record;
    trial.sensors[trial.count] = *sensor;
    trial.active_index = trial.count;
    trial.count++;
    if(save_record(store, &trial) != SensorsSaveOk) {
        return false;
    }
    store->record = trial;
    return true;
}

bool sensors_store_replace_at(SensorsStore* store, uint32_t index, const SensorData* sensor) {
    furi_assert(store && sensor);
    if(store->save_blocked || index >= store->record.count) {
        return false;
    }
    if(!sensor_data_valid(sensor)) {
        return false;
    }
    SensorsRecord trial = store->record;
    trial.sensors[index] = *sensor;
    if(save_record(store, &trial) != SensorsSaveOk) {
        return false;
    }
    store->record = trial;
    return true;
}

bool sensors_store_delete_at(SensorsStore* store, uint32_t index) {
    furi_assert(store);
    if(store->save_blocked || index >= store->record.count || store->record.count == 0) {
        return false;
    }
    SensorsRecord trial = store->record;
    for(uint32_t i = index + 1; i < trial.count; i++) {
        trial.sensors[i - 1] = trial.sensors[i];
    }
    trial.count--;
    if(trial.active_index >= trial.count && trial.count > 0) {
        trial.active_index = trial.count - 1;
    } else if(trial.active_index > index && trial.active_index > 0) {
        trial.active_index--;
    }
    if(trial.count == 0) {
        sensor_data_set_defaults(&trial.sensors[0]);
        trial.count = 1;
        trial.active_index = 0;
    }
    if(save_record(store, &trial) != SensorsSaveOk) {
        return false;
    }
    store->record = trial;
    return true;
}
