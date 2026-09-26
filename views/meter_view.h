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

/* FdyNfcCapture, mirrored so this radio-agnostic view does not have to include
 * the NFC helper's header. The NFC scene _Static_asserts that the two agree. */
#define FDY_NFC_LIVE 0
#define FDY_NFC_ARMING 1
#define FDY_NFC_MEASURING 2

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
    /* The figure signal_ok is actually decided on: dB of peak-hold above the
     * tracked noise floor for Sub-GHz, peak field duty-% for NFC. Shown on
     * screen because the meter bar tracks the LIVE reading while the lock
     * decision tracks the PEAK - so a bar sitting at nothing next to a strip
     * reading "Signal found" looked like the app contradicting itself. */
    int16_t margin;
    /* The best grade this SETUP could possibly produce, given that a shielded
     * reading can never sink below the noise floor. Max measurable
     * attenuation is (peak - floor), so a baseline only 55 dB above the floor
     * caps the test at A however good the pouch is - and the user has no way
     * to know that from a screen showing a healthy signal. Shown while the
     * baseline is still being taken, when moving the fob closer still helps.
     * FdyRatingCount means "not applicable / no signal yet". */
    uint8_t ceiling;
    /* NFC only: which stage the timed shielded capture is in (FdyNfcCapture)
     * and how many whole seconds are left in it. The Flipper is inside the
     * pouch for this, so the countdown is the only way the user knows whether
     * to keep it sealed or go and fetch it. */
    uint8_t capture;
    uint8_t capture_seconds;

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

/**
 * Take over the action strip for about a second and a half, inverted, with a
 * one-line message. NULL clears it.
 *
 * Used for the two things the user must not miss and that nothing else on
 * screen would tell them:
 *   - why an OK press was refused. A refusal used to be a beep and nothing
 *     else, which from the user's side is indistinguishable from a dead
 *     button: they press OK, hear a noise, the screen does not change, and
 *     they conclude the app is broken.
 *   - that a finished result could not be written to the SD card, so the
 *     measurement they just took is not in the log they think it is in.
 */
void meter_view_flash_notice(MeterView* v, const char* why);

/**
 * Re-arm the opening card that says which object goes in the pouch.
 *
 * The two tests are opposites - Sub-GHz shields the fob, NFC shields the
 * Flipper - and getting it the wrong way round measures nothing while looking
 * exactly like a real test. Call from the scene's on_enter.
 */
void meter_view_reset_intro(MeterView* v);

/** Push a fresh frame. */
void meter_view_update(MeterView* v, const MeterData* data);

/** Advance the animation clock (call on the scene tick). */
void meter_view_tick(MeterView* v);

#ifdef __cplusplus
}
#endif
