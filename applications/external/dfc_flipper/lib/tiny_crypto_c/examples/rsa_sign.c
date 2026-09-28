/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_sign.h"

enum { MAX_KEY_BITS = 3072, BLINDING_ATTEMPTS = 4, SHA256_BYTES = EXAMPLE_RSA_PSS_SHA256_BYTES };

static TC_RSA_result private_budget(const TC_RSA_private_key* key, uint32_t* work)
{
  if (!key) return TC_RSA_ARGUMENT;
  const size_t length = key->public_key.modulus.length;
  const size_t exponent_length = key->public_key.exponent.length;
  if (length > MAX_KEY_BITS / 8 || !TC_RSA_sign_workspace_words(length * 8) ||
      exponent_length > length) return TC_RSA_INVALID;
  *work = UINT32_C(32) * length + UINT32_C(32) * exponent_length + 8 +
      BLINDING_ATTEMPTS * (UINT32_C(16) * length + 1);
  return TC_RSA_OK;
}

TC_RSA_result example_sign_rsa_v15_digest(const TC_RSA_private_key* key,
    TC_hash_algorithm hash, TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  uint32_t work;
  const TC_RSA_result result = private_budget(key,&work);
  if (result != TC_RSA_OK) return result;
  work += key->public_key.modulus.length;
  TC_RSA_workspace workspace = {scratch,scratch_words};
  /* Bound the budget before converting to the platform's work-counter type. */
#if SIZE_MAX < UINT32_MAX
  if (work > SIZE_MAX) return TC_RSA_LIMIT;
#endif
  const TC_RSA_v15_options options = {hash};
  TC_RSA_execution execution = {{random,random_context},BLINDING_ATTEMPTS,{work}};
  return TC_RSA_sign_v15_digest(key,&options,digest,&workspace,
      (TC_buffer){signature,signature_length},&execution);
}

TC_RSA_result example_sign_rsa_pss_sha256_digest(const TC_RSA_private_key* key,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  uint32_t work;
  const TC_RSA_result result = private_budget(key,&work);
  if (result != TC_RSA_OK) return result;
  const uint32_t length = key->public_key.modulus.length;
  const uint32_t db = length - SHA256_BYTES - 1;
  const uint32_t blocks = (db + SHA256_BYTES - 1) / SHA256_BYTES;
  /* SHA-256 for both hashes, a digest-length salt, and one salt RNG request. */
  work += length + 2 * SHA256_BYTES + 9 + db + blocks * (SHA256_BYTES + 5) + 1;
#if SIZE_MAX < UINT32_MAX
  if (work > SIZE_MAX) return TC_RSA_LIMIT;
#endif
  TC_RSA_workspace workspace = {scratch,scratch_words};
  const TC_RSA_pss_options options = {
    TC_HASH_SHA256,TC_HASH_SHA256,SHA256_BYTES
  };
  TC_RSA_execution execution = {{random,random_context},BLINDING_ATTEMPTS,{work}};
  return TC_RSA_sign_pss_digest(key,&options,digest,&workspace,
      (TC_buffer){signature,signature_length},&execution);
}
