/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_validate.h"

TC_RSA_result example_validate_rsa_key(const TC_RSA_private_key* key, TC_random_source random,
                                       const TC_RSA_workspace* workspace)
{
  enum { MAX_KEY_BITS = 4096, REQUESTS_PER_FACTOR = 4 * TC_RSA_VALIDATION_ROUNDS };
  if (!key)
    return TC_RSA_ARGUMENT;
  const size_t length = key->public_key.modulus.length;
  if (length > MAX_KEY_BITS / 8 || !TC_RSA_workspace_words(TC_RSA_OPERATION_VALIDATE, length * 8))
    return TC_RSA_INVALID;
  /* Use 32-bit arithmetic, including on targets with 16-bit size_t. */
  const uint32_t work = TC_RSA_VALIDATE_WORK((uint32_t)length * 8u, REQUESTS_PER_FACTOR);
  TC_RSA_execution execution = {random, REQUESTS_PER_FACTOR, {work}};
  /* Applications that accept keys outside FIPS 186-5, such as e = 3, pass
   * TC_RSA_EXPONENT_ANY_ODD instead. */
  return TC_RSA_validate_private_key(key, TC_RSA_EXPONENT_FIPS, workspace, &execution);
}
