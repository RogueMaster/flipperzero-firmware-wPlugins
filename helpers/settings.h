#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * User settings, their labels, and their persistence.
 *
 * Before v2.0 these lived in RAM only, so the app forgot everything the moment
 * it closed. That is not just an annoyance: somebody who turns Sound OFF
 * because they do not want a stalker to hear the alert would have it switch
 * itself back ON the next time they opened the app.
 */

#define GHOSTTAG_SENS_COUNT   3
#define GHOSTTAG_FOLLOW_COUNT 4

typedef struct {
    uint8_t sensitivity_index; /* 0 Near .. 2 Far (RSSI cutoff) */
    uint8_t follow_index; /* dwell time before the "following" flag */
    bool sound;
    bool vibro;
    bool led;
    bool keep_lit; /* hold the backlight on for the whole hunt */
    bool log_session; /* append findings to the SD card log */
} GhostTagSettings;

/** The values a fresh install starts from. */
void ghosttag_settings_defaults(GhostTagSettings* s);

/**
 * Clamp every index into range and normalise every bool to 0/1.
 *
 * saved_struct validates the magic, version and size of a file - never its
 * contents - and the file is reachable by anything that can write the SD card.
 * An out-of-range index would walk off a label table, and a _Bool holding
 * anything other than 0 or 1 is undefined behaviour the moment it is read.
 */
void ghosttag_settings_sanitise(GhostTagSettings* s);

/** Load from internal storage; leaves @p out at whatever it held on failure. */
bool ghosttag_settings_load(GhostTagSettings* out);

/** Persist to internal storage. Called when the settings screen is left. */
bool ghosttag_settings_save(const GhostTagSettings* in);

/* ---- derived values ---- */
int8_t ghosttag_settings_rssi_cutoff(const GhostTagSettings* s);
uint32_t ghosttag_settings_follow_ms(const GhostTagSettings* s);
const char* ghosttag_settings_sensitivity_label(uint8_t index);
const char* ghosttag_settings_follow_label(uint8_t index);

/** Metres-ish hint shown beside the range setting, e.g. "~5 m". */
const char* ghosttag_settings_range_hint(uint8_t index);
