/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hash_DRBG (SP 800-90A section 10.1.1) and Hash_df (section 10.3.1). The
 * working state is V and C, each seedlen bits. Hashing runs through the
 * shared hash dispatch in the DRBG's scratch context. */
#include <tiny_crypto/drbg.h>

#if TC_DRBG_HAVE_HASH
#include <string.h>
#include "drbg_internal.h"
#include "hash_dispatch_internal.h"
#include "internal.h"

TC_DRBG_result tc_drbg_hash_parameters(TC_hash_algorithm hash, tc_drbg_parameters* out)
{
  const uint16_t strength = tc_drbg_hash_strength(hash);
  const tc_hash_algorithm_info* info = tc_hash_core_lookup(hash);
  if (strength == 0)
    return TC_DRBG_ARGUMENT;
  if (info == NULL)
    return TC_DRBG_UNSUPPORTED;
  memset(out, 0, sizeof *out);
  out->strength_bits = strength;
  out->seed_bytes = hash == TC_HASH_SHA384 || hash == TC_HASH_SHA512 ? 111u : 55u;
  out->output_bytes = (uint8_t)tc_hash_core_digest_bytes(info);
  out->uses_nonce = 1;
  return TC_DRBG_OK;
}

/* Hash the parts in order into digest. */
static TC_DRBG_result hash_parts(TC_DRBG* drbg, const TC_bytes* parts, size_t count,
                                 uint8_t* digest)
{
  return tc_hash_digest_parts((TC_hash_algorithm)drbg->hash, parts, count, digest,
                              &drbg->scratch.hash) == TC_OK
             ? TC_DRBG_OK
             : TC_DRBG_ERROR;
}

/* Hash_df: out = leftmost length bytes of
 * Hash(1 || bits) || Hash(2 || bits) || ..., each over the input parts. The
 * prefix is the counter byte and the 32-bit big-endian output bit count. At
 * most 4 input parts follow the prefix. out must stay clear of the parts. */
static TC_DRBG_result hash_df(TC_DRBG* drbg, const TC_bytes* input, size_t count, uint8_t* out,
                              size_t length)
{
  uint8_t prefix[5];
  uint8_t digest[64]; /* largest SHA-2 digest */
  TC_bytes parts[5];
  size_t offset = 0, i;
  TC_DRBG_result result = TC_DRBG_OK;

  tc_internal_store_be32(prefix + 1, (uint32_t)(length * 8u));
  parts[0] = (TC_bytes){prefix, sizeof prefix};
  for (i = 0; i < count; ++i)
    parts[i + 1] = input[i];
  for (prefix[0] = 1; result == TC_DRBG_OK && offset < length; ++prefix[0]) {
    const size_t take = length - offset < drbg->output_bytes ? length - offset : drbg->output_bytes;
    result = hash_parts(drbg, parts, count + 1, digest);
    if (result == TC_DRBG_OK)
      memcpy(out + offset, digest, take);
    offset += take;
  }
  TC_secure_zero(digest, sizeof digest);
  return result;
}

/* value = (value + addend) mod 2^(8 * value_length), big-endian, with addend
 * right-aligned and at most value_length bytes long. */
static void add_mod(uint8_t* value, size_t value_length, const uint8_t* addend,
                    size_t addend_length)
{
  unsigned carry = 0;
  size_t i;
  for (i = 0; i < value_length; ++i) {
    const size_t v = value_length - 1u - i;
    const unsigned term = i < addend_length ? addend[addend_length - 1u - i] : 0u;
    carry += value[v] + term;
    value[v] = (uint8_t)carry;
    carry >>= 8;
  }
}

/* Instantiate (10.1.1.2): seed = Hash_df(entropy || nonce || personalization).
 * Reseed (10.1.1.3): seed = Hash_df(0x01 || V || entropy || additional).
 * Both then set V = seed and C = Hash_df(0x00 || V). */
TC_DRBG_result tc_drbg_hash_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed)
{
  static const uint8_t one = 0x01, zero = 0x00;
  uint8_t seed[TC_DRBG_HASH_SEED_BYTES];
  TC_bytes input[4];
  size_t used = 0, i;
  TC_DRBG_result result;

  if (reseed) {
    input[used++] = (TC_bytes){&one, 1};
    input[used++] = (TC_bytes){drbg->state.hash.v, drbg->seed_bytes};
  }
  for (i = 0; i < count; ++i)
    input[used++] = parts[i];
  result = hash_df(drbg, input, used, seed, drbg->seed_bytes);
  if (result == TC_DRBG_OK) {
    memcpy(drbg->state.hash.v, seed, drbg->seed_bytes);
    input[0] = (TC_bytes){&zero, 1};
    input[1] = (TC_bytes){drbg->state.hash.v, drbg->seed_bytes};
    result = hash_df(drbg, input, 2, drbg->state.hash.c, drbg->seed_bytes);
  }
  TC_secure_zero(seed, sizeof seed);
  return result;
}

/* Generate (10.1.1.4) with Hashgen (10.1.1.4 step 3). */
TC_DRBG_result tc_drbg_hash_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                     TC_bytes additional)
{
  static const uint8_t two = 0x02, three = 0x03;
  const size_t seed_bytes = drbg->seed_bytes;
  uint8_t data[TC_DRBG_HASH_SEED_BYTES];
  uint8_t digest[64];
  uint8_t counter[8];
  TC_bytes parts[3];
  size_t offset = 0;
  TC_DRBG_result result = TC_DRBG_OK;

  /* Step 2: fold additional input into V. */
  if (additional.length != 0) {
    parts[0] = (TC_bytes){&two, 1};
    parts[1] = (TC_bytes){drbg->state.hash.v, seed_bytes};
    parts[2] = additional;
    result = hash_parts(drbg, parts, 3, digest);
    if (result == TC_DRBG_OK)
      add_mod(drbg->state.hash.v, seed_bytes, digest, drbg->output_bytes);
  }

  /* Step 3: Hashgen hashes successive values of data = V. */
  memcpy(data, drbg->state.hash.v, seed_bytes);
  parts[0] = (TC_bytes){data, seed_bytes};
  while (result == TC_DRBG_OK && offset < length) {
    const size_t take = length - offset < drbg->output_bytes ? length - offset : drbg->output_bytes;
    result = hash_parts(drbg, parts, 1, digest);
    if (result == TC_DRBG_OK)
      memcpy(output + offset, digest, take);
    offset += take;
    tc_internal_increment_be(data, seed_bytes);
  }

  /* Steps 4 to 6: V = V + Hash(0x03 || V) + C + reseed_counter. */
  if (result == TC_DRBG_OK) {
    parts[0] = (TC_bytes){&three, 1};
    parts[1] = (TC_bytes){drbg->state.hash.v, seed_bytes};
    result = hash_parts(drbg, parts, 2, digest);
  }
  if (result == TC_DRBG_OK) {
    tc_internal_store_be64(counter, drbg->reseed_counter);
    add_mod(drbg->state.hash.v, seed_bytes, digest, drbg->output_bytes);
    add_mod(drbg->state.hash.v, seed_bytes, drbg->state.hash.c, seed_bytes);
    add_mod(drbg->state.hash.v, seed_bytes, counter, sizeof counter);
  }
  TC_secure_zero(data, sizeof data);
  TC_secure_zero(digest, sizeof digest);
  return result;
}

#endif
