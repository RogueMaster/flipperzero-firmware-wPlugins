#pragma once
/*
 * fsd_events.h — shared CAN event-core (abort / bus-off / disengage detector).
 *
 * Called once per RX frame, after the DAS parse has updated das_ap_state. It
 * detects state transitions and returns a typed event so consumers (the
 * black-box recorder, the future abort-watchdog) don't each re-implement
 * trigger detection.
 *
 * Two entry points:
 *   - fsd_events_poll()   detects EVT_ABORT / EVT_DISENGAGE from das_ap_state
 *                         transitions since the previous poll.
 *   - fsd_events_inject() records caller-sourced events (EVT_MANUAL from the
 *                         dashboard Mark button, EVT_BUSOFF from the TWAI
 *                         bus-off recovery in esp32/.firmware/can_driver.cpp).
 *                         Bus-off is a controller state, not a frame, so it
 *                         can't be derived inside fsd_logic.
 *
 * Both apply a per-event-type cooldown (FSD_EVENT_COOLDOWN_MS) so a flapping
 * abort doesn't emit repeatedly. The emitted event carries from->to state and
 * a timestamp via the evt_last_* fields in FSDState.
 *
 * Header-only (static inline), mirroring fsd_capture.h: this is the shared
 * spine, so both the Flipper FAP and every ESP32 env compile the SAME code by
 * #include alone. The ESP32 build compiles no shared fsd_logic source files, so
 * a separate .c here would force a reimplementation — defeating the point of a
 * shared event-core. Pure / deterministic: no I/O, no platform calls; all
 * per-instance state lives in FSDState.
 */

#include "fsd_handler.h" // DAS_APSTATE_ABORTING / DAS_APSTATE_ABORTED
#include "fsd_state.h" // FSDState (carries the per-instance event bookkeeping)
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EVT_NONE = 0, // no transition this frame
    EVT_ABORT, // das_ap_state entered an abort state (8 ABORTING / 9 ABORTED)
    EVT_DISENGAGE, // das_ap_state high (>= 2) -> not engaged (< 2)
    EVT_MANUAL, // caller-injected (dashboard Mark button)
    EVT_BUSOFF, // caller-injected (TWAI bus-off recovery fired)
    EVT__COUNT, // sentinel: number of event types (= cooldown-slot count)
} FSDEventType;

// FSDState sizes evt_cooldown_until_ms[] with FSD_EVENT_COUNT (it can't include
// this header without a cycle); keep the two in lock-step.
_Static_assert(
    EVT__COUNT == FSD_EVENT_COUNT,
    "FSD_EVENT_COUNT (fsd_state.h) must match the FSDEventType enum");

// Per-event-type cooldown: suppress a repeat of the same event within this
// window so a flapping abort doesn't spam the consumer.
#define FSD_EVENT_COOLDOWN_MS 10000u

static inline bool fsd_events_is_abort_state(uint8_t ap_state) {
    return ap_state == DAS_APSTATE_ABORTING || ap_state == DAS_APSTATE_ABORTED;
}

// Apply the per-type cooldown and, if the event passes, stamp the transition
// detail. Returns evt when emitted, EVT_NONE when still cooling down.
static inline FSDEventType
    fsd_events_emit(FSDState* st, FSDEventType evt, uint8_t from, uint8_t to, uint32_t now_ms) {
    if(now_ms < st->evt_cooldown_until_ms[evt]) return EVT_NONE; // still cooling
    st->evt_cooldown_until_ms[evt] = now_ms + FSD_EVENT_COOLDOWN_MS;
    st->evt_last_from = from;
    st->evt_last_to = to;
    st->evt_last_ms = now_ms;
    return evt;
}

/** Detect an event from the das_ap_state transition since the previous poll.
 *  Reads st->das_ap_state, compares it to the stored previous value, advances
 *  the baseline, and applies the per-type cooldown. Returns EVT_ABORT,
 *  EVT_DISENGAGE, or EVT_NONE. On a fired event the evt_last_* fields are set
 *  to from/to/now_ms. Call once per RX frame after the DAS parse. */
static inline FSDEventType fsd_events_poll(FSDState* st, uint32_t now_ms) {
    uint8_t from = st->evt_prev_ap_state;
    uint8_t to = st->das_ap_state;
    st->evt_prev_ap_state = to; // baseline always advances, even when suppressed

    FSDEventType evt = EVT_NONE;
    if(fsd_events_is_abort_state(to) && !fsd_events_is_abort_state(from)) {
        evt = EVT_ABORT; // entering an abort state (fires once per entry)
    } else if(from >= 2u && to < 2u) {
        evt = EVT_DISENGAGE; // engaged -> not engaged
    }

    if(evt == EVT_NONE) return EVT_NONE;
    return fsd_events_emit(st, evt, from, to, now_ms);
}

/** Record a caller-sourced event. Only EVT_MANUAL and EVT_BUSOFF are accepted
 *  (detection-only / EVT_NONE return EVT_NONE). Applies the same per-type
 *  cooldown as fsd_events_poll(). On a fired event the evt_last_* fields are
 *  set (from == to == current das_ap_state, now_ms). */
static inline FSDEventType fsd_events_inject(FSDState* st, FSDEventType evt, uint32_t now_ms) {
    if(evt != EVT_MANUAL && evt != EVT_BUSOFF) return EVT_NONE; // caller-sourced only
    return fsd_events_emit(st, evt, st->das_ap_state, st->das_ap_state, now_ms);
}
