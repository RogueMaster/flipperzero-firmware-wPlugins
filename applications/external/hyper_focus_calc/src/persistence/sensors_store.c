#include "include/persistence/sensors_store.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define HFC_FILE_MAGIC 0x48464331u
#define HFC_FILE_VERSION 1u

#define APPS_DATA_DIR "/ext/apps_data/hyperfocuscalc"
#define SENSORS_PATH APPS_DATA_DIR "/sensors.bin"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t active_index;
} SensorsFileHeader;

void sensors_store_init(SensorsStore *store) {
    furi_assert(store);
    memset(store, 0, sizeof(*store));
}

static bool read_body(File *file, SensorsStore *store) {
    for (uint32_t i = 0; i < store->count; i++) {
        if (storage_file_read(file, &store->sensors[i], sizeof(SensorData)) != sizeof(SensorData)) {
            return false;
        }
    }
    return true;
}

bool sensors_store_load(SensorsStore *store) {
    furi_assert(store);

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    bool ok = false;
    sensors_store_init(store);

    if (storage_file_open(file, SENSORS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SensorsFileHeader head = {0};
        const size_t rh = storage_file_read(file, &head, sizeof(head));
        if (rh == sizeof(head) && head.magic == HFC_FILE_MAGIC &&
            head.version == HFC_FILE_VERSION && head.count <= SENSOR_MAX_COUNT &&
            head.active_index < head.count) {
            store->count = head.count;
            store->active_index = head.active_index;
            if (read_body(file, store)) {
                ok = true;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if (!ok) {
        sensors_store_init(store);
        SensorData def;
        sensor_data_set_defaults(&def);
        store->sensors[0] = def;
        store->count = 1;
        store->active_index = 0;
        sensors_store_save(store);
        return false;
    }

    return true;
}

bool sensors_store_save(const SensorsStore *store) {
    furi_assert(store);
    if (store->count == 0 || store->count > SENSOR_MAX_COUNT ||
        store->active_index >= store->count) {
        return false;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    storage_common_mkdir(storage, APPS_DATA_DIR);

    bool ok = false;
    if (storage_file_open(file, SENSORS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const SensorsFileHeader head = {
            .magic = HFC_FILE_MAGIC,
            .version = HFC_FILE_VERSION,
            .count = store->count,
            .active_index = store->active_index,
        };
        if (storage_file_write(file, &head, sizeof(head)) == sizeof(head)) {
            ok = true;
            for (uint32_t i = 0; i < store->count && ok; i++) {
                if (storage_file_write(file, &store->sensors[i], sizeof(SensorData)) !=
                    sizeof(SensorData)) {
                    ok = false;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return ok;
}

bool sensors_store_set_active(SensorsStore *store, uint32_t index) {
    furi_assert(store);
    if (index >= store->count) {
        return false;
    }
    store->active_index = index;
    return sensors_store_save(store);
}

const SensorData *sensors_store_active(const SensorsStore *store) {
    furi_assert(store);
    if (store->count == 0 || store->active_index >= store->count) {
        return NULL;
    }
    return &store->sensors[store->active_index];
}

bool sensors_store_add(SensorsStore *store, const SensorData *sensor) {
    furi_assert(store && sensor);
    if (store->count >= SENSOR_MAX_COUNT) {
        return false;
    }
    if (!sensor_data_valid(sensor)) {
        return false;
    }
    store->sensors[store->count] = *sensor;
    store->active_index = store->count;
    store->count++;
    return sensors_store_save(store);
}

bool sensors_store_replace_at(SensorsStore *store, uint32_t index, const SensorData *sensor) {
    furi_assert(store && sensor);
    if (index >= store->count) {
        return false;
    }
    if (!sensor_data_valid(sensor)) {
        return false;
    }
    store->sensors[index] = *sensor;
    return sensors_store_save(store);
}

bool sensors_store_delete_at(SensorsStore *store, uint32_t index) {
    furi_assert(store);
    if (index >= store->count || store->count == 0) {
        return false;
    }
    for (uint32_t i = index + 1; i < store->count; i++) {
        store->sensors[i - 1] = store->sensors[i];
    }
    store->count--;
    if (store->active_index >= store->count && store->count > 0) {
        store->active_index = store->count - 1;
    } else if (store->active_index > index && store->active_index > 0) {
        store->active_index--;
    }
    if (store->count == 0) {
        SensorData def;
        sensor_data_set_defaults(&def);
        store->sensors[0] = def;
        store->count = 1;
        store->active_index = 0;
    }
    return sensors_store_save(store);
}
