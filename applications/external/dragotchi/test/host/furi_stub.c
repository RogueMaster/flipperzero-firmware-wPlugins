/* Host implementation of the hardware/OS seams the game logic touches.
 * Deterministic, seedable PRNG so probabilistic tests are reproducible. */
#include "rng_control.h"
#include <stdint.h>

static uint32_t s_state = 0x12345678u;

void rng_seed(uint32_t seed) {
    /* avoid the zero state for xorshift */
    s_state = seed ? seed : 0xA5A5A5A5u;
}

uint32_t furi_hal_random_get(void) {
    /* xorshift32 */
    uint32_t x = s_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_state = x;
    return x;
}
