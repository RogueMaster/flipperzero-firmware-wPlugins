#pragma once

#include <stdint.h>

#include "sr_types.h"

/*
 * ★ Pure-logic capture-health verdict. Must not include any furi header (ADR-003).
 *
 * Card F2 — Dash headline from the board's Qual: facts. Thresholds are UX and
 * live here so they can change without a firmware reflash. Evaluation order is
 * frozen (docs/exec-plans/f2-capture-health.md §1.3); do not reorder.
 */

typedef enum {
    SrHealthAcquiring = 0,
    SrHealthOk,
    SrHealthWarn,
    SrHealthCrit
} SrHealthVerdict;

/* reason: topd category 1–6, or an exclusive code that does not collide. */
enum {
    SrHealthReasonNone = 0,
    SrHealthReasonGps = 1,
    SrHealthReasonSdDrop = 2,
    SrHealthReasonLink = 3,
    SrHealthReasonQueue = 4,
    SrHealthReasonScan = 5,
    SrHealthReasonDedup = 6,
    SrHealthReasonSdDead = 7, /* sd==0 CRIT; not topd=2 (SD drop category) */
    SrHealthReasonNoFix = 8,
    SrHealthReasonLowFix = 9,
    SrHealthReasonAcquiring = 10
};

typedef struct {
    SrHealthVerdict v;
    uint8_t reason; /* topd category or exclusive code */
    uint8_t fixpct;
} SrHealthEval;

/*
 * ⬜ PLACEHOLDER, not a measurement. Owner: product.
 * Method: G0-2 / G0-3 cold-start-to-first-fix distribution (card §6).
 * In-repo TTFF constant: MISSING — pmtk.h:164 PMTK_BOOT_POS_FIX_MS is one
 * position epoch AFTER lock (1000 ms), not cold-start TTFF. The #error pins
 * that weak floor so a 0/1 ms placeholder cannot compile. Do not take the
 * floor as the value (tbd-placeholder-must-clear-physical-floor).
 */
#define SR_HEALTH_ACQUIRE_EPOCH_MS_FLOOR 1000u /* PMTK_BOOT_POS_FIX_MS; NOT TTFF */
#define ACQUIRE_GRACE_MS_TBD             60000u
#if ACQUIRE_GRACE_MS_TBD <= SR_HEALTH_ACQUIRE_EPOCH_MS_FLOOR
#error \
    "ACQUIRE_GRACE_MS_TBD must exceed one position epoch (PMTK_BOOT_POS_FIX_MS); TTFF anchor missing until G0-2/G0-3"
#endif

/*
 * ⬜ PLACEHOLDER, not a measurement. Owner: product.
 * Method: real-trip vs antenna-fault fix% distributions (card §6).
 * 0 would make the WARN-low-fix branch unreachable; 100 would make OK
 * unreachable when gga>0. Both sides are pinned.
 */
#define FIXPCT_WARN_TBD 90u
#if FIXPCT_WARN_TBD == 0
#error "FIXPCT_WARN_TBD == 0 makes the low-fix WARN branch unreachable"
#endif
#if FIXPCT_WARN_TBD >= 100u
#error "FIXPCT_WARN_TBD >= 100 makes the OK branch unreachable when gga>0"
#endif

/*
 * ⬜ PLACEHOLDER, not a measurement. Owner: product.
 * Method: real-trip drop-rate baseline (card §6).
 * 0 would make every session with any drop WARN; 100 would make the
 * drop-rate WARN branch unreachable. Both sides are pinned.
 */
#define DROP_RATE_WARN_PCT_TBD 10u
#if DROP_RATE_WARN_PCT_TBD == 0
#error "DROP_RATE_WARN_PCT_TBD == 0 makes the OK branch unreachable when drop>0"
#endif
#if DROP_RATE_WARN_PCT_TBD >= 100u
#error "DROP_RATE_WARN_PCT_TBD >= 100 makes the drop-rate WARN branch unreachable"
#endif

static inline SrHealthEval sr_capture_health_eval(const SrQualInfo* q, uint32_t elapsed_ms) {
    SrHealthEval e;
    uint32_t fixpct;
    uint64_t den;
    uint64_t rate;

    e.v = SrHealthAcquiring;
    e.reason = (uint8_t)SrHealthReasonAcquiring;
    e.fixpct = 0;

    if(q == NULL) {
        return e;
    }

    if(q->gga == 0u) {
        fixpct = 0u;
    } else {
        fixpct = (uint32_t)(((uint64_t)q->ggafix * 100u) / (uint64_t)q->gga);
    }
    e.fixpct = (uint8_t)(fixpct > 255u ? 255u : fixpct);

    /* 1. sd==0 → CRIT (binary, no threshold) */
    if(q->sd == 0u) {
        e.v = SrHealthCrit;
        e.reason = (uint8_t)SrHealthReasonSdDead;
        return e;
    }

    /* 2. gga==0 inside grace → Acquiring */
    if(q->gga == 0u && elapsed_ms < (uint32_t)ACQUIRE_GRACE_MS_TBD) {
        e.v = SrHealthAcquiring;
        e.reason = (uint8_t)SrHealthReasonAcquiring;
        return e;
    }

    /* 3. gga==0 past grace → WARN no-fix */
    if(q->gga == 0u && elapsed_ms >= (uint32_t)ACQUIRE_GRACE_MS_TBD) {
        e.v = SrHealthWarn;
        e.reason = (uint8_t)SrHealthReasonNoFix;
        return e;
    }

    /* 4. fixpct < threshold → WARN low-fix */
    if(fixpct < (uint32_t)FIXPCT_WARN_TBD) {
        e.v = SrHealthWarn;
        e.reason = (uint8_t)SrHealthReasonLowFix;
        return e;
    }

    /* 5. drop*100/(drop+net) > threshold (den>0) → WARN drop, reason=topd */
    den = (uint64_t)q->drop + (uint64_t)q->net;
    if(den > 0u) {
        rate = ((uint64_t)q->drop * 100u) / den;
        if(rate > (uint64_t)DROP_RATE_WARN_PCT_TBD) {
            e.v = SrHealthWarn;
            e.reason = q->topd;
            return e;
        }
    }

    /* 6. otherwise OK */
    e.v = SrHealthOk;
    e.reason = (uint8_t)SrHealthReasonNone;
    return e;
}
