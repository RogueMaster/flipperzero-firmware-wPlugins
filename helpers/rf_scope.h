#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Live Sub-GHz envelope scope. A worker parks the CC1101 on one frequency and
 * samples RSSI as fast as it can, pushing a normalised 0..RF_SCOPE_MAX level
 * into a ring buffer. Drawn over time it *is* the OOK envelope: carrier present
 * = high, carrier gone = low - the modulation the walkthrough describes, live.
 * Receive-only; it never transmits. */

#define RF_SCOPE_SAMPLES 128 // ring-buffer width (one per screen column-ish)
#define RF_SCOPE_MAX     48 // top of the normalised level range
#define RF_SCOPE_FREQ_COUNT 4

typedef struct {
    uint8_t level[RF_SCOPE_SAMPLES]; // most-recent-last envelope samples
    uint16_t head; // index just past the newest sample
    float rssi_dbm; // latest raw RSSI
    uint32_t freq_hz; // frequency being watched
    uint8_t threshold; // adaptive "carrier present" level
    bool present; // radio available
    bool running;
} RfSnapshot;

typedef struct RfScope RfScope;

RfScope* rf_scope_alloc(void);
void rf_scope_free(RfScope* s);

/* Frequency preset table (shared with Settings). */
const char* rf_scope_freq_label(uint8_t index);
uint32_t rf_scope_freq_hz(uint8_t index);

void rf_scope_set_freq_index(RfScope* s, uint8_t index); // before start

void rf_scope_start(RfScope* s);
void rf_scope_stop(RfScope* s);
bool rf_scope_is_running(RfScope* s);

void rf_scope_get(RfScope* s, RfSnapshot* out);
