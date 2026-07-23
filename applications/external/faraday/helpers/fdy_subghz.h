/**
 * Faraday - Sub-GHz signal-strength probe.
 *
 * Brings up the internal CC1101 parked in receive on one ISM band and samples
 * raw RSSI (dBm) in a background thread. It maintains the live level, a
 * peak-hold and a slowly-tracked noise floor, and publishes an atomic snapshot
 * for the meter view. Strictly listen-only: Faraday never transmits.
 *
 * The measurement: you press your own key fob / remote and Faraday records how
 * strong its carrier lands - once with the fob in the open (baseline) and once
 * sealed in the pouch (shielded). The dB gap is the pouch's attenuation.
 */
#pragma once

#include <furi.h>
#include <gui/view_dispatcher.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDY_HISTORY_LEN
#define FDY_HISTORY_LEN 64
#endif

/** One selectable ISM band. */
typedef struct {
    uint32_t frequency; /* Hz       */
    const char* label; /* "433.92" */
} FdyBand;

#define FDY_BAND_COUNT 4
extern const FdyBand fdy_bands[FDY_BAND_COUNT];

/** Atomic snapshot copied out for drawing. Plain integers only. */
typedef struct {
    bool running; /* worker active                         */
    bool valid; /* radio came up ok                      */
    int16_t rssi; /* current RSSI (dBm)                    */
    int16_t peak; /* peak-hold RSSI since last reset (dBm) */
    int16_t floor; /* tracked noise floor (dBm)             */
    uint32_t frequency; /* Hz currently tuned                    */
    uint8_t level; /* current RSSI normalised 0..100        */
    uint8_t peak_norm; /* peak normalised 0..100                */
    uint8_t history[FDY_HISTORY_LEN]; /* recent normalised levels */
    uint8_t history_head;
} FdySubGhzSnapshot;

typedef struct FdySubGhz FdySubGhz;

FdySubGhz* fdy_subghz_alloc(ViewDispatcher* view_dispatcher);
void fdy_subghz_free(FdySubGhz* s);

/** Tune before (or during) a run. Applied on the next sample. */
void fdy_subghz_set_freq(FdySubGhz* s, uint32_t frequency);

void fdy_subghz_start(FdySubGhz* s);
void fdy_subghz_stop(FdySubGhz* s);
bool fdy_subghz_is_running(FdySubGhz* s);

/** Drop the peak-hold so the next capture phase starts clean. */
void fdy_subghz_reset_peak(FdySubGhz* s);

void fdy_subghz_get(FdySubGhz* s, FdySubGhzSnapshot* out);

/** Map an RSSI in dBm onto the 0..100 meter scale (shared with the view). */
uint8_t fdy_subghz_normalize(int16_t rssi_dbm);

#ifdef __cplusplus
}
#endif
