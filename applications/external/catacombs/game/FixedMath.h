#pragma once

#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>
#if defined(_WIN32)
#include <math.h>
#endif
#include "game/Defines.h"

#define FIXED_SHIFT       8
#define FIXED_ONE         (1 << FIXED_SHIFT)
#define INT_TO_FIXED(x)   ((x) * FIXED_ONE)
#define FIXED_TO_INT(x)   ((x) >> 8)
#define FLOAT_TO_FIXED(x) ((int16_t)((x) * FIXED_ONE))

#ifndef ABS
#define ABS(x) (((x) < 0) ? -(x) : (x))
#endif

#define FIXED_ANGLE_MAX           256
#define FIXED_ANGLE_WRAP(x)       ((x) & 255)
#define FIXED_ANGLE_45            (FIXED_ANGLE_90 / 2)
#define FIXED_ANGLE_90            (FIXED_ANGLE_MAX / 4)
#define FIXED_ANGLE_180           (FIXED_ANGLE_90 * 2)
#define FIXED_ANGLE_270           (FIXED_ANGLE_90 * 3)
#define FIXED_ANGLE_TO_RADIANS(x) ((x) * (2.0f * 3.141592654f / FIXED_ANGLE_MAX))

extern const int16_t sinTable[FIXED_ANGLE_MAX] PROGMEM;

inline int16_t FixedSin(uint8_t angle) {
    return pgm_read_word(&sinTable[angle]);
}

inline int16_t FixedCos(uint8_t angle) {
    return pgm_read_word(&sinTable[FIXED_ANGLE_WRAP(FIXED_ANGLE_90 - angle)]);
}

// uint16_t Random();
// void SeedRandom();
// void SeedRandom(uint16_t seed);

inline uint16_t Random() {
    uint32_t r = furi_hal_random_get();
    return (uint16_t)(r ^ (r >> 16));
}
