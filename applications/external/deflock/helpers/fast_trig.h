// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file fast_trig.h
 * Compact sinf/cosf for this app's bounded inputs.
 *
 * WHY THIS EXISTS -- it is a size fix, not a speed one. newlib's sinf/cosf must
 * stay accurate for arguments of any magnitude, so they carry Payne-Hanek range
 * reduction: __ieee754_rem_pio2f + __kernel_rem_pio2f + a 792-byte `two_over_pi`
 * table, ~3.2 KB of .text and .rodata. The Flipper's app loader has to place the
 * whole image in one contiguous allocation, and FlipDeFlock is large enough that
 * users on heavier firmware were being refused with "Not enough RAM to run the
 * app" (issue #5). Every kilobyte off the image is a kilobyte less fragmentation
 * sensitivity at load.
 *
 * Every caller here passes either a latitude or a compass heading converted to
 * radians, so |x| <= 2*pi + rounding. That needs no Payne-Hanek: one subtract of
 * a multiple of 2*pi is exact enough, and a Taylor series through x^11 on the
 * folded [-pi/2, pi/2] interval lands well inside float precision.
 *
 * Accuracy is asserted against the host's libm in test/test_fast_trig.c across
 * the real input domain, so a regression here fails the build rather than
 * quietly moving a distance calculation.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * sin(x) for finite x. Accurate to within ~1e-7 absolute over |x| <= 2*pi, which
 * is every value this app produces; accuracy degrades gracefully (never wildly)
 * for larger |x|. Returns 0 for NaN/infinity rather than propagating -- callers
 * feed it GPS-derived values that are already NaN-checked, and a quiet 0 is
 * safer on a screen than a NaN that poisons a pixel coordinate.
 */
float trig_sinf(float x);

/** cos(x); same domain and accuracy contract as trig_sinf(). */
float trig_cosf(float x);

#ifdef __cplusplus
}
#endif
