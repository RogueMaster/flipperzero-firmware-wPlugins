/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_CRT_INTERNAL_H_
#define TC_RSA_CRT_INTERNAL_H_
#include "mp_internal.h"
#include "mp_inverse_internal.h"
#include "rsa_internal.h"
#include <tiny_crypto/rsa.h>

/* d/p/q have passed private-key validation. Check dP=d mod(p-1),
 * dQ=d mod(q-1), and the least positive q^-1 mod p.
 * Magnitudes fit a limb-aligned width of at most 384 bytes. Inputs, work and
 * scratch are disjoint. Scratch needs 8n limbs and is wiped after use. */
static inline TC_RSA_result tc_rsa_crt_consistent(size_t length,
    TC_bytes d_bytes, TC_bytes p_bytes, TC_bytes q_bytes,
    TC_bytes dp_bytes, TC_bytes dq_bytes, TC_bytes inverse_bytes,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  enum { MAX_BYTES = 384 };
  const TC_bytes inputs[] = {d_bytes,p_bytes,q_bytes,dp_bytes,dq_bytes,inverse_bytes};
  if (!scratch || !work) return TC_RSA_ARGUMENT;
  if (!length || length > MAX_BYTES || length % sizeof(tc_mp_word)) return TC_RSA_INVALID;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
    if (!inputs[i].data) return TC_RSA_ARGUMENT;
    if (!inputs[i].length || inputs[i].length > length) return TC_RSA_INVALID;
  }
  const size_t n = length / sizeof(tc_mp_word), required = 8 * n;
  const size_t cost = 32 * length + 1;
  if (scratch_words < required || *work < cost) return TC_RSA_LIMIT;
  *work -= cost;
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + n;
  tc_mp_word* d = q + n;
  tc_mp_word* value = d + n;
  tc_mp_word* product = value + n;
  tc_mp_word* remainder = product + 2 * n;
  tc_mp_word* temporary = remainder + n;
  tc_mp_from_be_padded(p,p_bytes.data,p_bytes.length,length);
  tc_mp_from_be_padded(q,q_bytes.data,q_bytes.length,length);
  tc_mp_from_be_padded(d,d_bytes.data,d_bytes.length,length);
  --p[0]; --q[0]; /* Validated factors are odd; subtraction needs no borrow. */
  const tc_mp_word* factors[] = {p,q};
  const TC_bytes exponents[] = {dp_bytes,dq_bytes};
  unsigned matches = 1;
  for (size_t i = 0; i < 2; ++i) {
    tc_mp_from_be_padded(value,exponents[i].data,exponents[i].length,length);
    tc_mp_reduce_words(remainder,d,n,factors[i],n,temporary);
    matches &= (unsigned)tc_mp_equal(value,remainder,n);
  }
  ++p[0]; ++q[0];
  tc_mp_from_be_padded(value,inverse_bytes.data,inverse_bytes.length,length);
  matches &= tc_mp_subtract(temporary,value,p,n);
  tc_mp_multiply(product,q,value,n);
  tc_mp_reduce_words(remainder,product,2 * n,p,n,temporary);
  tc_mp_word difference = remainder[0] ^ 1u;
  for (size_t i = 1; i < n; ++i) difference |= remainder[i];
  matches &= (unsigned)(difference == 0);
  TC_secure_zero(scratch,required * sizeof *scratch);
  return matches ? TC_RSA_OK : TC_RSA_INVALID;
}

/* Derive fixed-width CRT values from validated d, p and q magnitudes. Scratch
 * needs 8n limbs. Results stay in scratch until the caller publishes all three. */
static inline TC_RSA_result tc_rsa_crt_derive(size_t length, TC_bytes d_bytes,
    TC_bytes p_bytes, TC_bytes q_bytes, tc_mp_word* scratch,
    size_t scratch_words, size_t* work, tc_mp_word** dp_out,
    tc_mp_word** dq_out, tc_mp_word** inverse_out)
{
  if (!d_bytes.data || !p_bytes.data || !q_bytes.data || !scratch || !work ||
      !dp_out || !dq_out || !inverse_out) return TC_RSA_ARGUMENT;
  const size_t n = length / sizeof(tc_mp_word), h = n / 2;
  const size_t prime_length = length / 2, required = 8 * n;
  const size_t cost = 48 * length + 3;
  TC_bytes fields[] = {p_bytes,q_bytes};
  if (!d_bytes.length || d_bytes.length > length) return TC_RSA_INVALID;
  for (size_t i = 0; i < 2; ++i) {
    if (!fields[i].length || fields[i].length > length) return TC_RSA_INVALID;
    while (fields[i].length > prime_length && *fields[i].data == 0) {
      ++fields[i].data; --fields[i].length;
    }
    if (fields[i].length > prime_length) return TC_RSA_INVALID;
  }
  if (scratch_words < required || *work < cost) return TC_RSA_LIMIT;
  *work -= cost;
  tc_mp_word* p = scratch;
  tc_mp_word* q = p + h;
  tc_mp_word* d = q + h;
  tc_mp_word* dp = d + n;
  tc_mp_word* dq = dp + h;
  tc_mp_word* inverse = dq + h;
  tc_mp_word* arena = inverse + h;
  tc_mp_from_be_padded(p,fields[0].data,fields[0].length,prime_length);
  tc_mp_from_be_padded(q,fields[1].data,fields[1].length,prime_length);
  tc_mp_from_be_padded(d,d_bytes.data,d_bytes.length,length);
  if (!(p[0] & q[0] & 1u)) { TC_secure_zero(scratch,required * sizeof *scratch); return TC_RSA_INVALID; }
  --p[0]; tc_mp_reduce_words(dp,d,n,p,h,arena); ++p[0];
  --q[0]; tc_mp_reduce_words(dq,d,n,q,h,arena); ++q[0];
  tc_mp_reduce_words(arena,q,h,p,h,arena + h);
  if (!tc_mp_inverse(inverse,arena,p,h,arena + h)) {
    TC_secure_zero(scratch,required * sizeof *scratch); return TC_RSA_INVALID;
  }
  *dp_out = dp; *dq_out = dq; *inverse_out = inverse;
  return TC_RSA_OK;
}

/* Blinded two-prime CRT private operation. The key and CRT fields have already
 * passed their public validation APIs and remain unchanged. Scratch uses 13n
 * limbs, is disjoint from all inputs and output, and is wiped on return. */
static inline TC_RSA_result tc_rsa_crt_private_operation(
    const uint8_t* modulus, size_t length,
    const uint8_t* exponent, size_t exponent_length,
    TC_bytes p_bytes, TC_bytes q_bytes, const TC_RSA_crt* crt,
    const uint8_t* input, uint8_t* output,
    TC_random_fn random, void* random_context, size_t max_attempts,
    tc_mp_word* scratch, size_t scratch_words, size_t* work)
{
  if (!crt || !p_bytes.data || !q_bytes.data || !crt->dp.data ||
      !crt->dq.data || !crt->q_inverse.data || !input || !output ||
      !random || !scratch || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_public_key_check(modulus,length,exponent,exponent_length);
  if (status != TC_RSA_OK) return status;
  const size_t n = length / sizeof(tc_mp_word), h = n / 2;
  const size_t prime_length = length / 2, required = 13 * n;
  const size_t operations = 48 * length + 32 * exponent_length + 12;
  const size_t attempt_work = 16 * length + 1;
  TC_bytes fields[] = {p_bytes,q_bytes,crt->dp,crt->dq,crt->q_inverse};
  for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i) {
    if (!fields[i].length || fields[i].length > length) return TC_RSA_INVALID;
    while (fields[i].length > prime_length && *fields[i].data == 0) {
      ++fields[i].data; --fields[i].length;
    }
    if (fields[i].length > prime_length) return TC_RSA_INVALID;
  }
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
  tc_mp_from_be(n_words,modulus,length);
  tc_mp_from_be(c,input,length);
  status = TC_RSA_INVALID;
  if (!tc_mp_subtract(temporary,c,n_words,n)) goto cleanup;

  status = TC_RSA_LIMIT;
  tc_mp_word* blind = factors;
  for (size_t attempt = 0; attempt < max_attempts; ++attempt) {
    if (*work < attempt_work) goto cleanup;
    *work -= attempt_work;
    if (random(random_context,(uint8_t*)temporary,length) != TC_OK) {
      status = TC_RSA_ERROR; goto cleanup;
    }
    tc_mp_from_be(blind,(const uint8_t*)temporary,length);
    const tc_mp_word below_modulus = tc_mp_subtract(reduced,blind,n_words,n);
    tc_mp_word above_one = (tc_mp_word)(blind[0] & ~1u);
    for (size_t i = 1; i < n; ++i) above_one |= blind[i];
    if (!below_modulus || !above_one) continue;
    if (tc_mp_inverse(inverse,blind,n_words,n,arena)) { status = TC_RSA_OK; break; }
  }
  if (status != TC_RSA_OK) goto cleanup;

  /* Blind modulo n before reducing into the two prime fields. */
  const tc_mp_word nf = tc_mp_montgomery_factor(n_words[0]);
  tc_mp_montgomery_r2(residues,n_words,n,reduced);
  memset(results,0,length); results[0] = 1;
  tc_mp_montgomery(results,results,residues,n_words,n,nf,product,reduced);
  tc_mp_montgomery(blind,blind,residues,n_words,n,nf,product,reduced);
  tc_mp_montgomery(c,c,residues,n_words,n,nf,product,reduced);
  tc_mp_power(value,blind,exponent,exponent_length,results,n_words,n,nf,
      temporary,product,reduced);
  tc_mp_montgomery(value,c,value,n_words,n,nf,product,reduced);
  memset(temporary,0,length); temporary[0] = 1;
  tc_mp_montgomery(value,value,temporary,n_words,n,nf,product,reduced);

  tc_mp_word* p = factors;
  tc_mp_word* q = factors + h;
  tc_mp_from_be_padded(p,fields[0].data,fields[0].length,prime_length);
  tc_mp_from_be_padded(q,fields[1].data,fields[1].length,prime_length);
  tc_mp_word* rp = residues;
  tc_mp_word* rq = residues + h;
  tc_mp_reduce_words(rp,value,n,p,h,temporary);
  tc_mp_reduce_words(rq,value,n,q,h,temporary);

  /* Each exponentiation uses the same bounded half-width arithmetic arena. */
  const TC_bytes powers[] = {fields[2],fields[3]};
  tc_mp_word* moduli[] = {p,q};
  tc_mp_word* bases[] = {rp,rq};
  tc_mp_word* outputs[] = {results,results + h};
  tc_mp_word* half_r2 = value;
  tc_mp_word* half_one = value + h;
  tc_mp_word* half_temporary = arena;
  tc_mp_word* half_product = half_temporary + h;
  tc_mp_word* half_reduced = half_product + 2 * h + 2;
  for (size_t i = 0; i < 2; ++i) {
    const tc_mp_word factor = tc_mp_montgomery_factor(moduli[i][0]);
    tc_mp_montgomery_r2(half_r2,moduli[i],h,half_reduced);
    memset(half_one,0,prime_length); half_one[0] = 1;
    tc_mp_montgomery(half_one,half_one,half_r2,moduli[i],h,factor,
        half_product,half_reduced);
    tc_mp_montgomery(bases[i],bases[i],half_r2,moduli[i],h,factor,
        half_product,half_reduced);
    tc_mp_power_padded(outputs[i],bases[i],powers[i].data,powers[i].length,
        prime_length,half_one,moduli[i],h,factor,half_temporary,
        half_product,half_reduced);
    memset(half_temporary,0,prime_length); half_temporary[0] = 1;
    tc_mp_montgomery(outputs[i],outputs[i],half_temporary,moduli[i],h,
        factor,half_product,half_reduced);
  }

  /* Garner recombination: m2 + q * ((m1-m2) * qInv mod p). */
  tc_mp_word* m1 = results;
  tc_mp_word* m2 = results + h;
  tc_mp_word* m2_mod_p = residues;
  tc_mp_word* delta = residues + h;
  tc_mp_reduce_words(m2_mod_p,m2,h,p,h,temporary);
  tc_mp_sub_mod(delta,m1,m2_mod_p,p,h);
  tc_mp_from_be_padded(half_r2,fields[4].data,fields[4].length,prime_length);
  tc_mp_multiply(half_product,delta,half_r2,h);
  tc_mp_reduce_words(delta,half_product,2 * h,p,h,half_reduced);
  tc_mp_multiply(value,q,delta,h);
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)value[i] + (i < h ? m2[i] : 0);
    value[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }

  /* Unblind modulo n and verify with the public exponent before publication. */
  tc_mp_montgomery_r2(residues,n_words,n,reduced);
  memset(results,0,length); results[0] = 1;
  tc_mp_montgomery(results,results,residues,n_words,n,nf,product,reduced);
  tc_mp_montgomery(value,value,residues,n_words,n,nf,product,reduced);
  tc_mp_montgomery(inverse,inverse,residues,n_words,n,nf,product,reduced);
  tc_mp_montgomery(value,value,inverse,n_words,n,nf,product,reduced);
  tc_mp_power(residues,value,exponent,exponent_length,results,n_words,n,nf,
      temporary,product,reduced);
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i) difference |= residues[i] ^ c[i];
  if (difference) { status = TC_RSA_ERROR; goto cleanup; }
  memset(temporary,0,length); temporary[0] = 1;
  tc_mp_montgomery(value,value,temporary,n_words,n,nf,product,reduced);
  tc_mp_to_be(output,value,length);
  status = TC_RSA_OK;
cleanup:
  TC_secure_zero(scratch,required * sizeof *scratch);
  return status;
}
#endif
