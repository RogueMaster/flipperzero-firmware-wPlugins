#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Emitter fingerprinting.
 *
 * The field detector only ever gets one bit from the ST25R3916: "is an external
 * 13.56 MHz carrier present right now?". Sampled fast enough, the *timing* of
 * that bit still says a lot about what is emitting it:
 *
 *   - A reader parked in continuous-wave mode holds the carrier up permanently.
 *   - A reader in normal polling mode emits short bursts on a fixed cadence
 *     (wake up, listen for a card, sleep) - very regular, low jitter.
 *   - A handset doing peer-to-peer / a reader being actively used / RF noise
 *     bursts irregularly.
 *
 * This module is the pure decision layer: it takes accumulated burst statistics
 * and returns a class + a confidence. No furi, no hardware - so it is compiled
 * and tested on the host in test/.
 */

/* Sampling granularity of the detector, in milliseconds. Every duration this
 * module reasons about is quantised to this, and anything close to it must be
 * reported as unreliable rather than dressed up as a precise number. */
#define SPECTER_SAMPLE_MS 2u

/* A period must span this many samples before we quote a figure for it. */
#define SPECTER_MIN_PERIOD_SAMPLES 5u

/* Complete bursts needed before a cadence verdict is anything but a guess. */
#define SPECTER_MIN_BURSTS 3u

typedef enum {
    EmitterClassNoField = 0, // nothing above the noise floor
    EmitterClassUnknown, // seen something, not enough of it yet
    EmitterClassContinuous, // unbroken carrier
    EmitterClassPolling, // regular bursts on a fixed cadence
    EmitterClassIntermittent, // bursty but irregular
} EmitterClass;

typedef struct {
    uint16_t bursts; // complete on->off->on cycles measured
    uint16_t burst_ms; // mean carrier-on duration
    uint16_t gap_ms; // mean carrier-off duration
    uint16_t period_ms; // mean burst + gap
    uint16_t jitter_ms; // mean absolute deviation of the period
    uint8_t duty; // 0..100 carrier duty-cycle over the window
} CadenceStats;

typedef struct {
    EmitterClass klass;
    uint8_t confidence; // 0..100
    bool timing_reliable; // false => durations are at/below sampling resolution
} EmitterVerdict;

/* Classify accumulated cadence statistics. Pure, total, no allocation. */
EmitterVerdict emitter_classify(const CadenceStats* c);

/* Short display strings (never NULL). */
const char* emitter_class_name(EmitterClass k); // "POLLING"
const char* emitter_class_blurb(EmitterClass k); // one-line plain-English gloss
