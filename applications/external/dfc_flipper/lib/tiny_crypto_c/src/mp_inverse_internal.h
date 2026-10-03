/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_MP_INVERSE_INTERNAL_H_
#define TC_MP_INVERSE_INTERNAL_H_
#include "mp_internal.h"

/* a,b < p. out may equal a or b. */
static inline void tc_mp_sub_mod(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                 const tc_mp_word* p, size_t n)
{
  const tc_mp_word borrow = tc_mp_subtract(out, a, b, n);
  const tc_mp_word mask = (tc_mp_word)tc_internal_mask_barrier((tc_mp_word)(0u - borrow));
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)out[i] + (p[i] & mask);
    out[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
}

/* For odd p and a < p, (a + (a mod 2)*p)/2 is division by two modulo p. */
static inline void tc_mp_half_mod(tc_mp_word* a, const tc_mp_word* p, size_t n)
{
  const tc_mp_word mask = (tc_mp_word)tc_internal_mask_barrier((tc_mp_word)(0u - (a[0] & 1u)));
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)a[i] + (p[i] & mask);
    a[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  tc_mp_shift_right(a, n, (tc_mp_word)carry);
}

static inline void tc_mp_swap(tc_mp_word* a, tc_mp_word* b, tc_mp_word mask, size_t n)
{
  mask = (tc_mp_word)tc_internal_mask_barrier(mask);
  for (size_t i = 0; i < n; ++i) {
    const tc_mp_word difference = (a[i] ^ b[i]) & mask;
    a[i] ^= difference;
    b[i] ^= difference;
  }
}

/* Binary extended GCD for odd p > 1 and a < p. n > 0, and 2*n*word_bits fits
 * size_t. Every iteration halves one nonzero GCD operand until one reaches zero.
 * Their combined bit lengths bound the iteration count. r*a=u and s*a=v mod p.
 * Inputs, out and scratch (6n limbs) are disjoint. Failure preserves out.
 * The caller wipes scratch, which contains input-dependent coefficients. */
static inline int tc_mp_inverse(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* p, size_t n,
                                tc_mp_word* scratch)
{
  tc_mp_word* u = scratch;
  tc_mp_word* v = u + n;
  tc_mp_word* r = v + n;
  tc_mp_word* s = r + n;
  tc_mp_word* difference = s + n;
  tc_mp_word* coefficient = difference + n;
  memcpy(u, a, n * sizeof *u);
  memcpy(v, p, n * sizeof *v);
  memset(r, 0, n * sizeof *r);
  r[0] = 1;
  memset(s, 0, n * sizeof *s);
  for (size_t step = 0; step < 2 * n * TC_MP_WORD_BITS; ++step) {
    const tc_mp_word borrow = tc_mp_subtract(difference, u, v, n);
    const unsigned u_odd = u[0] & 1u, v_odd = v[0] & 1u;
    const tc_mp_word swap = (tc_mp_word)(0u - (u_odd & ((v_odd ^ 1u) | borrow)));
    /* Put an even operand first. For two odd operands, put the larger first. */
    tc_mp_swap(u, v, swap, n);
    tc_mp_swap(r, s, swap, n);
    const tc_mp_word odd = (tc_mp_word)(0u - (u[0] & 1u));
    tc_mp_subtract(difference, u, v, n);
    tc_mp_sub_mod(coefficient, r, s, p, n);
    tc_mp_select(u, difference, u, odd, n);
    tc_mp_select(r, coefficient, r, odd, n);
    tc_mp_shift_right(u, n, 0);
    tc_mp_half_mod(r, p, n);
  }
  tc_mp_word u_difference = u[0] ^ 1u, v_difference = v[0] ^ 1u;
  for (size_t i = 1; i < n; ++i) {
    u_difference |= u[i];
    v_difference |= v[i];
  }
  if (u_difference && v_difference)
    return 0;
  tc_mp_select(out, r, s, (tc_mp_word)(0u - (unsigned)(u_difference == 0)), n);
  return 1;
}
/* Greatest common divisor of nonzero a and b with a fixed iteration count
 * (binary GCD after Stein). out has n limbs. scratch has 3n limbs and is
 * disjoint from the inputs and out; the caller wipes it. Loop bounds and
 * memory access depend only on n. */
static inline void tc_mp_gcd(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b, size_t n,
                             tc_mp_word* scratch)
{
  const size_t bits = n * TC_MP_WORD_BITS;
  tc_mp_word* x = scratch;
  tc_mp_word* y = x + n;
  tc_mp_word* difference = y + n;
  memcpy(x, a, n * sizeof *x);
  memcpy(y, b, n * sizeof *y);
  /* Remove the common factor 2^k, counting k without branching. */
  size_t shift = 0;
  for (size_t i = 0; i < bits; ++i) {
    const tc_mp_word both_even = (tc_mp_word)(((x[0] | y[0]) & 1u) ^ 1u);
    const tc_mp_word mask = (tc_mp_word)(0u - both_even);
    memcpy(difference, x, n * sizeof *x);
    tc_mp_shift_right(difference, n, 0);
    tc_mp_select(x, difference, x, mask, n);
    memcpy(difference, y, n * sizeof *y);
    tc_mp_shift_right(difference, n, 0);
    tc_mp_select(y, difference, y, mask, n);
    shift += both_even;
  }
  /* Keep x odd: at least one operand is odd now. */
  tc_mp_swap(x, y, (tc_mp_word)(0u - (tc_mp_word)((x[0] & 1u) ^ 1u)), n);
  /* Each step clears one bit of x + y, so 2 * bits steps reach y = 0. */
  for (size_t i = 0; i < 2 * bits; ++i) {
    const tc_mp_word odd = (tc_mp_word)(y[0] & 1u);
    const tc_mp_word below = tc_mp_subtract(difference, y, x, n);
    /* For odd y below x, swap so that y - x is nonnegative. */
    tc_mp_swap(x, y, (tc_mp_word)(0u - (odd & below)), n);
    tc_mp_subtract(difference, y, x, n);
    tc_mp_select(y, difference, y, (tc_mp_word)(0u - odd), n);
    tc_mp_shift_right(y, n, 0);
  }
  /* Restore 2^k. shift <= bits, and every step is performed. */
  for (size_t i = 0; i < bits; ++i) {
    const tc_mp_word apply = (tc_mp_word)(0u - (tc_mp_word)(i < shift));
    tc_mp_wide carry = 0;
    for (size_t j = 0; j < n; ++j) {
      carry += (tc_mp_wide)x[j] * 2;
      difference[j] = (tc_mp_word)carry;
      carry >>= TC_MP_WORD_BITS;
    }
    tc_mp_select(x, difference, x, apply, n);
  }
  memcpy(out, x, n * sizeof *out);
}
#endif
