#pragma once

#include <furi.h>
#include <stdbool.h>
#include "../views/spectrum_view.h"
#include "../views/meter_view.h"

/*
 * CC1101 Sub-GHz analyzer + finder.
 *
 * A background worker drives the CC1101 (internal or the board's external
 * cc1101_ext device, with fallback) in one of two modes:
 *   - Sweep : step across a band in fixed bins, read RSSI, publish a
 *             SpectrumSnapshot (the band spectrum analyzer).
 *   - Camp  : hold one frequency, read RSSI continuously, publish a
 *             MeterSnapshot (the frequency finder).
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

// Handy presets for the finder.
typedef struct {
    uint32_t hz;
    const char* label;
} SubghzPreset;
#define TRIDENT_SUBGHZ_PRESET_COUNT 6
extern const SubghzPreset trident_subghz_presets[TRIDENT_SUBGHZ_PRESET_COUNT];

typedef enum {
    SubghzModeSweep = 0,
    SubghzModeCamp = 1,
} SubghzMode;

typedef struct SubghzRadio SubghzRadio;

SubghzRadio* subghz_radio_alloc(void);
void subghz_radio_free(SubghzRadio* radio);

// Configure before starting. `external` selects the board's CC1101 when present.
void subghz_radio_configure(SubghzRadio* radio, uint8_t band_index, bool external);
void subghz_radio_set_mode(SubghzRadio* radio, SubghzMode mode); // latched at start
void subghz_radio_set_band(SubghzRadio* radio, uint8_t band_index); // sweep, live
void subghz_radio_set_camp_freq(SubghzRadio* radio, uint32_t hz); // camp, live
uint32_t subghz_radio_get_camp_freq(SubghzRadio* radio);

void subghz_radio_start(SubghzRadio* radio);
void subghz_radio_stop(SubghzRadio* radio);
bool subghz_radio_is_running(SubghzRadio* radio);

void subghz_radio_reset(SubghzRadio* radio); // clear hold / peak

void subghz_radio_get_snapshot(SubghzRadio* radio, SpectrumSnapshot* out); // sweep
void subghz_radio_get_meter(SubghzRadio* radio, MeterSnapshot* out); // camp
