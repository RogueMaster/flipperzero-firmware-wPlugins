#pragma once

#include <furi.h>
#include <stdbool.h>
#include "../views/spectrum_view.h"

/*
 * CC1101 Sub-GHz spectrum analyzer.
 *
 * A background worker steps the CC1101 across a chosen band in fixed frequency
 * bins, reads RSSI at each step, and normalises it into a 0..100 level with a
 * decaying max-hold, publishing the result as a SpectrumSnapshot. It can drive
 * either the Flipper's internal CC1101 or the board's external CC1101 (the
 * cc1101_ext device, when the firmware provides that driver); if the external
 * device is unavailable it transparently falls back to the internal radio.
 *
 * Read-only: the analyzer only receives.
 */

typedef struct {
    uint32_t lo_hz;
    uint32_t hi_hz;
    const char* label; // e.g. "300-348"
    const char* lo_label; // e.g. "300"
    const char* hi_label; // e.g. "348"
} SubghzBand;

#define TRIDENT_SUBGHZ_BAND_COUNT 3
extern const SubghzBand trident_subghz_bands[TRIDENT_SUBGHZ_BAND_COUNT];
const char* trident_subghz_band_label(uint8_t index);

typedef struct SubghzRadio SubghzRadio;

SubghzRadio* subghz_radio_alloc(void);
void subghz_radio_free(SubghzRadio* radio);

// Set the band and device before starting (device latched at start; band is live).
void subghz_radio_configure(SubghzRadio* radio, uint8_t band_index, bool external);
void subghz_radio_set_band(SubghzRadio* radio, uint8_t band_index);

void subghz_radio_start(SubghzRadio* radio);
void subghz_radio_stop(SubghzRadio* radio);
bool subghz_radio_is_running(SubghzRadio* radio);

// Clear the max-hold / peak. Safe to call while running.
void subghz_radio_reset(SubghzRadio* radio);

void subghz_radio_get_snapshot(SubghzRadio* radio, SpectrumSnapshot* out);
