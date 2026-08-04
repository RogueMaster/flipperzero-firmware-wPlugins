// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file detect_rules.h
 * Pure detection "coincidence rules" extracted from the recon_app.c god-object.
 *
 * These are the decision functions (geotag hysteresis, the anti-stalking
 * waypoint/span track + "following" gate) that used to be inlined inside
 * recon_app.c's locked update paths. Pulled out as plain-input functions -- like
 * watchscore_eval already is -- so recon_app.c stays a thin lock+array shell and
 * the coincidence rules become host-testable. No app/lock/firmware dependencies.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

// ---- geodesy -----------------------------------------------------------

/** Rough planar distance in metres (equirectangular; fine for a "moved" test). */
float detect_dist_m(float lat1, float lon1, float lat2, float lon2);

// ---- Flock geotag hysteresis (recon_app_report_flock) ------------------

/**
 * Should a Flock entry's geotag be (re)written with the current fix? True when a
 * fix exists AND either the entry isn't tagged yet OR this sighting is
 * meaningfully stronger. RSSI oscillates +/-5-10 dB scan-to-scan, so the 6 dB
 * margin stops the tag jittering between roughly-equal sightings.
 */
bool flock_geotag_should_update(
    bool have_fix,
    bool already_tagged,
    int8_t rssi,
    int8_t geotag_rssi);

// ---- BLE anti-stalking "following" gate (recon_app_ble_add) ------------
//
// A real tracker following you clears all four thresholds; stationary shop Tiles
// and a single drive-by past a fixed beacon do not. This only TIGHTENS precision
// (never flags more loosely than the old single >100 m gate).
#define FOLLOW_MIN_COUNT     4 /**< seen at least this many scans */
#define FOLLOW_MIN_MS        90000u /**< over at least this long a window (90 s) */
#define FOLLOW_MIN_WAYPOINTS 3 /**< at this many distinct observer waypoints */
#define WAYPOINT_GAP_M       50.0f /**< min separation to count a new waypoint */
#define FOLLOW_MIN_SPAN_M    150.0f /**< min track span before "following" latches */

/** Rolling waypoint/span state for one tracked BLE device (subset of BleDevice). */
typedef struct {
    float first_lat, first_lon; /**< track origin (NAN until the creating fix); read-only here */
    float last_wp_lat, last_wp_lon; /**< last counted waypoint (NAN until seeded) */
    uint8_t waypoints; /**< distinct in-range waypoints counted so far */
    float max_span_m; /**< widest origin->waypoint distance so far */
} BleTrack;

/**
 * Fold one fresh in-range GPS fix into the track: seed the first waypoint, or --
 * once we have moved >= WAYPOINT_GAP_M from the last one -- count a new waypoint
 * and grow the origin->here span. Mutates *t; first_lat/first_lon are read-only.
 */
void ble_track_fold_fix(BleTrack* t, float lat, float lon);

/**
 * The "following" AND-gate: seen across many scans, over a real time window, at
 * several distinct waypoints, spanning real ground. All four must hold. The
 * caller latches the result (never un-follows).
 */
bool ble_following_gate(uint32_t count, uint32_t elapsed_ms, uint32_t waypoints, float span_m);

// ---- Flock detection alert gate (recon_app_report_flock) ---------------
//
// A hit is otherwise silent: it appears as a row on a screen you have to be
// looking at (GitHub issue #1 -- two cameras detected, noticed blocks later).
// This decides when the app raises its beep/vibro alert.

/** DEFAULT lowest confidence that may raise an alert. Mirrors FlockConfidenceLikely
 *  (2). "Possible" is an OUI-prefix-only lead -- generic vendor prefixes appear on
 *  unrelated hardware, so alerting on it by default would buzz at non-cameras.
 *  Precision over recall applies to the alert exactly as it does to the display.
 *
 *  The operator may lower it (GitHub issue #5): in a thin deployment "Possible"
 *  can be all you ever see, and an alert you cannot turn on is no alert. Opt-in
 *  only -- the shipped default is unchanged, so the loose mode is a deliberate
 *  choice to accept false positives, never something the app does on its own. */
#define ALERT_MIN_CONF    2u
/** Minimum gap between two alerts. A MAC-randomising unit, or driving into a
 *  dense deployment, can mint several qualifying entries inside one second;
 *  without this the vibro motor machine-guns and drains the battery. */
#define ALERT_COOLDOWN_MS 3000u

/** ReconSettings.alert_min_conf. Index-aligned with alert_conf_text[] in the
 *  settings scene; map to a confidence rung with flock_alert_min_conf_rung(). */
typedef enum {
    AlertConfPossible = 0, /**< OUI-prefix leads included -- expect false positives. */
    AlertConfLikely = 1, /**< default: ALERT_MIN_CONF. */
    AlertConfConfirmed = 2, /**< SSID-pattern matches only. */
    AlertConfCount = 3,
} AlertConfChoice;

/**
 * Map an AlertConfChoice to the FlockConfidence rung the gate compares against
 * (1 Possible / 2 Likely / 4 Confirmed). An out-of-range value -- a corrupt
 * settings file -- falls back to ALERT_MIN_CONF rather than the loosest rung,
 * so corruption can never silently turn on the false-positive-prone mode.
 *
 * Note 3 (ProbeFp, "Class?") is deliberately not offered: it sits BETWEEN Likely
 * and Confirmed, so both neighbouring choices already cover it.
 */
uint8_t flock_alert_min_conf_rung(uint8_t choice);

/**
 * Should this detection raise the alert? Fires at most ONCE per device, on the
 * first crossing from below ALERT_MIN_CONF to at or above it -- so a unit first
 * seen as "Possible" and later upgraded to "Confirmed" alerts exactly once,
 * while a camera that keeps being seen never re-alerts.
 *
 * @param prev_conf           entry confidence BEFORE this sighting was merged.
 * @param new_conf            entry confidence AFTER the merge.
 * @param already_alerted     the entry's own latch (this device already alerted).
 * @param first_live_sighting this entry was ARCHIVED (restored from hits.csv)
 *                            until this sighting, so both its latch and its
 *                            confidence were carried in from an earlier session
 *                            and neither describes anything that happened now.
 *                            Treated as a fresh device for alerting purposes.
 *
 *                            Without this, saving hits silently disabled alerts
 *                            for every camera you had ever recorded: the loader
 *                            sets `alerted` so a restored hit cannot buzz at
 *                            startup, nothing cleared it, and the restored
 *                            confidence also failed the crossing test below.
 *                            Driving past the same camera tomorrow is a new
 *                            event and must alert (issue #5).
 * @param now_tick            furi tick of this sighting.
 * @param last_alert_tick     tick of the last alert this session (any device).
 * @param have_alerted_before false until the first alert of the session, so a
 *                            fresh session's `last_alert_tick == 0` can't be
 *                            mistaken for "an alert 0 ms ago" and swallowed.
 * @param min_conf            lowest qualifying rung, from
 *                            flock_alert_min_conf_rung(settings.alert_min_conf).
 */
bool flock_alert_should_fire_ex(
    uint8_t prev_conf,
    uint8_t new_conf,
    bool already_alerted,
    bool first_live_sighting,
    uint32_t now_tick,
    uint32_t last_alert_tick,
    bool have_alerted_before,
    uint8_t min_conf);

/** Back-compat shim: a live entry that was never archived. */
static inline bool flock_alert_should_fire(
    uint8_t prev_conf,
    uint8_t new_conf,
    bool already_alerted,
    uint32_t now_tick,
    uint32_t last_alert_tick,
    bool have_alerted_before,
    uint8_t min_conf) {
    return flock_alert_should_fire_ex(
        prev_conf,
        new_conf,
        already_alerted,
        false,
        now_tick,
        last_alert_tick,
        have_alerted_before,
        min_conf);
}
