/**
 * RollCall - the health-check brain.
 *
 * Pure logic: given the captures from a run, decide whether the remote's code
 * is a healthy rolling code (resists replay) or a fixed code (replayable), and
 * back it up with what was actually observed on the air. No radio, no UI, no
 * globals - trivially testable.
 */
#pragma once

#include "rc_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RcHealthUnknown = 0, /* nothing decoded / not recognised   */
    RcHealthAtRisk, /* fixed code - replayable            */
    RcHealthCaution, /* rolling by design but didn't advance */
    RcHealthLikely, /* rolling, needs another press to be sure */
    RcHealthHealthy, /* rolling AND observed to advance     */
} RcHealth;

typedef struct {
    RcHealth health;
    char letter[3]; /* A / B / C / F / ?                   */
    char protocol[28]; /* representative protocol name        */
    RcCodeClass cls;
    char headline[40]; /* one-line verdict                    */
    char detail[352]; /* plain-English explanation           */
    uint8_t presses; /* distinct presses used               */
    uint8_t unique; /* distinct parcels among them         */
    uint8_t meter; /* 0..100 health bar fill              */
} RcVerdict;

/** Evaluate a run of captures into a verdict. Safe with n == 0. */
void rc_analyze(const RcCapture* caps, uint8_t n, RcVerdict* out);

/** Short tag for a code class ("Rolling" / "Fixed" / "Unknown"). */
const char* rc_class_label(RcCodeClass cls);

/** Short tag for a health band, for footers/badges. */
const char* rc_health_label(RcHealth h);

#ifdef __cplusplus
}
#endif
