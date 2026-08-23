/*
 * BioVault settings — small, persisted app preferences.
 *
 * Stored Flipper-local at /ext/apps_data/biovault/settings.bin:
 *   [magic 'BVS1':4][send_newline:1]
 * Loads defaults if the file is missing or unreadable; never fatal.
 */
#pragma once

#include <stdbool.h>

typedef struct {
    bool send_newline; // append Enter after typing a username/password over HID
} BvSettings;

// Populate `out` from the settings file, or with defaults if none exists.
void bv_settings_load(BvSettings* out);

// Persist `s`. Returns false on storage failure (non-fatal to the caller).
bool bv_settings_save(const BvSettings* s);
