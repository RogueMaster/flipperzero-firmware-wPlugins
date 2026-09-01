#pragma once

#include <stdint.h>

/* The strength smoother.
 *
 * This started life as `ema = (ema * 3 + duty) / 4`, which looks harmless and
 * is not. Integer division throws away the remainder on every single update, so
 * the filter cannot converge on its own input: fed a steady 31% it settles at
 * 28 and stays there. The meter was therefore under-reading by about three
 * points of raw duty forever - roughly ten points of displayed field once the
 * full-scale mapping is applied - which is exactly why resting the Flipper on a
 * reader showed 97% and never MAX.
 *
 * Keeping the state at 1/16 resolution fixes it: the remainder now lives in the
 * fractional bits instead of being discarded, and the filter converges exactly.
 * Same responsiveness, same one-line cost, no bias.
 *
 * Pure and header-only so the firmware and the host tests share one copy. */

#define SPECTER_EMA_FRAC  16 /* fixed-point resolution */
#define SPECTER_EMA_SHIFT 4 /* smoothing factor: new = old + (target-old)/4 */

typedef struct {
    int32_t q; /* smoothed value, in 1/SPECTER_EMA_FRAC units */
} Ema;

static inline void ema_reset(Ema* e) {
    e->q = 0;
}

/* Feed one sample (0..100), get the smoothed value back (0..100). */
static inline uint8_t ema_update(Ema* e, uint8_t sample) {
    int32_t target = (int32_t)sample * SPECTER_EMA_FRAC;
    /* Signed on purpose: a falling signal makes (target - q) negative, and
     * doing this in unsigned would wrap into a huge positive step. */
    e->q += (target - e->q) / SPECTER_EMA_SHIFT;
    int32_t v = (e->q + SPECTER_EMA_FRAC / 2) / SPECTER_EMA_FRAC; /* round */
    if(v < 0) v = 0;
    if(v > 100) v = 100;
    return (uint8_t)v;
}
