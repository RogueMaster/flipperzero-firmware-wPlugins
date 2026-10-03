#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Valid for deadlines less than INT32_MAX ticks away. */
static inline bool morse_flipper_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static inline bool morse_flipper_time_pending(uint32_t now_ms, uint32_t deadline_ms) {
    return !morse_flipper_time_reached(now_ms, deadline_ms);
}
