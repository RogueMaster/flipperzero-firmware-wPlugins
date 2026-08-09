// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// fast_trig replaces newlib's sinf/cosf to keep ~3.2 KB of Payne-Hanek range
// reduction out of an image that has to load into one contiguous allocation.
// That trade is only acceptable if the replacement is indistinguishable from
// libm across the inputs this app actually produces -- distance between two
// sightings feeds the BLE "following" signal and the alert coincidence gate, so
// an inaccuracy here would move a DETECTION, not just a pixel.
//
// These tests therefore compare against the host's libm directly, over the real
// domain, at a tolerance far tighter than the float inputs themselves carry.
#include "test.h"

#include "fast_trig.h"

#include <math.h>

// M_PI is a POSIX/GNU extension, not standard C, and the harness builds with
// -std=c11. Same guard detect_rules.c carries, for the same reason.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Tolerance: the polynomial's own truncation error at the interval edge is
// ~6e-8. 1e-6 leaves headroom for the host's libm differing in the last ulp
// while still being 100x tighter than the ~4e-6 resolution a float coordinate
// has near 47 degrees -- i.e. tight enough that no real input can drift.
#define TRIG_TOL 1e-6f

static int close_enough(float a, float b) {
    return fabsf(a - b) <= TRIG_TOL;
}

void suite_fast_trig(void) {
    printf("\n[fast_trig]\n");

    // ---- exact landmarks ---------------------------------------------------
    CHECK(close_enough(trig_sinf(0.0f), 0.0f));
    CHECK(close_enough(trig_cosf(0.0f), 1.0f));
    CHECK(close_enough(trig_sinf((float)M_PI / 2.0f), 1.0f));
    CHECK(close_enough(trig_cosf((float)M_PI / 2.0f), 0.0f));
    CHECK(close_enough(trig_sinf((float)M_PI), 0.0f));
    CHECK(close_enough(trig_cosf((float)M_PI), -1.0f));
    CHECK(close_enough(trig_sinf(-(float)M_PI / 2.0f), -1.0f));
    CHECK(close_enough(trig_cosf(-(float)M_PI / 2.0f), 0.0f));

    // ---- the real domain #1: cos(latitude in radians) ----------------------
    // Every distance helper in the app calls cos(lat * pi/180) to scale a
    // longitude delta. Sweep the whole legal latitude range at 0.5 deg.
    for(int deg = -900; deg <= 900; deg++) {
        float lat = (float)deg / 10.0f;
        float rad = lat * (float)M_PI / 180.0f;
        CHECK(close_enough(trig_cosf(rad), cosf(rad)));
    }

    // ---- the real domain #2: compass heading in radians --------------------
    // The map's heading tick uses both sin and cos over a full turn.
    for(int deg = 0; deg <= 360; deg++) {
        float rad = (float)deg * (float)M_PI / 180.0f;
        CHECK(close_enough(trig_sinf(rad), sinf(rad)));
        CHECK(close_enough(trig_cosf(rad), cosf(rad)));
    }

    // ---- beyond the domain: must degrade, never explode --------------------
    // Nothing in the app feeds these, but "wrong pixel" and "hang" are very
    // different failures, and the reduction is constant-time precisely so a bad
    // input cannot spin. Loosen the tolerance: at 1000 radians a float argument
    // has already lost the low bits, so libm and any reduction disagree.
    for(int i = 1; i <= 20; i++) {
        float x = (float)i * 50.0f;
        CHECK(fabsf(trig_sinf(x)) <= 1.0f + TRIG_TOL);
        CHECK(fabsf(trig_cosf(x)) <= 1.0f + TRIG_TOL);
    }

    // ---- non-finite input is absorbed, not propagated ----------------------
    // Callers hand these functions GPS-derived values. A NaN that reached a
    // canvas coordinate would be a crash or a garbage pixel; 0 is inert.
    CHECK(trig_sinf(NAN) == 0.0f);
    CHECK(trig_cosf(NAN) == 0.0f);
    CHECK(trig_sinf(INFINITY) == 0.0f);
    CHECK(trig_cosf(-INFINITY) == 0.0f);

    // ---- the identity the map's heading tick depends on ---------------------
    for(int deg = 0; deg < 360; deg += 7) {
        float rad = (float)deg * (float)M_PI / 180.0f;
        float s = trig_sinf(rad), c = trig_cosf(rad);
        CHECK(close_enough(s * s + c * c, 1.0f));
    }
}
