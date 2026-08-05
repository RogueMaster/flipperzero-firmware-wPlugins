// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
//
// Tests for the pure detection rules extracted from recon_app.c (R3): geotag
// hysteresis, the anti-stalking waypoint/span track, and the "following" AND-gate.
#include "detect_rules.h"
#include "test.h"

#include <math.h>
#include <stdio.h>

static bool nearf(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

void suite_detect_rules(void) {
    printf("[detect_rules]\n");

    // --- detect_dist_m ------------------------------------------------------
    CHECK(nearf(detect_dist_m(0, 0, 0, 0), 0.0f, 0.01f));
    CHECK(nearf(detect_dist_m(0, 0, 0.001f, 0), 111.32f, 1.0f)); // ~111 m per mdeg lat
    CHECK(nearf(detect_dist_m(0, 0, 0, 0.001f), 111.32f, 1.0f)); // lon at the equator
    CHECK(nearf(detect_dist_m(60, 0, 60, 0.001f), 55.66f, 1.0f)); // lon at 60N (cos60 = 0.5)

    // --- flock_geotag_should_update ----------------------------------------
    CHECK(!flock_geotag_should_update(false, false, -40, -80)); // no fix -> never
    CHECK(flock_geotag_should_update(true, false, -70, 0)); // fix + not yet tagged
    CHECK(!flock_geotag_should_update(true, true, -50, -50)); // tagged, not stronger
    CHECK(!flock_geotag_should_update(true, true, -44, -50)); // exactly +6 -> not strictly >
    CHECK(flock_geotag_should_update(true, true, -43, -50)); // +7 -> refresh
    CHECK(flock_geotag_should_update(true, true, -40, -50)); // clearly stronger

    // --- ble_following_gate (AND of all four thresholds) --------------------
    CHECK(ble_following_gate(4, 90000, 3, 150.0f)); // exactly at every threshold
    CHECK(ble_following_gate(10, 120000, 5, 300.0f)); // comfortably past
    CHECK(!ble_following_gate(3, 90000, 3, 150.0f)); // too few scans
    CHECK(!ble_following_gate(4, 89999, 3, 150.0f)); // window too short
    CHECK(!ble_following_gate(4, 90000, 2, 150.0f)); // too few waypoints
    CHECK(!ble_following_gate(4, 90000, 3, 149.9f)); // span too small

    // --- ble_track_fold_fix -------------------------------------------------
    // Seed the first waypoint from a fresh (NAN) track.
    BleTrack t = {
        .first_lat = NAN,
        .first_lon = NAN,
        .last_wp_lat = NAN,
        .last_wp_lon = NAN,
        .waypoints = 0,
        .max_span_m = 0.0f};
    ble_track_fold_fix(&t, 0.0f, 0.0f);
    CHECK_INT_EQ(t.waypoints, 1);
    CHECK(nearf(t.last_wp_lat, 0.0f, 1e-6f));

    // A fix within WAYPOINT_GAP_M does NOT advance the waypoint.
    ble_track_fold_fix(&t, 0.0002f, 0.0f); // ~22 m
    CHECK_INT_EQ(t.waypoints, 1);

    // Created WITHOUT an origin (first_lat NAN): a far fix advances the waypoint
    // but the span can't grow (no track origin to measure from).
    ble_track_fold_fix(&t, 0.001f, 0.0f); // ~111 m from the last waypoint -> advance
    CHECK_INT_EQ(t.waypoints, 2);
    CHECK(nearf(t.max_span_m, 0.0f, 0.01f));

    // A real "following" track: origin known, three >=50 m hops build the
    // waypoint count and span up past the gate.
    BleTrack tr = {
        .first_lat = 0.0f,
        .first_lon = 0.0f,
        .last_wp_lat = 0.0f,
        .last_wp_lon = 0.0f,
        .waypoints = 1,
        .max_span_m = 0.0f};
    ble_track_fold_fix(&tr, 0.000600f, 0.0f); // ~67 m -> wp2, span ~67
    ble_track_fold_fix(&tr, 0.001200f, 0.0f); // ~67 m hop -> wp3, span ~134
    ble_track_fold_fix(&tr, 0.001800f, 0.0f); // ~67 m hop -> wp4, span ~200
    CHECK_INT_EQ(tr.waypoints, 4);
    CHECK(tr.max_span_m >= FOLLOW_MIN_SPAN_M);
    CHECK(ble_following_gate(4, 90000, tr.waypoints, tr.max_span_m)); // now "following"

    // --- flock_alert_min_conf_rung (issue #5 configurable alert level) ------
    // The choice index maps to a confidence rung; anything unrecognised must land
    // on the shipped default, NEVER on the loose rung -- a corrupt settings file
    // must not silently switch the device into its false-positive-prone mode.
    CHECK_INT_EQ(flock_alert_min_conf_rung(AlertConfPossible), 1);
    CHECK_INT_EQ(flock_alert_min_conf_rung(AlertConfLikely), ALERT_MIN_CONF);
    CHECK_INT_EQ(flock_alert_min_conf_rung(AlertConfConfirmed), 4);
    CHECK_INT_EQ(flock_alert_min_conf_rung(AlertConfCount), ALERT_MIN_CONF); // out of range
    CHECK_INT_EQ(flock_alert_min_conf_rung(200), ALERT_MIN_CONF); // garbage
    CHECK_INT_EQ(flock_alert_min_conf_rung(255), ALERT_MIN_CONF);
    // The default choice must still BE the historical constant, so shipping this
    // setting did not quietly move the out-of-the-box behaviour.
    CHECK_INT_EQ(flock_alert_min_conf_rung(AlertConfLikely), 2);

    // --- flock_alert_should_fire (issue #1 alert gate) ----------------------
    // Confidence rungs: 0 None, 1 Possible, 2 Likely, 3 ProbeFp, 4 Confirmed.
    const uint8_t dflt = ALERT_MIN_CONF; // the shipped "Likely+" threshold

    // A brand-new device (prev 0) at Likely or better fires; below that, never.
    CHECK(flock_alert_should_fire(0, 2, false, 1000, 0, false, dflt)); // Likely
    CHECK(flock_alert_should_fire(0, 4, false, 1000, 0, false, dflt)); // Confirmed
    CHECK(!flock_alert_should_fire(0, 1, false, 1000, 0, false, dflt)); // OUI-only "Possible"
    CHECK(!flock_alert_should_fire(0, 0, false, 1000, 0, false, dflt)); // None

    // The per-entry latch stops a camera re-alerting every time it's seen again.
    CHECK(!flock_alert_should_fire(0, 4, true, 1000, 0, false, dflt));
    CHECK(!flock_alert_should_fire(2, 4, true, 100000, 1000, true, dflt));

    // Possible -> Confirmed is a crossing and fires exactly once; a device that
    // already qualified does not re-fire when it climbs further.
    CHECK(flock_alert_should_fire(1, 4, false, 100000, 1000, true, dflt));
    CHECK(!flock_alert_should_fire(2, 4, false, 100000, 1000, true, dflt)); // already >= Likely
    CHECK(!flock_alert_should_fire(4, 4, false, 100000, 1000, true, dflt)); // no change at all

    // Cooldown: a second device inside ALERT_COOLDOWN_MS is suppressed, and the
    // same device is allowed once the window has passed.
    CHECK(!flock_alert_should_fire(0, 4, false, 1000 + ALERT_COOLDOWN_MS - 1, 1000, true, dflt));
    CHECK(flock_alert_should_fire(0, 4, false, 1000 + ALERT_COOLDOWN_MS, 1000, true, dflt));

    // The FIRST alert of a session must not be swallowed by the cooldown: with
    // last_alert_tick still 0 and a small tick, the elapsed test would otherwise
    // read as "an alert 12 ms ago". have_alerted_before is what prevents that.
    CHECK(flock_alert_should_fire(0, 4, false, 12, 0, false, dflt));
    CHECK(!flock_alert_should_fire(0, 4, false, 12, 0, true, dflt)); // genuinely 12 ms ago

    // --- a STORED device met again on the air (issue #5) --------------------
    // recon_hits_add() sets `alerted` on every entry it restores from hits.csv so
    // a reboot cannot buzz, and the stored confidence comes back with it. Nothing
    // cleared either, so with Save Hits on, a camera you had already driven past
    // was muted forever: the latch vetoed it, and even without the latch the
    // restored confidence failed the crossing test. Reported twice as "still not
    // getting alerts".
    //
    // first_live_sighting says "this entry was archived until right now", and
    // must beat BOTH vetoes -- checking only one still leaves it silent.
    CHECK(flock_alert_should_fire_ex(4, 4, true, true, 100000, 1000, true, dflt));
    CHECK(flock_alert_should_fire_ex(2, 2, true, true, 100000, 1000, true, dflt));
    // Each veto on its own, to prove neither is doing the work alone.
    CHECK(!flock_alert_should_fire_ex(4, 4, true, false, 100000, 1000, true, dflt)); // latch
    CHECK(!flock_alert_should_fire_ex(4, 4, false, false, 100000, 1000, true, dflt)); // crossing

    // It is a bypass for provenance, NOT for the operator's threshold: a restored
    // entry below the chosen rung still stays silent.
    CHECK(!flock_alert_should_fire_ex(1, 1, true, true, 100000, 1000, true, dflt));
    // ...nor for the cooldown, or driving into a stored cluster would machine-gun
    // the vibro motor on every entry at once.
    CHECK(!flock_alert_should_fire_ex(
        4, 4, true, true, 1000 + ALERT_COOLDOWN_MS - 1, 1000, true, dflt));

    // A live entry re-seen in the SAME session is unaffected: still one alert per
    // device. Only the archived->live transition resets it.
    CHECK(!flock_alert_should_fire_ex(4, 4, true, false, 100000, 1000, true, dflt));

    // --- wifi_rogue_pair: only a DOWNGRADE is evil-twin shaped ---------------
    // esp wifi_auth_mode_t: 0 OPEN, 1 WEP, 2 WPA_PSK, 3 WPA2_PSK, 4 WPA_WPA2_PSK,
    // 5 WPA2_ENT, 6 WPA3_PSK, 7 WPA2_WPA3_PSK.
    //
    // The attack: a clone you can join with no password, or with breakable
    // crypto, standing in for a network you trust.
    CHECK(wifi_rogue_pair(0, 3)); // open twin of WPA2
    CHECK(wifi_rogue_pair(3, 0)); // order must not matter
    CHECK(wifi_rogue_pair(0, 7)); // open twin of WPA2/WPA3
    CHECK(wifi_rogue_pair(1, 6)); // WEP twin of WPA3 -- breakable, still a downgrade
    CHECK(wifi_rogue_pair(5, 0)); // open twin of an enterprise network

    // NOT rogue: the benign cases the old "any difference" rule shouted at.
    // WPA2/WPA3 transition mode across two radios or two mesh nodes is an
    // ordinary modern network, and calling it an evil twin is the kind of false
    // positive that makes every later warning worth less.
    CHECK(!wifi_rogue_pair(3, 7)); // WPA2_PSK vs WPA2_WPA3_PSK -- transition mode
    CHECK(!wifi_rogue_pair(6, 7)); // WPA3_PSK vs WPA2_WPA3_PSK
    CHECK(!wifi_rogue_pair(2, 3)); // WPA vs WPA2 -- mixed-generation mesh
    CHECK(!wifi_rogue_pair(3, 4)); // WPA2 vs WPA/WPA2
    CHECK(!wifi_rogue_pair(3, 3)); // identical, not a pair at all
    // Both weak is a badly configured network, not a clone impersonating a
    // secured one. Nothing is being downgraded.
    CHECK(!wifi_rogue_pair(0, 0));
    CHECK(!wifi_rogue_pair(0, 1));

    // --- esp_frames_rate: live activity, not a lifetime total (issue #5) -----
    CHECK_INT_EQ(esp_frames_rate(0, 100, 1000), 100); // 100 frames in 1 s
    CHECK_INT_EQ(esp_frames_rate(500, 1000, 1000), 500);
    CHECK_INT_EQ(esp_frames_rate(0, 50, 500), 100); // half a second, doubled
    CHECK_INT_EQ(esp_frames_rate(1000, 1000, 1000), 0); // link up, hearing nothing
    // -1 means "no honest rate", and the two cases are NOT the same as 0/s:
    // a divide-by-zero window, and a lifetime counter that FELL, which can only
    // happen when the ESP rebooted. Reporting 0/s for a reboot would read as "the
    // radio went deaf" when the board actually restarted -- the distinction a
    // user hit as the count "ticking back to 0" on a long drive.
    CHECK_INT_EQ(esp_frames_rate(100, 200, 0), -1);
    CHECK_INT_EQ(esp_frames_rate(9000, 12, 1000), -1);
    // Must not overflow on a big delta: delta * 1000 exceeds 32 bits well before
    // a uint32_t frame counter does.
    CHECK(esp_frames_rate(0, 4000000000u, 1000) > 0);

    // --- the threshold is honoured, not just accepted ------------------------
    // The whole point of issue #5: an operator who only ever sees "Possible" can
    // lower the bar and actually be told. Same input, three thresholds.
    const uint8_t loose = flock_alert_min_conf_rung(AlertConfPossible);
    const uint8_t strict = flock_alert_min_conf_rung(AlertConfConfirmed);

    CHECK(flock_alert_should_fire(0, 1, false, 1000, 0, false, loose)); // Possible now fires
    CHECK(!flock_alert_should_fire(0, 1, false, 1000, 0, false, dflt)); // ...but not by default
    CHECK(!flock_alert_should_fire(0, 1, false, 1000, 0, false, strict));

    // At the strict end only a Confirmed qualifies -- Likely and the "Class?"
    // (ProbeFp, rung 3) candidate-class match both stay silent.
    CHECK(!flock_alert_should_fire(0, 2, false, 1000, 0, false, strict));
    CHECK(!flock_alert_should_fire(0, 3, false, 1000, 0, false, strict));
    CHECK(flock_alert_should_fire(0, 4, false, 1000, 0, false, strict));

    // ProbeFp sits BETWEEN Likely and Confirmed, which is why no separate choice
    // is offered for it: the two neighbouring rungs already cover it.
    CHECK(flock_alert_should_fire(0, 3, false, 1000, 0, false, dflt));
    CHECK(flock_alert_should_fire(0, 3, false, 1000, 0, false, loose));

    // Crossing is measured against the ACTIVE threshold, not the default: at the
    // loose setting a device already at Possible has qualified, so climbing to
    // Confirmed is not a fresh crossing and must not double-alert.
    CHECK(!flock_alert_should_fire(1, 4, false, 100000, 1000, true, loose));
    CHECK(flock_alert_should_fire(1, 4, false, 100000, 1000, true, dflt));
}
