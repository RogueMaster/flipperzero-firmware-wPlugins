/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_sign.h"

enum { BLINDING_ATTEMPTS = 4, SHA256_BYTES = EXAMPLE_RSA_PSS_SHA256_BYTES };

/* Add two work costs. A zero cost means the operation rejects its arguments,
 * and a zero budget lets the call report that status. */
static uint32_t total_work(uint32_t private_work, uint32_t encoding_work, uint32_t requests)
{
  if (!private_work || !encoding_work || encoding_work > UINT32_MAX - private_work - requests)
    return 0;
  return private_work + encoding_work + requests;
}

TC_RSA_result example_sign_rsa_v15_digest(const TC_RSA_private_key* key, TC_hash_algorithm hash,
                                          TC_bytes digest, TC_buffer signature,
                                          TC_random_source random,
                                          const TC_RSA_workspace* workspace)
{
  const TC_RSA_v15_options options = {hash};
  const size_t length = key ? key->public_key.modulus.length : 0;
  /* Budget every blinding attempt so a rejected factor can be replaced. */
  const uint32_t work = total_work(TC_RSA_private_work(key, BLINDING_ATTEMPTS),
                                   TC_RSA_encode_v15_work(&options, length), 0);
  TC_RSA_execution execution = {random, BLINDING_ATTEMPTS, {work}};
  return TC_RSA_sign_v15_digest(key, &options, digest, signature, workspace, &execution);
}

TC_RSA_result example_sign_rsa_pss_sha256_digest(const TC_RSA_private_key* key, TC_bytes digest,
                                                 TC_buffer signature, TC_random_source random,
                                                 const TC_RSA_workspace* workspace)
{
  const TC_RSA_pss_options options = {TC_HASH_SHA256, TC_HASH_SHA256, SHA256_BYTES};
  const size_t length = key ? key->public_key.modulus.length : 0;
  /* The PSS encoding, one salt request and every blinding attempt. */
  const uint32_t work = total_work(TC_RSA_private_work(key, BLINDING_ATTEMPTS),
                                   TC_RSA_encode_pss_work(&options, length), 1);
  TC_RSA_execution execution = {random, BLINDING_ATTEMPTS, {work}};
  return TC_RSA_sign_pss_digest(key, &options, digest, signature, workspace, &execution);
}
