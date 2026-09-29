/**
 * Faraday - find which band your fob transmits on.
 *
 * A key fob transmits on ONE band, and which one depends on where you bought
 * the car: 315 MHz across much of North America, 433.92 across Europe, 868 and
 * 915 elsewhere. Picking the wrong one measures ambient noise and grades it,
 * which is a confident answer to a question nobody asked.
 *
 * This is NOT the "sweep every band and grade each" idea that was deliberately
 * left out of this app. That one is meaningless: a fob is only ever on one
 * band, so the other three would report a flattering attenuation measured
 * against noise. This finds the ONE band the fob is actually on and then hands
 * it to the normal single-band test.
 */
#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDY_BAND_COUNT
#define FDY_BAND_COUNT 4
#endif

typedef struct {
    const char* label[FDY_BAND_COUNT]; /* "433.92" etc          */
    int16_t peak[FDY_BAND_COUNT]; /* strongest dBm seen per band */
    uint8_t norm[FDY_BAND_COUNT]; /* the same, 0..100, for bars  */
    uint8_t active; /* band being sampled right now  */
    uint8_t pass; /* sweeps completed              */
    uint8_t passes; /* sweeps in total               */
    bool done;
    bool found; /* a band won by a clear margin  */
    bool weak; /* something led, but too faint to trust - the useful
                  distinction, because "hold it closer" is a fix and
                  "no fob heard" is not */
    uint8_t winner;
    int16_t margin; /* winner's lead over the rest, dB */
} BandData;

typedef void (*BandViewOkCallback)(void* context);

typedef struct BandView BandView;

BandView* band_view_alloc(void);
void band_view_free(BandView* v);
View* band_view_get_view(BandView* v);

/** OK adopts the winning band (or restarts the scan if it found nothing). */
void band_view_set_ok_callback(BandView* v, BandViewOkCallback cb, void* context);

void band_view_update(BandView* v, const BandData* data);

#ifdef __cplusplus
}
#endif
