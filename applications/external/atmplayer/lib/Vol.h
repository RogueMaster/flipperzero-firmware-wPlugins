#pragma once

#include <stdint.h>

typedef struct {
    uint16_t env_q8;
} VolMeter;

static inline void vol_meter_reset(VolMeter* m) {
    m->env_q8 = 0;
}

static inline uint8_t vol_meter_step(VolMeter* m, uint8_t sample_abs) {
    const uint16_t target = (uint16_t)sample_abs << 8;

    if(target >= m->env_q8) {
        m->env_q8 = target;
    } else {
        uint16_t decay = (uint16_t)((m->env_q8 >> 4) + 1);
        m->env_q8 = (m->env_q8 > decay) ? (uint16_t)(m->env_q8 - decay) : 0;
    }

    uint16_t lvl = (uint16_t)(m->env_q8 >> 8);
    if(lvl > 63) lvl = 63;
    return (uint8_t)lvl;
}
