// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file attack_triage.h
 * Pure classification for a Net Guardian attack: is it real, or noise?
 *
 * Net Guardian's HUD only ever showed a COUNT ("Atk 1"), which answered the
 * wrong question. The one the operator actually has is "is someone attacking my
 * network right now, or did my router just reboot?" -- and a count cannot tell
 * those apart. This turns the raw counters the companion already reports into a
 * verdict a human can act on.
 *
 * NO NEW SENSOR, NO TRANSMIT. It reads only what the passive scan already saw:
 * how many attack frames, over how long, how recently. The distinction is timing,
 * not magic:
 *
 *   - a reboot / band-steer / one congested moment is a BLIP: a short burst that
 *     stops. Few frames, brief span, and then silence.
 *   - a real attack PERSISTS: frames keep arriving across many status intervals,
 *     so the span grows into tens of seconds or minutes while it stays fresh.
 *
 * Pure so it is host-tested, and so the thresholds live in one place rather than
 * scattered through the scene code.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** What kind of attack the companion attributed. Drives the label and advice. */
typedef enum {
    AttackKindDeauth = 0, /**< deauth / disassoc flood against a specific BSSID */
    AttackKindBeaconFlood, /**< many distinct beaconing BSSIDs/s (Marauder/Pineapple) */
    AttackKindProbeFlood, /**< abnormal probe-request rate (KARMA / mass-probe) */
    AttackKindBleSpam, /**< Apple/Samsung/Google pairing-advert flood */
    AttackKindEvilTwin, /**< a clone of a network's SSID with mismatched security */
    AttackKindOther,
} AttackKind;

/** The verdict, worst first so a caller can pick the most severe of several. */
typedef enum {
    AttackStatusEnded = 0, /**< was seen, but nothing fresh -> it stopped */
    AttackStatusBlip, /**< brief and low-volume -> most likely NOT an attack */
    AttackStatusActive, /**< sustained and fresh -> treat as a live attack */
} AttackStatus;

/* Defaults, overridable by the caller so the scene and the tests agree on one
 * source of truth. Chosen conservatively: a genuine attack clears them within a
 * second or two, a reboot blip does not reach the span. */
#define ATTACK_FRESH_MS_DEFAULT     15000u /**< no frame in this long -> Ended */
#define ATTACK_SUSTAINED_MS_DEFAULT 4000u /**< span a real attack must exceed */
#define ATTACK_FLOOD_MIN_DEFAULT    5u /**< frames/events below this is just churn */

/**
 * Classify one attack from its counters.
 *
 * @param count      attack frames/events attributed to this target so far
 * @param first_tick furi tick when this target was first seen under attack
 * @param last_tick  furi tick of the most recent frame/event
 * @param now        current furi tick
 * @param flood_min  minimum count to be more than churn (0 -> the default)
 * @param fresh_ms   silence after which it has Ended (0 -> the default)
 * @param sustained_ms  span it must exceed to be Active (0 -> the default)
 */
static inline AttackStatus attack_triage_status(
    uint32_t count,
    uint32_t first_tick,
    uint32_t last_tick,
    uint32_t now,
    uint32_t flood_min,
    uint32_t fresh_ms,
    uint32_t sustained_ms) {
    if(flood_min == 0) flood_min = ATTACK_FLOOD_MIN_DEFAULT;
    if(fresh_ms == 0) fresh_ms = ATTACK_FRESH_MS_DEFAULT;
    if(sustained_ms == 0) sustained_ms = ATTACK_SUSTAINED_MS_DEFAULT;

    // Unsigned tick subtraction wraps correctly across the ~49-day rollover, the
    // same contract the rest of the app relies on.
    uint32_t age = now - last_tick;
    if(age > fresh_ms) return AttackStatusEnded;

    // last >= first by construction, but guard the subtraction anyway: a torn
    // read or a wrapped pair must not report a spuriously huge span.
    uint32_t span = (last_tick >= first_tick) ? (last_tick - first_tick) : 0;
    if(count >= flood_min && span >= sustained_ms) return AttackStatusActive;
    return AttackStatusBlip;
}

/** One-word status for the HUD/detail line. */
static inline const char* attack_status_str(AttackStatus s) {
    switch(s) {
    case AttackStatusActive:
        return "ACTIVE";
    case AttackStatusBlip:
        return "brief";
    case AttackStatusEnded:
    default:
        return "ended";
    }
}

/** Short human name for the kind, for the detail header. */
static inline const char* attack_kind_str(AttackKind k) {
    switch(k) {
    case AttackKindDeauth:
        return "Deauth flood";
    case AttackKindBeaconFlood:
        return "Beacon flood";
    case AttackKindProbeFlood:
        return "Probe flood";
    case AttackKindBleSpam:
        return "BLE spam";
    case AttackKindEvilTwin:
        return "Evil twin AP";
    case AttackKindOther:
    default:
        return "Attack tool";
    }
}

/**
 * What the operator should actually DO. Deliberately concrete: the whole point
 * of the triage is to end at an action, not a scary word.
 *
 * Deauth is the only one with a real fix (PMF), and it is the one that maps to
 * "someone is kicking my devices off" -- so it gets the direct instruction. The
 * flood/spam kinds are nuisances a passive tool cannot stop and should not
 * pretend to; the honest advice is to say so and where they are coming from.
 */
static inline const char* attack_advice(AttackKind k) {
    switch(k) {
    case AttackKindDeauth:
        return "Enable WPA3 / PMF (802.11w) on your router; a spoofed deauth is "
               "then ignored. If it persists, the source is close by.";
    case AttackKindEvilTwin:
        return "A clone of your SSID is on the air. Check the BSSID your devices "
               "join, and prefer WPA3 so a downgrade is refused.";
    case AttackKindBeaconFlood:
        return "SSID-spam tool nearby (Marauder/Pineapple). A nuisance, not a "
               "breach of your network. Note the location.";
    case AttackKindProbeFlood:
        return "Mass-probe / KARMA tool nearby. It baits devices to connect; do "
               "not auto-join open networks. Not a breach of yours.";
    case AttackKindBleSpam:
        return "Bluetooth pairing-spam nearby (Flipper/ESP). A nuisance to phones "
               "in range, not an attack on your Wi-Fi.";
    case AttackKindOther:
    default:
        return "Unrecognised attack-tool signature. Note the time and place; the "
               "evidence log has the details.";
    }
}
