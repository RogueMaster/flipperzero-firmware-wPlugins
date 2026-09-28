/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_validate.h"

TC_RSA_result example_validate_rsa_key(const TC_RSA_private_key* key,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch,
    size_t scratch_words)
{
  enum { MAX_KEY_BITS = 3072, REQUESTS_PER_FACTOR = 4 * TC_RSA_VALIDATION_ROUNDS };
  if (!key) return TC_RSA_ARGUMENT;
  const size_t length = key->public_key.modulus.length;
  if (length > MAX_KEY_BITS / 8 || !TC_RSA_validate_workspace_words(length * 8))
    return TC_RSA_INVALID;
  TC_RSA_workspace workspace = {scratch,scratch_words};
  /* Use wide operands throughout, including on targets with 16-bit size_t. */
  const uint32_t setup = UINT32_C(24) * length + 3;
  const uint32_t round = UINT32_C(24) * length + 1;
  const uint32_t work = UINT32_C(32) * length + 2 +
      2 * (setup + TC_RSA_VALIDATION_ROUNDS * round + REQUESTS_PER_FACTOR);
  TC_RSA_execution execution = {
    {random,random_context},REQUESTS_PER_FACTOR,{work}
  };
  return TC_RSA_validate_private_key(key,&workspace,&execution);
}
