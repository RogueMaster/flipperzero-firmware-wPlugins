/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_PADDING_INTERNAL_H_
#define TC_RSA_PADDING_INTERNAL_H_
#include <tiny_crypto/rsa.h>
#include "hash_dispatch_internal.h"
#include <string.h>

/* Work charged by tc_rsa_mgf1_xor for a mask of length bytes: the mask bytes,
 * plus the seed, the 4-byte counter and one hash invocation per block. An
 * empty mask costs nothing. Returns LIMIT when the counter would exceed 32
 * bits or the cost exceeds SIZE_MAX. */
static inline TC_RSA_result tc_rsa_mgf1_cost(TC_hash_algorithm hash, size_t seed_length,
                                             size_t length, size_t* cost)
{
  tc_hash_info info;
  if (!tc_hash_info_get(hash, &info) || !tc_hash_available(hash))
    return TC_RSA_UNSUPPORTED;
  const size_t blocks = length / info.digest_length + (length % info.digest_length != 0);
  *cost = 0;
  if (!blocks)
    return TC_RSA_OK;
#if SIZE_MAX > UINT32_MAX
  if (blocks - 1 > UINT32_MAX)
    return TC_RSA_LIMIT;
#endif
  if (seed_length > SIZE_MAX - 5)
    return TC_RSA_LIMIT;
  const size_t per_block = seed_length + 5;
  if (blocks > (SIZE_MAX - length) / per_block)
    return TC_RSA_LIMIT;
  *cost = length + blocks * per_block;
  return TC_RSA_OK;
}

/* RFC 8017 B.2.1. XOR into an existing mask buffer. Seed, output, block,
 * workspace and work are disjoint. block holds the selected digest size.
 * Caller validates address ranges. Preflight failures leave output unchanged.
 * An unexpected hash failure may leave a partially masked buffer. */
static inline TC_RSA_result tc_rsa_mgf1_xor(TC_hash_algorithm hash, TC_bytes seed, uint8_t* output,
                                            size_t length, uint8_t* block,
                                            TC_hash_context* workspace, uint32_t* work)
{
  tc_hash_info info;
  size_t cost, blocks, offset = 0;
  if ((seed.length && !seed.data) || (length && !output) || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_mgf1_cost(hash, seed.length, length, &cost);
  if (result != TC_RSA_OK)
    return result;
  if (!cost)
    return TC_RSA_OK;
  if (cost > *work)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost;             /* cost <= *work, checked above. */
  (void)tc_hash_info_get(hash, &info); /* tc_rsa_mgf1_cost accepted hash. */
  blocks = length / info.digest_length + (length % info.digest_length != 0);
  for (size_t i = 0; i < blocks; ++i) {
    uint32_t counter = (uint32_t)i;
    uint8_t encoded[] = {(uint8_t)(counter >> 24), (uint8_t)(counter >> 16),
                         (uint8_t)(counter >> 8), (uint8_t)counter};
    TC_bytes parts[] = {seed, {encoded, sizeof encoded}};
    size_t take = length - offset;
    if (take > info.digest_length)
      take = info.digest_length;
    if (tc_hash_digest_parts(hash, parts, 2, block, workspace) != TC_OK) {
      TC_secure_zero(block, info.digest_length);
      return TC_RSA_ERROR;
    }
    for (size_t j = 0; j < take; ++j)
      output[offset + j] ^= block[j];
    offset += take;
  }
  TC_secure_zero(block, info.digest_length);
  return TC_RSA_OK;
}

/* Hash scratch for one padding operation: a digest-sized block (64 bytes)
 * and a hash context. Both are wiped by the caller. */
typedef struct {
  uint8_t* block;
  TC_hash_context* context;
} tc_rsa_hash_scratch;

/* Validate EMSA-PSS parameters (RFC 8017 section 9.1) and return the work
 * charged before MGF1: the encoded message, salt, digest and the fixed
 * 8-byte prefix plus one hash invocation. */
static inline TC_RSA_result tc_rsa_pss_parameters(size_t length, size_t bits,
                                                  TC_hash_algorithm hash,
                                                  TC_hash_algorithm mgf_hash, size_t salt_length,
                                                  tc_hash_info* info, size_t* cost)
{
  if (!tc_hash_info_get(hash, info) || !tc_hash_available(hash) || !tc_hash_available(mgf_hash))
    return TC_RSA_UNSUPPORTED;
  if (!bits || length != bits / 8 + (bits % 8 != 0) || length < info->digest_length + 2 ||
      salt_length > length - info->digest_length - 2)
    return TC_RSA_INVALID;
  if (salt_length > SIZE_MAX - info->digest_length - 9 ||
      length > SIZE_MAX - salt_length - info->digest_length - 9)
    return TC_RSA_LIMIT;
  *cost = length + salt_length + info->digest_length + 9;
  return TC_RSA_OK;
}

/* Total work of a successful tc_rsa_pss_encode or tc_rsa_pss_check. */
static inline TC_RSA_result tc_rsa_pss_cost(size_t length, size_t bits, TC_hash_algorithm hash,
                                            TC_hash_algorithm mgf_hash, size_t salt_length,
                                            size_t* cost)
{
  tc_hash_info info;
  size_t mask_cost;
  TC_RSA_result result =
      tc_rsa_pss_parameters(length, bits, hash, mgf_hash, salt_length, &info, cost);
  if (result != TC_RSA_OK)
    return result;
  result =
      tc_rsa_mgf1_cost(mgf_hash, info.digest_length, length - info.digest_length - 1, &mask_cost);
  if (result != TC_RSA_OK)
    return result;
  if (mask_cost > SIZE_MAX - *cost)
    return TC_RSA_LIMIT;
  *cost += mask_cost;
  return TC_RSA_OK;
}

/* Check PSS arguments and report the encode or verify cost without charging
 * it. info and cost are valid on OK. */
static inline TC_RSA_result tc_rsa_pss_plan(size_t length, size_t bits, TC_hash_algorithm hash,
                                            TC_hash_algorithm mgf_hash, size_t digest_length,
                                            size_t salt_length, tc_hash_info* info, size_t* cost)
{
  TC_RSA_result result =
      tc_rsa_pss_parameters(length, bits, hash, mgf_hash, salt_length, info, cost);
  /* A digest of the wrong size is a caller error and outranks parameter
   * problems in the encoded-message shape. */
  if (result != TC_RSA_UNSUPPORTED && digest_length != info->digest_length)
    return TC_RSA_ARGUMENT;
  return result;
}

static inline TC_RSA_result tc_rsa_pss_prepare(size_t length, size_t bits, TC_hash_algorithm hash,
                                               TC_hash_algorithm mgf_hash, size_t digest_length,
                                               size_t salt_length, uint32_t* work,
                                               tc_hash_info* info)
{
  size_t cost;
  TC_RSA_result result =
      tc_rsa_pss_plan(length, bits, hash, mgf_hash, digest_length, salt_length, info, &cost);
  if (result != TC_RSA_OK)
    return result;
  if (cost > *work)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost; /* cost <= *work, checked above. */
  return TC_RSA_OK;
}

static inline TC_status tc_rsa_pss_hash(TC_hash_algorithm hash, TC_bytes digest, TC_bytes salt,
                                        uint8_t* output, TC_hash_context* workspace)
{
  static const uint8_t zeros[8] = {0};
  TC_bytes parts[] = {{zeros, sizeof zeros}, digest, salt};
  return tc_hash_digest_parts(hash, parts, 3, output, workspace);
}

/* Salt bytes come from the caller's cryptographic RNG. Output may change on
 * failure, and callers must discard it. Inputs, output, block and workspace are
 * disjoint. Salt is copied only into its encoded-message field. */
static inline TC_RSA_result tc_rsa_pss_encode(const TC_RSA_pss_options* options, TC_buffer output,
                                              size_t bits, TC_bytes digest, TC_bytes salt,
                                              tc_rsa_hash_scratch hashes, uint32_t* work)
{
  uint8_t* const encoded = output.data;
  const size_t length = output.capacity;
  const TC_hash_algorithm hash = options->hash, mgf_hash = options->mgf_hash;
  uint8_t* const block = hashes.block;
  TC_hash_context* const workspace = hashes.context;
  tc_hash_info info;
  TC_bytes h;
  TC_RSA_result result;
  size_t db_length, padding;
  if (!encoded || !digest.data || (salt.length && !salt.data) || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  result =
      tc_rsa_pss_prepare(length, bits, hash, mgf_hash, digest.length, salt.length, work, &info);
  if (result != TC_RSA_OK)
    return result;
  db_length = length - info.digest_length - 1;
  padding = db_length - salt.length - 1;
  if (tc_rsa_pss_hash(hash, digest, salt, encoded + db_length, workspace) != TC_OK)
    return TC_RSA_ERROR;
  memset(encoded, 0, padding);
  encoded[padding] = 1;
  if (salt.length)
    memcpy(encoded + padding + 1, salt.data, salt.length);
  h = (TC_bytes){encoded + db_length, info.digest_length};
  result = tc_rsa_mgf1_xor(mgf_hash, h, encoded, db_length, block, workspace, work);
  if (result != TC_RSA_OK)
    return result;
  encoded[0] &= (uint8_t)(0xffu >> ((8 - bits % 8) % 8));
  encoded[length - 1] = 0xbc;
  return TC_RSA_OK;
}

/* RFC 8017 section 9.1.2, with an explicit salt length and precomputed digest.
 * encoded is mutable scratch and may change on failure. Other inputs,
 * hash workspace, block and work are disjoint from it and each other. block
 * holds the larger of the message-hash and MGF-hash digests. */
static inline TC_RSA_result tc_rsa_pss_check(const TC_RSA_pss_options* options, TC_buffer message,
                                             size_t bits, TC_bytes digest,
                                             tc_rsa_hash_scratch hashes, uint32_t* work)
{
  uint8_t* const encoded = message.data;
  const size_t length = message.capacity, salt_length = options->salt_length;
  const TC_hash_algorithm hash = options->hash, mgf_hash = options->mgf_hash;
  uint8_t* const block = hashes.block;
  TC_hash_context* const workspace = hashes.context;
  tc_hash_info info;
  TC_RSA_result result;
  size_t db_length, padding;
  unsigned difference = 0, unused;
  uint8_t allowed;
  TC_bytes h;
  if (!encoded || !digest.data || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  result =
      tc_rsa_pss_prepare(length, bits, hash, mgf_hash, digest.length, salt_length, work, &info);
  if (result != TC_RSA_OK)
    return result;
  unused = (unsigned)((8 - bits % 8) % 8);
  allowed = (uint8_t)(0xffu >> unused);
  if (encoded[length - 1] != 0xbc || (encoded[0] & (uint8_t)~allowed))
    return TC_RSA_INVALID;
  db_length = length - info.digest_length - 1;
  h = (TC_bytes){encoded + db_length, info.digest_length};
  result = tc_rsa_mgf1_xor(mgf_hash, h, encoded, db_length, block, workspace, work);
  if (result != TC_RSA_OK)
    return result;
  encoded[0] &= allowed;
  padding = db_length - salt_length - 1;
  for (size_t i = 0; i < padding; ++i)
    difference |= encoded[i];
  difference |= encoded[padding] ^ 1u;
  if (tc_rsa_pss_hash(hash, digest, (TC_bytes){encoded + padding + 1, salt_length}, block,
                      workspace) != TC_OK) {
    TC_secure_zero(block, info.digest_length);
    return TC_RSA_ERROR;
  }
  for (size_t i = 0; i < info.digest_length; ++i)
    difference |= block[i] ^ h.data[i];
  TC_secure_zero(block, info.digest_length);
  return difference ? TC_RSA_INVALID : TC_RSA_OK;
}

/* Validate RSAES-OAEP parameters (RFC 8017 section 7.1) and return the work
 * charged before MGF1: the encoded message, the label and one hash
 * invocation. info and cost are valid on OK. */
static inline TC_RSA_result tc_rsa_oaep_parameters(size_t length, TC_hash_algorithm hash,
                                                   TC_hash_algorithm mgf_hash, size_t label_length,
                                                   tc_hash_info* info, size_t* cost)
{
  if (!tc_hash_info_get(hash, info) || !tc_hash_available(hash) || !tc_hash_available(mgf_hash))
    return TC_RSA_UNSUPPORTED;
  /* RFC 8017 section 7.1.2 step 1.c: k >= 2 hLen + 2. */
  if (length < 2 * info->digest_length + 2)
    return TC_RSA_INVALID;
  if (length == SIZE_MAX || label_length > SIZE_MAX - length - 1)
    return TC_RSA_LIMIT;
  *cost = length + label_length + 1;
  return TC_RSA_OK;
}

/* Total work of a successful tc_rsa_oaep_encode or tc_rsa_oaep_decode: the
 * parameter charge, the dbMask over DB and the seedMask over the seed. */
static inline TC_RSA_result tc_rsa_oaep_cost(size_t length, TC_hash_algorithm hash,
                                             TC_hash_algorithm mgf_hash, size_t label_length,
                                             size_t* cost)
{
  tc_hash_info info;
  size_t db_mask = 0, seed_mask = 0;
  TC_RSA_result result = tc_rsa_oaep_parameters(length, hash, mgf_hash, label_length, &info, cost);
  if (result != TC_RSA_OK)
    return result;
  const size_t h = info.digest_length, db_length = length - h - 1;
  result = tc_rsa_mgf1_cost(mgf_hash, h, db_length, &db_mask);
  if (result == TC_RSA_OK)
    result = tc_rsa_mgf1_cost(mgf_hash, db_length, h, &seed_mask);
  if (result != TC_RSA_OK)
    return result;
  if (db_mask > SIZE_MAX - *cost || seed_mask > SIZE_MAX - *cost - db_mask)
    return TC_RSA_LIMIT;
  *cost += db_mask + seed_mask;
  return TC_RSA_OK;
}

static inline TC_RSA_result tc_rsa_oaep_prepare(size_t length, TC_hash_algorithm hash,
                                                TC_hash_algorithm mgf_hash, TC_bytes label,
                                                uint32_t* work, tc_hash_info* info)
{
  size_t cost;
  TC_RSA_result result = tc_rsa_oaep_parameters(length, hash, mgf_hash, label.length, info, &cost);
  if (result != TC_RSA_OK)
    return result;
  if (cost > *work)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)cost; /* cost <= *work, checked above. */
  return TC_RSA_OK;
}

/* RFC 8017 7.1.1. Seed is hLen bytes from a cryptographic RNG. Caller validates
 * disjoint ranges. block holds the larger hash digest. Encoded bytes are
 * provisional on failure. Message and seed are copied into their fields only. */
static inline TC_RSA_result tc_rsa_oaep_encode(const TC_RSA_oaep_options* options, TC_buffer output,
                                               TC_bytes message, TC_bytes seed,
                                               tc_rsa_hash_scratch hashes, uint32_t* work)
{
  uint8_t* const encoded = output.data;
  const size_t length = output.capacity;
  const TC_bytes label = options->label;
  const TC_hash_algorithm hash = options->hash, mgf_hash = options->mgf_hash;
  uint8_t* const block = hashes.block;
  TC_hash_context* const workspace = hashes.context;
  tc_hash_info info;
  if (!encoded || (label.length && !label.data) || (message.length && !message.data) ||
      !seed.data || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_oaep_prepare(length, hash, mgf_hash, label, work, &info);
  if (result != TC_RSA_OK)
    return result;
  const size_t h = info.digest_length, db_length = length - h - 1;
  if (seed.length != h)
    return TC_RSA_ARGUMENT;
  if (message.length > db_length - h - 1)
    return TC_RSA_INVALID;
  uint8_t* db = encoded + h + 1;
  if (tc_hash_digest_parts(hash, &label, 1, db, workspace) != TC_OK)
    return TC_RSA_ERROR;
  const size_t padding = db_length - h - message.length - 1;
  memset(db + h, 0, padding);
  db[h + padding] = 1;
  if (message.length)
    memcpy(db + h + padding + 1, message.data, message.length);
  encoded[0] = 0;
  memcpy(encoded + 1, seed.data, h);
  result =
      tc_rsa_mgf1_xor(mgf_hash, (TC_bytes){encoded + 1, h}, db, db_length, block, workspace, work);
  if (result != TC_RSA_OK)
    return result;
  return tc_rsa_mgf1_xor(mgf_hash, (TC_bytes){db, db_length}, encoded + 1, h, block, workspace,
                         work);
}

/* RFC 8017 7.1.2. Decode in caller-owned scratch and publish a borrowed message
 * only on success. All other ranges are disjoint. Wipe encoded after use or
 * failure. Invalid padding takes the same scan and returns one error status. */
static inline TC_RSA_result tc_rsa_oaep_decode(const TC_RSA_oaep_options* options, TC_buffer input,
                                               tc_rsa_hash_scratch hashes, uint32_t* work,
                                               TC_bytes* message)
{
  uint8_t* const encoded = input.data;
  const size_t length = input.capacity;
  const TC_bytes label = options->label;
  const TC_hash_algorithm hash = options->hash, mgf_hash = options->mgf_hash;
  uint8_t* const block = hashes.block;
  TC_hash_context* const workspace = hashes.context;
  tc_hash_info info;
  if (!encoded || (label.length && !label.data) || !block || !workspace || !work || !message)
    return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_oaep_prepare(length, hash, mgf_hash, label, work, &info);
  if (result != TC_RSA_OK)
    return result;
  const size_t h = info.digest_length, db_length = length - h - 1;
  uint8_t* db = encoded + h + 1;
  result =
      tc_rsa_mgf1_xor(mgf_hash, (TC_bytes){db, db_length}, encoded + 1, h, block, workspace, work);
  if (result != TC_RSA_OK)
    return result;
  result =
      tc_rsa_mgf1_xor(mgf_hash, (TC_bytes){encoded + 1, h}, db, db_length, block, workspace, work);
  if (result != TC_RSA_OK)
    return result;
  if (tc_hash_digest_parts(hash, &label, 1, block, workspace) != TC_OK)
    return TC_RSA_ERROR;
  unsigned difference = encoded[0];
  for (size_t i = 0; i < h; ++i)
    difference |= db[i] ^ block[i];
  TC_secure_zero(block, h);
  size_t start = 0;
  unsigned searching = 1;
  for (size_t i = h; i < db_length; ++i) {
    const unsigned zero = (unsigned)(((uint32_t)db[i] - 1u) >> 31);
    const unsigned one = (unsigned)(((uint32_t)(db[i] ^ 1u) - 1u) >> 31);
    const size_t take = (size_t)0 - (size_t)(searching & one);
    start = (start & ~take) | ((i + 1) & take);
    difference |= searching & ((zero | one) ^ 1u);
    searching &= one ^ 1u;
  }
  if (difference | searching)
    return TC_RSA_INVALID;
  *message = (TC_bytes){db + start, db_length - start};
  return TC_RSA_OK;
}
#endif
