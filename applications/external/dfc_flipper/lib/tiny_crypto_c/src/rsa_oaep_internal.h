/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_OAEP_INTERNAL_H_
#define TC_RSA_OAEP_INTERNAL_H_
#include "rsa_mgf_internal.h"
#include <string.h>

static inline TC_RSA_result tc_rsa_oaep_prepare(size_t length,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes label,
    size_t* work, tc_hash_info* info)
{
  if (!tc_hash_info_get(hash,info) || !tc_hash_available(hash) || !tc_hash_available(mgf_hash))
    return TC_RSA_UNSUPPORTED;
  if (length < 2 * info->digest_length + 2) return TC_RSA_INVALID;
  if (length == SIZE_MAX || label.length > SIZE_MAX - length - 1) return TC_RSA_LIMIT;
  const size_t cost = length + label.length + 1;
  if (cost > *work) return TC_RSA_LIMIT;
  *work -= cost;
  return TC_RSA_OK;
}

/* RFC 8017 7.1.1. Seed is hLen bytes from a cryptographic RNG. Caller validates
 * disjoint ranges; block holds the larger hash digest. Encoded bytes are
 * provisional on failure. Message and seed are copied into their fields only. */
static inline TC_RSA_result tc_rsa_oaep_encode(uint8_t* encoded, size_t length,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes label,
    TC_bytes message, TC_bytes seed, uint8_t* block, tc_hash_workspace* workspace,
    size_t* work)
{
  tc_hash_info info;
  if (!encoded || (label.length && !label.data) || (message.length && !message.data) ||
      !seed.data || !block || !workspace || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_oaep_prepare(length,hash,mgf_hash,label,work,&info);
  if (result != TC_RSA_OK) return result;
  const size_t h = info.digest_length, db_length = length - h - 1;
  if (seed.length != h) return TC_RSA_ARGUMENT;
  if (message.length > db_length - h - 1) return TC_RSA_INVALID;
  uint8_t* db = encoded + h + 1;
  if (tc_hash_digest_parts(hash,&label,1,db,workspace) != TC_OK) return TC_RSA_ARGUMENT;
  const size_t padding = db_length - h - message.length - 1;
  memset(db + h,0,padding); db[h + padding] = 1;
  if (message.length) memcpy(db + h + padding + 1,message.data,message.length);
  encoded[0] = 0; memcpy(encoded + 1,seed.data,h);
  result = tc_rsa_mgf1_xor(mgf_hash,(TC_bytes){encoded + 1,h},db,db_length,block,workspace,work);
  if (result != TC_RSA_OK) return result;
  return tc_rsa_mgf1_xor(mgf_hash,(TC_bytes){db,db_length},encoded + 1,h,block,workspace,work);
}

/* RFC 8017 7.1.2. Decode in caller-owned scratch and publish a borrowed message
 * only on success. All other ranges are disjoint. Wipe encoded after use or
 * failure. Invalid padding takes the same scan and returns one error status. */
static inline TC_RSA_result tc_rsa_oaep_decode(uint8_t* encoded, size_t length,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes label,
    uint8_t* block, tc_hash_workspace* workspace, size_t* work, TC_bytes* message)
{
  tc_hash_info info;
  if (!encoded || (label.length && !label.data) || !block || !workspace || !work || !message)
    return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_oaep_prepare(length,hash,mgf_hash,label,work,&info);
  if (result != TC_RSA_OK) return result;
  const size_t h = info.digest_length, db_length = length - h - 1;
  uint8_t* db = encoded + h + 1;
  result = tc_rsa_mgf1_xor(mgf_hash,(TC_bytes){db,db_length},encoded + 1,h,block,workspace,work);
  if (result != TC_RSA_OK) return result;
  result = tc_rsa_mgf1_xor(mgf_hash,(TC_bytes){encoded + 1,h},db,db_length,block,workspace,work);
  if (result != TC_RSA_OK) return result;
  if (tc_hash_digest_parts(hash,&label,1,block,workspace) != TC_OK) return TC_RSA_ARGUMENT;
  unsigned difference = encoded[0];
  for (size_t i = 0; i < h; ++i) difference |= db[i] ^ block[i];
  TC_secure_zero(block,h);
  size_t start = 0;
  unsigned searching = 1;
  for (size_t i = h; i < db_length; ++i) {
    const unsigned zero = ((uint32_t)db[i] - 1u) >> 31;
    const unsigned one = ((uint32_t)(db[i] ^ 1u) - 1u) >> 31;
    const size_t take = (size_t)0 - (size_t)(searching & one);
    start = (start & ~take) | ((i + 1) & take);
    difference |= searching & ((zero | one) ^ 1u);
    searching &= one ^ 1u;
  }
  if (difference | searching) return TC_RSA_INVALID;
  *message = (TC_bytes){db + start,db_length - start};
  return TC_RSA_OK;
}
#endif
