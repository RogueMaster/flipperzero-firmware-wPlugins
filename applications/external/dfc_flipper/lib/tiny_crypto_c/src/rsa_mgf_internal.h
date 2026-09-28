/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_RSA_MGF_INTERNAL_H_
#define TC_RSA_MGF_INTERNAL_H_
#include <tiny_crypto/rsa.h>
#include "hash_dispatch_internal.h"

/* RFC 8017 B.2.1. XOR into an existing mask buffer. Seed, output, block,
 * workspace and work are disjoint. block holds the selected digest size.
 * Caller validates address ranges. Preflight failures leave output unchanged;
 * an unexpected hash failure may leave a partially masked buffer. */
static inline TC_RSA_result tc_rsa_mgf1_xor(TC_hash_algorithm hash, TC_bytes seed,
    uint8_t* output, size_t length, uint8_t* block, tc_hash_workspace* workspace,
    size_t* work)
{
  tc_hash_info info;
  size_t blocks, per_block, offset = 0;
  if ((seed.length && !seed.data) || (length && !output) || !block || !workspace || !work)
    return TC_RSA_ARGUMENT;
  if (!tc_hash_info_get(hash,&info) || !tc_hash_available(hash)) return TC_RSA_UNSUPPORTED;
  blocks = length / info.digest_length + (length % info.digest_length != 0);
#if SIZE_MAX > UINT32_MAX
  if (blocks && blocks - 1 > UINT32_MAX) return TC_RSA_LIMIT;
#endif
  if (!blocks) return TC_RSA_OK;
  if (seed.length > SIZE_MAX - 5) return TC_RSA_LIMIT;
  per_block = seed.length + 5; /* seed, counter, and one hash invocation */
  if (length > *work || blocks > (*work - length) / per_block) return TC_RSA_LIMIT;
  *work -= length + blocks * per_block;
  for (size_t i = 0; i < blocks; ++i) {
    uint32_t counter = (uint32_t)i;
    uint8_t encoded[] = {(uint8_t)(counter >> 24),(uint8_t)(counter >> 16),
                         (uint8_t)(counter >> 8),(uint8_t)counter};
    TC_bytes parts[] = {seed,{encoded,sizeof encoded}};
    size_t take = length - offset;
    if (take > info.digest_length) take = info.digest_length;
    if (tc_hash_digest_parts(hash,parts,2,block,workspace) != TC_OK) {
      TC_secure_zero(block,info.digest_length);
      return TC_RSA_ARGUMENT;
    }
    for (size_t j = 0; j < take; ++j) output[offset + j] ^= block[j];
    offset += take;
  }
  TC_secure_zero(block,info.digest_length);
  return TC_RSA_OK;
}
#endif
