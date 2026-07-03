/*
 * Purpose: Define the straight-key release debounce filter.
 * Owns: filter state layout and update/reset declarations.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_straight_filter.c.
 */

#ifndef yo3gnd_straight_filter_2314
#define yo3gnd_straight_filter_2314

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    bool release_pending;
    uint32_t release_started_at;
} MorseFlipperStraightFilter;

void morse_flipper_straight_filter_reset(MorseFlipperStraightFilter* filter);
bool morse_flipper_straight_filter_update(
    MorseFlipperStraightFilter* filter,
    bool raw_down,
    uint32_t now_ms,
    uint16_t release_debounce_ms);

#endif
