/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_KEYGEN_INTERNAL_H_
#define TC_RSA_KEYGEN_INTERNAL_H_
#include "rsa_private_internal.h"

enum {
  TC_RSA_KEYGEN_EMPTY = 0,
  TC_RSA_KEYGEN_P_NEW,
  TC_RSA_KEYGEN_P_PREPARE,
  TC_RSA_KEYGEN_P_ROUND,
  TC_RSA_KEYGEN_Q_NEW,
  TC_RSA_KEYGEN_Q_PREPARE,
  TC_RSA_KEYGEN_Q_ROUND
};

static inline uint32_t tc_rsa_keygen_mod_u32(const uint8_t* value,
    size_t length, uint32_t modulus)
{
  uint32_t remainder = 0;
  for (size_t i = 0; i < length; ++i)
    remainder = (uint32_t)(((uint64_t)remainder * 256u + value[i]) % modulus);
  return remainder;
}

static inline uint32_t tc_rsa_keygen_gcd(uint32_t a, uint32_t b)
{
  while (b) {
    uint32_t remainder = a % b;
    a = b; b = remainder;
  }
  return a;
}

/* Cheap public-candidate filtering avoids almost all expensive strong tests. */
static inline int tc_rsa_keygen_candidate_filter(const uint8_t* candidate,
    size_t length)
{
  static const uint8_t primes[] = {
    3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,
    97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,
    181,191,193,197,199,211,223,227,229,233,239,241,251
  };
  for (size_t i = 0; i < sizeof primes; ++i)
    if (tc_rsa_keygen_mod_u32(candidate,length,primes[i]) == 0) return 0;
  uint32_t residue = tc_rsa_keygen_mod_u32(candidate,length,
      TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  residue = residue ? residue - 1 : TC_RSA_KEYGEN_PUBLIC_EXPONENT - 1;
  return tc_rsa_keygen_gcd(residue,TC_RSA_KEYGEN_PUBLIC_EXPONENT) == 1;
}

static inline int tc_rsa_keygen_far_apart(const uint8_t* p, const uint8_t* q,
    size_t length, tc_mp_word* scratch)
{
  const size_t n = length / sizeof(tc_mp_word);
  tc_mp_word* left = scratch;
  tc_mp_word* right = left + n;
  tc_mp_word* difference = right + n;
  tc_mp_from_be(left,p,length); tc_mp_from_be(right,q,length);
  if (tc_mp_subtract(difference,left,right,n))
    tc_mp_subtract(difference,right,left,n);
  /* FIPS 186-5 B.3.1 requires |p-q| > 2^(prime_bits-100). */
  const size_t threshold_bit = length * 8 - 100;
  const size_t word = threshold_bit / TC_MP_WORD_BITS;
  const unsigned bit = (unsigned)(threshold_bit % TC_MP_WORD_BITS);
  for (size_t i = n; i-- > word + 1;)
    if (difference[i]) return 1;
  if (difference[word] > ((tc_mp_word)1u << bit)) return 1;
  if (difference[word] < ((tc_mp_word)1u << bit)) return 0;
  for (size_t i = 0; i < word; ++i)
    if (difference[i]) return 1;
  return 0;
}

static inline uint32_t tc_rsa_keygen_inverse_65537(uint32_t value)
{
  uint32_t result = 1;
  /* 65537 is prime. A fixed addition chain computes value^(65537-2). */
  for (unsigned bit = 0; bit < 16; ++bit) {
    result = (uint32_t)((uint64_t)result * result % TC_RSA_KEYGEN_PUBLIC_EXPONENT);
    result = (uint32_t)((uint64_t)result * value % TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  }
  return result;
}

/* Derive n and d from retained prime bytes. Scratch has at least 10h+2 limbs,
 * where h is the prime width. Results remain in scratch until publication. */
static inline void tc_rsa_keygen_derive(const uint8_t* p_bytes,
    const uint8_t* q_bytes, size_t prime_length, tc_mp_word* scratch,
    tc_mp_word** modulus_out, tc_mp_word** d_out)
{
  const size_t h = prime_length / sizeof(tc_mp_word), n = 2 * h;
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + h;
  tc_mp_word* modulus = q + h;
  tc_mp_word* phi = modulus + n;
  tc_mp_word* numerator = phi + n;
  const size_t extra = (16 + TC_MP_WORD_BITS - 1) / TC_MP_WORD_BITS;
  tc_mp_word* d = numerator + n + extra;
  tc_mp_from_be(p,p_bytes,prime_length); tc_mp_from_be(q,q_bytes,prime_length);
  tc_mp_multiply(modulus,p,q,h);
  --p[0]; --q[0];
  tc_mp_multiply(phi,p,q,h);
  uint32_t residue = 0;
  for (size_t i = n; i; --i)
    residue = (uint32_t)(((uint64_t)residue *
        ((uint64_t)1u << TC_MP_WORD_BITS) + phi[i - 1]) %
        TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  const uint32_t inverse = tc_rsa_keygen_inverse_65537(residue);
  const uint32_t multiplier = TC_RSA_KEYGEN_PUBLIC_EXPONENT - inverse;
  uint64_t carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (uint64_t)phi[i] * multiplier;
    numerator[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  for (size_t i = 0; i < extra; ++i) {
    numerator[n + i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  carry = 1;
  for (size_t i = 0; i < n + extra; ++i) {
    carry += numerator[i];
    numerator[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  uint32_t remainder = 0;
  for (size_t i = n + extra; i; --i) {
    uint64_t dividend = ((uint64_t)remainder << TC_MP_WORD_BITS) | numerator[i - 1];
    if (i - 1 < n) d[i - 1] = (tc_mp_word)(dividend / TC_RSA_KEYGEN_PUBLIC_EXPONENT);
    remainder = (uint32_t)(dividend % TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  }
  *modulus_out = modulus; *d_out = d;
}
#endif
