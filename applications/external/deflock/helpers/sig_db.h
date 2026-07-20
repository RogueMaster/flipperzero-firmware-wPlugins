// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file sig_db.h
 * Optional, updatable Flock/ALPR signature database loaded from the SD card.
 *
 * The compiled-in detection data (helpers/flock_db.c) is the trusted baseline.
 * This loader lets a user ADD extra OUI prefixes and SSID substrings WITHOUT a
 * rebuild, by dropping a JSON file at
 *
 *     RECON_APP_FOLDER "/signatures.json"   (apps_data/flipdeflock/signatures.json)
 *
 * The extras are merged OVER the built-ins via the flock_db_set_extra_*
 * registrars: they can only ADD matches, never remove or weaken a built-in.
 *
 * Posture (every line of this file honours these):
 *   - LOAD-ONLY: never writes the file, never touches the network. Parsing is
 *     done entirely on-device from a local file at app start.
 *   - FAIL-SAFE: if the file is absent, empty, malformed, or oversized,
 *     sig_db_load returns NULL and registers NOTHING -- the built-ins stay
 *     fully intact and detection keeps working. A bad user file can never
 *     corrupt detection.
 *   - UNVERIFIED: user signatures are not vetted. Because a false positive is
 *     worse than a missed detection, an OUI-only hit (built-in OR user) is
 *     still only scored "possible"; SSID matches follow the documented ladder.
 *   - BOUNDED RAM: counts are capped (<=64 OUIs, <=32 patterns per list, <=32
 *     IE-fingerprints) and all transient parse buffers are freed before return;
 *     the only lasting allocation is the small owned arrays held by the handle.
 *
 * JSON schema (all keys optional; extra keys ignored):
 *   { "ouis": ["aa:bb:cc"], "ssid_confirmed": ["flock-"], "ssid_likely": ["flock"],
 *     "ie_fps": ["deadbeef"] }
 *
 * "ie_fps" are 8-hex-char probe IE-skeleton fingerprints (the value shown as
 * "IE-fp:" on a Flock detection's detail screen). They survive MAC randomization
 * but, being UNVERIFIED user data, only ever score a candidate "Class?" -- never
 * "Confirmed".
 *
 * See docs/signatures.example.json for a documented sample.
 */
#pragma once

#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque owner handle for the loaded extra signatures. It owns the malloc'd
 * arrays that flock_db.c holds pointers into, so it must live for the whole app
 * session and be released with sig_db_free.
 */
typedef struct SigDb SigDb;

/**
 * Load apps_data/flipdeflock/signatures.json, register its contents as the
 * flock_db extras, and return the owner handle.
 *
 * @param storage  open Storage record.
 * @return  a SigDb* on success, or NULL if the file is absent / empty /
 *          malformed / oversized (the FAIL-SAFE path -- nothing is registered
 *          and the built-ins stay intact). NULL is a normal, expected result,
 *          NOT an error the caller must surface.
 */
SigDb* sig_db_load(Storage* storage);

/**
 * Clear the flock_db extra registrations (so the matchers fall back to the
 * built-ins) and free the handle and everything it owns. NULL-safe.
 */
void sig_db_free(SigDb* db);

#ifdef __cplusplus
}
#endif
