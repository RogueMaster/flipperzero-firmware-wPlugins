// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file watchscore.h
 * WATCHSCORE -- one fused, time-decaying "Am I being watched right now?" signal.
 *
 * Pure logic, no firmware dependencies, so it can be unit-tested on a host.
 *
 * It invents NO new sensor. It fuses the app's ALREADY-validated detections (a
 * CONFIRMED Flock co-located with you, a BLE tracker that cleared the
 * anti-stalking "following" gate, an attributed deauth flood active now, an
 * evil-twin/rogue AP) into a single weighted, decaying score with hysteresis,
 * a dwell window, and a cross-radio coincidence gate -- killing the alert
 * fatigue of independent per-screen pings.
 *
 * Three states: CLEAR / WATCHFUL / ELEVATED. ELEVATED requires >=2 INDEPENDENT
 * radio signals correlated in time, so a top-tier false positive needs two
 * simultaneous independent failures. The model is conservative by design (a
 * false "you're being watched" is worse than a missed one) and biases toward
 * WATCHFUL over ELEVATED when uncertain. Every contribution is explainable via
 * the breakdown string -- trust comes from explainability, not a black box.
 *
 * Weights/thresholds/decay/dwell are all tunable #defines in watchscore.c
 * (mirroring the anti-stalking FOLLOW_MIN_* defines).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Per-signal breakdown string capacity (e.g. "Flock CONFIRMED 40m + ..."). */
#define WATCHSCORE_BREAKDOWN_LEN 96

/** Fused surveillance-confidence state (with hysteresis between tiers). */
typedef enum {
    WatchStateClear = 0, /**< No correlated surveillance signal. */
    WatchStateWatchful, /**< A validated signal is live; not yet top-tier. */
    WatchStateElevated, /**< >=2 independent radios correlated -> high confidence. */
} WatchState;

/**
 * Tiny persistent scorer state (lives in ReconApp). No heap, no radio.
 * Holds the running score, the hysteresis/dwell counters, the latched state,
 * the live-radio coincidence summary, and the explainable breakdown.
 */
typedef struct {
    int16_t score; /**< current fused score, 0..WS_SCORE_MAX */
    uint8_t state; /**< WatchState (latched, with hysteresis) */
    uint8_t dwell; /**< consecutive ticks a candidate tier has held */
    uint8_t live_radios; /**< bitmask of independent radios contributing this eval */
    uint8_t live_count; /**< number of distinct independent radios this eval */
    bool just_elevated; /**< true only on the tick we transitioned INTO ELEVATED */
    char breakdown[WATCHSCORE_BREAKDOWN_LEN]; /**< per-signal contributors, "" if none */
} WatchScore;

/**
 * One evaluation's worth of already-validated, mutex-snapshotted inputs. The
 * caller computes these under app->mutex, releases, then calls watchscore_eval
 * with this plain-data struct (no app pointers, no locks held).
 */
typedef struct {
    bool flock_confirmed; /**< a CONFIRMED Flock was seen recently */
    bool flock_near; /**< ...and it is geotagged within WS_FLOCK_NEAR_M of you */
    bool flock_via_ble; /**< the Flock signal came from BLE (independent radio) */
    float flock_dist_m; /**< distance to the nearest CONFIRMED Flock (if flock_near) */

    bool ble_following; /**< a BLE device latched the anti-stalking "following" gate */
    uint32_t ble_follow_min; /**< minutes that follower has been tracked (for the breakdown) */

    bool deauth_active; /**< an attributed deauth/disassoc flood is active now */
    bool rogue_ap; /**< an evil-twin / rogue AP (mismatched-security clone) is present */

    bool flipper_near; /**< a Flipper Zero (recon multitool) is advertising nearby */
    bool attack_active; /**< an active attack-tool signature fired recently (BLE-spam / beacon flood / probe flood) */
    bool attack_via_ble; /**< the attack signature is BLE-borne (BLE-spam) vs Wi-Fi (beacon/probe flood) */
    char attack_label[16]; /**< short attack name for the breakdown, e.g. "BLE-spam" */
    bool anomaly; /**< (opt-in) an unidentified, strong, persistent device is sitting right on you */
} WatchInputs;

/** Reset the scorer to CLEAR. */
void watchscore_init(WatchScore* ws);

/**
 * Advance the scorer by one tick: integrate this evaluation's live signals,
 * decay stale weight, then run the hysteresis + dwell + coincidence state
 * machine. Sets ws->just_elevated exactly on the transition INTO ELEVATED.
 */
void watchscore_eval(WatchScore* ws, const WatchInputs* in);

/** Human-readable state label ("CLEAR" / "WATCHFUL" / "ELEVATED"). */
const char* watchscore_state_str(WatchState state);

#ifdef __cplusplus
}
#endif
