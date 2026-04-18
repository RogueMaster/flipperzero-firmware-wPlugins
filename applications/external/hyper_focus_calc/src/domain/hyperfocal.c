#include "include/domain/hyperfocal.h"
#include <math.h>

static const float k_f_stops[HFC_FSTOP_COUNT] = {
    1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f,
};

float hyperfocal_fstop_at_index(size_t idx) {
    if (idx >= HFC_FSTOP_COUNT) {
        idx = HFC_FSTOP_COUNT - 1u;
    }
    return k_f_stops[idx];
}

size_t hyperfocal_fstop_index_of(float fstop) {
    size_t best = 0;
    float best_d = fabsf(fstop - k_f_stops[0]);
    for (size_t i = 1; i < HFC_FSTOP_COUNT; i++) {
        const float d = fabsf(fstop - k_f_stops[i]);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

void hyperfocal_fstop_step(size_t *idx, int delta) {
    if (!idx) {
        return;
    }
    int i = (int)*idx + delta;
    if (i < 0) {
        i = 0;
    }
    if (i >= (int)HFC_FSTOP_COUNT) {
        i = (int)HFC_FSTOP_COUNT - 1;
    }
    *idx = (size_t)i;
}

float hyperfocal_distance_mm(float focal_mm, float f_number, float coc_mm) {
    if (f_number <= 0.0f || coc_mm <= 0.0f) {
        return 0.0f;
    }
    const float f = focal_mm;
    return (f * f) / (f_number * coc_mm) + f;
}

float hyperfocal_distance_m(float focal_mm, float f_number, float coc_mm) {
    return hyperfocal_distance_mm(focal_mm, f_number, coc_mm) / 1000.0f;
}
