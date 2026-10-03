/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_INTERNAL_H_
#define TC_RSA_INTERNAL_H_
#include <tiny_crypto/common.h>
#include <tiny_crypto/rsa.h>
#include "mp_internal.h"
#include "hash_info_internal.h"
#include "pki_storage_internal.h"

/* prefix is the canonical DER DigestInfo header for the selected hash.
 * The caller supplies its matching fixed-length digest. */
static inline uint8_t tc_rsa_v15_byte(size_t index, size_t separator, const uint8_t* prefix,
                                      size_t prefix_length, const uint8_t* digest)
{
  if (!index || index == separator)
    return 0;
  if (index == 1)
    return 1;
  if (index < separator)
    return 0xff;
  index -= separator + 1;
  return index < prefix_length ? prefix[index] : digest[index - prefix_length];
}

static inline int tc_rsa_v15_size(size_t length, size_t prefix_length, size_t digest_length)
{
  return prefix_length && digest_length && length >= 11 && prefix_length <= length - 11 &&
         digest_length <= length - 11 - prefix_length;
}

/* RFC 8017 section 9.2 encoding step. Inputs and output are disjoint, and pointers
 * cover the stated lengths. Invalid sizing leaves output unchanged. */
static inline TC_status tc_rsa_v15_encode(uint8_t* out, size_t length, const uint8_t* prefix,
                                          size_t prefix_length, const uint8_t* digest,
                                          size_t digest_length)
{
  size_t separator;
  if (!out || !prefix || !digest || !tc_rsa_v15_size(length, prefix_length, digest_length))
    return TC_ERROR;
  separator = length - prefix_length - digest_length - 1;
  for (size_t i = 0; i < length; ++i)
    out[i] = tc_rsa_v15_byte(i, separator, prefix, prefix_length, digest);
  return TC_OK;
}

/* Compare the full representative, including padding and exact DER header.
 * Recovered representatives with invalid lengths or bytes return MISMATCH. */
static inline TC_status tc_rsa_v15_check(const uint8_t* encoded, size_t length,
                                         const uint8_t* prefix, size_t prefix_length,
                                         const uint8_t* digest, size_t digest_length)
{
  size_t separator;
  unsigned difference = 0;
  if (!encoded || !prefix || !digest)
    return TC_ERROR;
  if (!tc_rsa_v15_size(length, prefix_length, digest_length))
    return TC_MISMATCH;
  separator = length - prefix_length - digest_length - 1;
  for (size_t i = 0; i < length; ++i)
    difference |= encoded[i] ^ tc_rsa_v15_byte(i, separator, prefix, prefix_length, digest);
  return difference ? TC_MISMATCH : TC_OK;
}

static inline int tc_rsa_supported_modulus_size(size_t length)
{
  return (TC_RSA_ENABLE_1024 && length == 128) || (TC_RSA_ENABLE_2048 && length == 256) ||
         (TC_RSA_ENABLE_3072 && length == 384) ||
         (TC_RSA_ENABLE_4096 && length == TC_RSA_MAX_MODULUS_BYTES);
}

static inline int tc_rsa_supported_bits(size_t bits)
{
  return bits % 8 == 0 && tc_rsa_supported_modulus_size(bits / 8);
}

/* Structural public-key checks: a supported modulus size with the top and
 * bottom bits set, and an odd minimal exponent 3 <= e < n (RFC 8017 section
 * 3.1). Public entries call this once, after their storage checks. */
static inline TC_RSA_result tc_rsa_public_key_check(const TC_RSA_public_key* key)
{
  if (!key || !key->modulus.data || !key->exponent.data)
    return TC_RSA_ARGUMENT;
  const uint8_t* modulus = key->modulus.data;
  const uint8_t* exponent = key->exponent.data;
  const size_t length = key->modulus.length, exponent_length = key->exponent.length;
  /* A size outside the supported set is well formed but not implemented. */
  if (!tc_rsa_supported_modulus_size(length))
    return TC_RSA_UNSUPPORTED;
  if (!exponent_length || exponent_length > length || !exponent[0] ||
      !(exponent[exponent_length - 1] & 1) || (exponent_length == 1 && exponent[0] < 3) ||
      !(modulus[0] & 0x80) || !(modulus[length - 1] & 1) ||
      (exponent_length == length && memcmp(exponent, modulus, length) >= 0))
    return TC_RSA_INVALID;
  return TC_RSA_OK;
}

/* Work units of one public operation on a checked key of length bytes and a
 * minimal exponent of exponent_length bytes: 16 * length for the R^2 setup,
 * skipped with a prepared R^2, then 16 * exponent_length + 4 for the
 * exponentiation. Supported sizes keep the total below 2^15. */
static inline uint32_t tc_rsa_public_cost(size_t length, size_t exponent_length, int prepared)
{
  const uint32_t setup = prepared ? 0u : UINT32_C(16) * (uint32_t)length;
  return setup + UINT32_C(16) * (uint32_t)exponent_length + 4u;
}

/* Work units of one private operation before blinding: 32 * length +
 * 32 * exponent_length + 8 at full width, or 48 * length + 32 *
 * exponent_length + 12 for the CRT form. */
static inline uint32_t tc_rsa_private_base_cost(size_t length, size_t exponent_length, int crt)
{
  return (crt ? UINT32_C(48) * (uint32_t)length + 12u : UINT32_C(32) * (uint32_t)length + 8u) +
         UINT32_C(32) * (uint32_t)exponent_length;
}

/* Work units of one blinding attempt: an RNG request and its checks. */
static inline uint32_t tc_rsa_blinding_cost(size_t length)
{
  return UINT32_C(16) * (uint32_t)length + 1u;
}

/* Public-key exponentiation on a key that passed tc_rsa_public_key_check.
 * Input and out have the modulus length and out has at least that capacity.
 * All ranges, work and scratch are disjoint, and the caller validated them.
 * Scratch needs TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS limbs and is wiped after
 * use. Work is tc_rsa_public_cost. Prepared R^2 belongs to the same unchanged
 * modulus and stays outside scratch. Output changes only on OK. An input of
 * at least the modulus returns INVALID. */
static inline TC_RSA_result tc_rsa_public_operation(const TC_RSA_public_key* key,
                                                    const uint8_t* input, uint8_t* out,
                                                    tc_mp_scratch scratch_area, uint32_t* work,
                                                    const tc_mp_word* prepared_r2)
{
  size_t n, required;
  uint32_t cost;
  tc_mp_word *p, *base, *one, *result, *temporary, *reduced, *product, factor;
  tc_mp_word* scratch = scratch_area.words;
  const uint8_t* modulus = key->modulus.data;
  const TC_bytes exponent = key->exponent;
  const size_t length = key->modulus.length;
  /* RFC 8017 sections 5.1.1 and 5.2.2, step 1: the representative is below
   * the modulus. */
  if (memcmp(input, modulus, length) >= 0)
    return TC_RSA_INVALID;
  n = length / sizeof(tc_mp_word);
  required = TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(length * 8);
  cost = tc_rsa_public_cost(length, exponent.length, prepared_r2 != NULL);
  if (scratch_area.capacity < required || *work < cost)
    return TC_RSA_LIMIT;
  *work -= cost;
  p = scratch;
  base = p + n;
  one = base + n;
  result = one + n;
  temporary = result + n;
  reduced = temporary + n;
  product = reduced + n;
  tc_mp_from_be(p, modulus, length);
  tc_mp_from_be(base, input, length);
  factor = tc_mp_montgomery_factor(p[0]);
  if (!prepared_r2) {
    tc_mp_montgomery_r2(temporary, p, n, reduced);
    prepared_r2 = temporary;
  }
  memset(one, 0, length);
  one[0] = 1;
  const tc_mp_modulus field = {p, n, factor, product, reduced};
  tc_mp_montgomery(one, one, prepared_r2, &field);
  tc_mp_montgomery(base, base, prepared_r2, &field);
  tc_mp_power_public(result, base, exponent, one, &field);
  memset(one, 0, length);
  one[0] = 1;
  tc_mp_montgomery(result, result, one, &field);
  tc_mp_to_be(out, result, length);
  TC_secure_zero(scratch, required * sizeof *scratch);
  return TC_RSA_OK;
}

/* ARGUMENT when a known hash has a digest length other than length. An
 * unknown hash passes here and is reported as UNSUPPORTED by the scheme. */
static inline TC_RSA_result tc_rsa_digest_length_check(TC_hash_algorithm hash, size_t length)
{
  tc_hash_info info;
  return tc_hash_info_get(hash, &info) && info.digest_length != length ? TC_RSA_ARGUMENT
                                                                       : TC_RSA_OK;
}

/* A private key after its entry checks. The public key passed
 * tc_rsa_public_key_check. d, p and q are nonempty and at most the modulus
 * length, and p and q have their leading zero octets stripped to at most half
 * the modulus length. When crt is nonzero, dp, dq and q_inverse are stripped
 * the same way. The spans borrow the caller's key bytes. */
typedef struct {
  const TC_RSA_public_key* public_key;
  TC_bytes d, p, q;
  int crt;
  TC_bytes dp, dq, q_inverse;
} tc_rsa_private_view;

/* Check key, and crt when it is not NULL, once at a public entry and fill
 * view. Returns the public-key status, or INVALID for a component that is
 * empty, longer than the modulus, or wider than its half-width field. The
 * caller has already checked every span pointer. */
TC_RSA_result tc_rsa_private_view_init(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
                                       tc_rsa_private_view* view);

/* One storage preflight per public entry:
 *
 *   1. begin records the workspace limbs and checks their alignment.
 *   2. write, output and workspace record every other range the call
 *      modifies: outputs, length objects, work counters and caches.
 *   3. seal checks the writes against each other and then treats the
 *      workspace descriptor as an input.
 *   4. input, span, required and the key helpers record every borrowed range.
 *   5. finish returns TC_RSA_ARGUMENT for the first failure.
 *
 * A NULL object, a NULL required span, a misaligned workspace, overlap
 * between a write and any other range, or a range that wraps fails the
 * plan. Later steps return at once after a failure, so a caller checks only
 * the result of finish. */
enum { TC_RSA_STORAGE_WRITES = 5 };
typedef struct {
  tc_pki_storage_plan plan;
  TC_bytes writes[TC_RSA_STORAGE_WRITES];
  const TC_RSA_workspace* workspace;
} tc_rsa_storage;

void tc_rsa_storage_begin(tc_rsa_storage* storage, const TC_RSA_workspace* workspace);
/* A second aligned limb array, such as a prepared-key cache. */
void tc_rsa_storage_workspace(tc_rsa_storage* storage, const TC_RSA_workspace* workspace);
void tc_rsa_storage_write(tc_rsa_storage* storage, const void* data, size_t size);
/* A caller output buffer. Its data must be non-NULL. */
void tc_rsa_storage_output(tc_rsa_storage* storage, TC_buffer output);
void tc_rsa_storage_seal(tc_rsa_storage* storage);
/* A borrowed object. NULL fails. */
void tc_rsa_storage_input(tc_rsa_storage* storage, const void* data, size_t size);
/* A borrowed span that may be empty with NULL data. */
void tc_rsa_storage_span(tc_rsa_storage* storage, TC_bytes span);
/* A borrowed span whose data must be non-NULL. */
void tc_rsa_storage_required(tc_rsa_storage* storage, TC_bytes span);
/* The key object, modulus and exponent. */
void tc_rsa_storage_public_key(tc_rsa_storage* storage, const TC_RSA_public_key* key);
/* The public key plus d, p and q. key->crt is recorded separately. */
void tc_rsa_storage_private_key(tc_rsa_storage* storage, const TC_RSA_private_key* key);
/* The CRT object and its three values. */
void tc_rsa_storage_crt(tc_rsa_storage* storage, const TC_RSA_crt* crt);
TC_RSA_result tc_rsa_storage_finish(const tc_rsa_storage* storage);
#endif
