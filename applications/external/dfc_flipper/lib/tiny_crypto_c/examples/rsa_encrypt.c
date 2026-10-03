/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_encrypt.h"

TC_RSA_result example_encrypt_rsa_oaep_sha256(const TC_RSA_public_key* key, TC_bytes label,
                                              TC_bytes plaintext, TC_buffer ciphertext,
                                              TC_random_source random,
                                              const TC_RSA_workspace* workspace)
{
  const TC_RSA_oaep_options options = {TC_HASH_SHA256, TC_HASH_SHA256, label};
  const size_t length = key ? key->modulus.length : 0;
  /* One seed request, the OAEP encoding and the public operation. A zero cost
   * means the call rejects its arguments, and a zero budget lets the call
   * report that status before any RNG request. */
  const uint32_t padding = TC_RSA_oaep_work(&options, length);
  const uint32_t exponentiation = TC_RSA_public_work(key);
  const uint32_t work = padding && exponentiation && padding <= UINT32_MAX - exponentiation - 1
                            ? 1 + padding + exponentiation
                            : 0;
  TC_RSA_execution execution = {random, 0, {work}};
  return TC_RSA_encrypt_oaep(key, &options, plaintext, ciphertext, workspace, &execution);
}
