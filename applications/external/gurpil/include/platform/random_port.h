#pragma once

#include <stdint.h>

/*
 * The furi isolation boundary for randomness: `src/platform/random_port.c` talks to the
 * Flipper HAL RNG, but this header stays plain C so domain code (sim/terrain) can depend on
 * it — or just on the `uint32_t seed` it produces — without ever including furi.
 */

/* Returns a fresh 32-bit seed drawn from the Flipper's hardware RNG. Never returns 0, so
 * callers can hand the result straight to a PRNG that treats 0 specially (e.g. xorshift). */
uint32_t platform_random_seed(void);
