/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_PSS_INTERNAL_H_
#define TC_RSA_PSS_INTERNAL_H_
#include "rsa_mgf_internal.h"
#include <string.h>

static inline TC_RSA_result tc_rsa_pss_prepare(size_t length, size_t bits,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, size_t digest_length,
    size_t salt_length, size_t* work, tc_hash_info* info)
{
  size_t cost;
  if (!tc_hash_info_get(hash,info) || !tc_hash_available(hash) || !tc_hash_available(mgf_hash))
    return TC_RSA_UNSUPPORTED;
  if (digest_length != info->digest_length) return TC_RSA_ARGUMENT;
  if (!bits || length != bits / 8 + (bits % 8 != 0) || length < info->digest_length + 2 ||
      salt_length > length - info->digest_length - 2) return TC_RSA_INVALID;
  if (salt_length > SIZE_MAX - info->digest_length - 9) return TC_RSA_LIMIT;
  cost = salt_length + info->digest_length + 9;
  if (length > *work || cost > *work - length) return TC_RSA_LIMIT;
  *work -= length + cost;
  return TC_RSA_OK;
}

static inline TC_status tc_rsa_pss_hash(TC_hash_algorithm hash, TC_bytes digest,
    TC_bytes salt, uint8_t* output, tc_hash_workspace* workspace)
{
  static const uint8_t zeros[8] = {0};
  TC_bytes parts[] = {{zeros,sizeof zeros},digest,salt};
  return tc_hash_digest_parts(hash,parts,3,output,workspace);
}

/* Salt bytes come from the caller's cryptographic RNG. Output may change on
 * failure; callers must discard it. Inputs, output, block and workspace are
 * disjoint. Salt is copied only into its encoded-message field. */
static inline TC_RSA_result tc_rsa_pss_encode(uint8_t* encoded, size_t length,
    size_t bits, TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes digest,
    TC_bytes salt, uint8_t* block, tc_hash_workspace* workspace, size_t* work)
{
  tc_hash_info info;
  TC_bytes h;
  TC_RSA_result result;
  size_t db_length, padding;
  if (!encoded || !digest.data || (salt.length && !salt.data) || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  result = tc_rsa_pss_prepare(length,bits,hash,mgf_hash,digest.length,salt.length,work,&info);
  if (result != TC_RSA_OK) return result;
  db_length = length - info.digest_length - 1;
  padding = db_length - salt.length - 1;
  if (tc_rsa_pss_hash(hash,digest,salt,encoded + db_length,workspace) != TC_OK) return TC_RSA_ARGUMENT;
  memset(encoded,0,padding); encoded[padding] = 1;
  if (salt.length) memcpy(encoded + padding + 1,salt.data,salt.length);
  h = (TC_bytes){encoded + db_length,info.digest_length};
  result = tc_rsa_mgf1_xor(mgf_hash,h,encoded,db_length,block,workspace,work);
  if (result != TC_RSA_OK) return result;
  encoded[0] &= (uint8_t)(0xffu >> ((8 - bits % 8) % 8));
  encoded[length - 1] = 0xbc;
  return TC_RSA_OK;
}

/* RFC 8017 section 9.1.2, with an explicit salt length and precomputed digest.
 * encoded is mutable scratch; it may change even on failure. Other inputs,
 * hash workspace, block and work are disjoint from it and each other. block
 * holds the larger of the message-hash and MGF-hash digests. */
static inline TC_RSA_result tc_rsa_pss_check(uint8_t* encoded, size_t length,
    size_t bits, TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes digest,
    size_t salt_length, uint8_t* block, tc_hash_workspace* workspace, size_t* work)
{
  tc_hash_info info;
  TC_RSA_result result;
  size_t db_length, padding;
  unsigned difference = 0, unused;
  uint8_t allowed;
  TC_bytes h;
  if (!encoded || !digest.data || !block || !workspace || !work) return TC_RSA_ARGUMENT;
  result = tc_rsa_pss_prepare(length,bits,hash,mgf_hash,digest.length,salt_length,work,&info);
  if (result != TC_RSA_OK) return result;
  unused = (unsigned)((8 - bits % 8) % 8);
  allowed = (uint8_t)(0xffu >> unused);
  if (encoded[length - 1] != 0xbc || (encoded[0] & (uint8_t)~allowed)) return TC_RSA_INVALID;
  db_length = length - info.digest_length - 1;
  h = (TC_bytes){encoded + db_length,info.digest_length};
  result = tc_rsa_mgf1_xor(mgf_hash,h,encoded,db_length,block,workspace,work);
  if (result != TC_RSA_OK) return result;
  encoded[0] &= allowed;
  padding = db_length - salt_length - 1;
  for (size_t i = 0; i < padding; ++i) difference |= encoded[i];
  difference |= encoded[padding] ^ 1u;
  if (tc_rsa_pss_hash(hash,digest,(TC_bytes){encoded + padding + 1,salt_length},block,workspace) != TC_OK) {
    TC_secure_zero(block,info.digest_length);
    return TC_RSA_ARGUMENT;
  }
  for (size_t i = 0; i < info.digest_length; ++i) difference |= block[i] ^ h.data[i];
  TC_secure_zero(block,info.digest_length);
  return difference ? TC_RSA_INVALID : TC_RSA_OK;
}
#endif
