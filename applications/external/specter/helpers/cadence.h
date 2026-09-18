#pragma once

#include "emitter_classify.h"
#include "present_hold.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Timing a carrier's on/off rhythm.
 *
 * This was the last non-trivial algorithm in the app still buried inside the
 * worker where no test could reach it, and both of the cadence bugs that
 * shipped lived here:
 *
 *   1. A run of any length was treated as a phase of a poll cycle. Walking
 *      twenty seconds between two readers pushed one "cycle" with a 20000 ms
 *      gap in beside fifteen 150 ms ones, which dragged the mean period and the
 *      jitter far enough out that a textbook POLLING reader read INTERMITTENT
 *      for the next sixteen cycles - long enough to save that wrong verdict.
 *
 *   2. The cycle count only ever went up and nothing aged the ring out, so the
 *      classifier's silence branch (duty == 0 && bursts == 0) became
 *      unreachable after the very first cycle. Fingerprint kept declaring
 *      "POLLING, 100%" with full timings in a room with no emitter in it.
 *
 * Both are the same mistake: treating the ring as a pile of numbers rather than
 * as evidence with a lifetime. So the rules are explicit here now -
 *
 *   - a run longer than we would ever hold presence for is a DISCONTINUITY, not
 *     a phase, and it ends the cycle instead of becoming one;
 *   - when the emitter goes away the evidence is dropped, so the verdict can
 *     fall back to SAMPLING and then to NO FIELD.
 *
 * Pure and header-only, so the firmware and the host tests share one copy. */

#define SPECTER_CADENCE_RING 16u

/* A run longer than this cannot be one phase of somebody's polling cycle. */
#define SPECTER_CADENCE_MAX_PHASE_MS SPECTER_PRESENT_HOLD_MAX_MS

typedef struct {
    uint16_t burst[SPECTER_CADENCE_RING];
    uint16_t gap[SPECTER_CADENCE_RING];
    uint16_t period[SPECTER_CADENCE_RING];
    uint8_t count; // entries filled, saturates at SPECTER_CADENCE_RING
    uint8_t head; // next slot to write
    uint16_t total; // complete cycles currently represented by the ring

    /* edge state */
    bool raw_prev;
    uint32_t run_start; // in ticks, as the caller counts them
    bool have_burst;
    uint32_t last_burst_ms;

    /* Ticks are not milliseconds by definition - they are milliseconds because
     * this firmware happens to run a 1 kHz scheduler. Everything downstream is
     * LABELLED ms (period_ms, jitter_ms, and the "204ms" the Fingerprint screen
     * prints), so the conversion is done here rather than assumed. */
    uint32_t tick_hz;
} CadenceTracker;

static inline uint16_t cadence_clamp_u16(uint32_t v) {
    return (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
}

/* Anchor the tracker to the carrier as it is right now. Anything measured
 * before this point is discarded - including the run in progress, which is why
 * `raw` and `now` are required rather than assumed. */
static inline void
    cadence_tracker_reset(CadenceTracker* t, bool raw, uint32_t now, uint32_t tick_hz) {
    memset(t, 0, sizeof(*t));
    t->raw_prev = raw;
    t->run_start = now;
    t->tick_hz = tick_hz ? tick_hz : 1000u;
}

/* Convert a tick delta to milliseconds. Deltas are small - a phase is capped at
 * SPECTER_CADENCE_MAX_PHASE_MS - so the multiply cannot overflow. */
static inline uint32_t cadence_ticks_to_ms(const CadenceTracker* t, uint32_t ticks) {
    if(t->tick_hz == 1000u) return ticks; // the common case, exactly
    if(ticks > 0xFFFFFu) ticks = 0xFFFFFu; // far beyond any real phase
    return (ticks * 1000u) / t->tick_hz;
}

/* Forget the measured rhythm but keep tracking edges. Called when presence is
 * released: the emitter left, so what we measured is no longer evidence. */
static inline void cadence_tracker_drop(CadenceTracker* t) {
    memset(t->burst, 0, sizeof(t->burst));
    memset(t->gap, 0, sizeof(t->gap));
    memset(t->period, 0, sizeof(t->period));
    t->count = 0;
    t->head = 0;
    t->total = 0;
    t->have_burst = false;
}

/* Feed one sample. Returns true if a complete cycle was just recorded. */
static inline bool cadence_tracker_sample(CadenceTracker* t, bool raw, uint32_t now) {
    if(raw == t->raw_prev) return false;

    uint32_t run_ms = cadence_ticks_to_ms(t, now - t->run_start);
    bool contiguous = run_ms <= SPECTER_CADENCE_MAX_PHASE_MS;
    bool pushed = false;

    if(t->raw_prev) {
        /* an ON run just ended - it is only a burst if it was plausibly one */
        t->last_burst_ms = run_ms;
        t->have_burst = contiguous;
    } else if(t->have_burst) {
        /* an OFF run just ended, and we have its burst: one full cycle -
         * unless the silence was so long that the emitter clearly went away */
        if(contiguous) {
            uint32_t b = t->last_burst_ms;
            t->burst[t->head] = cadence_clamp_u16(b);
            t->gap[t->head] = cadence_clamp_u16(run_ms);
            t->period[t->head] = cadence_clamp_u16(b + run_ms);
            t->head = (uint8_t)((t->head + 1u) % SPECTER_CADENCE_RING);
            if(t->count < SPECTER_CADENCE_RING) t->count++;
            if(t->total < UINT16_MAX) t->total++;
            pushed = true;
        } else {
            t->have_burst = false;
        }
    }

    t->run_start = now;
    t->raw_prev = raw;
    return pushed;
}

/* Condense into the figures the classifier wants. Jitter is the mean absolute
 * deviation of the period - a plain, explainable measure of how steady the
 * rhythm is, and one that needs no square root. */
static inline void
    cadence_tracker_summarise(const CadenceTracker* t, CadenceStats* out, uint8_t duty) {
    memset(out, 0, sizeof(*out));
    out->duty = duty;
    out->bursts = t->total;
    if(t->count == 0) return;

    uint32_t n = t->count;
    uint32_t burst_sum = 0, gap_sum = 0, period_sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        burst_sum += t->burst[i];
        gap_sum += t->gap[i];
        period_sum += t->period[i];
    }
    out->burst_ms = cadence_clamp_u16(burst_sum / n);
    out->gap_ms = cadence_clamp_u16(gap_sum / n);
    out->period_ms = cadence_clamp_u16(period_sum / n);

    uint32_t mean = period_sum / n;
    uint32_t dev_sum = 0;
    for(uint32_t i = 0; i < n; i++) {
        uint32_t p = t->period[i];
        dev_sum += (p > mean) ? (p - mean) : (mean - p);
    }
    out->jitter_ms = cadence_clamp_u16(dev_sum / n);
}
