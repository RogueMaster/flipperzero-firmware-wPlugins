#pragma once

#include "include/domain/sensor_data.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSORS_FILE_MAGIC   0x48464331u
#define SENSORS_FILE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t active_index;
} SensorsFileHeader;

#define SENSORS_FILE_MAX_BYTES \
    (sizeof(SensorsFileHeader) + (size_t)SENSOR_MAX_COUNT * sizeof(SensorData))

typedef struct {
    uint32_t count;
    uint32_t active_index;
    SensorData sensors[SENSOR_MAX_COUNT];
} SensorsRecord;

// Same on-disk layout used since version 1: a fixed header followed by `count` raw
// SensorData structs. `buf` must be at least SENSORS_FILE_MAX_BYTES. Returns the number
// of bytes written, or 0 if `rec` doesn't fit (count > SENSOR_MAX_COUNT).
size_t sensors_codec_encode(const SensorsRecord* rec, uint8_t* buf, size_t buf_size);

// Decodes `buf`/`len` into `rec`. Rejects a short header, bad magic/version, a count over
// SENSOR_MAX_COUNT, an out-of-range active_index, or a body shorter than `count` records.
// A layout change without a version bump is rejected the same way as real corruption.
bool sensors_codec_decode(const uint8_t* buf, size_t len, SensorsRecord* rec);
