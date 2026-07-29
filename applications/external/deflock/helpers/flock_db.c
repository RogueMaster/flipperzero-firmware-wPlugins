// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_db.h"
#include <string.h>

/**
 * 31 OUI prefixes observed in fielded Flock Safety deployments.
 * Mostly @NitekryDPaul research; 82:6b:f2 from DeFlockJoplin field testing;
 * the last entry b4:1e:52 is Flock Safety's own IEEE-registered OUI (GainSec).
 * These are generic vendor prefixes (Liteon, Espressif, etc.), hence OUI-only
 * matches are scored "possible", never "confirmed".
 *
 * SOURCE OF TRUTH is now nitekry/nite-oui-collection ->
 * groups/flockers/my_tested_flock.md, a per-prefix table with Confidence and
 * Status columns. It SUPERSEDES the flat, statusless
 * colonelpanichacks/flock-you -> datasets/NitekryDPaul_wifi_ouis.md list this
 * table was originally imported from -- the flat list cannot record that a
 * prefix was later doubted, so re-importing from it silently undoes retractions.
 *
 * f8:a2:d6 was DROPPED 2026-07-27: upstream marks it Removed ("low confidence;
 * hit on a Sony Media Player"). Do NOT re-add it from the flat list. Also
 * retracted upstream and deliberately absent here: 6c:cd:d6 (Netgear),
 * 94:2a:6f + f4:e2:c6 (Ubiquiti), cc:cc:cc (no hits), 00:0c:e7 (possible FP).
 *
 * This table is a claim of FIELD CORROBORATION. Uncorroborated candidates
 * belong in the user signature file, not here -- see docs/signatures.md.
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino, which
 * scores ESP-side. No shared header is possible (that side is an Arduino
 * sketch), so change BOTH and keep the row layout identical so they can be
 * diffed by eye. tools/check_oui_parity.py enforces this as a required CI gate.
 */
static const uint8_t flock_ouis[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
    {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88},
    {0x9c, 0x2f, 0x9d}, {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0x24, 0xb2, 0xb9}, {0x00, 0xf4, 0x8d}, {0xd0, 0x39, 0x57},
    {0xe8, 0xd0, 0xfc}, {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4}, {0x70, 0x08, 0x94},
    {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x3c, 0x71, 0xbf}, {0x58, 0x00, 0xe3},
    {0x90, 0x35, 0xea}, {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69}, {0x48, 0x27, 0xea},
    {0xa4, 0xcf, 0x12}, {0x82, 0x6b, 0xf2}, {0xb4, 0x1e, 0x52},
};

#define FLOCK_OUI_COUNT (sizeof(flock_ouis) / sizeof(flock_ouis[0]))

/**
 * SoundThinking (formerly ShotSpotter) acoustic gunshot sensors.
 *
 * A DIFFERENT DEVICE CLASS, not an ALPR: these listen, they do not read plates.
 * Kept in its own table so a hit can be reported as what it is. Folding it into
 * flock_ouis[] would have the app announce a camera it never saw, which is the
 * over-claiming the project rules forbid.
 *
 * d4:11:d6 via JakeSwiz/WatchFlock (esp32_marauder/WiFiScan.cpp,
 * fy_soundthinking_mac_prefixes[]). Like every OUI here it is a vendor prefix,
 * not proof: an OUI-only hit scores "possible", and there is no known SSID tell
 * for this hardware, so an acoustic detection can never reach "confirmed".
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino -- same
 * hand-sync rule as flock_ouis[] above, and covered by the same CI parity gate.
 */
static const uint8_t soundthinking_ouis[][3] = {
    {0xd4, 0x11, 0xd6},
};

#define SOUNDTHINKING_OUI_COUNT (sizeof(soundthinking_ouis) / sizeof(soundthinking_ouis[0]))

bool soundthinking_oui_match(const uint8_t* mac) {
    if(!mac) return false;
    for(size_t i = 0; i < SOUNDTHINKING_OUI_COUNT; i++) {
        if(mac[0] == soundthinking_ouis[i][0] && mac[1] == soundthinking_ouis[i][1] &&
           mac[2] == soundthinking_ouis[i][2]) {
            return true;
        }
    }
    // Deliberately NOT extended by signatures.json: the user schema has no class
    // field, so a user OUI is always read as ALPR. Adding acoustic prefixes needs
    // a schema change, not a silent reinterpretation of existing user files.
    return false;
}

FlockDevClass flock_class_from_mac(const uint8_t* mac) {
    return soundthinking_oui_match(mac) ? FlockClassAcoustic : FlockClassAlpr;
}

const char* flock_class_str(FlockDevClass cls) {
    return (cls == FlockClassAcoustic) ? "Acoustic" : "ALPR";
}

const char* flock_class_long_str(FlockDevClass cls) {
    // "SoundThinking (acoustic sensor)" was 31 characters and overran the detail
    // screen's 128 px row. Shortened rather than truncated at draw time, so the
    // device class -- the thing that stops a gunshot sensor being read as a
    // camera -- is never the field that gets cut off.
    return (cls == FlockClassAcoustic) ? "SoundThinking sensor" : "Flock / ALPR camera";
}

/**
 * OPTIONAL user-supplied extras, registered at runtime from the SD card by
 * sig_db.c and merged OVER the built-ins (extras can only ADD matches). These
 * default NULL/0 -- the fail-safe state in which only the built-ins above are
 * consulted -- and are CALLER-OWNED (flock_db.c just holds the pointers, so it
 * stays firmware-free / host-testable). User signatures are LOAD-ONLY and
 * UNVERIFIED; per precision-over-recall they never upgrade an OUI hit past
 * "possible".
 */
// Single caller-owned extras context (NULL = only the built-ins are consulted).
static const FlockDbExtras* g_extras = NULL;

void flock_db_set_extras(const FlockDbExtras* extras) {
    g_extras = extras; // atomic single-pointer swap; caller owns the struct + arrays
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
    if(g_extras) {
        for(size_t i = 0; i < g_extras->oui_count; i++) {
            if(mac[0] == g_extras->ouis[i][0] && mac[1] == g_extras->ouis[i][1] &&
               mac[2] == g_extras->ouis[i][2]) {
                return true;
            }
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

/** True if `ssid` is exactly "Flock-" + 6 hex digits (the provisioning-AP name). */
static bool is_flock_provisioning_ssid(const char* ssid) {
    const char* pfx = "flock-"; // case-insensitive prefix
    for(int i = 0; i < 6; i++) {
        if(ssid[i] == '\0' || ascii_lower(ssid[i]) != pfx[i]) return false;
    }
    for(int i = 6; i < 12; i++) {
        char c = ssid[i]; // '\0' (short SSID) is not hex -> correctly rejected
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if(!hex) return false;
    }
    return ssid[12] == '\0';
}

FlockConfidence flock_ssid_confidence(const char* ssid) {
    if(!ssid || ssid[0] == '\0') return FlockConfidenceNone;

    // Strong, near-unique naming -> confirmed. Anchor the provisioning-AP name
    // exactly ("Flock-" + 6 hex): an unanchored "flock-" substring wrongly
    // confirmed benign names like "Flock-Guest" or the Flock Freight / chat SSIDs.
    // Those still fall through to the "likely" contains-check below.
    //
    // "test_flck" is the hard-coded development SSID disclosed as CVE-2025-59409;
    // on the air it is close to self-identifying, hence Confirmed on substring.
    if(is_flock_provisioning_ssid(ssid) || ci_contains(ssid, "test_flck")) {
        return FlockConfidenceConfirmed;
    }

    // Optional user-supplied confirmed needles (already lower-case). Merged
    // over the built-ins: they can only ADD a confirmed match.
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ssid_confirmed_count; i++) {
            if(ci_contains(ssid, g_extras->ssid_confirmed[i])) return FlockConfidenceConfirmed;
        }
    }

    // Weaker substrings -> likely (could be a coincidental network name).
    if(ci_contains(ssid, "flock") || ci_contains(ssid, "flck")) {
        return FlockConfidenceLikely;
    }

    // Optional user-supplied likely needles (already lower-case).
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ssid_likely_count; i++) {
            if(ci_contains(ssid, g_extras->ssid_likely[i])) return FlockConfidenceLikely;
        }
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
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ie_fp_count; i++) {
            if(g_extras->ie_fps[i] == fp) return FlockIeFpUser;
        }
    }
    return FlockIeFpNone;
}

/*
 * flock_score() USED TO LIVE HERE and was deleted in v0.48.
 *
 * It read like the canonical scorer and had a full test suite, but it had ZERO
 * production callers -- nothing on the device ever ran it. The shipped
 * combination logic is parse_flock() in helpers/esp_parser.c (companion backend)
 * and the inline block in esp_parse_generic() (Marauder backend), and those are
 * different code. So the tests that "covered scoring" were guarding a function
 * the product did not use, while the code that WAS used had no such guard.
 *
 * That is not hypothetical: it is exactly how v0.46 shipped "Flock-Guest" as
 * CONFIRMED with every test green. Rather than leave a second, diverging ladder
 * around to be maintained and mistaken for the real one, the assertions moved to
 * esp_parse_companion_line() in test/test_esp_parser.c, where they exercise the
 * boundary the product actually uses.
 *
 * Do not reintroduce a standalone scorer here. If you need combination logic,
 * put it where the caller is, and test it through the wire protocol.
 */

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

FlockMethod flock_method_of(const uint8_t* mac, const char* ssid, char ftype, uint32_t ie_fp) {
    // Strongest re-derivable indicator wins, mirroring the ladder's own ordering
    // so the label never claims more than the confidence rung does.
    if(flock_ssid_confidence(ssid) != FlockConfidenceNone) return FlockMethodSsid;
    if(flock_ie_fp_match(ie_fp) != FlockIeFpNone) return FlockMethodIeFp;
    // Either table: a SoundThinking prefix is an OUI match too, just for the other
    // device class. Reporting it as "unclassified" would hide the one indicator we
    // actually have for an acoustic sensor.
    if(flock_oui_match(mac) || soundthinking_oui_match(mac)) return FlockMethodOui;
    // BLE is classified on the companion (mfg id 0x09C8 / Raven GATT) from advert
    // bytes that never reach this side, so name the source rather than guess.
    if(ftype == 'L') return FlockMethodBle;
    return FlockMethodUnknown;
}

const char* flock_method_str(FlockMethod method) {
    // TERSE ON PURPOSE. These are composed into "Method: <this> + <frame>" on a
    // 128 px row that also carries a scrollbar, leaving ~26 characters. The
    // first draft ("OUI prefix", "IE fingerprint") pushed the longest
    // combination off the right edge on real hardware. "OUI" and "SSID" are also
    // the terms the reporter used, so nothing is lost by the shorter form.
    switch(method) {
    case FlockMethodSsid:
        return "SSID";
    case FlockMethodIeFp:
        return "IE fp";
    case FlockMethodOui:
        return "OUI";
    case FlockMethodBle:
        return "BLE mfg ID";
    case FlockMethodUnknown:
    default:
        // Not "none": the companion DID score it, on probe behaviour we cannot
        // re-derive here. Saying "no indicator" would be the wrong claim.
        return "ESP probe rule";
    }
}
