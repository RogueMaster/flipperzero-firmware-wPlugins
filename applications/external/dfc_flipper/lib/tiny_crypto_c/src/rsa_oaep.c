/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RSAES-OAEP encryption and decryption (RFC 8017 section 7.1).
 *
 * Both entries check their arguments once, in this order: NULL and
 * overlapping storage (ARGUMENT), the key (UNSUPPORTED or INVALID), the OAEP
 * parameters (UNSUPPORTED or INVALID), the message or ciphertext length
 * (INVALID), then output capacity, workspace and the whole work budget
 * (LIMIT). Every check before the RNG request depends only on public
 * values. */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_private_internal.h"
#include "rsa_padding_internal.h"
#include "rsa_internal.h"

TC_RSA_result TC_RSA_encrypt_oaep(const TC_RSA_public_key* key, const TC_RSA_oaep_options* options,
                                  TC_bytes plaintext, TC_buffer ciphertext,
                                  const TC_RSA_workspace* workspace, TC_RSA_execution* execution)
{
  tc_rsa_storage storage;
  if (!options)
    return TC_RSA_ARGUMENT;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_output(&storage, ciphertext);
  tc_rsa_storage_write(&storage, execution, sizeof *execution);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_input(&storage, options, sizeof *options);
  tc_rsa_storage_span(&storage, options->label);
  tc_rsa_storage_public_key(&storage, key);
  tc_rsa_storage_span(&storage, plaintext);
  TC_RSA_result status = tc_rsa_storage_finish(&storage);
  if (status == TC_RSA_OK && !execution->random.fill)
    status = TC_RSA_ARGUMENT;
  if (status == TC_RSA_OK)
    status = tc_rsa_public_key_check(key);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->modulus.length;
  tc_hash_info info;
  size_t encode_cost;
  status = tc_rsa_oaep_cost(length, options->hash, options->mgf_hash, options->label.length,
                            &encode_cost);
  if (status != TC_RSA_OK)
    return status;
  if (!tc_hash_info_get(options->hash, &info))
    return TC_RSA_UNSUPPORTED;
  /* RFC 8017 section 7.1.1 step 1.b: mLen <= k - 2 hLen - 2. */
  if (plaintext.length > length - 2 * info.digest_length - 2)
    return TC_RSA_INVALID;
  const size_t arithmetic_words = TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(length * 8);
  const size_t required = TC_RSA_ENCRYPT_WORKSPACE_WORDS(length * 8);
  const uint32_t public_cost = tc_rsa_public_cost(length, key->exponent.length, 0);
  uint32_t* work = &execution->work.remaining;
  /* The seed request, the encoding and the public operation. */
  if (ciphertext.capacity < length || workspace->capacity < required || *work <= public_cost ||
      *work - public_cost - 1 < encode_cost)
    return TC_RSA_LIMIT;
  TC_hash_context hash_workspace;
  uint8_t block[64];
  /* Seed storage is reused by the modular operation after OAEP encoding. */
  uint8_t* seed = (uint8_t*)workspace->words;
  uint8_t* encoded = (uint8_t*)(workspace->words + arithmetic_words);
  --*work;
  if (execution->random.fill(execution->random.context, seed, info.digest_length) != TC_OK)
    status = TC_RSA_ERROR;
  else {
    status = tc_rsa_oaep_encode(options, (TC_buffer){encoded, length}, plaintext,
                                (TC_bytes){seed, info.digest_length},
                                (tc_rsa_hash_scratch){block, &hash_workspace}, work);
    if (status == TC_RSA_OK)
      status =
          tc_rsa_public_operation(key, encoded, ciphertext.data,
                                  (tc_mp_scratch){workspace->words, arithmetic_words}, work, NULL);
  }
  TC_secure_zero(workspace->words, required * sizeof(TC_RSA_word));
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(&hash_workspace, sizeof hash_workspace);
  return status;
}

TC_RSA_result TC_RSA_decrypt_oaep(const TC_RSA_private_key* key, const TC_RSA_oaep_options* options,
                                  TC_bytes ciphertext, TC_buffer plaintext,
                                  size_t* plaintext_length, const TC_RSA_workspace* workspace,
                                  TC_RSA_execution* execution)
{
  tc_rsa_private_view view = {0};
  tc_rsa_storage storage;
  if (!options || (uintptr_t)plaintext_length % sizeof *plaintext_length)
    return TC_RSA_ARGUMENT;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_output(&storage, plaintext);
  tc_rsa_storage_write(&storage, plaintext_length, sizeof *plaintext_length);
  tc_rsa_storage_write(&storage, execution, sizeof *execution);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_input(&storage, options, sizeof *options);
  tc_rsa_storage_span(&storage, options->label);
  tc_rsa_storage_private_key(&storage, key);
  if (key && key->crt)
    tc_rsa_storage_crt(&storage, key->crt);
  tc_rsa_storage_required(&storage, ciphertext);
  TC_RSA_result status = tc_rsa_storage_finish(&storage);
  if (status == TC_RSA_OK && !execution->random.fill)
    status = TC_RSA_ARGUMENT;
  if (status == TC_RSA_OK)
    status = tc_rsa_private_view_init(key, key->crt, &view);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->public_key.modulus.length;
  tc_hash_info info;
  size_t decode_cost;
  status = tc_rsa_oaep_cost(length, options->hash, options->mgf_hash, options->label.length,
                            &decode_cost);
  if (status != TC_RSA_OK)
    return status;
  if (!tc_hash_info_get(options->hash, &info))
    return TC_RSA_UNSUPPORTED;
  /* RFC 8017 section 7.1.2 step 1.b: the ciphertext has the modulus length. */
  if (ciphertext.length != length)
    return TC_RSA_INVALID;
  const size_t required = TC_RSA_DECRYPT_WORKSPACE_WORDS(length * 8);
  const size_t arithmetic_words = TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8);
  const size_t max_message = length - 2 * info.digest_length - 2;
  const uint32_t private_cost = TC_RSA_private_work(key, execution->random_attempts);
  uint32_t* work = &execution->work.remaining;
  /* RFC 8017 section 7.1.2, note after step 4: an opponent must not learn
   * which decryption error occurred. Plaintext capacity and the whole budget
   * are checked before decryption, so LIMIT depends only on public sizes. */
  if (plaintext.capacity < max_message || !private_cost || workspace->capacity < required ||
      *work < private_cost || *work - private_cost < decode_cost)
    return TC_RSA_LIMIT;
  TC_hash_context hash_workspace;
  uint8_t block[64];
  uint8_t* encoded = (uint8_t*)(workspace->words + arithmetic_words);
  TC_bytes message = {0};
  const tc_rsa_random rng = {execution->random, execution->random_attempts};
  status = tc_rsa_private_apply(&view, ciphertext.data, encoded, &rng,
                                (tc_mp_scratch){workspace->words, arithmetic_words}, work);
  if (status == TC_RSA_OK)
    status = tc_rsa_oaep_decode(options, (TC_buffer){encoded, length},
                                (tc_rsa_hash_scratch){block, &hash_workspace}, work, &message);
  if (status == TC_RSA_OK) {
    if (message.length)
      memcpy(plaintext.data, message.data, message.length);
    *plaintext_length = message.length;
  }
  TC_secure_zero(workspace->words, required * sizeof(TC_RSA_word));
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(&hash_workspace, sizeof hash_workspace);
  return status;
}
#endif
