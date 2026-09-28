/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "rsa_encrypt.h"

enum { MAX_KEY_BITS = 3072, SHA256_BYTES = 32 };

TC_RSA_result example_encrypt_rsa_oaep_sha256(const TC_RSA_public_key* key,
    TC_bytes label, TC_bytes plaintext, uint8_t* ciphertext, size_t ciphertext_length,
    TC_random_fn random, void* random_context, TC_RSA_word* scratch, size_t scratch_words)
{
  if (!key) return TC_RSA_ARGUMENT;
  const size_t length = key->modulus.length, exponent_length = key->exponent.length;
  if (length > MAX_KEY_BITS / 8 || !TC_RSA_encrypt_workspace_words(length * 8) ||
      exponent_length > length) return TC_RSA_INVALID;
  const size_t db = length - SHA256_BYTES - 1;
  const size_t blocks = (db + SHA256_BYTES - 1) / SHA256_BYTES;
  /* RNG, encoding, two masks and exponentiation. Key bounds keep this subtotal
   * within 16-bit size_t; the caller's label length needs a separate check. */
  size_t work = 1 + length + 1 + db + blocks * (SHA256_BYTES + 5) +
      SHA256_BYTES + db + 5 + 16 * length + 16 * exponent_length + 4;
  if (label.length > SIZE_MAX - work) return TC_RSA_LIMIT;
  work += label.length;
  const TC_RSA_workspace workspace = {scratch,scratch_words};
  const TC_RSA_oaep_options options = {TC_HASH_SHA256,TC_HASH_SHA256,label};
  TC_RSA_execution execution = {{random,random_context},0,{(uint32_t)work}};
  return TC_RSA_encrypt_oaep(key,&options,plaintext,&workspace,
      (TC_buffer){ciphertext,ciphertext_length},&execution);
}
