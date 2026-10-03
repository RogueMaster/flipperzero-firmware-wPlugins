/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RSA public-key operations: raw public, prepared keys and PKCS #1 v1.5
 * and PSS signature verification.
 *
 * Every public entry checks its arguments once, in this order: NULL and
 * overlapping storage (ARGUMENT), the digest length of a known hash
 * (ARGUMENT), the key (UNSUPPORTED or INVALID), the scheme parameters
 * (UNSUPPORTED or INVALID), the received signature or input length (INVALID),
 * then workspace, output capacity and work (LIMIT). */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_padding_internal.h"
#include "rsa_internal.h"

TC_RSA_result TC_RSA_raw_public(const TC_RSA_public_key* key, TC_bytes input, TC_buffer output,
                                const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_output(&storage, output);
  tc_rsa_storage_write(&storage, work, sizeof *work);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_public_key(&storage, key);
  tc_rsa_storage_required(&storage, input);
  TC_RSA_result result = tc_rsa_storage_finish(&storage);
  if (result == TC_RSA_OK)
    result = tc_rsa_public_key_check(key);
  if (result != TC_RSA_OK)
    return result;
  const size_t length = key->modulus.length;
  if (input.length != length)
    return TC_RSA_INVALID;
  if (output.capacity < length ||
      workspace->capacity < TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(length * 8))
    return TC_RSA_LIMIT;
  return tc_rsa_public_operation(key, input.data, output.data,
                                 (tc_mp_scratch){workspace->words, workspace->capacity},
                                 &work->remaining, NULL);
}

enum { TC_RSA_PUBLIC_SETUP_MARKER = 0x52533250u };

TC_RSA_result TC_RSA_prepare_public_key(TC_RSA_prepared_public_key* setup,
                                        const TC_RSA_public_key* key, const TC_RSA_workspace* cache,
                                        const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_workspace(&storage, cache);
  tc_rsa_storage_write(&storage, setup, sizeof *setup);
  tc_rsa_storage_write(&storage, work, sizeof *work);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_input(&storage, cache, sizeof *cache);
  tc_rsa_storage_public_key(&storage, key);
  TC_RSA_result status = tc_rsa_storage_finish(&storage);
  if (status == TC_RSA_OK)
    status = tc_rsa_public_key_check(key);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->modulus.length;
  const size_t n = length / sizeof(TC_RSA_word);
  const uint32_t cost = UINT32_C(16) * (uint32_t)length + 1u;
  if (cache->capacity < n || workspace->capacity < 2 * n || work->remaining < cost)
    return TC_RSA_LIMIT;
  work->remaining -= cost;
  tc_mp_word* modulus_words = workspace->words;
  tc_mp_from_be(modulus_words, key->modulus.data, length);
  tc_mp_montgomery_r2(cache->words, modulus_words, n, workspace->words + n);
  setup->key = *key;
  setup->r2 = *cache;
  setup->marker = TC_RSA_PUBLIC_SETUP_MARKER;
  TC_secure_zero(workspace->words, 2 * length);
  return TC_RSA_OK;
}

void TC_RSA_prepared_public_key_clear(TC_RSA_prepared_public_key* setup)
{
  if (!setup)
    return;
  /* R^2 is derived entirely from the public modulus and is owned by the
   * caller.  Do not follow fields in a setup that may be uninitialized. */
  TC_secure_zero(setup, sizeof *setup);
}

/* An initialized setup whose cache holds one modulus width of limbs. */
static int tc_rsa_prepared_ready(const TC_RSA_prepared_public_key* setup)
{
  return setup && setup->marker == TC_RSA_PUBLIC_SETUP_MARKER && setup->r2.words &&
         setup->r2.capacity >= setup->key.modulus.length / sizeof(TC_RSA_word);
}

uint32_t TC_RSA_prepared_public_work(const TC_RSA_prepared_public_key* setup)
{
  if (!tc_rsa_prepared_ready(setup) || tc_rsa_public_key_check(&setup->key) != TC_RSA_OK)
    return 0;
  return tc_rsa_public_cost(setup->key.modulus.length, setup->key.exponent.length, 1);
}

/* Arguments shared by the four verifiers. setup is NULL for one-shot calls. */
typedef struct {
  const TC_RSA_prepared_public_key* setup;
  const TC_RSA_public_key* key;
  TC_bytes options;
  TC_hash_algorithm hash;
  TC_bytes digest, signature;
  const TC_RSA_workspace* workspace;
  TC_work_budget* work;
} tc_rsa_verify_call;

/* Storage, digest-length and key checks. The prepared setup and its cached
 * R^2 stay unchanged while the operation runs, so both are recorded as
 * protected ranges. */
static TC_RSA_result tc_rsa_verify_check(const tc_rsa_verify_call* call)
{
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, call->workspace);
  tc_rsa_storage_write(&storage, call->work, sizeof *call->work);
  if (call->setup) {
    tc_rsa_storage_write(&storage, call->setup, sizeof *call->setup);
    TC_PKI_PLAN_WRITE(&storage.plan, call->setup->r2.words, call->setup->r2.capacity);
  }
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_required(&storage, call->options);
  /* A prepared key object lies inside its setup, so only its bytes are
   * borrowed inputs. */
  if (call->setup) {
    tc_rsa_storage_required(&storage, call->key->modulus);
    tc_rsa_storage_required(&storage, call->key->exponent);
  } else
    tc_rsa_storage_public_key(&storage, call->key);
  tc_rsa_storage_required(&storage, call->digest);
  tc_rsa_storage_required(&storage, call->signature);
  TC_RSA_result result = tc_rsa_storage_finish(&storage);
  if (result == TC_RSA_OK)
    result = tc_rsa_digest_length_check(call->hash, call->digest.length);
  if (result == TC_RSA_OK)
    result = tc_rsa_public_key_check(call->key);
  return result;
}

static const tc_mp_word* tc_rsa_verify_r2(const tc_rsa_verify_call* call)
{
  return call->setup ? call->setup->r2.words : NULL;
}

/* RFC 8017 section 8.2.2: RSAVP1, then EMSA-PKCS1-v1_5 encoding and an exact
 * comparison of the whole representative. */
static TC_RSA_result tc_rsa_verify_v15(const tc_rsa_verify_call* call)
{
  tc_hash_info info;
  TC_RSA_result result = tc_rsa_verify_check(call);
  if (result != TC_RSA_OK)
    return result;
  const TC_RSA_public_key* key = call->key;
  const size_t length = key->modulus.length;
  if (!tc_hash_info_get(call->hash, &info))
    return TC_RSA_UNSUPPORTED;
  if (!tc_rsa_v15_size(length, info.digest_info.length, info.digest_length))
    return TC_RSA_INVALID;
  /* Step 1: the signature has the modulus length. */
  if (call->signature.length != length)
    return TC_RSA_INVALID;
  const size_t arithmetic_words = TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(length * 8);
  const uint32_t cost =
      (uint32_t)length + tc_rsa_public_cost(length, key->exponent.length, call->setup != NULL);
  uint32_t* work = &call->work->remaining;
  if (call->workspace->capacity < TC_RSA_VERIFY_WORKSPACE_WORDS(length * 8) || *work < cost)
    return TC_RSA_LIMIT;
  *work -= (uint32_t)length;
  uint8_t* encoded = (uint8_t*)(call->workspace->words + arithmetic_words);
  result = tc_rsa_public_operation(key, call->signature.data, encoded,
                                   (tc_mp_scratch){call->workspace->words, arithmetic_words}, work,
                                   tc_rsa_verify_r2(call));
  if (result != TC_RSA_OK)
    return result;
  const TC_status checked =
      tc_rsa_v15_check(encoded, length, info.digest_info.data, info.digest_info.length,
                       call->digest.data, call->digest.length);
  TC_secure_zero(encoded, length);
  return checked == TC_OK ? TC_RSA_OK : TC_RSA_INVALID;
}

/* RFC 8017 section 8.1.2: RSAVP1, then EMSA-PSS verification with the
 * caller's salt length. */
static TC_RSA_result tc_rsa_verify_pss(const tc_rsa_verify_call* call,
                                       const TC_RSA_pss_options* options)
{
  TC_hash_context hash_workspace;
  uint8_t block[64];
  size_t pss_cost;
  TC_RSA_result result = tc_rsa_verify_check(call);
  if (result != TC_RSA_OK)
    return result;
  const TC_RSA_public_key* key = call->key;
  const size_t length = key->modulus.length;
  result = tc_rsa_pss_cost(length, length * 8 - 1, options->hash, options->mgf_hash,
                           options->salt_length, &pss_cost);
  if (result != TC_RSA_OK)
    return result;
  /* Step 1: the signature has the modulus length. */
  if (call->signature.length != length)
    return TC_RSA_INVALID;
  const size_t words = TC_RSA_VERIFY_WORKSPACE_WORDS(length * 8);
  const size_t arithmetic_words = TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(length * 8);
  const uint32_t public_cost =
      tc_rsa_public_cost(length, key->exponent.length, call->setup != NULL);
  uint32_t* work = &call->work->remaining;
  if (call->workspace->capacity < words || *work < public_cost || *work - public_cost < pss_cost)
    return TC_RSA_LIMIT;
  uint8_t* encoded = (uint8_t*)(call->workspace->words + arithmetic_words);
  result = tc_rsa_public_operation(key, call->signature.data, encoded,
                                   (tc_mp_scratch){call->workspace->words, arithmetic_words}, work,
                                   tc_rsa_verify_r2(call));
  if (result == TC_RSA_OK)
    result = tc_rsa_pss_check(options, (TC_buffer){encoded, length}, length * 8 - 1, call->digest,
                              (tc_rsa_hash_scratch){block, &hash_workspace}, work);
  TC_secure_zero(call->workspace->words, words * sizeof(TC_RSA_word));
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(&hash_workspace, sizeof hash_workspace);
  return result;
}

TC_RSA_result TC_RSA_verify_v15_digest(const TC_RSA_public_key* key,
                                       const TC_RSA_v15_options* options, TC_bytes digest,
                                       TC_bytes signature, const TC_RSA_workspace* workspace,
                                       TC_work_budget* work)
{
  if (!options)
    return TC_RSA_ARGUMENT;
  const tc_rsa_verify_call call = {
      NULL,      key, {(const uint8_t*)options, sizeof *options}, options->hash, digest, signature,
      workspace, work};
  return tc_rsa_verify_v15(&call);
}

TC_RSA_result TC_RSA_verify_v15_prepared(const TC_RSA_prepared_public_key* setup,
                                         const TC_RSA_v15_options* options, TC_bytes digest,
                                         TC_bytes signature, const TC_RSA_workspace* workspace,
                                         TC_work_budget* work)
{
  if (!options || !tc_rsa_prepared_ready(setup))
    return TC_RSA_ARGUMENT;
  const tc_rsa_verify_call call = {
      setup,         &setup->key, {(const uint8_t*)options, sizeof *options},
      options->hash, digest,      signature,
      workspace,     work};
  return tc_rsa_verify_v15(&call);
}

TC_RSA_result TC_RSA_verify_pss_digest(const TC_RSA_public_key* key,
                                       const TC_RSA_pss_options* options, TC_bytes digest,
                                       TC_bytes signature, const TC_RSA_workspace* workspace,
                                       TC_work_budget* work)
{
  if (!options)
    return TC_RSA_ARGUMENT;
  const tc_rsa_verify_call call = {
      NULL,      key, {(const uint8_t*)options, sizeof *options}, options->hash, digest, signature,
      workspace, work};
  return tc_rsa_verify_pss(&call, options);
}

TC_RSA_result TC_RSA_verify_pss_prepared(const TC_RSA_prepared_public_key* setup,
                                         const TC_RSA_pss_options* options, TC_bytes digest,
                                         TC_bytes signature, const TC_RSA_workspace* workspace,
                                         TC_work_budget* work)
{
  if (!options || !tc_rsa_prepared_ready(setup))
    return TC_RSA_ARGUMENT;
  const tc_rsa_verify_call call = {
      setup,         &setup->key, {(const uint8_t*)options, sizeof *options},
      options->hash, digest,      signature,
      workspace,     work};
  return tc_rsa_verify_pss(&call, options);
}
#endif
