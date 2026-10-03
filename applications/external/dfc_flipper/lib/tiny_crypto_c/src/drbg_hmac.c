/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * HMAC_DRBG (SP 800-90A section 10.1.2). The working state is Key and V,
 * each outlen bits. HMAC runs through the shared hash core in the DRBG's
 * scratch context, which is rekeyed for every HMAC(Key, ...) call so only one
 * HMAC context is needed. */
#include <tiny_crypto/drbg.h>

#if TC_DRBG_HAVE_HMAC
#include <string.h>
#include "drbg_internal.h"
#include "hash_core_internal.h"

TC_DRBG_result tc_drbg_hmac_parameters(TC_hash_algorithm hash, tc_drbg_parameters* out)
{
  const uint16_t strength = tc_drbg_hash_strength(hash);
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(hash);
  if (strength == 0)
    return TC_DRBG_ARGUMENT;
  if (info == NULL)
    return TC_DRBG_UNSUPPORTED;
  memset(out, 0, sizeof *out);
  out->strength_bits = strength;
  out->output_bytes = (uint8_t)tc_hash_core_digest_bytes(info);
  out->seed_bytes = out->output_bytes;
  out->uses_nonce = 1;
  return TC_DRBG_OK;
}

/* result = HMAC(Key, parts...). result may be Key or V, because every part
 * is read before the tag is written. */
static TC_DRBG_result hmac_parts(TC_DRBG* drbg, const TC_bytes* parts, size_t count,
                                 uint8_t* result)
{
  const tc_hash_algorithm_info* info = tc_hash_core_lookup((TC_hash_algorithm)drbg->hash);
  return tc_hmac_core_parts(info, &drbg->scratch.hmac, drbg->state.hmac.key, drbg->output_bytes,
                            parts, count, result) == TC_OK
             ? TC_DRBG_OK
             : TC_DRBG_ERROR;
}

/* HMAC_DRBG_Update (10.1.2.2) over provided_data given as up to 3 parts:
 *   Key = HMAC(Key, V || 0x00 || provided_data), V = HMAC(Key, V)
 * and, when provided_data is nonempty, the same again with 0x01. */
static TC_DRBG_result update(TC_DRBG* drbg, const TC_bytes* provided, size_t count)
{
  static const uint8_t separators[2] = {0x00, 0x01};
  const TC_bytes v = {drbg->state.hmac.v, drbg->output_bytes};
  const size_t rounds = tc_drbg_parts_length(provided, count) != 0 ? 2 : 1;
  TC_bytes parts[5];
  TC_DRBG_result result = TC_DRBG_OK;
  size_t round, i;

  parts[0] = v;
  for (i = 0; i < count; ++i)
    parts[i + 2] = provided[i];
  for (round = 0; result == TC_DRBG_OK && round < rounds; ++round) {
    parts[1] = (TC_bytes){&separators[round], 1};
    result = hmac_parts(drbg, parts, count + 2, drbg->state.hmac.key);
    if (result == TC_DRBG_OK)
      result = hmac_parts(drbg, &v, 1, drbg->state.hmac.v);
  }
  return result;
}

/* Instantiate (10.1.2.3): Key = 0x00..00, V = 0x01..01, then
 * Update(entropy || nonce || personalization). Reseed (10.1.2.4) runs
 * Update(entropy || additional) on the current state. */
TC_DRBG_result tc_drbg_hmac_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed)
{
  if (!reseed) {
    memset(drbg->state.hmac.key, 0x00, drbg->output_bytes);
    memset(drbg->state.hmac.v, 0x01, drbg->output_bytes);
  }
  return update(drbg, parts, count);
}

/* Generate (10.1.2.5). */
TC_DRBG_result tc_drbg_hmac_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                     TC_bytes additional)
{
  const TC_bytes v = {drbg->state.hmac.v, drbg->output_bytes};
  size_t offset = 0;
  TC_DRBG_result result = TC_DRBG_OK;

  if (additional.length != 0)
    result = update(drbg, &additional, 1);
  while (result == TC_DRBG_OK && offset < length) {
    const size_t take = length - offset < drbg->output_bytes ? length - offset : drbg->output_bytes;
    result = hmac_parts(drbg, &v, 1, drbg->state.hmac.v);
    if (result == TC_DRBG_OK)
      memcpy(output + offset, drbg->state.hmac.v, take);
    offset += take;
  }
  if (result == TC_DRBG_OK)
    result = update(drbg, &additional, 1);
  return result;
}

#endif
