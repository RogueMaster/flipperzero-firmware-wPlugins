// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "detect_rules.h"

#include <math.h>
#include "fast_trig.h"

// M_PI is a POSIX/GNU extension, not standard C -- define it if the host's
// <math.h> (strict -std=c11) doesn't. The firmware toolchain provides it.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float detect_dist_m(float lat1, float lon1, float lat2, float lon2) {
    float dlat = (lat2 - lat1) * 111320.0f;
    float dlon = (lon2 - lon1) * 111320.0f * trig_cosf(lat1 * (float)M_PI / 180.0f);
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

bool flock_alert_should_fire_ex(
    uint8_t prev_conf,
    uint8_t new_conf,
    bool already_alerted,
    bool first_live_sighting,
    uint32_t now_tick,
    uint32_t last_alert_tick,
    bool have_alerted_before,
    uint8_t min_conf) {
    // A stored device met again on the air is a NEW event. Its latch and its
    // confidence both came off the SD card and describe an earlier session, so
    // neither may veto this alert -- otherwise turning on Save Hits quietly
    // disabled alerts for every camera you had already driven past, permanently
    // and across reboots. Reported twice on issue #5 as "alerts don't work".
    if(first_live_sighting) {
        already_alerted = false;
        prev_conf = 0;
    }
    if(already_alerted) return false; // one alert per device, per session
    if(new_conf < min_conf) return false; // below the operator's chosen rung
    if(prev_conf >= min_conf) return false; // it already qualified -> not a crossing
    // Rate limit across devices. Guarded on have_alerted_before so the very first
    // alert of a session isn't measured against a last_alert_tick of 0.
    if(have_alerted_before && (now_tick - last_alert_tick) < ALERT_COOLDOWN_MS) return false;
    return true;
}

int32_t esp_frames_rate(uint32_t prev_frames, uint32_t now_frames, uint32_t elapsed_ms) {
    if(elapsed_ms == 0) return -1;
    if(now_frames < prev_frames) return -1; // lifetime counter fell -> ESP rebooted
    uint32_t delta = now_frames - prev_frames;
    // 64-bit intermediate: a 921600-baud link can move enough frames between two
    // status lines that delta * 1000 overflows 32 bits on a long interval.
    uint64_t r = ((uint64_t)delta * 1000u) / elapsed_ms;
    if(r > 99999u) r = 99999u; // clamp so the header can never be blown open
    return (int32_t)r;
}

bool wifi_rogue_pair(uint8_t auth_a, uint8_t auth_b) {
    bool a_weak = (auth_a == WIFI_AUTH_MODE_OPEN || auth_a == WIFI_AUTH_MODE_WEP);
    bool b_weak = (auth_b == WIFI_AUTH_MODE_OPEN || auth_b == WIFI_AUTH_MODE_WEP);
    // Exactly one side weak. Both weak is a badly configured network, not a
    // clone standing in for a secured one; neither weak is transition mode or a
    // mixed mesh, which is the benign case this rule exists to stop shouting at.
    return a_weak != b_weak;
}
