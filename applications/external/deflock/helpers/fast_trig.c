// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "fast_trig.h"

#include <math.h>

#define TRIG_PI      3.14159265358979323846f
#define TRIG_HALF_PI 1.57079632679489661923f
#define TRIG_TWO_PI  6.28318530717958647692f
#define TRIG_INV_2PI 0.15915494309189533577f

/**
 * Reduce x to [-pi, pi] by subtracting the nearest multiple of 2*pi.
 *
 * Deliberately NOT a `while(x > pi) x -= 2*pi` loop: that is one iteration for
 * our inputs but unbounded for a bad one, and a hang is a worse failure than a
 * wrong pixel. The round-to-nearest form is constant time for any finite x.
 */
static float trig_wrap(float x) {
    float k = x * TRIG_INV_2PI;
    // Round to nearest, away from zero on .5 -- direction does not matter, only
    // that |k| lands within one of x/(2*pi).
    k = (k >= 0.0f) ? (float)(long)(k + 0.5f) : -(float)(long)(0.5f - k);
    return x - k * TRIG_TWO_PI;
}

float trig_sinf(float x) {
    if(isnan(x) || isinf(x)) return 0.0f;

    x = trig_wrap(x);

    // Fold [-pi, pi] onto [-pi/2, pi/2]; sin is symmetric about +-pi/2, so the
    // value is unchanged by either reflection.
    if(x > TRIG_HALF_PI) {
        x = TRIG_PI - x;
    } else if(x < -TRIG_HALF_PI) {
        x = -TRIG_PI - x;
    }

    // Taylor through x^11. The first dropped term is x^13/6227020800, which at
    // the interval edge (pi/2) is 5.8e-8 -- below one ulp of the result there,
    // so the polynomial is the accuracy limit only in the sense that float is.
    const float x2 = x * x;
    return x *
           (1.0f +
            x2 * (-1.66666667e-1f +
                  x2 * (8.33333333e-3f +
                        x2 * (-1.98412698e-4f + x2 * (2.75573192e-6f + x2 * -2.50521084e-8f)))));
}

float trig_cosf(float x) {
    if(isnan(x) || isinf(x)) return 0.0f;
    // cos(x) == sin(x + pi/2). Adding before the wrap keeps one code path for
    // the reduction and the polynomial, which is the whole point of the file.
    return trig_sinf(x + TRIG_HALF_PI);
}
