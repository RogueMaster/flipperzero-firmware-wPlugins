#include "include/domain/sensor_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float sensor_compute_coc_mm(float w_mm, float h_mm) {
    const float w = w_mm > 0.0f ? w_mm : 1.0f;
    const float h = h_mm > 0.0f ? h_mm : 1.0f;
    const float diag = sqrtf(w * w + h * h);
    return diag / 1500.0f;
}

void sensor_data_set_defaults(SensorData *s) {
    if (!s) {
        return;
    }
    memset(s, 0, sizeof(*s));
    snprintf(s->name, SENSOR_NAME_MAX, "Default");
    s->w = 36.0f;
    s->h = 24.0f;
    s->coc = sensor_compute_coc_mm(s->w, s->h);
}

bool sensor_data_valid(const SensorData *s) {
    if (!s) {
        return false;
    }
    if (s->w < 0.01f || s->h < 0.01f) {
        return false;
    }
    if (s->coc < 0.0001f || s->coc > 1.0f) {
        return false;
    }
    return true;
}
