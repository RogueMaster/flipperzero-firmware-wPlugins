/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RFC 5869 HKDF over the shared HMAC core. */
#include <tiny_crypto/hkdf.h>

#if TC_ENABLE_HKDF
#include <string.h>
#include "hash_core_internal.h"
#include "internal.h"

/* Inputs of one HKDF call. Every span is validated once by hkdf_arguments. */
typedef struct {
  TC_bytes salt;
  const TC_bytes* ikm;
  size_t ikm_count;
  TC_bytes info;
} hkdf_inputs;

static int hkdf_output_length_ok(size_t hash_len, size_t output_len)
{
  return output_len != 0 && output_len <= 255u * hash_len;
}

/* Every input span is valid and disjoint from output. The ikm array size
 * must fit in size_t before its range is compared. */
static int hkdf_arguments(const hkdf_inputs* in, const uint8_t* output, size_t output_len)
{
  if (!output || !tc_internal_span_valid(in->salt.data, in->salt.length) ||
      !tc_internal_span_valid(in->info.data, in->info.length) || (in->ikm_count && !in->ikm) ||
      in->ikm_count > SIZE_MAX / sizeof *in->ikm ||
      !tc_internal_ranges_disjoint(output, output_len, in->salt.data, in->salt.length) ||
      !tc_internal_ranges_disjoint(output, output_len, in->info.data, in->info.length) ||
      !tc_internal_ranges_disjoint(output, output_len, in->ikm, in->ikm_count * sizeof *in->ikm))
    return 0;
  for (size_t i = 0; i < in->ikm_count; ++i)
    if (!tc_internal_span_valid(in->ikm[i].data, in->ikm[i].length) ||
        !tc_internal_ranges_disjoint(output, output_len, in->ikm[i].data, in->ikm[i].length))
      return 0;
  return 1;
}

/* PRK = HMAC(salt, IKM parts). A salt shorter than the block pads with
 * zeros, so an empty salt gives RFC 5869's zero salt. */
static TC_status hkdf_extract(const tc_hash_algorithm_info* hash, void* context,
                              const hkdf_inputs* in, uint8_t* prk)
{
  return tc_hmac_core_parts(hash, context, in->salt.data, in->salt.length, in->ikm, in->ikm_count,
                            prk);
}

/* Caller-owned HMAC storage for one expansion. keyed and context each hold
 * one HMAC context, and previous holds one digest of hash_len bytes. */
typedef struct {
  void* keyed;
  void* context;
  uint8_t* previous;
  size_t hash_len;
} hkdf_state;

/* T(i) = HMAC(PRK, T(i-1) || info || i); output = T(1) || T(2) || ...
 * PRK is keyed once into keyed, and each block continues from it. Exactly
 * output.capacity bytes are written. */
static TC_status hkdf_expand(const tc_hash_algorithm_info* hash, const hkdf_state* state,
                             TC_bytes prk, TC_bytes info, TC_buffer output)
{
  const size_t hash_len = state->hash_len;
  uint8_t* previous = state->previous;
  TC_status status = tc_hmac_core_init(hash, state->keyed, prk.data, prk.length);
  size_t offset = 0;
  uint8_t counter = 1;
  while (status == TC_OK && offset < output.capacity) {
    const TC_bytes parts[] = {{previous, offset ? hash_len : 0}, info, {&counter, 1}};
    status = tc_hmac_core_resume_parts(hash, state->keyed, state->context, parts, 3, previous);
    const size_t remaining = output.capacity - offset;
    const size_t take = remaining < hash_len ? remaining : hash_len;
    if (status == TC_OK)
      memcpy(output.data + offset, previous, take);
    offset += take;
    ++counter;
  }
  tc_hmac_core_clear(hash, state->keyed);
  TC_secure_zero(previous, hash_len);
  if (status != TC_OK)
    TC_secure_zero(output.data, output.capacity);
  return status;
}

#define TC_HKDF_DEFINE(N, DIGESTLEN)                                                               \
  TC_status TC_HKDF_SHA##N##_extract(TC_bytes salt, const TC_bytes* ikm, size_t ikm_count,         \
                                     uint8_t prk[DIGESTLEN])                                       \
  {                                                                                                \
    const hkdf_inputs in = {salt, ikm, ikm_count, {NULL, 0}};                                      \
    struct TC_HMAC_SHA##N##_ctx ctx;                                                               \
    if (!hkdf_arguments(&in, prk, DIGESTLEN))                                                      \
      return TC_ERROR;                                                                             \
    const TC_status status = hkdf_extract(&tc_sha##N##_info, &ctx, &in, prk);                      \
    if (status != TC_OK)                                                                           \
      TC_secure_zero(prk, DIGESTLEN);                                                              \
    return status;                                                                                 \
  }                                                                                                \
  TC_status TC_HKDF_SHA##N##_expand(TC_bytes prk, TC_bytes info, TC_buffer output)                 \
  {                                                                                                \
    const hkdf_inputs in = {prk, NULL, 0, info};                                                   \
    struct TC_HMAC_SHA##N##_ctx keyed, ctx;                                                        \
    uint8_t previous[DIGESTLEN];                                                                   \
    const hkdf_state state = {&keyed, &ctx, previous, DIGESTLEN};                                  \
    if (!prk.data || prk.length < DIGESTLEN ||                                                     \
        !hkdf_output_length_ok(DIGESTLEN, output.capacity) ||                                      \
        !hkdf_arguments(&in, output.data, output.capacity))                                        \
      return TC_ERROR;                                                                             \
    return hkdf_expand(&tc_sha##N##_info, &state, prk, info, output);                              \
  }                                                                                                \
  TC_status TC_HKDF_SHA##N##_derive(TC_bytes salt, const TC_bytes* ikm, size_t ikm_count,          \
                                    TC_bytes info, TC_buffer output)                               \
  {                                                                                                \
    const hkdf_inputs in = {salt, ikm, ikm_count, info};                                           \
    struct TC_HMAC_SHA##N##_ctx keyed, ctx;                                                        \
    uint8_t prk[DIGESTLEN], previous[DIGESTLEN];                                                   \
    const hkdf_state state = {&keyed, &ctx, previous, DIGESTLEN};                                  \
    if (!hkdf_output_length_ok(DIGESTLEN, output.capacity) ||                                      \
        !hkdf_arguments(&in, output.data, output.capacity))                                        \
      return TC_ERROR;                                                                             \
    /* Extract reads every input before expand writes output. */                                   \
    TC_status status = hkdf_extract(&tc_sha##N##_info, &ctx, &in, prk);                            \
    if (status == TC_OK)                                                                           \
      status = hkdf_expand(&tc_sha##N##_info, &state, (TC_bytes){prk, sizeof prk}, info, output);  \
    else                                                                                           \
      TC_secure_zero(output.data, output.capacity);                                                \
    TC_secure_zero(prk, sizeof prk);                                                               \
    return status;                                                                                 \
  }

#if TC_ENABLE_SHA1
TC_HKDF_DEFINE(1, TC_SHA1_DIGESTLEN)
#endif
#if TC_ENABLE_SHA224
TC_HKDF_DEFINE(224, TC_SHA224_DIGESTLEN)
#endif
#if TC_ENABLE_SHA256
TC_HKDF_DEFINE(256, TC_SHA256_DIGESTLEN)
#endif
#if TC_ENABLE_SHA384
TC_HKDF_DEFINE(384, TC_SHA384_DIGESTLEN)
#endif
#if TC_ENABLE_SHA512
TC_HKDF_DEFINE(512, TC_SHA512_DIGESTLEN)
#endif

#undef TC_HKDF_DEFINE
#endif /* TC_ENABLE_HKDF */
