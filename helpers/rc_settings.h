/**
 * RollCall - persistent settings.
 *
 * Band, modulation, press target and feedback survive between runs, so you set
 * up your fob's frequency once instead of re-picking it every launch. Stored as
 * a versioned struct in the app's own data directory; a missing, corrupt or
 * older file falls back to defaults rather than failing to start.
 */
#pragma once

#include <furi.h>
#include "rc_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** How long a quiet gap must be before the next frame counts as a new press. */
typedef struct {
    const char* label; /* "0.5s"  */
    uint32_t ms;
} RcGap;

#define RC_GAP_COUNT 5
extern const RcGap rc_gaps[RC_GAP_COUNT];

#define RC_TARGET_MIN 2
#define RC_TARGET_MAX 8
#define RC_TARGET_COUNT (RC_TARGET_MAX - RC_TARGET_MIN + 1)

typedef struct {
    uint8_t band_idx; /* index into rc_bands */
    uint8_t mod_idx; /* index into rc_mods  */
    uint8_t target; /* presses to auto-finish (RC_TARGET_MIN..MAX) */
    uint8_t gap_idx; /* index into rc_gaps  */
    bool sound;
    bool vibro;
    bool led;
} RcSettings;

/** Reset to shipped defaults: 433.92 MHz, AM650, 3 presses, all feedback on. */
void rc_settings_default(RcSettings* s);

/** Load from disk, falling back to defaults. Always leaves `s` usable. */
void rc_settings_load(RcSettings* s);

/** Persist to disk. Best effort - a failed write is not worth an error dialog. */
void rc_settings_save(const RcSettings* s);

#ifdef __cplusplus
}
#endif
