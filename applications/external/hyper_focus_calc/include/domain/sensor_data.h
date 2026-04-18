#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSOR_NAME_MAX 32u
#define SENSOR_MAX_COUNT 32u

typedef struct {
    char name[SENSOR_NAME_MAX];
    float w;
    float h;
    float coc;
} SensorData;

/** Zeiss-style CoC from sensor width/height in mm: sqrt(w^2+h^2) / 1500. */
float sensor_compute_coc_mm(float w_mm, float h_mm);

void sensor_data_set_defaults(SensorData *s);

bool sensor_data_valid(const SensorData *s);
