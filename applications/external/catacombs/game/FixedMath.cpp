// #include "FixedMath.h"

// static uint16_t xs = 1;

// uint16_t Random() {
//     xs ^= xs << 7;
//     xs ^= xs >> 9;
//     xs ^= xs << 8;
//     return xs;
// }

// void SeedRandom() {
//     uint32_t r = furi_hal_random_get();
//     xs = (uint16_t)(r ^ (r >> 16));
// }

// void SeedRandom(uint16_t seed) {
//     xs = seed | 1;
// }
