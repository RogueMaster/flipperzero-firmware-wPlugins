/**
 * Faraday - persistence.
 *
 * Two jobs, both on the SD card under the app's own data directory:
 *   - Settings survive a reboot (saved_struct gives us magic + version +
 *     checksum, so a stale or corrupt file falls back to defaults instead of
 *     loading garbage).
 *   - Finished tests are appended to a CSV log, so you can measure three
 *     pouches in a shop and compare them later instead of trusting memory.
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/** User settings. Lives here (not in faraday_i.h) so the store owns its shape. */
typedef struct {
    uint8_t band_index; // index into fdy_bands
    bool sound;
    bool led;
} FaradaySettings;

/** One finished test, as written to the log. */
typedef struct {
    bool is_nfc;
    uint32_t frequency; // Hz; 0 for the NFC test
    int16_t base_value; // dBm, or field %
    int16_t shield_value;
    int16_t atten; // dB, or % blocked
    bool floored; // attenuation is a ">=" lower bound
    uint8_t rating; // FdyRating
} FdyResult;

/** Settings. load() leaves *s untouched if there is nothing valid to read. */
void fdy_store_settings_save(const FaradaySettings* s);
void fdy_store_settings_load(FaradaySettings* s);

/** Append a finished test to the log. Returns false if the write failed. */
bool fdy_store_result_append(const FdyResult* r);

/**
 * Render the most recent results, newest first, into `out` as widget markup.
 * Returns how many entries were rendered (0 = nothing logged yet).
 */
uint8_t fdy_store_results_render(FuriString* out, uint8_t max);

/** Where the log lives, for showing the user. */
const char* fdy_store_results_path(void);

#ifdef __cplusplus
}
#endif
