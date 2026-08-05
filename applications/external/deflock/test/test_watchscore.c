// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// WATCHSCORE state-machine tests. Locks in the >=2-independent-radio ELEVATED
// coincidence gate and B16 (a strong 2-radio signal must pass THROUGH WATCHFUL,
// never jump CLEAR->ELEVATED in a single tick). Thresholds/dwell live as private
// #defines in watchscore.c, so the tests assert on observable behaviour (state
// progression, live_count) rather than the raw constants.
#include "watchscore.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void run_ticks(WatchScore* ws, const WatchInputs* in, int ticks) {
    for(int i = 0; i < ticks; i++)
        watchscore_eval(ws, in);
}

void suite_watchscore(void) {
    printf("[watchscore]\n");

    WatchInputs empty;
    memset(&empty, 0, sizeof(empty));

    // --- init ---------------------------------------------------------------
    WatchScore ws;
    watchscore_init(&ws);
    CHECK_INT_EQ(ws.state, WatchStateClear);
    CHECK_INT_EQ(ws.score, 0);

    // --- >=2 independent-radio ELEVATED gate --------------------------------
    // A strong signal confined to ONE radio (all Wi-Fi) must NEVER elevate,
    // however long it persists: flock_near + deauth + rogue all share the Wi-Fi
    // class -> live_count 1. It still raises WATCHFUL.
    watchscore_init(&ws);
    WatchInputs one_radio;
    memset(&one_radio, 0, sizeof(one_radio));
    one_radio.flock_confirmed = true;
    one_radio.flock_near = true; // Wi-Fi (flock_via_ble left false)
    one_radio.deauth_active = true; // Wi-Fi
    one_radio.rogue_ap = true; // Wi-Fi
    run_ticks(&ws, &one_radio, 50);
    CHECK_INT_EQ(ws.live_count, 1);
    CHECK(ws.state != WatchStateElevated); // the coincidence gate holds
    CHECK_INT_EQ(ws.state, WatchStateWatchful);

    // --- two independent radios -> ELEVATED, via WATCHFUL, fire-once --------
    watchscore_init(&ws);
    WatchInputs two_radio;
    memset(&two_radio, 0, sizeof(two_radio));
    two_radio.flock_confirmed = true;
    two_radio.flock_near = true; // Wi-Fi
    two_radio.ble_following = true; // BLE (independent second radio)

    int elevated_events = 0;
    bool saw_watchful_before_elevated = false;
    bool reached_elevated = false;
    uint8_t prev = ws.state;
    for(int i = 0; i < 20; i++) {
        watchscore_eval(&ws, &two_radio);
        CHECK(!(prev == WatchStateClear && ws.state == WatchStateElevated)); // B16: no jump
        if(ws.state == WatchStateWatchful && !reached_elevated)
            saw_watchful_before_elevated = true;
        if(ws.state == WatchStateElevated) reached_elevated = true;
        if(ws.just_elevated) elevated_events++;
        prev = ws.state;
    }
    CHECK(reached_elevated);
    CHECK(saw_watchful_before_elevated);
    CHECK_INT_EQ(ws.live_count, 2);
    CHECK_INT_EQ(elevated_events, 1); // just_elevated fires exactly once on entry
    CHECK(ws.breakdown[0] != '\0'); // explainable: contributors are listed

    // --- decays back to CLEAR when signals stop -----------------------------
    run_ticks(&ws, &empty, 60);
    CHECK_INT_EQ(ws.state, WatchStateClear);
    CHECK_INT_EQ(ws.score, 0);

    // --- state label strings ------------------------------------------------
    CHECK_STR_EQ(watchscore_state_str(WatchStateClear), "CLEAR");
    CHECK_STR_EQ(watchscore_state_str(WatchStateWatchful), "WATCHFUL");
    CHECK_STR_EQ(watchscore_state_str(WatchStateElevated), "ELEVATED");
}
