/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_INTERNAL_H_
#define TC_RSA_INTERNAL_H_
#include "mp_internal.h"
#include "rsa_encoding_internal.h"
#include "hash_info_internal.h"
#include <tiny_crypto/rsa.h>

typedef TC_RSA_result tc_rsa_result;

static inline tc_rsa_result tc_rsa_public_key_check(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length)
{
  if (!modulus || !exponent) return TC_RSA_ARGUMENT;
  if (length != 128 && length != 256 && length != 384) return TC_RSA_INVALID;
  if (!exponent_length || exponent_length > length || !exponent[0] ||
      !(exponent[exponent_length - 1] & 1) ||
      (exponent_length == 1 && exponent[0] < 3) ||
      !(modulus[0] & 0x80) || !(modulus[length - 1] & 1) ||
      (exponent_length == length && memcmp(exponent,modulus,length) >= 0)) return TC_RSA_INVALID;
  return TC_RSA_OK;
}

/* Internal public-key exponentiation. Modulus and input
 * have length bytes; out has the same capacity. Exponent is minimally encoded.
 * All ranges, work and scratch are disjoint. The caller validates address ranges.
 * Scratch needs 8*(length/sizeof(word))+2 limbs and is wiped after use.
 * Work counts modular additions/multiplications plus one setup unit; each has a
 * size-bounded loop.
 * Output changes only on OK. Validation covers encodings and numeric bounds. */
static inline tc_rsa_result tc_rsa_public_operation(const uint8_t* modulus,
    size_t length, const uint8_t* exponent, size_t exponent_length,
    const uint8_t* input, uint8_t* out, tc_mp_word* scratch, size_t scratch_words,
    size_t* work)
{
  size_t n, required, cost;
  tc_mp_word *p, *base, *one, *result, *temporary, *reduced, *product, factor;
  if (!modulus || !exponent || !input || !out || !scratch || !work) return TC_RSA_ARGUMENT;
  tc_rsa_result checked = tc_rsa_public_key_check(modulus,length,exponent,exponent_length);
  if (checked != TC_RSA_OK) return checked;
  if (memcmp(input,modulus,length) >= 0) return TC_RSA_INVALID;
  n = length / sizeof(tc_mp_word);
  required = 8 * n + 2;
  cost = 16 * length + 16 * exponent_length + 4;
  if (scratch_words < required || *work < cost) return TC_RSA_LIMIT;
  *work -= cost;
  p = scratch; base = p + n; one = base + n; result = one + n;
  temporary = result + n; reduced = temporary + n; product = reduced + n;
  tc_mp_from_be(p,modulus,length);
  tc_mp_from_be(base,input,length);
  factor = tc_mp_montgomery_factor(p[0]);
  tc_mp_montgomery_r2(temporary,p,n,reduced);
  memset(one,0,length); one[0] = 1;
  tc_mp_montgomery(one,one,temporary,p,n,factor,product,reduced);
  tc_mp_montgomery(base,base,temporary,p,n,factor,product,reduced);
  tc_mp_power(result,base,exponent,exponent_length,one,p,n,factor,temporary,product,reduced);
  memset(one,0,length); one[0] = 1;
  tc_mp_montgomery(result,result,one,p,n,factor,product,reduced);
  tc_mp_to_be(out,result,length);
  TC_secure_zero(scratch,required * sizeof *scratch);
  return TC_RSA_OK;
}

/* digest is a precomputed hash. No key-size acceptance policy
 * is implied. Scratch needs 9n+2 limbs, including the recovered representative.
 * Other storage preconditions match tc_rsa_public_operation. */
static inline tc_rsa_result tc_rsa_verify_v15(const uint8_t* modulus, size_t length,
    const uint8_t* exponent, size_t exponent_length, const uint8_t* signature,
    size_t signature_length, TC_hash_algorithm hash,
    const uint8_t* digest, size_t digest_length, tc_mp_word* scratch,
    size_t scratch_words, size_t* work)
{
  size_t n, arithmetic_words;
  uint8_t* encoded;
  tc_rsa_result result;
  TC_status checked;
  tc_hash_info info;
  if (!modulus || !exponent || !signature || !digest || !scratch || !work)
    return TC_RSA_ARGUMENT;
  if (!tc_hash_info_get(hash,&info)) return TC_RSA_UNSUPPORTED;
  if (digest_length != info.digest_length) return TC_RSA_ARGUMENT;
  if ((length != 128 && length != 256 && length != 384) || signature_length != length ||
      !tc_rsa_v15_size(length,info.digest_info.length,digest_length)) return TC_RSA_INVALID;
  n = length / sizeof(tc_mp_word);
  arithmetic_words = 8 * n + 2;
  if (scratch_words < arithmetic_words + n || *work < length) return TC_RSA_LIMIT;
  *work -= length;
  encoded = (uint8_t*)(scratch + arithmetic_words);
  result = tc_rsa_public_operation(modulus,length,exponent,exponent_length,
      signature,encoded,scratch,arithmetic_words,work);
  if (result != TC_RSA_OK) return result;
  checked = tc_rsa_v15_check(encoded,length,info.digest_info.data,info.digest_info.length,digest,digest_length);
  TC_secure_zero(encoded,length);
  return checked == TC_OK ? TC_RSA_OK : TC_RSA_INVALID;
}
#endif
