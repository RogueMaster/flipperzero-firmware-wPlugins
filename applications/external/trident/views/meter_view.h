#pragma once

#include <gui/view.h>
#include <input/input.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Signal meter view — a live "finder" gauge shared by the CC1101 Frequency
 * Finder and the NRF24 Channel Finder. It renders a big numeric readout, a
 * segmented bar meter with peak-hold, a sub-label (frequency / channel) and a
 * footer hint. The owning scene copies a fresh MeterSnapshot in on every tick;
 * directional keys are forwarded to the scene so it can retune the radio.
 */

typedef struct {
    bool running;
    bool present;
    char title[20]; // header, e.g. "CC1101 Finder"
    int16_t level; // 0..100 gauge fill
    int16_t peak; // 0..100 peak marker
    char value[12]; // big readout, e.g. "-52" or "72"
    char unit[6]; // "dBm" / "%"
    char sub[24]; // "433.92 MHz" / "Ch 52  2452 MHz"
    char foot[26]; // footer hint
    uint32_t count; // packets / events (shown when > 0)
} MeterSnapshot;

typedef struct MeterView MeterView;
typedef void (*MeterViewInputCb)(void* context, InputKey key);

MeterView* meter_view_alloc(void);
void meter_view_free(MeterView* v);
View* meter_view_get_view(MeterView* v);

// Directional / OK keys are forwarded here (scene retunes the radio).
void meter_view_set_input_callback(MeterView* v, MeterViewInputCb cb, void* context);

void meter_view_set_snapshot(MeterView* v, const MeterSnapshot* snap);
