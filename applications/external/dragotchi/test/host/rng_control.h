#pragma once
#include <stdint.h>
/* Seed the deterministic host PRNG that backs furi_hal_random_get(). */
void rng_seed(uint32_t seed);
