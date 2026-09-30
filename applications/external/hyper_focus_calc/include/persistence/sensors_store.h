#pragma once

#include "include/domain/sensor_data.h"
#include "include/domain/sensors_recovery.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    SensorsRecord record;
    bool save_blocked; // true once a load or save couldn't rename into place safely, or a
        // stat/read on the sensors file hit a storage error (StoreLoadIoError)
} SensorsStore;

void sensors_store_init(SensorsStore* store);

// Loads the store, recovering from a crashed save and quarantining a genuinely unreadable
// file; see sensors_recovery_load for the exact rules. `notice` (if not NULL) is filled
// with a message for the user whenever the load found something wrong; otherwise it's left
// empty. A missing file only seeds the in-memory default — nothing is written until the
// user's first save.
StoreLoadStatus sensors_store_load(SensorsStore* store, char* notice, size_t notice_size);

bool sensors_store_save(SensorsStore* store);

bool sensors_store_set_active(SensorsStore* store, uint32_t index);

const SensorData* sensors_store_active(const SensorsStore* store);

bool sensors_store_add(SensorsStore* store, const SensorData* sensor);

bool sensors_store_replace_at(SensorsStore* store, uint32_t index, const SensorData* sensor);

bool sensors_store_delete_at(SensorsStore* store, uint32_t index);
