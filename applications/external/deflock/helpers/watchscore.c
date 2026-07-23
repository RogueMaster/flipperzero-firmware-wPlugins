// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "watchscore.h"

#include <string.h>
#include <stdio.h>

// ---- tunable model (all #defines, like the anti-stalking FOLLOW_MIN_*) -----
//
// WatchScore fuses ONLY already-validated detections into one decaying
// confidence. It invents no sensor; it scores correlation across radios and
// time. Per the project ethos a false "you're being watched" is worse than a
// missed one, so the gates are conservative and ELEVATED needs two simultaneous
// independent-radio failures.

// Per-signal weights (points added each evaluation while the signal is live).
#define WS_W_FLOCK_NEAR   35 /**< CONFIRMED Flock co-located with you (recent + near) */
#define WS_W_FLOCK_RECENT 20 /**< CONFIRMED Flock seen recently, distance unknown */
#define WS_W_BLE_FOLLOW   35 /**< BLE tracker that cleared the anti-stalking gate */
#define WS_W_DEAUTH       25 /**< attributed deauth/disassoc flood active now */
#define WS_W_ROGUE_AP     20 /**< evil-twin / rogue AP (mismatched-security clone) */
#define WS_W_ATTACK       30 /**< an active attack-tool signature (BLE-spam / beacon / probe flood) */
#define WS_W_FLIPPER      25 /**< a Flipper Zero advertising nearby (recon-capable device) */
#define WS_W_ANOMALY \
    15 /**< (opt-in) unidentified strong persistent device on you -- soft, needs backup */

// Recency / co-location gating (whether a signal is "live" and "near") is done
// by the snapshot caller in recon_app.c (WATCH_FLOCK_FRESH_MS / WATCH_DEAUTH_-
// FRESH_MS / WATCH_FLOCK_NEAR_M) since it needs app state + GPS; this file owns
// only the pure scoring envelope below.

// Score envelope, decay, and hysteresis.
#define WS_SCORE_MAX      100 /**< clamp ceiling */
#define WS_DECAY_PER_TICK 4 /**< points bled off each tick when a signal is absent */
#define WS_WATCHFUL_ON    25 /**< rise above this -> WATCHFUL candidate */
#define WS_WATCHFUL_OFF   12 /**< fall below this -> back toward CLEAR (hysteresis) */
#define WS_ELEVATED_ON    60 /**< rise above this (+ coincidence) -> ELEVATED candidate */
#define WS_ELEVATED_OFF   40 /**< fall below this -> drop out of ELEVATED (hysteresis) */

// Dwell: a candidate state must persist this many consecutive ticks before it
// surfaces, so a single-frame spike never flickers the UI or fires the alert.
// 250 ms tick -> WATCHFUL ~1 s, ELEVATED ~2 s of sustained signal.
#define WS_DWELL_WATCHFUL 4
#define WS_DWELL_ELEVATED 8

// ELEVATED also requires >=2 DISTINCT independent-radio signals correlated now,
// so a top-tier false positive needs two simultaneous independent failures.
#define WS_ELEVATED_MIN_RADIOS 2

const char* watchscore_state_str(WatchState state) {
    switch(state) {
    case WatchStateClear:
        return "CLEAR";
    case WatchStateWatchful:
        return "WATCHFUL";
    case WatchStateElevated:
        return "ELEVATED";
    default:
        return "-";
    }
}

void watchscore_init(WatchScore* ws) {
    memset(ws, 0, sizeof(WatchScore));
    ws->state = WatchStateClear;
    ws->just_elevated = false;
    ws->breakdown[0] = '\0';
}

// Distinct independent-radio classes among the live contributors. ELEVATED
// requires >=2 of these to be set in the same evaluation. WiFi-borne Flock,
// deauth and rogue all share the WiFi radio, so they count as ONE class; BLE
// (follower or BLE-Flock) is a genuinely independent second radio.
#define WS_RADIO_WIFI 0x01
#define WS_RADIO_BLE  0x02

/** Append " + frag" (no leading sep for the first piece) with safe clamping. */
static void breakdown_add(char* buf, size_t cap, const char* frag) {
    size_t used = strlen(buf);
    if(used >= cap - 1) return; // already full -> drop quietly
    snprintf(buf + used, cap - used, "%s%s", used ? " + " : "", frag);
}

void watchscore_eval(WatchScore* ws, const WatchInputs* in) {
    int gain = 0;
    uint8_t radios = 0;
    char parts[WATCHSCORE_BREAKDOWN_LEN];
    char frag[40];
    parts[0] = '\0';

    // --- accumulate weight from each live, already-validated signal ---------

    if(in->flock_confirmed) {
        if(in->flock_near) {
            gain += WS_W_FLOCK_NEAR;
            radios |= in->flock_via_ble ? WS_RADIO_BLE : WS_RADIO_WIFI;
            snprintf(frag, sizeof(frag), "Flock CONFIRMED %dm", (int)in->flock_dist_m);
            breakdown_add(parts, sizeof(parts), frag);
        } else {
            gain += WS_W_FLOCK_RECENT;
            radios |= in->flock_via_ble ? WS_RADIO_BLE : WS_RADIO_WIFI;
            breakdown_add(parts, sizeof(parts), "Flock CONFIRMED");
        }
    }

    if(in->ble_following) {
        gain += WS_W_BLE_FOLLOW;
        radios |= WS_RADIO_BLE;
        snprintf(frag, sizeof(frag), "BLE follower %lumin", (unsigned long)in->ble_follow_min);
        breakdown_add(parts, sizeof(parts), frag);
    }

    if(in->deauth_active) {
        gain += WS_W_DEAUTH;
        radios |= WS_RADIO_WIFI;
        breakdown_add(parts, sizeof(parts), "deauth flood");
    }

    if(in->rogue_ap) {
        gain += WS_W_ROGUE_AP;
        radios |= WS_RADIO_WIFI;
        breakdown_add(parts, sizeof(parts), "evil-twin AP");
    }

    // An active attack-tool signature (BLE-spam advert flood, beacon flood, or
    // probe flood). BLE-spam is a genuinely independent radio (BLE); the Wi-Fi
    // floods share the Wi-Fi class. Stays WATCHFUL on its own (one radio) -- a
    // second independent radio is still required to reach ELEVATED.
    if(in->attack_active) {
        gain += WS_W_ATTACK;
        radios |= in->attack_via_ble ? WS_RADIO_BLE : WS_RADIO_WIFI;
        breakdown_add(
            parts, sizeof(parts), in->attack_label[0] ? in->attack_label : "attack tool");
    }

    // A Flipper Zero advertising nearby. A recon-capable multitool, not proof of
    // an attack -- so it raises WATCHFUL by itself and only adds to ELEVATED when
    // a second independent radio agrees (its BLE class can be that radio).
    if(in->flipper_near) {
        gain += WS_W_FLIPPER;
        radios |= WS_RADIO_BLE;
        breakdown_add(parts, sizeof(parts), "Flipper Zero");
    }

    // Opt-in anomaly: an unnamed, unidentified device transmitting strongly right
    // on you, seen repeatedly. Deliberately light (below the WATCHFUL floor on its
    // own) so it nudges rather than alarms -- it needs corroboration to surface.
    if(in->anomaly) {
        gain += WS_W_ANOMALY;
        radios |= WS_RADIO_BLE;
        breakdown_add(parts, sizeof(parts), "unknown device on you");
    }

    // --- integrate: rise toward `gain`, decay when nothing is live ----------
    // The instantaneous weight is the target; we move the score toward it but
    // only bleed DOWN slowly (decay), so a signal that clears fades instead of
    // snapping off. Rising is immediate (a real threat shouldn't lag).
    if(gain > ws->score) {
        ws->score = (gain > WS_SCORE_MAX) ? WS_SCORE_MAX : gain;
    } else {
        ws->score -= WS_DECAY_PER_TICK;
        if(ws->score < gain) ws->score = gain; // never decay below a live floor
        if(ws->score < 0) ws->score = 0;
    }

    ws->live_radios = radios;
    ws->live_count = (uint8_t)((radios & WS_RADIO_WIFI ? 1 : 0) + (radios & WS_RADIO_BLE ? 1 : 0));

    // Keep an explainable breakdown of who contributed (trust != black box).
    if(parts[0]) {
        strncpy(ws->breakdown, parts, sizeof(ws->breakdown) - 1);
        ws->breakdown[sizeof(ws->breakdown) - 1] = '\0';
    } else {
        ws->breakdown[0] = '\0';
    }

    // --- hysteresis + dwell state machine -----------------------------------
    bool elevated_ok = (ws->score >= WS_ELEVATED_ON) && (ws->live_count >= WS_ELEVATED_MIN_RADIOS);
    WatchState prev = ws->state;
    ws->just_elevated = false;

    switch(ws->state) {
    case WatchStateClear:
        // Evaluate WATCHFUL independently of elevated_ok so a strong 2-radio signal
        // passes THROUGH watchful instead of snapping CLEAR->ELEVATED. The climb to
        // ELEVATED happens from WatchStateWatchful below; the shared dwell counter
        // isn't reset on the CLEAR->WATCHFUL hop, so time-to-ELEVATED is unchanged --
        // watchful is just shown during the ramp.
        if(ws->score >= WS_WATCHFUL_ON) {
            ws->dwell++;
            if(ws->dwell >= WS_DWELL_WATCHFUL) ws->state = WatchStateWatchful;
        } else {
            ws->dwell = 0;
        }
        break;

    case WatchStateWatchful:
        if(elevated_ok) {
            ws->dwell++;
            if(ws->dwell >= WS_DWELL_ELEVATED) ws->state = WatchStateElevated;
        } else if(ws->score < WS_WATCHFUL_OFF) {
            ws->state = WatchStateClear;
            ws->dwell = 0;
        } else {
            ws->dwell = 0; // sustained WATCHFUL: reset the climb-to-ELEVATED counter
        }
        break;

    case WatchStateElevated:
        // Drop out conservatively: the moment EITHER the score decays below the
        // (lower, hysteretic) clear threshold OR the >=2-radio coincidence gate
        // is no longer met, ELEVATED is no longer justified. Biasing away from
        // the top tier is the intended ethos. The OFF<ON gap stops chatter.
        if(ws->score < WS_ELEVATED_OFF || ws->live_count < WS_ELEVATED_MIN_RADIOS) {
            if(ws->score >= WS_WATCHFUL_OFF) {
                ws->state = WatchStateWatchful;
            } else {
                ws->state = WatchStateClear;
            }
            ws->dwell = 0;
        }
        break;

    default:
        ws->state = WatchStateClear;
        ws->dwell = 0;
        break;
    }

    // Fire-once edge: only on the CLEAR/WATCHFUL -> ELEVATED transition.
    if(ws->state == WatchStateElevated && prev != WatchStateElevated) {
        ws->just_elevated = true;
    }
}
