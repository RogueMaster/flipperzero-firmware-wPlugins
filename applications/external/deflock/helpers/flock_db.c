// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_db.h"
#include <string.h>

/**
 * 32 OUI prefixes observed in fielded Flock Safety deployments.
 * First 30 from @NitekryDPaul research; 82:6b:f2 from DeFlockJoplin field
 * testing; b4:1e:52 is Flock Safety's own IEEE-registered OUI (GainSec). These
 * are generic vendor prefixes (Liteon, Espressif, etc.), hence OUI-only matches
 * are scored "possible", never "confirmed".
 */
static const uint8_t flock_ouis[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc},
    {0x80, 0x30, 0x49}, {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc},
    {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88}, {0x9c, 0x2f, 0x9d},
    {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0xf8, 0xa2, 0xd6}, {0x24, 0xb2, 0xb9},
    {0x00, 0xf4, 0x8d}, {0xd0, 0x39, 0x57}, {0xe8, 0xd0, 0xfc},
    {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4}, {0x70, 0x08, 0x94},
    {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x3c, 0x71, 0xbf},
    {0x58, 0x00, 0xe3}, {0x90, 0x35, 0xea}, {0x5c, 0x93, 0xa2},
    {0x64, 0x6e, 0x69}, {0x48, 0x27, 0xea}, {0xa4, 0xcf, 0x12},
    {0x82, 0x6b, 0xf2}, {0xb4, 0x1e, 0x52}, // Flock Safety's own registered OUI (GainSec)
};

#define FLOCK_OUI_COUNT (sizeof(flock_ouis) / sizeof(flock_ouis[0]))

/**
 * OPTIONAL user-supplied extras, registered at runtime from the SD card by
 * sig_db.c and merged OVER the built-ins (extras can only ADD matches). These
 * default NULL/0 -- the fail-safe state in which only the built-ins above are
 * consulted -- and are CALLER-OWNED (flock_db.c just holds the pointers, so it
 * stays firmware-free / host-testable). User signatures are LOAD-ONLY and
 * UNVERIFIED; per precision-over-recall they never upgrade an OUI hit past
 * "possible".
 */
static const uint8_t (*extra_ouis)[3] = NULL;
static size_t extra_oui_count = 0;
static const char* const* extra_ssid_confirmed = NULL;
static size_t extra_ssid_confirmed_count = 0;
static const char* const* extra_ssid_likely = NULL;
static size_t extra_ssid_likely_count = 0;
static const uint32_t* extra_ie_fps = NULL;
static size_t extra_ie_fp_count = 0;

void flock_db_set_extra_ouis(const uint8_t (*ouis)[3], size_t count) {
    // NULL/0 clears the registration; otherwise just hold the caller's pointer.
    extra_ouis = (ouis && count) ? ouis : NULL;
    extra_oui_count = (ouis && count) ? count : 0;
}

void flock_db_set_extra_ssid_patterns(
    const char* const* confirmed,
    size_t confirmed_count,
    const char* const* likely,
    size_t likely_count) {
    extra_ssid_confirmed = (confirmed && confirmed_count) ? confirmed : NULL;
    extra_ssid_confirmed_count = (confirmed && confirmed_count) ? confirmed_count : 0;
    extra_ssid_likely = (likely && likely_count) ? likely : NULL;
    extra_ssid_likely_count = (likely && likely_count) ? likely_count : 0;
}

void flock_db_set_extra_ie_fps(const uint32_t* fps, size_t count) {
    // NULL/0 clears the registration; otherwise just hold the caller's pointer.
    extra_ie_fps = (fps && count) ? fps : NULL;
    extra_ie_fp_count = (fps && count) ? count : 0;
}

size_t flock_oui_count(void) {
    return FLOCK_OUI_COUNT;
}

const uint8_t* flock_oui_get(size_t index) {
    if(index >= FLOCK_OUI_COUNT) return NULL;
    return flock_ouis[index];
}

bool flock_oui_match(const uint8_t* mac) {
    if(!mac) return false;
    for(size_t i = 0; i < FLOCK_OUI_COUNT; i++) {
        if(mac[0] == flock_ouis[i][0] && mac[1] == flock_ouis[i][1] &&
           mac[2] == flock_ouis[i][2]) {
            return true;
        }
    }
    // Also scan the optional user-supplied extras (merged over the built-ins).
    for(size_t i = 0; i < extra_oui_count; i++) {
        if(mac[0] == extra_ouis[i][0] && mac[1] == extra_ouis[i][1] &&
           mac[2] == extra_ouis[i][2]) {
            return true;
        }
    }
    return false;
}

/** ASCII lower-case (no locale, safe for embedded). */
static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/** Case-insensitive substring search (needle assumed already lower-case). */
static bool ci_contains(const char* haystack, const char* needle_lower) {
    if(!haystack || !needle_lower) return false;
    size_t nlen = strlen(needle_lower);
    if(nlen == 0) return false;
    for(const char* h = haystack; *h; h++) {
        size_t k = 0;
        while(needle_lower[k] && ascii_lower(h[k]) == needle_lower[k]) {
            k++;
        }
        if(k == nlen) return true;
    }
    return false;
}

FlockConfidence flock_ssid_confidence(const char* ssid) {
    if(!ssid || ssid[0] == '\0') return FlockConfidenceNone;

    // Strong, near-unique naming patterns -> confirmed.
    // "Flock-XXXXXX" provisioning APs and the "test_flck" service SSID.
    if(ci_contains(ssid, "flock-") || ci_contains(ssid, "test_flck")) {
        return FlockConfidenceConfirmed;
    }

    // Optional user-supplied confirmed needles (already lower-case). Merged
    // over the built-ins: they can only ADD a confirmed match.
    for(size_t i = 0; i < extra_ssid_confirmed_count; i++) {
        if(ci_contains(ssid, extra_ssid_confirmed[i])) return FlockConfidenceConfirmed;
    }

    // Weaker substrings -> likely (could be a coincidental network name).
    if(ci_contains(ssid, "flock") || ci_contains(ssid, "flck")) {
        return FlockConfidenceLikely;
    }

    // Optional user-supplied likely needles (already lower-case).
    for(size_t i = 0; i < extra_ssid_likely_count; i++) {
        if(ci_contains(ssid, extra_ssid_likely[i])) return FlockConfidenceLikely;
    }

    return FlockConfidenceNone;
}

/**
 * B1: curated table of known-Flock probe IE-skeleton fingerprints (FNV-1a
 * uint32 of the tagged-IE skeleton, computed on the ESP companion).
 *
 * SHIPS EMPTY / INERT. We do NOT yet have confirmed-Flock IE-fp captures, so
 * this table is intentionally empty: nothing matches -> zero behaviour change ->
 * zero false positives, which is exactly right per precision-over-recall. The
 * full pipeline (hash on ESP -> transmit -> parse -> compare) ships and works;
 * it simply has no seeds to match until real captures are validated.
 *
 * TO SEED: add the FNV-1a hash(es) emitted in the companion's `,fp=` field for
 * a probe request from a *corroborated* Flock unit. Each entry is a
 * device-CLASS / firmware-stack signature, NOT a unique device ID -- only add a
 * hash once it is confirmed against a known deployment.
 *
 * Placeholder (compiled out -- NEEDS VALIDATION, do not enable):
 *   // 0x00000000u,  // <model> probe template -- NEEDS VALIDATION, unverified
 */
static const uint32_t flock_ie_fps[] = {
    0, // sentinel so the array is never zero-length; ignored by the matcher.
};

#define FLOCK_IE_FP_COUNT (sizeof(flock_ie_fps) / sizeof(flock_ie_fps[0]))

FlockIeFp flock_ie_fp_match(uint32_t fp) {
    if(fp == 0) return FlockIeFpNone; // 0 = "no fingerprint", never a match
    // Built-ins first: a compiled-in (maintainer-verified) hit is the strongest.
    for(size_t i = 0; i < FLOCK_IE_FP_COUNT; i++) {
        if(flock_ie_fps[i] == 0) continue; // skip the sentinel / unseeded slots
        if(flock_ie_fps[i] == fp) return FlockIeFpBuiltin;
    }
    // Then the optional user-supplied extras (UNVERIFIED -> the caller caps these
    // at FlockConfidenceProbeFp; they can only ADD a candidate-class match).
    for(size_t i = 0; i < extra_ie_fp_count; i++) {
        if(extra_ie_fps[i] == fp) return FlockIeFpUser;
    }
    return FlockIeFpNone;
}

FlockConfidence flock_score(const uint8_t* mac, const char* ssid, bool is_probe_req) {
    FlockConfidence by_ssid = flock_ssid_confidence(ssid);
    if(by_ssid == FlockConfidenceConfirmed) return FlockConfidenceConfirmed;

    bool oui = flock_oui_match(mac);

    // OUI + the camera's phone-home probe behaviour is a strong combination.
    if(oui && is_probe_req) {
        return FlockConfidenceLikely > by_ssid ? FlockConfidenceLikely : by_ssid;
    }
    if(oui) {
        return FlockConfidencePossible > by_ssid ? FlockConfidencePossible : by_ssid;
    }

    return by_ssid;
}

const char* flock_confidence_str(FlockConfidence confidence) {
    switch(confidence) {
    case FlockConfidenceConfirmed:
        return "CONFIRMED";
    case FlockConfidenceProbeFp:
        return "Class?"; // candidate device-CLASS match, not a unique device
    case FlockConfidenceLikely:
        return "Likely";
    case FlockConfidencePossible:
        return "Possible";
    case FlockConfidenceNone:
    default:
        return "-";
    }
}
