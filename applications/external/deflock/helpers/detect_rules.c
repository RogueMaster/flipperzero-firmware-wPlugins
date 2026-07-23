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
