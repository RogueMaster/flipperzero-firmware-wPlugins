// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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
 * A SECOND, APP-OWNED FILE sits alongside it:
 *
 *     RECON_APP_FOLDER "/learned.txt"       (apps_data/flipdeflock/learned.txt)
 *
 * That one holds IE fingerprints the OPERATOR taught the app, by using
 * "Confirm: I saw it" on a detection they physically looked at. It is the only
 * file this module writes, and it exists because a fingerprint is the one thing
 * that survives MAC randomisation -- which is what every modern Flock camera
 * now uses, so the OUI tables never match them (issue #25).
 *
 * Learned fingerprints are merged into the SAME user tier as signatures.json,
 * so they are capped at the candidate "Class?" rung and can never auto-Confirm.
 * That cap is the whole safety argument: an operator picking the wrong row out
 * of a survey -- easy, since a camera and a passing phone look alike in a list
 * -- costs a weak lead, not a false camera.
 *
 * Posture (every line of this file honours these):
 *   - NEVER TOUCHES THE NETWORK. Everything is local, on-device, at app start.
 *   - WRITES ONLY learned.txt, and only on an explicit operator confirmation.
 *     signatures.json is the user's file and is never modified.
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

/**
 * Append `fp` to learned.txt, so this device is recognised again after it
 * randomises its MAC.
 *
 * Called when the operator confirms they SAW a device. Skips fp == 0 (no
 * fingerprint captured), duplicates, and hashes on the known-generic denylist
 * (flock_ie_fp_is_generic) -- a commodity scan skeleton would flag phones
 * everywhere, and it is the likeliest thing to capture by picking the wrong row
 * out of a list. Failure is silent and harmless: the detection is unaffected,
 * the operator just does not gain the signature.
 *
 * Does NOT take effect until the next app start, because the loaded arrays are
 * registered with flock_db for the session. Saying so is better than rebuilding
 * the registration underneath a running scan.
 *
 * @return true if a new fingerprint was written.
 */
bool sig_db_learn_fp(Storage* storage, uint32_t fp);

/**
 * Pin a WHOLE address the operator physically looked at, to the same file.
 *
 * For the camera whose randomised MAC turns out to be STABLE. Such an address is
 * invented, so no OUI table can ever match it, but it does not change between
 * visits, so the address itself identifies the unit. An `ouis` entry cannot
 * express that: three bytes of a random address is a prefix shared with whatever
 * else randomises into it.
 *
 * Stored in learned.txt as 12 hex digits beside the 8-digit fingerprints, read
 * by the same parser. Rejects all-zero and broadcast. Capped at "Class?" like
 * every other user signature.
 *
 * @return true if a new address was written.
 */
bool sig_db_learn_mac(Storage* storage, const uint8_t* mac);

/** Delete learned.txt. Returns true if it existed and is gone. */
bool sig_db_forget_learned(Storage* storage);

/** How many fingerprints learned.txt currently holds (0 if absent). */
size_t sig_db_learned_count(Storage* storage);

/* ---- operator exclusions (ignored.txt) ---------------------------------- */
/** Caps on the loaded exclusion lists. Public because ReconApp holds the arrays
 *  for the whole session: the gate runs on every single companion detection
 *  line, so it reads a fixed in-memory table rather than touching the card. */
#define SIG_IGNORED_MAX_FPS  32u
#define SIG_IGNORED_MAX_MACS 32u

/**
 * A THIRD file, written only on an explicit "it's mine":
 *
 *     RECON_APP_FOLDER "/ignored.txt"       (apps_data/flipdeflock/ignored.txt)
 *
 * learned.txt says "this IS a camera"; this one says "this is MINE, never
 * report it". Same line format, same parser, opposite polarity, and consulted
 * before a sighting is allowed into the hit table at all.
 *
 * Asked for in discussion #27, where a field tester's own phone and household
 * gear kept filling his list across a week of drives and there was no way to
 * silence any of it. He stopped trusting the screen, which is the real cost of
 * a false positive on a tool like this.
 */

/**
 * Exclude a device the operator says is theirs.
 *
 * Always records the ADDRESS. Also records the probe fingerprint when one was
 * captured AND it is not a commodity skeleton -- see the implementation for why
 * refusing a generic hash here is the mirror image of refusing one in
 * sig_db_learn_fp(). `out_fp_written` reports which of the two happened so the
 * UI can tell the operator whether the exclusion survives the device changing
 * its MAC; pass NULL if you do not care.
 *
 * @return true if the device is now excluded, INCLUDING when it already was.
 */
bool sig_db_ignore_add(Storage* storage, const uint8_t* mac, uint32_t fp, bool* out_fp_written);

/**
 * Read ignored.txt into caller-supplied arrays, either of which may be NULL.
 * Absent file, unreadable file and garbage lines all yield 0, the same
 * fail-safe posture as every other read in this module: a bad exclusions file
 * must never be able to suppress a detection.
 */
void sig_db_ignored_read(
    Storage* storage,
    uint32_t* fps,
    size_t fp_max,
    uint8_t (*macs)[6],
    size_t mac_max,
    size_t* out_fps,
    size_t* out_macs);

/** How many exclusions ignored.txt holds, addresses and fingerprints together. */
size_t sig_db_ignored_count(Storage* storage);

/** Delete ignored.txt. Returns true if it existed and is gone. */
bool sig_db_forget_ignored(Storage* storage);

#ifdef __cplusplus
}
#endif
