/*
 * BioVault settings — small, persisted app preferences.
 *
 * Stored Flipper-local at /ext/apps_data/biovault/settings.bin:
 *   [magic 'BVS2':4][send_newline:1][protect_reads:1][authlim:1][tag_protected:1]
 * Loads defaults if the file is missing or unreadable; never fatal. Older 'BVS1'
 * (send_newline only) files still load, with the new fields defaulted. Note: the
 * implant is NOT identified here by UID — reveal/auth is gated cryptographically
 * (PWD_AUTH), since a UID is public and spoofable, so nothing tag-identifying is
 * persisted.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool send_newline; // append Enter after typing a username/password over HID

    // M4 provisioning parameters (opt-in; only applied by an explicit Settings
    // action, never automatically).
    bool protect_reads; // NFC_PROT: true = read+write protected, false = write-only
    uint8_t authlim; // 0 = disabled; 1..7 = max 2^authlim failed PWD_AUTHs (irreversible!)
    bool tag_protected; // true once the implant has been provisioned by this app
} BvSettings;

// Populate `out` from the settings file, or with defaults if none exists.
void bv_settings_load(BvSettings* out);

// Persist `s`. Returns false on storage failure (non-fatal to the caller).
bool bv_settings_save(const BvSettings* s);
