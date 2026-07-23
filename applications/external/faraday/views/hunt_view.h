/**
 * Faraday - leak hunt screen.
 *
 * A grade tells you a pouch leaks. This tells you WHERE. Seal your fob in the
 * pouch, hold the button down, and sweep the Flipper along the seams, the
 * zip, the fold and the corners: the meter, the warmer/colder word and the
 * rolling trace all peak over the spot the RF is escaping from.
 *
 * Everything is measured relative to the tracked noise floor, so it reads the
 * same whether you are hunting a strong fob or a weak one.
 */
#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef FDY_HISTORY_LEN
#define FDY_HISTORY_LEN 64
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* band; /* static label, e.g. "433.92 MHz" */
    int16_t rssi; /* live level (dBm)                 */
    int16_t peak; /* peak-hold since reset (dBm)      */
    int16_t floor; /* tracked noise floor (dBm)        */
    uint8_t level; /* live level normalised 0..100     */
    uint8_t peak_norm; /* peak normalised 0..100           */
    const uint8_t* history; /* FDY_HISTORY_LEN ring, or NULL */
    uint8_t history_head;
} HuntData;

typedef void (*HuntViewOkCallback)(void* context);

typedef struct HuntView HuntView;

HuntView* hunt_view_alloc(void);
void hunt_view_free(HuntView* v);
View* hunt_view_get_view(HuntView* v);

/** OK resets the peak-hold so you can re-sweep a spot cleanly. */
void hunt_view_set_ok_callback(HuntView* v, HuntViewOkCallback cb, void* context);

void hunt_view_update(HuntView* v, const HuntData* data);
void hunt_view_tick(HuntView* v);

/** How far above the noise floor the live reading is, in dB. Exposed so the
 *  scene can pace its geiger clicks off exactly what the screen is showing. */
int16_t hunt_view_margin(const HuntData* data);

#ifdef __cplusplus
}
#endif
