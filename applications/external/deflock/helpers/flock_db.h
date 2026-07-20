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
 * Register an OPTIONAL, user-supplied set of extra OUI prefixes that
 * `flock_oui_match` scans IN ADDITION to the compiled-in table. Loaded at
 * runtime from the SD card (see sig_db.h) and merged OVER the built-ins: the
 * extras can only ADD matches, never remove a built-in. Pass `ouis == NULL` or
 * `count == 0` to clear the registration (the fail-safe / default state, in
 * which only the built-ins are consulted).
 *
 * Ownership: the array is CALLER-OWNED and must outlive every call into the
 * matcher (i.e. for the whole app lifetime, until cleared). This keeps
 * flock_db.c firmware-free / host-testable -- it merely holds the pointer.
 *
 * PRECISION NOTE: user signatures are LOAD-ONLY and UNVERIFIED. A false
 * positive is worse than a missed detection, so OUI-only hits (built-in OR
 * extra) stay scored "possible", never "confirmed".
 */
void flock_db_set_extra_ouis(const uint8_t (*ouis)[3], size_t count);

/**
 * Register OPTIONAL, user-supplied SSID substrings that
 * `flock_ssid_confidence` tests IN ADDITION to the built-in patterns: a
 * `confirmed` hit yields FlockConfidenceConfirmed, a `likely` hit yields
 * FlockConfidenceLikely. Merged OVER the built-ins (extras can only ADD
 * matches). Pass NULL/0 lists to clear (the fail-safe default).
 *
 * CONTRACT: the needles MUST already be lower-case -- the matcher lowercases
 * only the haystack and assumes a lowercase needle (see ci_contains). The
 * caller (sig_db.c) lowercases before registering.
 *
 * Ownership: both arrays and the strings they point at are CALLER-OWNED and
 * must outlive use. LOAD-ONLY / UNVERIFIED, same precision posture as above.
 */
void flock_db_set_extra_ssid_patterns(
    const char* const* confirmed,
    size_t confirmed_count,
    const char* const* likely,
    size_t likely_count);

/**
 * Register OPTIONAL, user-supplied IE-fingerprint hashes (FNV-1a uint32 of the
 * probe IE skeleton, as emitted in the companion `,fp=` field) that
 * `flock_ie_fp_match` tests IN ADDITION to the compiled-in table, reporting them
 * as FlockIeFpUser. Loaded at runtime from signatures.json and merged OVER the
 * built-ins (can only ADD matches). Pass NULL/0 to clear (the fail-safe default).
 *
 * Ownership: the array is CALLER-OWNED and must outlive use. LOAD-ONLY /
 * UNVERIFIED: because a false positive is worse than a missed detection, a user
 * fingerprint match is capped at FlockConfidenceProbeFp ("Class?") by the caller
 * -- it can never, even with a Flock OUI, assert a Confirmed detection.
 */
void flock_db_set_extra_ie_fps(const uint32_t* fps, size_t count);

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
