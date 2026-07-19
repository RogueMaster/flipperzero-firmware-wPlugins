#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/* The detector samples the onboard NFC chip's "external field present" bit at a
 * high rate on a worker thread and condenses it into a strength reading (the
 * duty-cycle of an active reader's carrier over a short window). A hidden POS
 * skimmer or rogue door reader polls continuously, so its 13.56 MHz field shows
 * up here even though nothing is ever presented to it. We never transmit. */

#define SPECTER_HISTORY_LEN 64u // samples kept for the on-screen waveform

typedef struct {
    bool armed; // worker is running
    bool error; // could not take over the NFC HAL (another NFC app is open)
    bool present; // a reader field is being detected right now
    uint8_t strength; // 0..100 smoothed field strength (carrier duty-cycle)
    uint8_t peak; // 0..100 strongest reading since the last reset
    uint32_t contacts; // number of distinct reader "appearances"
    uint32_t last_seen_tick; // furi tick of the last detection (0 = never)
    uint32_t armed_tick; // when the sweep started

    uint8_t history[SPECTER_HISTORY_LEN]; // ring buffer of recent strength
    uint8_t history_head; // index of the newest sample
} FieldStats;

typedef struct FieldDetector FieldDetector;

FieldDetector* field_detector_alloc(void);
void field_detector_free(FieldDetector* fd);

/* Noise floor: a window must exceed this duty-cycle (%) to count as a reader.
 * Lower = more sensitive (catches fainter/farther readers, more false blips). */
void field_detector_set_threshold(FieldDetector* fd, uint8_t duty_threshold);

void field_detector_start(FieldDetector* fd);
void field_detector_stop(FieldDetector* fd);
bool field_detector_is_running(FieldDetector* fd);

/* Clear peak / contacts / history without dropping the radio. */
void field_detector_reset(FieldDetector* fd);

/* Atomically copy the latest stats out for the UI. */
void field_detector_get(FieldDetector* fd, FieldStats* out);
