/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_PRIVATE_INTERNAL_H_
#define TC_RSA_PRIVATE_INTERNAL_H_
#include "rsa_internal.h"
#include "mp_inverse_internal.h"
#include "rsa_prime_internal.h"

static inline int tc_rsa_private_exponent_check(const tc_mp_word* d,
    const tc_mp_word* modulus, size_t n, tc_mp_word* temporary)
{
  tc_mp_word nonzero = 0;
  for (size_t i = 0; i < n; ++i) nonzero |= d[i];
  const tc_mp_word below_modulus = tc_mp_subtract(temporary,d,modulus,n);
  return (nonzero != 0) & (d[0] & 1u) & below_modulus;
}

/* Check n=p*q and e*d=1 modulo both p-1 and q-1, with distinct odd factors.
 * Full key validation also requires primality testing. Magnitudes fit length
 * bytes. All ranges and work are disjoint. Scratch has 8n
 * limbs and is wiped after use. Work counts products and bit-reduction steps. */
static inline TC_RSA_result tc_rsa_private_magnitudes_consistent(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    TC_bytes d_bytes, TC_bytes p_bytes, TC_bytes q_bytes,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  if (!d_bytes.data || !p_bytes.data || !q_bytes.data || !scratch || !work) return TC_RSA_ARGUMENT;
  if (!d_bytes.length || !p_bytes.length || !q_bytes.length ||
      d_bytes.length > length || p_bytes.length > length || q_bytes.length > length)
    return TC_RSA_INVALID;
  TC_RSA_result status = tc_rsa_public_key_check(modulus,length,exponent,exponent_length);
  if (status != TC_RSA_OK) return status;
  const size_t n = length / sizeof(tc_mp_word), required = 8 * n;
  const size_t cost = 32 * length + 2;
  if (scratch_words < required || *work < cost) return TC_RSA_LIMIT;
  *work -= cost;
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + n;
  tc_mp_word* d = q + n;
  tc_mp_word* e = d + n;
  tc_mp_word* product = e + n;
  tc_mp_word* remainder = product + 2 * n;
  tc_mp_word* temporary = remainder + n;
  tc_mp_from_be_padded(p,p_bytes.data,p_bytes.length,length);
  tc_mp_from_be_padded(q,q_bytes.data,q_bytes.length,length);
  tc_mp_from_be_padded(d,d_bytes.data,d_bytes.length,length);
  tc_mp_from_be(remainder,modulus,length);
  status = TC_RSA_INVALID;
  if (!tc_rsa_private_exponent_check(d,remainder,n,temporary)) goto cleanup;
  tc_mp_word p_above_one = (tc_mp_word)(p[0] & ~1u);
  tc_mp_word q_above_one = (tc_mp_word)(q[0] & ~1u);
  tc_mp_word distinct = p[0] ^ q[0];
  for (size_t i = 1; i < n; ++i) {
    p_above_one |= p[i]; q_above_one |= q[i]; distinct |= p[i] ^ q[i];
  }
  if (!(p[0] & q[0] & 1u) || !p_above_one || !q_above_one || !distinct) goto cleanup;
  tc_mp_multiply(product,p,q,n);
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i) difference |= (product[i] ^ remainder[i]) | product[n + i];
  if (difference) goto cleanup;
  tc_mp_from_be_padded(e,exponent,exponent_length,length);
  tc_mp_multiply(product,d,e,n);
  --p[0]; --q[0]; /* Both factors are odd, so subtraction needs no borrow. */
  const tc_mp_word* factors[] = {p,q};
  for (size_t factor = 0; factor < 2; ++factor) {
    tc_mp_reduce_words(remainder,product,2 * n,factors[factor],n,temporary);
    difference |= remainder[0] ^ 1u;
    for (size_t i = 1; i < n; ++i) difference |= remainder[i];
  }
  if (!difference) status = TC_RSA_OK;
cleanup:
  TC_secure_zero(scratch,required * sizeof *scratch);
  return status;
}

static inline TC_RSA_result tc_rsa_private_key_consistent(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    const uint8_t* d, const uint8_t* p, const uint8_t* q,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  return tc_rsa_private_magnitudes_consistent(modulus,length,exponent,exponent_length,
      (TC_bytes){d,length},(TC_bytes){p,length},(TC_bytes){q,length},scratch,scratch_words,work);
}

/* Validate two-prime components and test each factor with the selected rounds.
 * Factors and d fit length bytes. max_attempts applies per factor.
 * Storage ownership matches the component check; scratch needs 12n+2 limbs.
 * The caller's assurance policy supplies rounds and an independent uniform RNG. */
static inline TC_RSA_result tc_rsa_private_magnitudes_check(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    TC_bytes d, TC_bytes p, TC_bytes q, size_t rounds,
    TC_random_fn random, void* random_context, size_t max_attempts,
    tc_mp_word* scratch, size_t scratch_words, uint32_t* work)
{
  if (!d.data || !p.data || !q.data || !rounds || !random || !scratch || !work)
    return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_public_key_check(modulus,length,exponent,exponent_length);
  if (status != TC_RSA_OK) return status;
  const size_t required = 12 * (length / sizeof(tc_mp_word)) + 2;
  const size_t component_cost = 32 * length + 2;
  if (scratch_words < required || max_attempts < rounds || *work < component_cost)
    return TC_RSA_LIMIT;
  /* The component check's bounded cost fits a 16-bit size_t. */
  size_t component_work = component_cost;
  status = tc_rsa_private_magnitudes_consistent(modulus,length,exponent,exponent_length,
      d,p,q,scratch,scratch_words,&component_work);
  *work -= (uint32_t)(component_cost - component_work);
  const TC_bytes factors[] = {p,q};
  for (size_t i = 0; status == TC_RSA_OK && i < 2; ++i)
    status = tc_rsa_probable_prime_magnitude(factors[i],length,rounds,random,random_context,
        max_attempts,scratch,scratch_words,work);
  TC_secure_zero(scratch,required * sizeof *scratch);
  return status;
}

static inline TC_RSA_result tc_rsa_private_key_check(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    const uint8_t* d, const uint8_t* p, const uint8_t* q, size_t rounds,
    TC_random_fn random, void* random_context, size_t max_attempts,
    tc_mp_word* scratch, size_t scratch_words, uint32_t* work)
{
  return tc_rsa_private_magnitudes_check(modulus,length,exponent,exponent_length,
      (TC_bytes){d,length},(TC_bytes){p,length},(TC_bytes){q,length},rounds,
      random,random_context,max_attempts,scratch,scratch_words,work);
}

/* Full-width private operation for an already validated RSA key. d fits length
 * bytes; input and output have length bytes. Input, output, scratch and work ranges are
 * disjoint; the caller checks ranges and random-source context ownership.
 * Scratch uses 13n limbs, n=length/sizeof(word), and is wiped after use.
 * Work counts size-bounded modular operations, inverse steps and RNG requests.
 * max_attempts bounds rejection sampling. Output changes only after verification. */
static inline TC_RSA_result tc_rsa_private_operation_magnitude(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    TC_bytes d, const uint8_t* input, uint8_t* output,
    TC_random_fn random, void* random_context, size_t max_attempts,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  if (!d.data || !input || !output || !random || !scratch || !work) return TC_RSA_ARGUMENT;
  if (!d.length || d.length > length) return TC_RSA_INVALID;
  TC_RSA_result status = tc_rsa_public_key_check(modulus,length,exponent,exponent_length);
  if (status != TC_RSA_OK) return status;
  const size_t n = length / sizeof(tc_mp_word), required = 13 * n;
  const size_t operations = 32 * length + 32 * exponent_length + 8;
  const size_t attempt_work = 16 * length + 1;
  if (!max_attempts || scratch_words < required || *work < operations) return TC_RSA_LIMIT;
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
  tc_mp_from_be(p,modulus,length); tc_mp_from_be(c,input,length);
  tc_mp_from_be_padded(one,d.data,d.length,length);
  status = TC_RSA_INVALID;
  if (!tc_rsa_private_exponent_check(one,p,n,temporary) ||
      !tc_mp_subtract(temporary,c,p,n)) goto cleanup;
  status = TC_RSA_LIMIT;
  for (size_t attempt = 0; attempt < max_attempts; ++attempt) {
    if (*work < attempt_work) goto cleanup;
    *work -= attempt_work;
    if (random(random_context,(uint8_t*)temporary,length) != TC_OK) {
      status = TC_RSA_ERROR; goto cleanup;
    }
    tc_mp_from_be(blind,(const uint8_t*)temporary,length);
    const tc_mp_word below_modulus = tc_mp_subtract(reduced,blind,p,n);
    tc_mp_word above_one = (tc_mp_word)(blind[0] & ~1u);
    for (size_t i = 1; i < n; ++i) above_one |= blind[i];
    if (!below_modulus || !above_one) continue;
    if (tc_mp_inverse(inverse,blind,p,n,arena)) { status = TC_RSA_OK; break; }
  }
  if (status != TC_RSA_OK) goto cleanup;
  const tc_mp_word factor = tc_mp_montgomery_factor(p[0]);
  tc_mp_montgomery_r2(r2,p,n,reduced);
  memset(one,0,length); one[0] = 1;
  tc_mp_montgomery(one,one,r2,p,n,factor,product,reduced);
  tc_mp_montgomery(blind,blind,r2,p,n,factor,product,reduced);
  tc_mp_montgomery(inverse,inverse,r2,p,n,factor,product,reduced);
  tc_mp_montgomery(c,c,r2,p,n,factor,product,reduced);
  tc_mp_power(result,blind,exponent,exponent_length,one,p,n,factor,temporary,product,reduced);
  tc_mp_montgomery(blind,c,result,p,n,factor,product,reduced);
  tc_mp_power_padded(result,blind,d.data,d.length,length,one,p,n,factor,temporary,product,reduced);
  tc_mp_montgomery(result,result,inverse,p,n,factor,product,reduced);
  /* Verify in Montgomery form before converting or publishing the result. */
  tc_mp_power(blind,result,exponent,exponent_length,one,p,n,factor,temporary,product,reduced);
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i) difference |= blind[i] ^ c[i];
  if (difference) { status = TC_RSA_ERROR; goto cleanup; }
  memset(one,0,length); one[0] = 1;
  tc_mp_montgomery(result,result,one,p,n,factor,product,reduced);
  tc_mp_to_be(output,result,length);
cleanup:
  TC_secure_zero(scratch,required * sizeof *scratch);
  return status;
}
static inline TC_RSA_result tc_rsa_private_operation(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    const uint8_t* d, const uint8_t* input, uint8_t* output,
    TC_random_fn random, void* random_context, size_t max_attempts,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  return tc_rsa_private_operation_magnitude(modulus,length,exponent,exponent_length,
      (TC_bytes){d,length},input,output,random,random_context,max_attempts,
      scratch,scratch_words,work);
}
#endif
