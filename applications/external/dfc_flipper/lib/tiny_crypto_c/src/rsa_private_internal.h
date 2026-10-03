/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_PRIVATE_INTERNAL_H_
#define TC_RSA_PRIVATE_INTERNAL_H_
#include <tiny_crypto/rsa.h>
#include "rsa_internal.h"
#include "mp_inverse_internal.h"
#include "rsa_prime_internal.h"

/* Sample 1 < blind < modulus with an inverse. Modulus, blind, inverse and
 * arena are separate. Arena has at least 6n limbs, the scratch size of
 * tc_mp_inverse. Its first n limbs hold the RNG draw and its next n limbs are
 * the range-check scratch until the inverse reuses the whole arena. */
static inline TC_RSA_result tc_rsa_sample_blinding(const tc_mp_word* modulus, size_t n,
                                                   tc_mp_word* blind, tc_mp_word* inverse,
                                                   tc_mp_word* arena, const tc_rsa_random* rng,
                                                   uint32_t* work)
{
  const TC_random_fn random = rng->source.fill;
  void* const random_context = rng->source.context;
  const size_t max_attempts = rng->attempts;
  const size_t length = n * sizeof(tc_mp_word);
  const uint32_t attempt_work = tc_rsa_blinding_cost(length);
  tc_mp_word* reduced = arena + n;
  for (size_t attempt = 0; attempt < max_attempts; ++attempt) {
    if (*work < attempt_work)
      return TC_RSA_LIMIT;
    *work -= attempt_work;
    if (random(random_context, (uint8_t*)arena, length) != TC_OK)
      return TC_RSA_ERROR;
    tc_mp_from_be(blind, (const uint8_t*)arena, length);
    const tc_mp_word below_modulus = tc_mp_subtract(reduced, blind, modulus, n);
    tc_mp_word above_one = (tc_mp_word)(blind[0] & ~1u);
    for (size_t i = 1; i < n; ++i)
      above_one |= blind[i];
    if (below_modulus && above_one && tc_mp_inverse(inverse, blind, modulus, n, arena))
      return TC_RSA_OK;
  }
  return TC_RSA_LIMIT;
}

static inline int tc_rsa_private_exponent_check(const tc_mp_word* d, const tc_mp_word* modulus,
                                                size_t n, tc_mp_word* temporary)
{
  tc_mp_word nonzero = 0;
  for (size_t i = 0; i < n; ++i)
    nonzero |= d[i];
  const tc_mp_word below_modulus = tc_mp_subtract(temporary, d, modulus, n);
  return (nonzero != 0) & (d[0] & 1u) & below_modulus;
}

/* FIPS 186-5 A.1.1 2(d): |p - q| > 2^(k-100) for k-bit primes held in h
 * limbs. scratch has 3h limbs. The comparison runs in time that depends only
 * on h. */
static inline int tc_rsa_factors_far_apart(const tc_mp_word* p, const tc_mp_word* q, size_t h,
                                           tc_mp_word* scratch)
{
  const size_t threshold = h * TC_MP_WORD_BITS - 100;
  tc_mp_word* difference = scratch;
  tc_mp_word* other = difference + h;
  tc_mp_word* bound = other + h;
  const tc_mp_word below = tc_mp_subtract(difference, p, q, h);
  tc_mp_subtract(other, q, p, h);
  tc_mp_select(difference, other, difference, (tc_mp_word)(0u - below), h);
  /* |p - q| > 2^threshold exactly when |p - q| - (2^threshold + 1) does not borrow. */
  memset(bound, 0, h * sizeof *bound);
  bound[threshold / TC_MP_WORD_BITS] =
      (tc_mp_word)((tc_mp_word)1u << (threshold % TC_MP_WORD_BITS));
  bound[0] |= 1u;
  return tc_mp_subtract(other, difference, bound, h) == 0;
}

/* FIPS 186-5 A.1.3: each prime has half the modulus bit length. */
static inline int tc_rsa_factor_has_half_bits(TC_bytes factor, size_t modulus_bytes)
{
  size_t first = 0;
  while (first < factor.length && factor.data[first] == 0)
    ++first;
  return first < factor.length && factor.length - first == modulus_bytes / 2 &&
         (factor.data[first] & 0x80u) != 0;
}

/* FIPS 186-5 A.1.1 1(b): e odd with 2^16 < e < 2^256. Leading zero octets
 * are ignored. */
static inline int tc_rsa_exponent_fips(const uint8_t* exponent, size_t length)
{
  while (length && !*exponent) {
    ++exponent;
    --length;
  }
  /* Three or more significant octets mean e >= 2^16, and an odd e is never
   * 2^16 itself. At most 32 octets mean e < 2^256. */
  return length >= 3 && length <= 32 && (exponent[length - 1] & 1u);
}

/* Check the two-prime component equations and the FIPS 186-5 A.1.1 key
 * criteria: n = p q with distinct odd p, q of half the modulus width;
 * sqrt(2) 2^(k-1) <= p, q for k = nlen/2; |p - q| > 2^(k-100);
 * 2^k < d < LCM(p-1, q-1); and e d = 1 mod LCM(p-1, q-1). exponent_policy
 * selects the e range. Primality testing is separate. The key passed its
 * entry checks, and its magnitudes fit length bytes. All ranges and work are
 * disjoint. Scratch has 12n limbs and is wiped after use. Arithmetic on
 * secret values runs in time that depends only on the key size. Work counts
 * products and bit-serial reduction passes. */
static inline TC_RSA_result
tc_rsa_private_magnitudes_consistent(const tc_rsa_private_view* key,
                                     TC_RSA_exponent_policy exponent_policy, tc_mp_scratch area,
                                     uint32_t* work)
{
  const uint8_t* const modulus = key->public_key->modulus.data;
  const size_t length = key->public_key->modulus.length;
  const uint8_t* const exponent = key->public_key->exponent.data;
  const size_t exponent_length = key->public_key->exponent.length;
  const TC_bytes d_bytes = key->d, p_bytes = key->p, q_bytes = key->q;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  TC_RSA_result status;
  if (exponent_policy == TC_RSA_EXPONENT_FIPS && !tc_rsa_exponent_fips(exponent, exponent_length))
    return TC_RSA_INVALID;
  const size_t n = length / sizeof(tc_mp_word), h = n / 2, required = 12 * n;
  const size_t cost = 48 * length + 2;
  if (scratch_words < required || *work < cost)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost; /* cost <= *work, checked above. */
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + n;
  tc_mp_word* d = q + n;
  tc_mp_word* e = d + n;
  tc_mp_word* product = e + n; /* 2n limbs */
  tc_mp_word* remainder = product + 2 * n;
  tc_mp_word* temporary = remainder + n;
  tc_mp_word* lambda = temporary + n;
  tc_mp_word* gcd = lambda + n;
  tc_mp_word* extra = gcd + n; /* 2n limbs */
  tc_mp_from_be_padded(p, p_bytes.data, p_bytes.length, length);
  tc_mp_from_be_padded(q, q_bytes.data, q_bytes.length, length);
  tc_mp_from_be_padded(d, d_bytes.data, d_bytes.length, length);
  tc_mp_from_be(remainder, modulus, length);
  status = TC_RSA_INVALID;
  /* Public shape checks: each factor has exactly half the modulus width. */
  if (!tc_rsa_factor_has_half_bits(p_bytes, length) ||
      !tc_rsa_factor_has_half_bits(q_bytes, length))
    goto cleanup;
  if (!tc_rsa_private_exponent_check(d, remainder, n, temporary))
    goto cleanup;
  tc_mp_word p_above_one = (tc_mp_word)(p[0] & ~1u);
  tc_mp_word q_above_one = (tc_mp_word)(q[0] & ~1u);
  tc_mp_word distinct = p[0] ^ q[0];
  for (size_t i = 1; i < n; ++i) {
    p_above_one |= p[i];
    q_above_one |= q[i];
    distinct |= p[i] ^ q[i];
  }
  if (!(p[0] & q[0] & 1u) || !p_above_one || !q_above_one || !distinct)
    goto cleanup;
  tc_mp_multiply(product, p, q, n);
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i)
    difference |= (product[i] ^ remainder[i]) | product[n + i];
  if (difference)
    goto cleanup;
  /* The factors fit h limbs. Accumulate the remaining criteria as masks. */
  tc_mp_word failed = 0;
  const tc_mp_word* factors[] = {p, q};
  for (size_t i = 0; i < 2; ++i) {
    /* p >= sqrt(2) 2^(k-1) exactly when p^2 >= 2^(2k-1), the top bit of 2k bits. */
    tc_mp_multiply(product, factors[i], factors[i], h);
    failed |= (tc_mp_word)(((product[n - 1] >> (TC_MP_WORD_BITS - 1)) & 1u) ^ 1u);
  }
  failed |= (tc_mp_word)!tc_rsa_factors_far_apart(p, q, h, extra);
  /* Both factors are odd, so the decrements need no borrow. */
  --p[0];
  --q[0];
  tc_mp_multiply(product, p, q, h); /* phi, n limbs */
  memset(gcd, 0, n * sizeof *gcd);
  tc_mp_gcd(gcd, p, q, h, extra);
  tc_mp_divide_words(lambda, remainder, product, n, gcd, n, temporary);
  /* 2^k < d: d - (2^k + 1) does not borrow. d < lambda: d - lambda borrows. */
  memset(gcd, 0, n * sizeof *gcd);
  gcd[0] = 1;
  gcd[h] = 1;
  failed |= tc_mp_subtract(temporary, d, gcd, n);
  failed |= (tc_mp_word)(tc_mp_subtract(temporary, d, lambda, n) ^ 1u);
  /* e d = 1 mod lambda. */
  tc_mp_from_be_padded(e, exponent, exponent_length, length);
  tc_mp_multiply(product, d, e, n);
  tc_mp_divide_words(NULL, remainder, product, 2 * n, lambda, n, temporary);
  remainder[0] ^= 1u;
  failed |= (tc_mp_word)~tc_mp_zero_mask(remainder, n);
  if (!failed)
    status = TC_RSA_OK;
cleanup:
  TC_secure_zero(scratch, required * sizeof *scratch);
  return status;
}

/* Full work for a length-byte modulus, matching TC_RSA_VALIDATE_WORK: the
 * component checks, then for each half-width factor one Miller-Rabin setup,
 * rounds rounds and attempts RNG requests. Returns 0 when the cost exceeds
 * a uint32_t budget. */
static inline int tc_rsa_validation_cost(size_t length, size_t rounds, size_t attempts,
                                         uint32_t* cost)
{
  const uint32_t component = UINT32_C(48) * (uint32_t)length + 2;
  const uint32_t setup = UINT32_C(24) * (uint32_t)(length / 2) + 3;
  const uint32_t round = UINT32_C(24) * (uint32_t)(length / 2) + 1;
  if (rounds > (UINT32_MAX - setup) / round)
    return 0;
  uint32_t factor = setup + (uint32_t)rounds * round;
  if (attempts > UINT32_MAX - factor)
    return 0;
  factor += (uint32_t)attempts;
  if (factor > (UINT32_MAX - component) / 2)
    return 0;
  *cost = component + 2 * factor;
  return 1;
}

/* Validate two-prime components and test each factor with rounds
 * Miller-Rabin rounds. The key passed its entry checks. max_attempts applies
 * per factor. Storage ownership matches the component check. Scratch needs
 * TC_RSA_VALIDATE_WORKSPACE_WORDS limbs. The caller's assurance policy
 * supplies rounds and an independent uniform RNG. The full cost from
 * tc_rsa_validation_cost is checked before any arithmetic, so a short
 * budget returns LIMIT with work, scratch and the RNG unchanged. */
static inline TC_RSA_result tc_rsa_private_magnitudes_check(const tc_rsa_private_view* key,
                                                            TC_RSA_exponent_policy exponent_policy,
                                                            size_t rounds, const tc_rsa_random* rng,
                                                            tc_mp_scratch area, uint32_t* work)
{
  const size_t length = key->public_key->modulus.length;
  const size_t max_attempts = rng->attempts;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  const size_t required = TC_RSA_VALIDATE_WORKSPACE_WORDS(length * 8);
  uint32_t full_cost;
  if (scratch_words < required || max_attempts < rounds ||
      !tc_rsa_validation_cost(length, rounds, max_attempts, &full_cost) || *work < full_cost)
    return TC_RSA_LIMIT;
  TC_RSA_result status = tc_rsa_private_magnitudes_consistent(key, exponent_policy, area, work);
  /* The entry checks stripped each factor to half the modulus width, so
   * Miller-Rabin runs at that width. */
  const TC_bytes factors[] = {key->p, key->q};
  for (size_t i = 0; status == TC_RSA_OK && i < 2; ++i)
    status = tc_rsa_probable_prime_magnitude(factors[i], length / 2, rounds, rng, area, work);
  TC_secure_zero(scratch, required * sizeof *scratch);
  return status;
}

/* Full-width private operation for an already validated RSA key. The public
 * key passed tc_rsa_public_key_check, and d is nonempty and fits length
 * bytes. Input and output have length bytes. Input, output, scratch and work
 * ranges are disjoint. The caller checks ranges and random-source context
 * ownership. Scratch uses TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS limbs and is
 * wiped after use. Work is tc_rsa_private_base_cost plus
 * tc_rsa_blinding_cost per RNG request. max_attempts bounds rejection
 * sampling. Output changes only after verification. */
static inline TC_RSA_result tc_rsa_private_operation_magnitude(const TC_RSA_public_key* key,
                                                               TC_bytes d, const uint8_t* input,
                                                               uint8_t* output,
                                                               const tc_rsa_random* rng,
                                                               tc_mp_scratch area, uint32_t* work)
{
  const uint8_t* const modulus = key->modulus.data;
  const size_t length = key->modulus.length;
  const uint8_t* const exponent = key->exponent.data;
  const size_t exponent_length = key->exponent.length;
  const size_t max_attempts = rng->attempts;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  TC_RSA_result status;
  const size_t n = length / sizeof(tc_mp_word);
  const size_t required = TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8);
  const uint32_t operations = tc_rsa_private_base_cost(length, exponent_length, 0);
  if (!max_attempts || scratch_words < required || *work < operations)
    return TC_RSA_LIMIT;
  *work -= operations;
  tc_mp_word* p = scratch;
  tc_mp_word* c = p + n;
  tc_mp_word* blind = c + n;
  tc_mp_word* inverse = blind + n;
  tc_mp_word* r2 = inverse + n;
  tc_mp_word* one = r2 + n;
  tc_mp_word* result = one + n;
  tc_mp_word* arena = result + n;
  tc_mp_word* temporary = arena;
  tc_mp_word* reduced = temporary + n;
  tc_mp_word* product = reduced + n;
  tc_mp_from_be(p, modulus, length);
  tc_mp_from_be(c, input, length);
  tc_mp_from_be_padded(one, d.data, d.length, length);
  status = TC_RSA_INVALID;
  if (!tc_rsa_private_exponent_check(one, p, n, temporary) || !tc_mp_subtract(temporary, c, p, n))
    goto cleanup;
  status = tc_rsa_sample_blinding(p, n, blind, inverse, arena, rng, work);
  if (status != TC_RSA_OK)
    goto cleanup;
  const tc_mp_word factor = tc_mp_montgomery_factor(p[0]);
  tc_mp_montgomery_r2(r2, p, n, reduced);
  memset(one, 0, length);
  one[0] = 1;
  const tc_mp_modulus field = {p, n, factor, product, reduced};
  tc_mp_montgomery(one, one, r2, &field);
  tc_mp_montgomery(blind, blind, r2, &field);
  tc_mp_montgomery(inverse, inverse, r2, &field);
  tc_mp_montgomery(c, c, r2, &field);
  tc_mp_power(result, blind, (TC_bytes){exponent, exponent_length}, one, &field, temporary);
  tc_mp_montgomery(blind, c, result, &field);
  tc_mp_power_padded(result, blind, d, length, one, &field, temporary);
  tc_mp_montgomery(result, result, inverse, &field);
  /* Verify in Montgomery form before converting or publishing the result. */
  tc_mp_power(blind, result, (TC_bytes){exponent, exponent_length}, one, &field, temporary);
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i)
    difference |= blind[i] ^ c[i];
  if (difference) {
    status = TC_RSA_ERROR;
    goto cleanup;
  }
  memset(one, 0, length);
  one[0] = 1;
  tc_mp_montgomery(result, result, one, &field);
  tc_mp_to_be(output, result, length);
cleanup:
  TC_secure_zero(scratch, required * sizeof *scratch);
  return status;
}

/* d/p/q have passed private-key validation. Check dP=d mod(p-1),
 * dQ=d mod(q-1), and the least positive q^-1 mod p (RFC 8017 section 3.2).
 * key has CRT fields, and every magnitude fits the supported modulus length.
 * Inputs, work and scratch are disjoint. Scratch needs
 * TC_RSA_CRT_WORKSPACE_WORDS limbs and is wiped after use. */
static inline TC_RSA_result tc_rsa_crt_consistent(const tc_rsa_private_view* key,
                                                  tc_mp_scratch area, uint32_t* work)
{
  const size_t length = key->public_key->modulus.length;
  const TC_bytes d_bytes = key->d, p_bytes = key->p, q_bytes = key->q;
  const TC_bytes dp_bytes = key->dp, dq_bytes = key->dq, inverse_bytes = key->q_inverse;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  const size_t n = length / sizeof(tc_mp_word);
  const size_t required = TC_RSA_CRT_WORKSPACE_WORDS(length * 8);
  const size_t cost = 32 * length + 1;
  if (scratch_words < required || *work < cost)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost; /* cost <= *work, checked above. */
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + n;
  tc_mp_word* d = q + n;
  tc_mp_word* value = d + n;
  tc_mp_word* product = value + n;
  tc_mp_word* remainder = product + 2 * n;
  tc_mp_word* temporary = remainder + n;
  tc_mp_from_be_padded(p, p_bytes.data, p_bytes.length, length);
  tc_mp_from_be_padded(q, q_bytes.data, q_bytes.length, length);
  tc_mp_from_be_padded(d, d_bytes.data, d_bytes.length, length);
  --p[0];
  --q[0]; /* Validated factors are odd; subtraction needs no borrow. */
  const tc_mp_word* factors[] = {p, q};
  const TC_bytes exponents[] = {dp_bytes, dq_bytes};
  unsigned matches = 1;
  for (size_t i = 0; i < 2; ++i) {
    tc_mp_from_be_padded(value, exponents[i].data, exponents[i].length, length);
    tc_mp_reduce_words(remainder, d, n, factors[i], n, temporary);
    matches &= (unsigned)tc_mp_equal(value, remainder, n);
  }
  ++p[0];
  ++q[0];
  tc_mp_from_be_padded(value, inverse_bytes.data, inverse_bytes.length, length);
  matches &= tc_mp_subtract(temporary, value, p, n);
  tc_mp_multiply(product, q, value, n);
  tc_mp_reduce_words(remainder, product, 2 * n, p, n, temporary);
  tc_mp_word difference = remainder[0] ^ 1u;
  for (size_t i = 1; i < n; ++i)
    difference |= remainder[i];
  matches &= (unsigned)(difference == 0);
  TC_secure_zero(scratch, required * sizeof *scratch);
  return matches ? TC_RSA_OK : TC_RSA_INVALID;
}

/* Derive fixed-width CRT values from validated d, p and q magnitudes. The
 * view holds half-width p and q. Scratch needs TC_RSA_CRT_WORKSPACE_WORDS
 * limbs. Results stay in scratch until the caller publishes all three. */
static inline TC_RSA_result tc_rsa_crt_derive(const tc_rsa_private_view* key, tc_mp_scratch area,
                                              uint32_t* work, tc_mp_word** dp_out,
                                              tc_mp_word** dq_out, tc_mp_word** inverse_out)
{
  const size_t length = key->public_key->modulus.length;
  const TC_bytes d_bytes = key->d;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  const size_t n = length / sizeof(tc_mp_word), h = n / 2;
  const size_t prime_length = length / 2;
  const size_t required = TC_RSA_CRT_WORKSPACE_WORDS(length * 8);
  const size_t cost = 48 * length + 3;
  const TC_bytes fields[] = {key->p, key->q};
  if (scratch_words < required || *work < cost)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost; /* cost <= *work, checked above. */
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + h;
  tc_mp_word* d = q + h;
  tc_mp_word* dp = d + n;
  tc_mp_word* dq = dp + h;
  tc_mp_word* inverse = dq + h;
  tc_mp_word* arena = inverse + h;
  tc_mp_from_be_padded(p, fields[0].data, fields[0].length, prime_length);
  tc_mp_from_be_padded(q, fields[1].data, fields[1].length, prime_length);
  tc_mp_from_be_padded(d, d_bytes.data, d_bytes.length, length);
  if (!(p[0] & q[0] & 1u)) {
    TC_secure_zero(scratch, required * sizeof *scratch);
    return TC_RSA_INVALID;
  }
  --p[0];
  tc_mp_reduce_words(dp, d, n, p, h, arena);
  ++p[0];
  --q[0];
  tc_mp_reduce_words(dq, d, n, q, h, arena);
  ++q[0];
  tc_mp_reduce_words(arena, q, h, p, h, arena + h);
  if (!tc_mp_inverse(inverse, arena, p, h, arena + h)) {
    TC_secure_zero(scratch, required * sizeof *scratch);
    return TC_RSA_INVALID;
  }
  *dp_out = dp;
  *dq_out = dq;
  *inverse_out = inverse;
  return TC_RSA_OK;
}

/* Blinded two-prime CRT private operation (RFC 8017 section 5.1.2 step 2.b).
 * The key and CRT fields have already passed their public validation APIs,
 * remain unchanged, and hold half-width values in the view. Scratch uses
 * TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS limbs, is disjoint from all inputs and
 * output, and is wiped on return. Work is tc_rsa_private_base_cost for the
 * CRT form plus tc_rsa_blinding_cost per RNG request. */
static inline TC_RSA_result tc_rsa_crt_private_operation(const tc_rsa_private_view* key,
                                                         const uint8_t* input, uint8_t* output,
                                                         const tc_rsa_random* rng,
                                                         tc_mp_scratch area, uint32_t* work)
{
  const uint8_t* const modulus = key->public_key->modulus.data;
  const size_t length = key->public_key->modulus.length;
  const uint8_t* const exponent = key->public_key->exponent.data;
  const size_t exponent_length = key->public_key->exponent.length;
  const size_t max_attempts = rng->attempts;
  tc_mp_word* const scratch = area.words;
  const size_t scratch_words = area.capacity;
  TC_RSA_result status;
  const size_t n = length / sizeof(tc_mp_word), h = n / 2;
  const size_t prime_length = length / 2;
  const size_t required = TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8);
  const uint32_t operations = tc_rsa_private_base_cost(length, exponent_length, 1);
  const TC_bytes fields[] = {key->p, key->q, key->dp, key->dq, key->q_inverse};
  if (!max_attempts || scratch_words < required || *work < operations)
    return TC_RSA_LIMIT;
  *work -= operations;

  tc_mp_word* n_words = scratch;
  tc_mp_word* c = n_words + n;
  tc_mp_word* factors = c + n;
  tc_mp_word* inverse = factors + n;
  tc_mp_word* residues = inverse + n;
  tc_mp_word* results = residues + n;
  tc_mp_word* value = results + n;
  tc_mp_word* arena = value + n;
  tc_mp_word* temporary = arena;
  tc_mp_word* reduced = temporary + n;
  tc_mp_word* product = reduced + n;
  tc_mp_from_be(n_words, modulus, length);
  tc_mp_from_be(c, input, length);
  status = TC_RSA_INVALID;
  if (!tc_mp_subtract(temporary, c, n_words, n))
    goto cleanup;

  tc_mp_word* blind = factors;
  status = tc_rsa_sample_blinding(n_words, n, blind, inverse, arena, rng, work);
  if (status != TC_RSA_OK)
    goto cleanup;

  /* Blind modulo n before reducing into the two prime fields. */
  const tc_mp_word nf = tc_mp_montgomery_factor(n_words[0]);
  tc_mp_montgomery_r2(residues, n_words, n, reduced);
  memset(results, 0, length);
  results[0] = 1;
  const tc_mp_modulus field = {n_words, n, nf, product, reduced};
  tc_mp_montgomery(results, results, residues, &field);
  tc_mp_montgomery(blind, blind, residues, &field);
  tc_mp_montgomery(c, c, residues, &field);
  tc_mp_power(value, blind, (TC_bytes){exponent, exponent_length}, results, &field, temporary);
  tc_mp_montgomery(value, c, value, &field);
  /* The encoded input remains available for the final fault check. Keep R² in
   * c while residues is reused for the two prime fields. */
  memcpy(c, residues, length);
  memset(temporary, 0, length);
  temporary[0] = 1;
  tc_mp_montgomery(value, value, temporary, &field);

  tc_mp_word* p = factors;
  tc_mp_word* q = factors + h;
  tc_mp_from_be_padded(p, fields[0].data, fields[0].length, prime_length);
  tc_mp_from_be_padded(q, fields[1].data, fields[1].length, prime_length);
  tc_mp_word* rp = residues;
  tc_mp_word* rq = residues + h;
  tc_mp_reduce_words(rp, value, n, p, h, temporary);
  tc_mp_reduce_words(rq, value, n, q, h, temporary);

  /* Each exponentiation uses the same bounded half-width arithmetic arena. */
  const TC_bytes powers[] = {fields[2], fields[3]};
  tc_mp_word* moduli[] = {p, q};
  tc_mp_word* bases[] = {rp, rq};
  tc_mp_word* outputs[] = {results, results + h};
  tc_mp_word* half_r2 = value;
  tc_mp_word* half_one = value + h;
  tc_mp_word* half_temporary = arena;
  tc_mp_word* half_product = half_temporary + h;
  tc_mp_word* half_reduced = half_product + 2 * h + 2;
  for (size_t i = 0; i < 2; ++i) {
    const tc_mp_word factor = tc_mp_montgomery_factor(moduli[i][0]);
    tc_mp_montgomery_r2(half_r2, moduli[i], h, half_reduced);
    memset(half_one, 0, prime_length);
    half_one[0] = 1;
    const tc_mp_modulus half = {moduli[i], h, factor, half_product, half_reduced};
    tc_mp_montgomery(half_one, half_one, half_r2, &half);
    tc_mp_montgomery(bases[i], bases[i], half_r2, &half);
    tc_mp_power_padded(outputs[i], bases[i], powers[i], prime_length, half_one, &half,
                       half_temporary);
    memset(half_temporary, 0, prime_length);
    half_temporary[0] = 1;
    tc_mp_montgomery(outputs[i], outputs[i], half_temporary, &half);
  }

  /* Garner recombination: m2 + q * ((m1-m2) * qInv mod p). */
  tc_mp_word* m1 = results;
  tc_mp_word* m2 = results + h;
  tc_mp_word* m2_mod_p = residues;
  tc_mp_word* delta = residues + h;
  tc_mp_reduce_words(m2_mod_p, m2, h, p, h, temporary);
  tc_mp_sub_mod(delta, m1, m2_mod_p, p, h);
  tc_mp_from_be_padded(half_r2, fields[4].data, fields[4].length, prime_length);
  tc_mp_multiply(half_product, delta, half_r2, h);
  tc_mp_reduce_words(delta, half_product, 2 * h, p, h, half_reduced);
  tc_mp_multiply(value, q, delta, h);
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)value[i] + (i < h ? m2[i] : 0);
    value[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }

  /* Unblind modulo n and verify with the public exponent before publication. */
  memset(results, 0, length);
  results[0] = 1;
  tc_mp_montgomery(results, results, c, &field);
  tc_mp_montgomery(value, value, c, &field);
  tc_mp_montgomery(inverse, inverse, c, &field);
  tc_mp_montgomery(value, value, inverse, &field);
  tc_mp_power(residues, value, (TC_bytes){exponent, exponent_length}, results, &field, temporary);
  memset(temporary, 0, length);
  temporary[0] = 1;
  tc_mp_montgomery(residues, residues, temporary, &field);
  tc_mp_to_be((uint8_t*)temporary, residues, length);
  uint8_t difference = 0;
  for (size_t i = 0; i < length; ++i)
    difference |= ((uint8_t*)temporary)[i] ^ input[i];
  if (difference) {
    status = TC_RSA_ERROR;
    goto cleanup;
  }
  memset(temporary, 0, length);
  temporary[0] = 1;
  tc_mp_montgomery(value, value, temporary, &field);
  tc_mp_to_be(output, value, length);
  status = TC_RSA_OK;
cleanup:
  TC_secure_zero(scratch, required * sizeof *scratch);
  return status;
}
/* Private operation for a validated key: the CRT form when the view has CRT
 * values, otherwise full-width exponentiation with d. Storage, cost and
 * output rules match the selected operation. */
static inline TC_RSA_result tc_rsa_private_apply(const tc_rsa_private_view* key,
                                                 const uint8_t* input, uint8_t* output,
                                                 const tc_rsa_random* rng, tc_mp_scratch scratch,
                                                 uint32_t* work)
{
  if (key->crt)
    return tc_rsa_crt_private_operation(key, input, output, rng, scratch, work);
  return tc_rsa_private_operation_magnitude(key->public_key, key->d, input, output, rng, scratch,
                                            work);
}
#endif
