// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file flock_db.h
 * Flock Safety / ALPR surveillance-device detection database and scoring.
 *
 * Pure logic, no firmware dependencies, so it can be unit-tested on a host.
 *
 * Detection data is sourced from the open-source counter-surveillance research
 * projects (colonelpanichacks/flock-you, 0xXyc/flock-you-wifi-recon) and the
 * DeFlock community (deflock.org). The OUI prefixes are generic vendor prefixes
 * observed in fielded Flock deployments, so an OUI match alone is "possible",
 * not "confirmed" -- behaviour (probe requests) and SSID naming raise the
 * confidence. We never present an OUI-only hit as certain.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How sure we are that a wireless device is Flock/ALPR surveillance gear. */
typedef enum {
    FlockConfidenceNone = 0, /**< No indicators matched. */
    FlockConfidencePossible, /**< OUI prefix match only (generic vendor prefix). */
    FlockConfidenceLikely, /**< OUI + phone-home probe behaviour, or "flock/flck" substring. */
    FlockConfidenceProbeFp, /**< B1: probe IE-fingerprint matched a curated Flock
                              *   device-CLASS hash (survives MAC randomization).
                              *   A candidate class match, NOT a unique device --
                              *   sits above a loose OUI+probe "Likely" but below a
                              *   near-unique SSID "Confirmed". */
    FlockConfidenceConfirmed, /**< SSID matches a known Flock naming pattern. */
} FlockConfidence;

/** Source of an IE-fingerprint match, so a caller can gate confidence by trust. */
typedef enum {
    FlockIeFpNone = 0, /**< no match (or fp==0 "no fingerprint"). */
    FlockIeFpBuiltin, /**< matched a compiled-in, maintainer-verified fingerprint. */
    FlockIeFpUser, /**< matched a user-supplied (signatures.json) fingerprint -- UNVERIFIED. */
} FlockIeFp;

/**
 * B1: match a probe-request IE-skeleton FNV-1a hash (from the companion's
 * `,fp=<hex32>` field) against the curated table of confirmed-Flock fingerprints
 * PLUS any user-supplied ones registered from signatures.json.
 *
 * PRECISION GUARD: the compiled-in table ships EMPTY/inert -- we do not yet have
 * validated captures, so a built-in match is currently impossible (zero false
 * positives). User fingerprints are UNVERIFIED: a FlockIeFpUser match MUST be
 * capped at the candidate-class level (FlockConfidenceProbeFp) by the caller and
 * can never reach Confirmed. The match is a device-CLASS / firmware-stack
 * signature, never a unique device ID.
 *
 * @param fp  IE-skeleton hash; 0 means "no fingerprint" and never matches.
 * @return    FlockIeFpBuiltin / FlockIeFpUser / FlockIeFpNone.
 */
FlockIeFp flock_ie_fp_match(uint32_t fp);

/** Number of known Flock-associated OUI prefixes. */
size_t flock_oui_count(void);

/** Get the i-th OUI prefix (3 bytes) for display. Returns NULL if out of range. */
const uint8_t* flock_oui_get(size_t index);

/** True if the first 3 bytes of `mac` match a known Flock-associated OUI. */
bool flock_oui_match(const uint8_t* mac);

/**
 * OPTIONAL, user-supplied extra signatures the matchers consult IN ADDITION to
 * the compiled-in tables (merged OVER them -- extras can only ADD matches, never
 * remove a built-in). Loaded at runtime from the SD card by sig_db.c.
 *
 * All arrays (and the strings they point at) are CALLER-OWNED and must outlive
 * the registration. SSID needles MUST already be lower-case (the matcher
 * lowercases only the haystack; sig_db.c lowercases before registering).
 *
 * PRECISION: user signatures are LOAD-ONLY and UNVERIFIED. A false positive is
 * worse than a missed detection, so an OUI-only hit (built-in OR extra) stays
 * "possible" and a user IE-fp is capped at "Class?" (FlockConfidenceProbeFp) --
 * never Confirmed.
 */
typedef struct {
    const uint8_t (*ouis)[3]; /**< extra OUI prefixes (3 bytes each) -> flock_oui_match */
    size_t oui_count;
    const char* const* ssid_confirmed; /**< lower-case substrings -> Confirmed */
    size_t ssid_confirmed_count;
    const char* const* ssid_likely; /**< lower-case substrings -> Likely */
    size_t ssid_likely_count;
    const uint32_t* ie_fps; /**< extra IE-fingerprint hashes -> FlockIeFpUser */
    size_t ie_fp_count;
} FlockDbExtras;

/**
 * Register (or clear, with `extras == NULL`) the caller-owned extra signatures.
 * A SINGLE atomic pointer swap: there is no partial-registration window, and
 * clearing before the caller frees the backing store is one call -- so there is
 * no deregister-order footgun. The struct and its arrays must outlive use.
 */
void flock_db_set_extras(const FlockDbExtras* extras);

/**
 * Confidence contributed by an SSID string alone (may be NULL/empty for hidden
 * networks or probe requests with no SSID).
 */
FlockConfidence flock_ssid_confidence(const char* ssid);

/**
 * Combined confidence for an observed device.
 *
 * @param mac          6-byte MAC of the transmitter (must not be NULL).
 * @param ssid         Advertised/probed SSID, or NULL/"" if unknown.
 * @param is_probe_req True if this frame is a station-mode probe request
 *                     (the "phoning home" behaviour Flock cameras exhibit).
 */
FlockConfidence flock_score(const uint8_t* mac, const char* ssid, bool is_probe_req);

/** Human-readable label for a confidence level. */
const char* flock_confidence_str(FlockConfidence confidence);

#ifdef __cplusplus
}
#endif
