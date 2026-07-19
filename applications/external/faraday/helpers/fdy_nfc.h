/**
 * Faraday - NFC field-strength probe.
 *
 * Parks the onboard ST25R3916 in external-field detect mode and samples the
 * "carrier present?" bit hundreds of times a second, condensing it to a
 * duty-cycle strength (0..100%). It never energises its own field.
 *
 * The measurement: hold the Flipper in a reader's 13.56 MHz field (a phone
 * doing NFC, a contactless terminal) and record how much of that field lands -
 * once bare (baseline), once with the Flipper sealed in the pouch (shielded).
 * The pouch's job is to keep that interrogation field off the card inside it.
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDY_HISTORY_LEN
#define FDY_HISTORY_LEN 64
#endif

/** Atomic snapshot for the meter view. */
typedef struct {
    bool armed; /* detector running                     */
    bool error; /* NFC chip unavailable (held elsewhere) */
    bool present; /* a carrier is over the noise floor now */
    uint8_t strength; /* current field duty-cycle 0..100%      */
    uint8_t peak; /* peak-hold strength since reset        */
    uint8_t history[FDY_HISTORY_LEN];
    uint8_t history_head;
} FdyNfcSnapshot;

typedef struct FdyNfc FdyNfc;

FdyNfc* fdy_nfc_alloc(void);
void fdy_nfc_free(FdyNfc* n);

void fdy_nfc_start(FdyNfc* n);
void fdy_nfc_stop(FdyNfc* n);
bool fdy_nfc_is_running(FdyNfc* n);

/** Drop the peak-hold so the next capture phase starts clean. */
void fdy_nfc_reset_peak(FdyNfc* n);

void fdy_nfc_get(FdyNfc* n, FdyNfcSnapshot* out);

#ifdef __cplusplus
}
#endif
