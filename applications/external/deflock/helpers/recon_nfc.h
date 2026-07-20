// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <core/string.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

/**
 * NFC/RFID site-survey auditor.
 *
 * Uses the firmware NfcScanner to detect which protocol(s) a presented card
 * supports, then grades the credential's security posture (e.g. MIFARE Classic
 * = broken Crypto1, DESFire = strong if configured). The user logs findings to
 * an SD CSV with timestamp + GPS from the scene.
 *
 * For MIFARE Classic a passive "deep check" reads the UID and tries the on-SD
 * stock key dictionary (default keys only) against every sector to gauge how
 * trivially the card is cloneable. Active key recovery (mfkey32/nested) is a
 * planned next iteration.
 */

typedef struct ReconNfc ReconNfc;

/** Result of a MIFARE Classic default-key deep check. */
typedef struct {
    bool valid; /**< a deep check has completed with usable data */
    uint8_t uid[10];
    uint8_t uid_len;
    MfClassicType type;
    uint8_t total_sectors;
    uint8_t default_keyed; /**< sectors opened with at least one DEFAULT key */
    uint8_t recovered_keys; /**< distinct A/B keys found via the dictionary */
    bool aborted; /**< card removed mid-check; data is partial */
} ReconMfcResult;

/** @param app ReconApp*. */
ReconNfc* recon_nfc_alloc(void* app);
void recon_nfc_free(ReconNfc* nfc);

void recon_nfc_start(ReconNfc* nfc);
void recon_nfc_stop(ReconNfc* nfc);

/**
 * Get the current detection, if any.
 * @param[out] title   short protocol name (e.g. "MIFARE Classic")
 * @param[out] grade   one-word grade ("WEAK"/"MEDIUM"/"STRONG"/"INFO")
 * @param[out] detail  human-readable security notes (multiline)
 * @returns true if a card is currently detected.
 */
bool recon_nfc_get(ReconNfc* nfc, FuriString* title, FuriString* grade, FuriString* detail);

/**
 * Start a passive MIFARE Classic default-key deep check on a worker thread.
 * No-op if a check is already running. Pauses the scanner for the duration.
 */
void recon_nfc_deep_check_start(ReconNfc* nfc);

/** @returns true while a deep check worker is running. */
bool recon_nfc_deep_check_busy(ReconNfc* nfc);

/**
 * Snapshot the latest deep-check result.
 * @param[out] out  filled with the last completed result.
 * @returns true if a result is available (out->valid mirrors this).
 */
bool recon_nfc_deep_check_get(ReconNfc* nfc, ReconMfcResult* out);
