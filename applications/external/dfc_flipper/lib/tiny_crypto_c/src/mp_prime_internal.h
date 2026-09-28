/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_MP_PRIME_INTERNAL_H_
#define TC_MP_PRIME_INTERNAL_H_
#include "mp_internal.h"

/* Candidate setup for FIPS 186-5 B.3.1. Odd p >3; n >0 and n*word_bits
 * fits size_t. Scratch has 10n+2 limbs, disjoint from p. Its first four rows
 * retain R^2, the odd exponent, Montgomery one and minus one across rounds.
 * Return the trailing-zero count of p-1. Caller bounds work and wipes scratch. */
static inline size_t tc_mp_miller_rabin_prepare(const tc_mp_word* p,
    size_t n, tc_mp_word* scratch)
{
  const size_t length = n * sizeof(tc_mp_word), bits = n * TC_MP_WORD_BITS;
  tc_mp_word* odd = scratch;
  uint8_t* exponent = (uint8_t*)(odd + n);
  tc_mp_word* one = odd + 2 * n;
  tc_mp_word* minus_one = one + n;
  tc_mp_word* value = minus_one + n;
  tc_mp_word* encoded_base = value + n;
  tc_mp_word* temporary = encoded_base + n;
  tc_mp_word* reduced = temporary + n;
  tc_mp_word* product = reduced + n;
  memcpy(odd,p,length); --odd[0];
  size_t twos = 0;
  unsigned counting = 1;
  for (size_t i = 0; i < n; ++i) {
    for (unsigned bit = 0; bit < TC_MP_WORD_BITS; ++bit) {
      counting &= ((odd[i] >> bit) & 1u) ^ 1u;
      twos += counting;
    }
  }
  /* Fixed passes keep the shift schedule independent of trailing zero bits. */
  for (size_t step = 0; step < bits; ++step) {
    memcpy(temporary,odd,length); tc_mp_shift_right(temporary,n,0);
    tc_mp_select(odd,temporary,odd,(tc_mp_word)(0u - (unsigned)(step < twos)),n);
  }
  tc_mp_to_be(exponent,odd,length);
  const tc_mp_word factor = tc_mp_montgomery_factor(p[0]);
  /* The odd-part buffer can now hold R^2. */
  tc_mp_montgomery_r2(odd,p,n,reduced);
  memset(one,0,length); one[0] = 1;
  tc_mp_montgomery(one,one,odd,p,n,factor,product,reduced);
  tc_mp_subtract(minus_one,p,one,n);
  return twos;
}

/* Base satisfies 1 < base < p-1. Reuse the unchanged candidate and prepared
 * scratch; rows four onward are temporary. Return 1 for a passing round. */
static inline int tc_mp_miller_rabin_round(const tc_mp_word* p, const tc_mp_word* base,
    size_t n, size_t twos, tc_mp_word* scratch)
{
  const size_t length = n * sizeof(tc_mp_word), bits = n * TC_MP_WORD_BITS;
  const tc_mp_word* r2 = scratch;
  const uint8_t* exponent = (const uint8_t*)(scratch + n);
  const tc_mp_word* one = scratch + 2 * n;
  const tc_mp_word* minus_one = one + n;
  tc_mp_word* value = scratch + 4 * n;
  tc_mp_word* encoded_base = value + n;
  tc_mp_word* temporary = encoded_base + n;
  tc_mp_word* reduced = temporary + n;
  tc_mp_word* product = reduced + n;
  const tc_mp_word factor = tc_mp_montgomery_factor(p[0]);
  tc_mp_montgomery(encoded_base,base,r2,p,n,factor,product,reduced);
  tc_mp_power(value,encoded_base,exponent,length,one,p,n,factor,temporary,product,reduced);
  unsigned passed = (unsigned)(tc_mp_equal(value,one,n) | tc_mp_equal(value,minus_one,n));
  /* Only the first twos-1 squares contribute to the round's result. */
  for (size_t step = 1; step < bits; ++step) {
    tc_mp_montgomery(value,value,value,p,n,factor,product,reduced);
    passed |= (unsigned)(step < twos) & (unsigned)tc_mp_equal(value,minus_one,n);
  }
  return (int)passed;
}

static inline int tc_mp_miller_rabin(const tc_mp_word* p, const tc_mp_word* base,
    size_t n, tc_mp_word* scratch)
{
  size_t twos = tc_mp_miller_rabin_prepare(p,n,scratch);
  return tc_mp_miller_rabin_round(p,base,n,twos,scratch);
}
#endif
