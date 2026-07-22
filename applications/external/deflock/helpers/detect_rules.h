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
