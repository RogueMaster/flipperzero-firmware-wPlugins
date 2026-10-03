/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RSA private-key operations: raw private, CRT validation and derivation,
 * key validation and PKCS #1 v1.5 and PSS signing.
 *
 * Every public entry checks its arguments once, in this order: NULL and
 * overlapping storage (ARGUMENT), the digest length of a known hash
 * (ARGUMENT), the key and CRT values (UNSUPPORTED or INVALID), the scheme
 * parameters (UNSUPPORTED or INVALID), received input lengths (INVALID), then
 * output capacity, workspace, RNG attempts and work (LIMIT). Kernels receive
 * the checked tc_rsa_private_view and repeat none of these checks. */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_padding_internal.h"
#include "rsa_private_internal.h"
#include "rsa_internal.h"

TC_RSA_result TC_RSA_encode_v15_digest(const TC_RSA_v15_options* options, TC_bytes digest,
                                       TC_buffer encoded, TC_work_budget* work)
{
  tc_hash_info info;
  if (!options || !work || !digest.data || !encoded.data)
    return TC_RSA_ARGUMENT;
  /* Encoded bytes and the work budget are written. Both stay separate from
   * each other and from the options and digest. */
  const TC_bytes inputs[] = {{(const uint8_t*)options, sizeof *options}, digest};
  if (!tc_internal_ranges_disjoint(work, sizeof *work, encoded.data, encoded.capacity))
    return TC_RSA_ARGUMENT;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
    if (!tc_internal_ranges_disjoint(inputs[i].data, inputs[i].length, encoded.data,
                                     encoded.capacity) ||
        !tc_internal_ranges_disjoint(inputs[i].data, inputs[i].length, work, sizeof *work))
      return TC_RSA_ARGUMENT;
  }
  if (!tc_hash_info_get(options->hash, &info))
    return TC_RSA_UNSUPPORTED;
  if (digest.length != info.digest_length)
    return TC_RSA_ARGUMENT;
  if (!tc_rsa_supported_modulus_size(encoded.capacity))
    return TC_RSA_UNSUPPORTED;
  if (work->remaining < encoded.capacity)
    return TC_RSA_LIMIT;
  work->remaining -= (uint32_t)encoded.capacity;
  return tc_rsa_v15_encode(encoded.data, encoded.capacity, info.digest_info.data,
                           info.digest_info.length, digest.data, digest.length) == TC_OK
             ? TC_RSA_OK
             : TC_RSA_ARGUMENT;
}

TC_RSA_result TC_RSA_encode_pss_digest(const TC_RSA_pss_options* options, TC_bytes digest,
                                       TC_bytes salt, TC_buffer encoded, TC_work_budget* work)
{
  if (!options || !work || !digest.data || (salt.length && !salt.data) || !encoded.data)
    return TC_RSA_ARGUMENT;
  const TC_bytes inputs[] = {{(const uint8_t*)options, sizeof *options}, digest, salt};
  if (!tc_internal_ranges_disjoint(work, sizeof *work, encoded.data, encoded.capacity))
    return TC_RSA_ARGUMENT;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
    if (!tc_internal_ranges_disjoint(inputs[i].data, inputs[i].length, encoded.data,
                                     encoded.capacity) ||
        !tc_internal_ranges_disjoint(inputs[i].data, inputs[i].length, work, sizeof *work))
      return TC_RSA_ARGUMENT;
  }
  if (salt.length != options->salt_length)
    return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_digest_length_check(options->hash, digest.length);
  if (result != TC_RSA_OK)
    return result;
  if (!tc_rsa_supported_modulus_size(encoded.capacity))
    return TC_RSA_UNSUPPORTED;
  /* The whole cost is checked first, so a short budget leaves output and work
   * unchanged. */
  size_t cost;
  result = tc_rsa_pss_cost(encoded.capacity, encoded.capacity * 8 - 1, options->hash,
                           options->mgf_hash, salt.length, &cost);
  if (result != TC_RSA_OK)
    return result;
  if (work->remaining < cost)
    return TC_RSA_LIMIT;
  TC_hash_context workspace;
  uint8_t block[64];
  result = tc_rsa_pss_encode(options, (TC_buffer){encoded.data, encoded.capacity},
                             encoded.capacity * 8 - 1, digest, salt,
                             (tc_rsa_hash_scratch){block, &workspace}, &work->remaining);
  if (result != TC_RSA_OK)
    TC_secure_zero(encoded.data, encoded.capacity);
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(&workspace, sizeof workspace);
  return result;
}

TC_RSA_result TC_RSA_raw_private(const TC_RSA_public_key* key, TC_bytes private_exponent,
                                 TC_bytes input, TC_buffer output,
                                 const TC_RSA_workspace* workspace, TC_RSA_execution* execution)
{
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_output(&storage, output);
  tc_rsa_storage_write(&storage, execution, sizeof *execution);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_public_key(&storage, key);
  tc_rsa_storage_required(&storage, private_exponent);
  tc_rsa_storage_required(&storage, input);
  TC_RSA_result result = tc_rsa_storage_finish(&storage);
  if (result == TC_RSA_OK && !execution->random.fill)
    result = TC_RSA_ARGUMENT;
  if (result == TC_RSA_OK)
    result = tc_rsa_public_key_check(key);
  if (result != TC_RSA_OK)
    return result;
  const size_t length = key->modulus.length;
  if (!private_exponent.length || private_exponent.length > length || input.length != length)
    return TC_RSA_INVALID;
  /* Preflight every allowed blinding attempt before private arithmetic. */
  const uint32_t cost =
      TC_RSA_private_work(&(TC_RSA_private_key){*key, private_exponent, {NULL, 0}, {NULL, 0}, NULL},
                          execution->random_attempts);
  if (!cost || output.capacity < length ||
      workspace->capacity < TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8) ||
      execution->work.remaining < cost)
    return TC_RSA_LIMIT;
  return tc_rsa_private_operation_magnitude(
      key, private_exponent, input.data, output.data,
      &(tc_rsa_random){execution->random, execution->random_attempts},
      (tc_mp_scratch){workspace->words, workspace->capacity}, &execution->work.remaining);
}

TC_RSA_result TC_RSA_validate_crt(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
                                  const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  tc_rsa_private_view view = {0};
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_write(&storage, work, sizeof *work);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_private_key(&storage, key);
  tc_rsa_storage_crt(&storage, crt);
  TC_RSA_result result = tc_rsa_storage_finish(&storage);
  if (result == TC_RSA_OK)
    result = tc_rsa_private_view_init(key, crt, &view);
  if (result != TC_RSA_OK)
    return result;
  return tc_rsa_crt_consistent(&view, (tc_mp_scratch){workspace->words, workspace->capacity},
                               &work->remaining);
}

TC_RSA_result TC_RSA_derive_crt(const TC_RSA_private_key* key, const TC_RSA_crt_output* output,
                                const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  tc_rsa_private_view view = {0};
  tc_rsa_storage storage;
  if (!output)
    return TC_RSA_ARGUMENT;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_output(&storage, output->dp);
  tc_rsa_storage_output(&storage, output->dq);
  tc_rsa_storage_output(&storage, output->q_inverse);
  tc_rsa_storage_write(&storage, work, sizeof *work);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_input(&storage, output, sizeof *output);
  tc_rsa_storage_private_key(&storage, key);
  TC_RSA_result status = tc_rsa_storage_finish(&storage);
  if (status == TC_RSA_OK)
    status = tc_rsa_private_view_init(key, NULL, &view);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->public_key.modulus.length, prime_length = length / 2;
  const size_t required = TC_RSA_CRT_WORKSPACE_WORDS(length * 8);
  if (output->dp.capacity < prime_length || output->dq.capacity < prime_length ||
      output->q_inverse.capacity < prime_length)
    return TC_RSA_LIMIT;
  tc_mp_word *dp, *dq, *inverse;
  status = tc_rsa_crt_derive(&view, (tc_mp_scratch){workspace->words, workspace->capacity},
                             &work->remaining, &dp, &dq, &inverse);
  if (status == TC_RSA_OK) {
    tc_mp_to_be(output->dp.data, dp, prime_length);
    tc_mp_to_be(output->dq.data, dq, prime_length);
    tc_mp_to_be(output->q_inverse.data, inverse, prime_length);
  }
  if (workspace->capacity >= required)
    TC_secure_zero(workspace->words, required * sizeof *workspace->words);
  return status;
}

int TC_RSA_exponent_in_fips_range(TC_bytes exponent)
{
  return tc_rsa_exponent_fips(exponent.data, exponent.length);
}

TC_RSA_result TC_RSA_validate_private_key(const TC_RSA_private_key* key,
                                          TC_RSA_exponent_policy exponent_policy,
                                          const TC_RSA_workspace* workspace,
                                          TC_RSA_execution* execution)
{
  tc_rsa_private_view view = {0};
  tc_rsa_storage storage;
  if (exponent_policy != TC_RSA_EXPONENT_FIPS && exponent_policy != TC_RSA_EXPONENT_ANY_ODD)
    return TC_RSA_ARGUMENT;
  tc_rsa_storage_begin(&storage, workspace);
  tc_rsa_storage_write(&storage, execution, sizeof *execution);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_private_key(&storage, key);
  TC_RSA_result result = tc_rsa_storage_finish(&storage);
  if (result == TC_RSA_OK && !execution->random.fill)
    result = TC_RSA_ARGUMENT;
  if (result == TC_RSA_OK)
    result = tc_rsa_private_view_init(key, NULL, &view);
  if (result != TC_RSA_OK)
    return result;
  const tc_rsa_random rng = {execution->random, execution->random_attempts};
  return tc_rsa_private_magnitudes_check(&view, exponent_policy, TC_RSA_VALIDATION_ROUNDS, &rng,
                                         (tc_mp_scratch){workspace->words, workspace->capacity},
                                         &execution->work.remaining);
}

/* Arguments shared by both signature schemes. */
typedef struct {
  const TC_RSA_private_key* key;
  TC_bytes options;
  TC_hash_algorithm hash;
  TC_bytes digest;
  const TC_RSA_workspace* workspace;
  TC_buffer signature;
  TC_RSA_execution* execution;
} tc_rsa_sign_call;

/* Storage, digest-length and key checks, including the key's optional CRT
 * values. view holds the checked key on OK. */
static TC_RSA_result tc_rsa_sign_check(const tc_rsa_sign_call* call, tc_rsa_private_view* view)
{
  const TC_RSA_private_key* key = call->key;
  tc_rsa_storage storage;
  tc_rsa_storage_begin(&storage, call->workspace);
  tc_rsa_storage_output(&storage, call->signature);
  tc_rsa_storage_write(&storage, call->execution, sizeof *call->execution);
  tc_rsa_storage_seal(&storage);
  tc_rsa_storage_required(&storage, call->options);
  tc_rsa_storage_private_key(&storage, key);
  if (key && key->crt)
    tc_rsa_storage_crt(&storage, key->crt);
  tc_rsa_storage_required(&storage, call->digest);
  TC_RSA_result status = tc_rsa_storage_finish(&storage);
  if (status == TC_RSA_OK && !call->execution->random.fill)
    status = TC_RSA_ARGUMENT;
  if (status == TC_RSA_OK)
    status = tc_rsa_digest_length_check(call->hash, call->digest.length);
  if (status == TC_RSA_OK)
    status = tc_rsa_private_view_init(key, key->crt, view);
  return status;
}

/* Limits checked after every data check: signature capacity, workspace,
 * blinding attempts, and a budget that covers the encoding plus one
 * successful blinding attempt, before any RNG request. */
static TC_RSA_result tc_rsa_sign_limits(const tc_rsa_sign_call* call,
                                        const tc_rsa_private_view* view, size_t encode_cost)
{
  const size_t length = view->public_key->modulus.length;
  const TC_RSA_crt crt = {view->dp, view->dq, view->q_inverse};
  const TC_RSA_private_key key = {*view->public_key, view->d, view->p, view->q,
                                  view->crt ? &crt : NULL};
  const uint32_t private_cost = TC_RSA_private_work(&key, call->execution->random_attempts);
  const uint32_t remaining = call->execution->work.remaining;
  if (call->signature.capacity < length ||
      call->workspace->capacity < TC_RSA_SIGN_WORKSPACE_WORDS(length * 8) || !private_cost ||
      remaining < private_cost || remaining - private_cost < encode_cost)
    return TC_RSA_LIMIT;
  return TC_RSA_OK;
}

/* The encoded message sits after the private operation's scratch, in the
 * last n limbs of the signing workspace. */
static uint8_t* tc_rsa_sign_message(const TC_RSA_workspace* workspace, size_t length)
{
  return (uint8_t*)(workspace->words + TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8));
}

/* Apply the private key to the encoded message and publish exactly the
 * modulus length. */
static TC_RSA_result tc_rsa_sign_encoded(const tc_rsa_sign_call* call,
                                         const tc_rsa_private_view* view)
{
  const size_t length = view->public_key->modulus.length;
  const tc_rsa_random rng = {call->execution->random, call->execution->random_attempts};
  return tc_rsa_private_apply(
      view, tc_rsa_sign_message(call->workspace, length), call->signature.data, &rng,
      (tc_mp_scratch){call->workspace->words, TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(length * 8)},
      &call->execution->work.remaining);
}

/* RFC 8017 section 8.2.1: EMSA-PKCS1-v1_5 encoding, then RSASP1. */
TC_RSA_result TC_RSA_sign_v15_digest(const TC_RSA_private_key* key,
                                     const TC_RSA_v15_options* options, TC_bytes digest,
                                     TC_buffer signature, const TC_RSA_workspace* workspace,
                                     TC_RSA_execution* execution)
{
  tc_hash_info info;
  tc_rsa_private_view view = {0};
  if (!options)
    return TC_RSA_ARGUMENT;
  const tc_rsa_sign_call call = {key,           {(const uint8_t*)options, sizeof *options},
                                 options->hash, digest,
                                 workspace,     signature,
                                 execution};
  TC_RSA_result status = tc_rsa_sign_check(&call, &view);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->public_key.modulus.length;
  if (!tc_hash_info_get(options->hash, &info))
    return TC_RSA_UNSUPPORTED;
  if (!tc_rsa_v15_size(length, info.digest_info.length, digest.length))
    return TC_RSA_INVALID;
  status = tc_rsa_sign_limits(&call, &view, length);
  if (status != TC_RSA_OK)
    return status;
  execution->work.remaining -= (uint32_t)length;
  if (tc_rsa_v15_encode(tc_rsa_sign_message(workspace, length), length, info.digest_info.data,
                        info.digest_info.length, digest.data, digest.length) != TC_OK)
    status = TC_RSA_ARGUMENT;
  else
    status = tc_rsa_sign_encoded(&call, &view);
  TC_secure_zero(workspace->words, TC_RSA_SIGN_WORKSPACE_WORDS(length * 8) * sizeof(TC_RSA_word));
  return status;
}

/* RFC 8017 section 8.1.1: EMSA-PSS encoding with a fresh salt, then RSASP1. */
TC_RSA_result TC_RSA_sign_pss_digest(const TC_RSA_private_key* key,
                                     const TC_RSA_pss_options* options, TC_bytes digest,
                                     TC_buffer signature, const TC_RSA_workspace* workspace,
                                     TC_RSA_execution* execution)
{
  tc_rsa_private_view view = {0};
  size_t encode_cost;
  if (!options)
    return TC_RSA_ARGUMENT;
  const tc_rsa_sign_call call = {key,           {(const uint8_t*)options, sizeof *options},
                                 options->hash, digest,
                                 workspace,     signature,
                                 execution};
  TC_RSA_result status = tc_rsa_sign_check(&call, &view);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->public_key.modulus.length;
  const size_t salt_length = options->salt_length;
  status = tc_rsa_pss_cost(length, length * 8 - 1, options->hash, options->mgf_hash, salt_length,
                           &encode_cost);
  if (status != TC_RSA_OK)
    return status;
  /* The encoding cost, plus one unit for a salt request. */
  if (encode_cost == SIZE_MAX)
    return TC_RSA_LIMIT;
  status = tc_rsa_sign_limits(&call, &view, encode_cost + (salt_length != 0));
  if (status != TC_RSA_OK)
    return status;
  TC_hash_context hash_workspace;
  uint8_t block[64];
  /* The salt occupies scratch that the private operation reuses later. */
  uint8_t* salt = (uint8_t*)workspace->words;
  if (salt_length) {
    --execution->work.remaining;
    if (execution->random.fill(execution->random.context, salt, salt_length) != TC_OK) {
      status = TC_RSA_ERROR;
      goto cleanup;
    }
  }
  status =
      tc_rsa_pss_encode(options, (TC_buffer){tc_rsa_sign_message(workspace, length), length},
                        length * 8 - 1, digest, (TC_bytes){salt, salt_length},
                        (tc_rsa_hash_scratch){block, &hash_workspace}, &execution->work.remaining);
  if (status == TC_RSA_OK)
    status = tc_rsa_sign_encoded(&call, &view);
cleanup:
  TC_secure_zero(workspace->words, TC_RSA_SIGN_WORKSPACE_WORDS(length * 8) * sizeof(TC_RSA_word));
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(&hash_workspace, sizeof hash_workspace);
  return status;
}
#endif
