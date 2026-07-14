/*
 * Purpose: Debounce straight-key release transitions.
 * Owns: straight filter state updates and release debounce policy.
 * Depends on: morse_flipper_straight_filter.h.
 * Tests: tests/test_straight_filter.c.
 */

#include "morse_flipper_straight_filter.h"

#include <stddef.h>

void morse_flipper_straight_filter_reset(MorseFlipperStraightFilter* filter) {
    if(filter == NULL) return;
    filter->active = false;
    filter->release_pending = false;
    filter->release_started_at = 0U;
}

bool morse_flipper_straight_filter_update(
    MorseFlipperStraightFilter* filter,
    bool raw_down,
    uint32_t now_ms,
    uint16_t release_debounce_ms) {
    if(filter == NULL) return raw_down;

    if(raw_down) {
        filter->active = true;
        filter->release_pending = false;
        filter->release_started_at = 0U;
        return true;
    }

    if(!filter->active) {
        filter->release_pending = false;
        filter->release_started_at = 0U;
        return false;
    }

    if(!filter->release_pending) {
        filter->release_pending = true;
        filter->release_started_at = now_ms;
        return true;
    }

    if((uint32_t)(now_ms - filter->release_started_at) < release_debounce_ms) {
        return true;
    }

    filter->active = false;
    filter->release_pending = false;
    filter->release_started_at = 0U;
    return false;
}
