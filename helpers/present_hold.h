#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Debouncing "a reader is present".
 *
 * The detector decides presence one sampling window at a time: did the carrier
 * exceed the noise floor during these ~96 ms? For a continuous-wave emitter
 * that is a fine answer. For a *polling* reader it is not - it emits a short
 * burst every 100-300 ms, so consecutive windows legitimately alternate between
 * "carrier seen" and "nothing at all". Reporting that raw makes a perfectly
 * steady reader look like it is flickering in and out of existence.
 *
 * That flicker was not cosmetic. Every rising edge counted a fresh contact and
 * fired the found-alert and the watch-mode screen wake. Alert sequences are
 * queued to the notification service with an unbounded wait, so posting one
 * every couple of hundred milliseconds could fill that queue and block the GUI
 * thread outright - a frozen app you cannot even back out of.
 *
 * So presence latches: one window above the floor asserts it, and it stays
 * asserted until nothing has been seen for hold_ms. Set the hold longer than
 * the slowest poll cycle you care to track as continuous.
 *
 * Pure and header-only so the firmware and the host tests share one copy. */

/* Longer than a typical reader's poll gap (100-300 ms) with room to spare, and
 * still short enough that walking away from a reader updates promptly. */
#define SPECTER_PRESENT_HOLD_MS 1500u

typedef struct {
    uint32_t last_hit_tick; // when the carrier was last seen above the floor
    bool seen; // anything at all since the last reset
} PresentHold;

static inline void present_hold_reset(PresentHold* h) {
    h->last_hit_tick = 0;
    h->seen = false;
}

/* Feed one window's raw verdict; get back the debounced presence.
 * Tick arithmetic is unsigned throughout, so it survives the tick counter
 * wrapping past 2^32 mid-sweep. */
static inline bool present_hold_update(PresentHold* h, bool hit, uint32_t now, uint32_t hold_ms) {
    if(hit) {
        h->last_hit_tick = now;
        h->seen = true;
        return true;
    }
    if(!h->seen) return false;
    return (uint32_t)(now - h->last_hit_tick) < hold_ms;
}
