// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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

/**
 * What KIND of surveillance device a detection is, independent of how sure we are.
 *
 * Kept separate from FlockConfidence on purpose: they answer different questions
 * ("what is it" vs "how sure are we"), and folding an acoustic gunshot sensor into
 * the ALPR list would make the app claim something it has not detected. Reported
 * and persisted alongside the confidence rung.
 */
typedef enum {
    FlockClassAlpr = 0, /**< ALPR camera. The default, and NOT a claim about who
                          *  made it -- see FlockVendor. */
    FlockClassAcoustic, /**< SoundThinking (formerly ShotSpotter) acoustic sensor. */
    FlockClassBodycam, /**< Axon body-worn / in-car law-enforcement equipment. */
    FlockClassGear, /**< Surveillance-vendor equipment, KIND NOT DETERMINED.
                      *  The honest bucket for a vendor-exclusive OUI whose owner
                      *  ships several unrelated product lines: Motorola Solutions
                      *  sells both ALPR poles and hand-held radios on one OUI, so
                      *  calling either "ALPR" would invent a detection. The VENDOR
                      *  is what we actually know; the kind is not. */
} FlockDevClass;

/**
 * WHO made the device, kept strictly separate from FlockDevClass (what it is)
 * and FlockConfidence (how sure we are).
 *
 * WHY THIS EXISTS. Until v0.77 the app had one ALPR class whose label read
 * "Flock / ALPR camera", so every ALPR-class detection was announced as a FLOCK
 * camera -- including ones nothing had tied to Flock, and including the
 * competitor hardware now replacing Flock in cities that dropped it. That is the
 * same over-claim the class enum itself was added to prevent, one level up: it
 * is not enough to avoid calling a gunshot sensor a camera if we then call an
 * Axon camera a Flock camera.
 *
 * DERIVED, NEVER TRANSMITTED. The vendor is re-derived on this side from the MAC
 * and SSID we stored, exactly like flock_method_of(), and is deliberately NOT a
 * field the companion asserts. So it cannot inherit an over-claim from firmware
 * that lags the app, it costs nothing on the wire, and it needs no store-format
 * change -- a record loaded from a v1 CSV re-derives its vendor for free.
 *
 * FlockVendorUnknown is a REAL ANSWER, not a failure: the companion scores probe
 * behaviour on MACs outside every table, and "we saw surveillance-shaped
 * behaviour but cannot name the vendor" is the truth in that case.
 */
typedef enum {
    FlockVendorUnknown = 0, /**< no vendor table matched -- do NOT name a vendor. */
    FlockVendorFlock, /**< Flock Safety (Falcon / Sparrow / Condor / Raven). */
    FlockVendorSoundThinking, /**< SoundThinking, formerly ShotSpotter. */
    FlockVendorAxon, /**< Axon Enterprise (Body / Fleet / Outpost / Lightpost). */
    FlockVendorUbicquia, /**< Ubicquia (UbiHub / UbiCell -- Axon Lightpost's base). */
    FlockVendorMotorola, /**< Motorola Solutions (Vigilant / L6Q / radios). */
    FlockVendorVerkada, /**< Verkada. */
    FlockVendorGenetec, /**< Genetec (AutoVu). */
    FlockVendorAvigilon, /**< Avigilon Alta (Motorola-owned). */
} FlockVendor;

/**
 * Vendor implied by a MAC's OUI alone. FlockVendorUnknown when no built-in
 * vendor table matches.
 *
 * A user-supplied OUI from signatures.json NEVER names a vendor: that schema has
 * no vendor key, so attributing one would be inventing an attribution the file
 * never made. Those keep returning FlockVendorUnknown.
 */
FlockVendor flock_vendor_from_mac(const uint8_t* mac);

/**
 * Vendor from all the evidence we hold, strongest first: an SSID that matches a
 * Flock naming pattern names Flock even when the MAC is randomized or outside
 * every table; otherwise the MAC's OUI decides.
 *
 * @param mac   6-byte MAC (NULL-safe).
 * @param ssid  SSID as stored, may be NULL/empty.
 */
FlockVendor flock_vendor_of(const uint8_t* mac, const char* ssid);

/** Short vendor label for a list row or report column ("Flock", "Axon", "-"). */
const char* flock_vendor_str(FlockVendor vendor);

/**
 * Vendor-aware long label for the detail screen -- the one string an operator
 * reads to decide what they are looking at.
 *
 * PREFER THIS OVER flock_class_long_str(), which cannot see the vendor and so
 * still answers "Flock / ALPR camera" for the ALPR class. Budget is 20 chars:
 * the detail screen's 128 px row cuts anything longer, and the device identity
 * is the last field that should ever be the one truncated.
 */
const char* flock_device_long_str(FlockVendor vendor, FlockDevClass cls);

/** Short human-readable label for a device class ("ALPR", "Acoustic", "Axon"). */
/* Says WHAT, not WHO. Pair it with flock_vendor_str() wherever there is room. */
const char* flock_class_str(FlockDevClass cls);

/** Long human-readable label for the detail screen. */
const char* flock_class_long_str(FlockDevClass cls);

/**
 * True if the first 3 bytes of `mac` match a known SoundThinking / ShotSpotter
 * OUI prefix. Disjoint from flock_oui_match() -- a MAC is one class or the other,
 * never both.
 */
bool soundthinking_oui_match(const uint8_t* mac);

/**
 * True if the first 3 bytes of `mac` are Axon Enterprise's IEEE-registered OUI.
 *
 * NO LONGER IMPLIES "BODY-WORN". This used to be documented as movable
 * equipment only -- Axon made body cameras and in-car kit, not poles -- and the
 * label said so. In 2026 Axon shipped Outpost and Lightpost, two FIXED ALPR
 * cameras, onto the same single OUI registration. So a hit here can now be a
 * camera on a pole OR a camera on a person, and this side cannot tell which.
 * The label was corrected to say exactly that; see flock_db.c.
 *
 * Still registry-verified and never field-observed. Disjoint from the other
 * vendor tables.
 */
bool axon_oui_match(const uint8_t* mac);

/**
 * Device class implied by a MAC's OUI. Defaults to FlockClassAlpr.
 *
 * Note this answers "what kind", not "made by whom" -- FlockClassAlpr does NOT
 * mean Flock Safety. Call flock_vendor_from_mac() for the vendor.
 */
FlockDevClass flock_class_from_mac(const uint8_t* mac);

/**
 * True if `mac` is in one of the VENDOR-EXCLUSIVE competitor tables added in
 * v0.77 (Ubicquia, Motorola Solutions, Verkada, Genetec, Avigilon).
 *
 * A STRONGER CLASS OF EVIDENCE than flock_oui_match(), and the reason it is a
 * separate predicate. Those prefixes are registered to the surveillance vendor
 * ITSELF, not to a chip supplier it happens to buy from, so "this beacon came
 * from a Ubicquia device" is a real statement where "this beacon contains a
 * Liteon radio" is not. That is what lets the companion score these on a bare
 * beacon at "possible" -- a rung bare-OUI scoring was removed from for
 * flock_ouis[] after it flagged a T-Mobile gateway as a possible camera.
 *
 * Excludes Flock, SoundThinking and Axon, which have their own matchers.
 */
bool vendor_exclusive_oui_match(const uint8_t* mac);

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

/*
 * NOTE: there is deliberately no combined flock_score() here. It existed until
 * v0.48 with zero production callers while the real ladder lived in
 * helpers/esp_parser.c, so its tests guarded code the device never ran. See the
 * block comment in flock_db.c for the full rationale before adding one back.
 */

/** Human-readable label for a confidence level. */
const char* flock_confidence_str(FlockConfidence confidence);

/**
 * WHICH indicator put a detection on the list, so the operator can weigh it
 * (GitHub issue #5). "Possible" on its own says how sure we are but not why,
 * and an OUI-prefix lead and an SSID-pattern match deserve very different
 * trust.
 *
 * Derived from stored evidence (MAC / SSID / IE-fp), never from a field the
 * companion asserts, so it cannot inherit an over-claim from firmware that
 * lags the app -- the same trust-boundary reasoning as parse_flock().
 */
typedef enum {
    FlockMethodUnknown = 0, /**< nothing WE can re-derive matched (see below). */
    FlockMethodSsid, /**< SSID matched a known Flock naming pattern. */
    FlockMethodIeFp, /**< probe IE-skeleton fingerprint matched. */
    FlockMethodOui, /**< MAC is in a Flock/SoundThinking-associated OUI table. */
    FlockMethodBle, /**< BLE sighting: the companion classified it by mfg id / GATT. */
} FlockMethod;

/**
 * Re-derive the strongest indicator behind a detection, strongest first:
 * SSID pattern > IE fingerprint > OUI prefix. A BLE sighting (`ftype == 'L'`)
 * reports FlockMethodBle when nothing stronger is re-derivable, because its
 * classification happened on the companion (mfg id 0x09C8 / Raven GATT) and is
 * not reconstructible from these fields.
 *
 * FlockMethodUnknown is a HONEST answer, not a failure: the companion scores
 * probe-request behaviour we never see, so a "Likely" from a MAC outside our
 * tables genuinely has no indicator this side can name.
 *
 * @param mac    6-byte MAC (NULL-safe).
 * @param ssid   SSID as stored, may be NULL/empty.
 * @param ftype  frame-type tag: P/B/R/O/F/L.
 * @param ie_fp  IE-skeleton fingerprint, 0 = none.
 */
FlockMethod flock_method_of(const uint8_t* mac, const char* ssid, char ftype, uint32_t ie_fp);

/** Short label for a detection method ("SSID name", "OUI prefix", ...). */
const char* flock_method_str(FlockMethod method);

#ifdef __cplusplus
}
#endif
