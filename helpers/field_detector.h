#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#include "emitter_classify.h"
#include "field_scale.h"
#include "present_hold.h"

/* The detector samples the onboard NFC chip's "external field present" bit at a
 * high rate on a worker thread and condenses it into two things:
 *
 *   1. a strength reading - the duty-cycle of an active reader's carrier over a
 *      short window, which behaves like proximity as you sweep;
 *   2. the cadence of the carrier's on/off edges, which fingerprints *what kind*
 *      of emitter it is (see emitter_classify.h).
 *
 * A hidden POS skimmer or rogue door reader polls continuously, so its 13.56 MHz
 * field shows up here even though nothing is ever presented to it. We never
 * transmit. */

#define SPECTER_HISTORY_LEN 64u // samples kept for the on-screen waveform
#define SPECTER_TRACE_LEN   128u // raw carrier level per slice - one screen column each

typedef struct {
    bool armed; // worker is running
    bool error; // could not take over the NFC HAL (another NFC app is open)
    bool present; // a reader field is being detected right now

    /* Display values, mapped onto a full 0..100 meter - see field_scale.h for
     * why raw duty makes a poor gauge reading. */
    uint8_t strength; // 0..100 smoothed field strength
    uint8_t peak; // 0..100 strongest reading since the last reset
    uint8_t average; // 0..100 mean strength across the session
    bool saturated; // meter is pegged; closing in further will not move it

    /* The measurement behind those, untouched: smoothed carrier duty-cycle in
     * percent. The noise floor, calibration and the classifier all work here. */
    uint8_t strength_raw;
    uint32_t contacts; // number of distinct reader "appearances"
    uint32_t last_seen_tick; // furi tick of the last detection (0 = never)
    uint32_t armed_tick; // when the sweep started
    uint32_t elapsed_ms; // wall time since arming
    uint32_t in_field_ms; // of which, time a carrier was actually up

    uint8_t history[SPECTER_HISTORY_LEN]; // ring buffer of recent strength
    uint8_t history_head; // index of the newest sample

    /* Raw carrier on/off, one bit per sample window slice, for the pulse trace.
     * Unlike history[] this is not smoothed - it is what the chip actually saw. */
    uint8_t trace[SPECTER_TRACE_LEN];
    uint8_t trace_head;

    CadenceStats cadence; // edge timing; feed to emitter_classify()

    /* Noise-floor calibration (see field_detector_calibrate_begin). */
    bool calibrating;
    bool calibration_ready;
    uint8_t calibration_floor; // highest ambient duty seen while calibrating
    uint8_t calibration_suggest; // recommended threshold = floor + margin
    uint8_t calibration_progress; // 0..100
} FieldStats;

typedef struct FieldDetector FieldDetector;

FieldDetector* field_detector_alloc(void);
void field_detector_free(FieldDetector* fd);

/* Noise floor: a window must exceed this duty-cycle (%) to count as a reader.
 * Lower = more sensitive (catches fainter/farther readers, more false blips). */
void field_detector_set_threshold(FieldDetector* fd, uint8_t duty_threshold);

/* Raw duty that should read as a full meter. SPECTER_FULL_SCALE_DUTY suits real
 * polling readers; SPECTER_SCALE_RAW shows the unscaled duty instead. Affects
 * only the display values - never detection, calibration or classification. */
void field_detector_set_full_scale(FieldDetector* fd, uint8_t full_scale);

void field_detector_start(FieldDetector* fd);
void field_detector_stop(FieldDetector* fd);
bool field_detector_is_running(FieldDetector* fd);

/* Clear peak / contacts / history / cadence without dropping the radio. */
void field_detector_reset(FieldDetector* fd);

/* Measure the ambient noise floor for duration_ms with the radio already
 * running, then publish a suggested threshold. Progress and the result land in
 * FieldStats. The caller is responsible for standing somewhere quiet. */
void field_detector_calibrate_begin(FieldDetector* fd, uint32_t duration_ms);

/* Abandon a calibration in progress. Nothing is adopted and the previous
 * threshold stays in force. Safe to call when no calibration is running. */
void field_detector_calibrate_cancel(FieldDetector* fd);

/* Atomically copy the latest stats out for the UI. */
void field_detector_get(FieldDetector* fd, FieldStats* out);
