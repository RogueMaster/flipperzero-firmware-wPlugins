/**
 * Faraday - the measurement screen.
 *
 * One radio-agnostic view drives the whole test. It renders three faces:
 *   - Baseline  : live signal meter while you capture the open-air reference.
 *   - Shielded  : same meter while you capture the pouch-sealed level.
 *   - Verdict   : before/after comparison bars, the attenuation figure and a
 *                 letter grade.
 * The owning scene fills a MeterData each tick; the view knows all the on-screen
 * copy so both the Sub-GHz and NFC flows share pixel-for-pixel presentation.
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

typedef enum {
    FdyPhaseBaseline = 0, /* capturing the open-air reference */
    FdyPhaseShield, /* capturing the pouch-sealed level */
    FdyPhaseVerdict, /* grade + comparison               */
    FdyPhaseError, /* radio unavailable                */
} FdyPhase;

/** Everything the view needs for one frame. Filled by the scene. */
typedef struct {
    bool is_nfc; /* selects unit (%/dBm) + prompts     */
    const char* band; /* static label, e.g. "433.92 MHz"   */
    FdyPhase phase;

    /* live meter */
    uint8_t level; /* current level normalised 0..100    */
    uint8_t peak; /* peak-hold normalised 0..100        */
    int16_t live_value; /* real reading to print (dBm or %)   */
    bool signal_ok; /* a signal is actually present now   */

    /* captured references */
    bool have_base;
    bool have_shield;
    int16_t base_value; /* real units */
    int16_t shield_value;
    uint8_t base_norm; /* 0..100     */
    uint8_t shield_norm;
    bool shield_floored; /* shielded reading sat at the noise floor */

    /* verdict */
    int16_t atten; /* dB attenuation, or % of field blocked */
    bool atten_floored; /* attenuation is a ">=" lower bound     */
    uint8_t rating; /* FdyRating                             */

    /* decoration */
    const uint8_t* history; /* FDY_HISTORY_LEN ring, or NULL */
    uint8_t history_head;

    /* error face */
    const char* err1;
    const char* err2;
} MeterData;

typedef void (*MeterViewOkCallback)(void* context);

typedef struct MeterView MeterView;

MeterView* meter_view_alloc(void);
void meter_view_free(MeterView* v);
View* meter_view_get_view(MeterView* v);

/** OK fires when the user locks/advances. */
void meter_view_set_ok_callback(MeterView* v, MeterViewOkCallback cb, void* context);

/** Push a fresh frame. */
void meter_view_update(MeterView* v, const MeterData* data);

/** Advance the animation clock (call on the scene tick). */
void meter_view_tick(MeterView* v);

#ifdef __cplusplus
}
#endif
