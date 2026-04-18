#pragma once

#include "include/domain/sensor_data.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t count;
    uint32_t active_index;
    SensorData sensors[SENSOR_MAX_COUNT];
} SensorsStore;

void sensors_store_init(SensorsStore *store);

bool sensors_store_load(SensorsStore *store);

bool sensors_store_save(const SensorsStore *store);

bool sensors_store_set_active(SensorsStore *store, uint32_t index);

const SensorData *sensors_store_active(const SensorsStore *store);

bool sensors_store_add(SensorsStore *store, const SensorData *sensor);

bool sensors_store_replace_at(SensorsStore *store, uint32_t index, const SensorData *sensor);

bool sensors_store_delete_at(SensorsStore *store, uint32_t index);
