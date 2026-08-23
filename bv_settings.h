/*
 * BioVault settings — persisted app preferences.
 * Stored at /ext/apps_data/biovault/settings.bin:
 *   [magic 'BVS2':4][send_newline:1][protect_reads:1][authlim:1][tag_protected:1]
 * Missing/unreadable file loads defaults (never fatal). Older 'BVS1'
 * (send_newline only) files still load with new fields defaulted.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool send_newline; // append Enter after typing over HID

    // Provisioning parameters (opt-in; applied only by explicit Settings action).
    bool protect_reads; // true = read+write protected, false = write-only
    uint8_t authlim; // 0 = disabled; 1..7 = max 2^authlim failed PWD_AUTHs (irreversible!)
    bool tag_protected; // true once the implant is provisioned by this app
} BvSettings;

// Populate `out` from the settings file, or with defaults if none exists.
void bv_settings_load(BvSettings* out);

// Persist `s`. Returns false on storage failure (non-fatal to the caller).
bool bv_settings_save(const BvSettings* s);
