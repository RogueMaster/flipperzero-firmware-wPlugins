// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Net Guardian attack triage: the classifier that answers "reboot blip or live
// attack?" from the counters the companion already reports. The whole feature
// exists to stop the HUD showing a bare "Atk 1" that a human cannot act on, so
// the boundary between blip and active is exactly what needs pinning.
#include "attack_triage.h"
#include "test.h"

#include <stdio.h>

void suite_attack_triage(void) {
    printf("[attack_triage]\n");

    const uint32_t now = 1000000;
    const uint32_t fresh = ATTACK_FRESH_MS_DEFAULT;
    const uint32_t sustained = ATTACK_SUSTAINED_MS_DEFAULT;
    const uint32_t flood = ATTACK_FLOOD_MIN_DEFAULT;

    // --- ENDED: nothing fresh, whatever the history --------------------------
    // A flood that stopped 20 s ago must not keep reading as an attack.
    CHECK_INT_EQ(
        attack_triage_status(500, now - 60000, now - 20000, now, 0, 0, 0), AttackStatusEnded);

    // --- BLIP: fresh but brief and/or low-volume -----------------------------
    // A two-frame, half-second burst (router reboot, band steer): fresh, but
    // neither the count nor the span reaches the bar.
    CHECK_INT_EQ(attack_triage_status(2, now - 500, now - 100, now, 0, 0, 0), AttackStatusBlip);
    // High count but no duration: a single dense instant is still a blip until it
    // persists (span below sustained).
    CHECK_INT_EQ(attack_triage_status(200, now - 1000, now - 100, now, 0, 0, 0), AttackStatusBlip);
    // Long span but almost no frames: something ticked twice over 10 s -- churn,
    // not a flood.
    CHECK_INT_EQ(attack_triage_status(3, now - 10000, now - 100, now, 0, 0, 0), AttackStatusBlip);

    // --- ACTIVE: sustained AND fresh AND above the floor ---------------------
    // Frames still arriving, span well past the sustained bar, plenty of them.
    CHECK_INT_EQ(
        attack_triage_status(340, now - 47000, now - 200, now, 0, 0, 0), AttackStatusActive);

    // --- boundaries ----------------------------------------------------------
    // Exactly at both thresholds is Active (>=, not >).
    CHECK_INT_EQ(
        attack_triage_status(flood, now - sustained, now, now, 0, 0, 0), AttackStatusActive);
    // One under the floor -> Blip even with the span.
    CHECK_INT_EQ(
        attack_triage_status(flood - 1, now - sustained, now, now, 0, 0, 0), AttackStatusBlip);
    // One ms under the span -> Blip even with the count.
    CHECK_INT_EQ(
        attack_triage_status(flood, now - (sustained - 1), now, now, 0, 0, 0), AttackStatusBlip);
    // Exactly at the freshness edge is still alive; one ms past it has Ended.
    CHECK_INT_EQ(
        attack_triage_status(flood, now - 60000, now - fresh, now, 0, 0, 0), AttackStatusActive);
    CHECK_INT_EQ(
        attack_triage_status(flood, now - 60000, now - fresh - 1, now, 0, 0, 0),
        AttackStatusEnded);

    // --- tick wrap: last_tick before first_tick must not fabricate a span ----
    // A torn/wrapped pair (last < first) yields span 0, so it degrades to Blip
    // rather than a spuriously huge "active for 4 billion ms".
    CHECK_INT_EQ(attack_triage_status(500, now - 100, now - 5000, now, 0, 0, 0), AttackStatusBlip);

    // --- caller overrides are honoured ---------------------------------------
    // A stricter sustained bar reclassifies the same input.
    CHECK_INT_EQ(
        attack_triage_status(340, now - 5000, now - 100, now, 0, 0, 10000), AttackStatusBlip);

    // --- labels + advice are total (no NULLs, deauth advice names PMF) -------
    for(int k = 0; k <= AttackKindOther; k++) {
        CHECK(attack_kind_str((AttackKind)k) != NULL);
        CHECK(attack_advice((AttackKind)k) != NULL);
        CHECK(attack_advice((AttackKind)k)[0] != '\0');
    }
    for(int s = 0; s <= AttackStatusActive; s++) {
        CHECK(attack_status_str((AttackStatus)s) != NULL);
    }
    // Deauth is the one with a real fix; the advice must actually name it.
    CHECK(strstr(attack_advice(AttackKindDeauth), "PMF") != NULL);
}
