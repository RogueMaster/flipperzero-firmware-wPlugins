#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Spectrum analyzer view — shared by the NRF24 (2.4 GHz, 126 channels) and the
 * CC1101 (Sub-GHz band sweep) radios. Both produce the same SpectrumSnapshot;
 * the owning scene copies a fresh snapshot into the view on every tick and the
 * view renders it as a live bar graph with peak-hold and a status header.
 *
 * The view keeps no radio state of its own: it is a pure renderer. Reset (OK)
 * is surfaced to the scene through a callback so the scene can clear the peak
 * hold / noise floor in the worker.
 */

#define SPECTRUM_MAX_BINS 126

typedef struct {
    uint8_t count; // active bins (<= SPECTRUM_MAX_BINS)
    uint8_t level[SPECTRUM_MAX_BINS]; // 0..100 activity/strength per bin
    bool running; // worker sampling
    bool present; // radio hardware answered on the bus
    int16_t peak_bin; // strongest bin, -1 if none
    int16_t peak_value; // raw value at the peak (dBm or hit-%)
    char title[20]; // header label, e.g. "NRF24 - 2.4 GHz"
    char peak_label[20]; // e.g. "Ch 42  2442 MHz"
    char unit[6]; // "dBm" or "%"
    char lo_label[10]; // left x-axis label
    char hi_label[10]; // right x-axis label
    uint32_t sweeps; // completed sweeps
} SpectrumSnapshot;

typedef struct SpectrumView SpectrumView;
typedef void (*SpectrumViewCallback)(void* context);

SpectrumView* spectrum_view_alloc(void);
void spectrum_view_free(SpectrumView* v);
View* spectrum_view_get_view(SpectrumView* v);

// OK button -> scene resets peak-hold / floor in the worker.
void spectrum_view_set_ok_callback(SpectrumView* v, SpectrumViewCallback cb, void* context);

// Copy a fresh snapshot in for rendering (call from the scene tick).
void spectrum_view_set_snapshot(SpectrumView* v, const SpectrumSnapshot* snap);
