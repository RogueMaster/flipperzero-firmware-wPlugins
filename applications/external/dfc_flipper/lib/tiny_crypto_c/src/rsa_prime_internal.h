/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_PRIME_INTERNAL_H_
#define TC_RSA_PRIME_INTERNAL_H_
#include <tiny_crypto/rsa.h>
#include "mp_prime_internal.h"

/* Test an odd candidate using independent uniformly random bases.
 * length selects the working width, a limb multiple at most 384 bytes.
 * The candidate fits that width; leading zero bytes are accepted.
 * Caller selects rounds and bounds total RNG requests with max_attempts.
 * Inputs, scratch (12n+2 limbs), work and RNG state are disjoint.
 * Work counts limb scans, modular operations and RNG requests. The exact prime
 * three returns OK without consuming randomness, work or scratch. */
static inline TC_RSA_result tc_rsa_probable_prime_magnitude(TC_bytes candidate,
    size_t length, size_t rounds, TC_random_fn random, void* random_context,
    size_t max_attempts, tc_mp_word* scratch, size_t scratch_words, uint32_t* work)
{
  if (!candidate.data || !random || !scratch || !work || !rounds)
    return TC_RSA_ARGUMENT;
  if (!length || length > 384 || length % sizeof(tc_mp_word) ||
      !candidate.length || candidate.length > length ||
      !(candidate.data[candidate.length - 1] & 1u))
    return TC_RSA_INVALID;
  unsigned above_three = candidate.data[candidate.length - 1] & ~3u;
  for (size_t i = 0; i + 1 < candidate.length; ++i) above_three |= candidate.data[i];
  if (!above_three) return candidate.data[candidate.length - 1] == 3 ? TC_RSA_OK : TC_RSA_INVALID;
  const size_t n = length / sizeof(tc_mp_word), required = 12 * n + 2;
  const uint32_t setup_work = UINT32_C(24) * (uint32_t)length + 3;
  const uint32_t round_work = UINT32_C(24) * (uint32_t)length + 1;
  if (scratch_words < required || max_attempts < rounds || *work < setup_work + round_work + 1)
    return TC_RSA_LIMIT;
  tc_mp_word* p = scratch;
  tc_mp_word* base = p + n;
  tc_mp_word* arena = base + n;
  tc_mp_word* last = arena + 5 * n;
  tc_mp_word* temporary = last + n;
  tc_mp_from_be_padded(p,candidate.data,candidate.length,length);
  *work -= setup_work;
  const size_t twos = tc_mp_miller_rabin_prepare(p,n,arena);
  TC_RSA_result status = TC_RSA_LIMIT;
  size_t completed = 0;
  for (size_t attempt = 0; attempt < max_attempts; ++attempt) {
    if (*work < round_work + 1) break;
    --*work;
    if (random(random_context,(uint8_t*)temporary,length) != TC_OK) {
      status = TC_RSA_ERROR; break;
    }
    /* Mask to the candidate's bit width before rejection sampling. */
    unsigned seen = 0;
    uint8_t* bytes = (uint8_t*)temporary;
    const size_t padding = length - candidate.length;
    for (size_t i = 0; i < length; ++i) {
      const unsigned value = i < padding ? 0 : candidate.data[i - padding];
      unsigned mask = value;
      mask |= mask >> 1; mask |= mask >> 2; mask |= mask >> 4;
      mask |= 0u - seen;
      bytes[i] &= (uint8_t)mask;
      seen |= (unsigned)(value != 0);
    }
    tc_mp_from_be(base,(const uint8_t*)temporary,length);
    memcpy(last,p,length); --last[0];
    const tc_mp_word below_last = tc_mp_subtract(temporary,base,last,n);
    tc_mp_word above_one = (tc_mp_word)(base[0] & ~1u);
    for (size_t i = 1; i < n; ++i) above_one |= base[i];
    if (!below_last || !above_one) continue;
    *work -= round_work;
    if (!tc_mp_miller_rabin_round(p,base,n,twos,arena)) {
      status = TC_RSA_INVALID; break;
    }
    if (++completed == rounds) { status = TC_RSA_OK; break; }
  }
  TC_secure_zero(scratch,required * sizeof *scratch);
  return status;
}
static inline TC_RSA_result tc_rsa_probable_prime(const uint8_t* candidate,
    size_t length, size_t rounds, TC_random_fn random, void* random_context,
    size_t max_attempts, tc_mp_word* scratch, size_t scratch_words, uint32_t* work)
{
  return tc_rsa_probable_prime_magnitude((TC_bytes){candidate,length},length,rounds,
      random,random_context,max_attempts,scratch,scratch_words,work);
}
#endif
