#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef ARDUBBOYLIB_RANDOM_LEGACY
#include <furi_hal_random.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Two explicit backends:
 * - Default (no define): Flipper HAL RNG
 * - With ARDULIB_RANDOM_LEGACY: legacy xorshift16 RNG
 */

#ifdef ARDULIB_RANDOM_LEGACY
/* ============================ LEGACY BACKEND ============================ */

static inline uint16_t* arduboy_legacy_random_state_ptr(void) {
    static uint16_t state = 1u;
    return &state;
}

static inline uint16_t Random(void) {
    uint16_t x = *arduboy_legacy_random_state_ptr();
    if(x == 0u) x = 1u;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    *arduboy_legacy_random_state_ptr() = x;
    return x;
}

static inline void SeedRandom(uint16_t seed) {
    *arduboy_legacy_random_state_ptr() = (uint16_t)(seed | 1u);
}

static inline uint32_t random32(void) {
    return ((uint32_t)Random() << 16u) | (uint32_t)Random();
}

static inline uint16_t random16(void) {
    return Random();
}

static inline uint8_t random8(void) {
    return (uint8_t)(Random() & 0xFFu);
}

static inline void randomSeed(uint32_t seed) {
    SeedRandom((uint16_t)seed);
}

static inline long randomLong(void) {
    return (long)(random32() & 0x7FFFFFFFUL);
}

static inline long randomMax(long max_value) {
    if(max_value <= 0) return 0;
    return (long)(random32() % (uint32_t)max_value);
}

static inline long randomRange(long min_value, long max_value) {
    const int64_t span64 = (int64_t)max_value - (int64_t)min_value;
    if(span64 <= 0) return min_value;
    return (long)(min_value + (long)(random32() % (uint32_t)span64));
}

static inline void randomBytes(uint8_t* buf, uint32_t len) {
    if(!buf || !len) return;
    while(len) {
        const uint16_t v = Random();
        *buf++ = (uint8_t)(v & 0xFFu);
        len--;
        if(!len) break;
        *buf++ = (uint8_t)(v >> 8u);
        len--;
    }
}

#else
/* =========================== FLIPPER HAL BACKEND ======================== */

static inline uint16_t Random(void) {
    return (uint16_t)(furi_hal_random_get() & 0xFFFFu);
}

static inline void SeedRandom(uint16_t seed) {
    (void)seed;
}

static inline uint32_t random32(void) {
    return furi_hal_random_get();
}

static inline uint16_t random16(void) {
    return (uint16_t)(furi_hal_random_get() & 0xFFFFu);
}

static inline uint8_t random8(void) {
    return (uint8_t)(furi_hal_random_get() & 0xFFu);
}

static inline void randomSeed(uint32_t seed) {
    (void)seed;
}

static inline long randomLong(void) {
    return (long)(furi_hal_random_get() & 0x7FFFFFFFUL);
}

static inline long randomMax(long max_value) {
    if(max_value <= 0) return 0;
    return (long)(furi_hal_random_get() % (uint32_t)max_value);
}

static inline long randomRange(long min_value, long max_value) {
    const int64_t span64 = (int64_t)max_value - (int64_t)min_value;
    if(span64 <= 0) return min_value;
    return (long)(min_value + (long)(furi_hal_random_get() % (uint32_t)span64));
}

static inline void randomBytes(uint8_t* buf, uint32_t len) {
    if(!buf || !len) return;
    furi_hal_random_fill_buf(buf, len);
}

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
static inline long random(long max_value) {
    return randomMax(max_value);
}

static inline long random(long min_value, long max_value) {
    return randomRange(min_value, max_value);
}
#endif
