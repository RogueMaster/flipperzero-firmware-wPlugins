// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "detect_rules.h"

#include <math.h>

// M_PI is a POSIX/GNU extension, not standard C -- define it if the host's
// <math.h> (strict -std=c11) doesn't. The firmware toolchain provides it.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float detect_dist_m(float lat1, float lon1, float lat2, float lon2) {
    float dlat = (lat2 - lat1) * 111320.0f;
    float dlon = (lon2 - lon1) * 111320.0f * cosf(lat1 * (float)M_PI / 180.0f);
    return sqrtf(dlat * dlat + dlon * dlon);
}

bool flock_geotag_should_update(
    bool have_fix,
    bool already_tagged,
    int8_t rssi,
    int8_t geotag_rssi) {
    // Tag if we have a fix and either it isn't tagged yet, or this sighting is
    // meaningfully stronger (6 dB margin absorbs scan-to-scan RSSI jitter).
    return have_fix && (!already_tagged || rssi > geotag_rssi + 6);
}

void ble_track_fold_fix(BleTrack* t, float lat, float lon) {
    if(isnan(t->last_wp_lat)) {
        // First counted waypoint is wherever we are now.
        t->last_wp_lat = lat;
        t->last_wp_lon = lon;
        if(t->waypoints == 0) t->waypoints = 1;
    } else if(detect_dist_m(t->last_wp_lat, t->last_wp_lon, lat, lon) >= WAYPOINT_GAP_M) {
        // Moved a fresh waypoint's distance and the device is still in range:
        // count it, advance the marker, grow the track span.
        t->waypoints++;
        t->last_wp_lat = lat;
        t->last_wp_lon = lon;
        if(!isnan(t->first_lat)) {
            float span = detect_dist_m(t->first_lat, t->first_lon, lat, lon);
            if(span > t->max_span_m) t->max_span_m = span;
        }
    }
}

bool ble_following_gate(uint32_t count, uint32_t elapsed_ms, uint32_t waypoints, float span_m) {
    return count >= FOLLOW_MIN_COUNT && elapsed_ms >= FOLLOW_MIN_MS &&
           waypoints >= FOLLOW_MIN_WAYPOINTS && span_m >= FOLLOW_MIN_SPAN_M;
}

uint8_t flock_alert_min_conf_rung(uint8_t choice) {
    switch(choice) {
    case AlertConfPossible:
        return 1u; // FlockConfidencePossible
    case AlertConfConfirmed:
        return 4u; // FlockConfidenceConfirmed
    case AlertConfLikely:
    default:
        // Anything unrecognised (corrupt settings file) lands on the shipped
        // default, never on the looser rung.
        return ALERT_MIN_CONF;
    }
}

bool flock_alert_should_fire(
    uint8_t prev_conf,
    uint8_t new_conf,
    bool already_alerted,
    uint32_t now_tick,
    uint32_t last_alert_tick,
    bool have_alerted_before,
    uint8_t min_conf) {
    if(already_alerted) return false; // one alert per device, ever
    if(new_conf < min_conf) return false; // below the operator's chosen rung
    if(prev_conf >= min_conf) return false; // it already qualified -> not a crossing
    // Rate limit across devices. Guarded on have_alerted_before so the very first
    // alert of a session isn't measured against a last_alert_tick of 0.
    if(have_alerted_before && (now_tick - last_alert_tick) < ALERT_COOLDOWN_MS) return false;
    return true;
}
