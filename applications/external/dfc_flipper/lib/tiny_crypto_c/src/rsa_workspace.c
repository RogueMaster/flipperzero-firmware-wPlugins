/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RSA workspace and work sizing, and the storage and key checks that every
 * public entry runs once. */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_padding_internal.h"
#include "rsa_internal.h"

int TC_RSA_modulus_supported(size_t bits)
{
  return tc_rsa_supported_bits(bits);
}

size_t TC_RSA_workspace_words(TC_RSA_operation operation, size_t bits)
{
  if (!tc_rsa_supported_bits(bits))
    return 0;
  switch (operation) {
  case TC_RSA_OPERATION_VERIFY:
    return TC_RSA_VERIFY_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_ENCRYPT:
    return TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_RAW_PUBLIC:
    return TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_VALIDATE:
    return TC_RSA_VALIDATE_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_CRT:
    return TC_RSA_CRT_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_SIGN:
    return TC_RSA_SIGN_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_DECRYPT:
    return TC_RSA_DECRYPT_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_RAW_PRIVATE:
    return TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(bits);
  case TC_RSA_OPERATION_KEYGEN:
    return TC_RSA_KEYGEN_WORKSPACE_WORDS(bits);
  }
  return 0;
}

static uint32_t tc_rsa_work_value(TC_RSA_result result, size_t cost)
{
  if (result != TC_RSA_OK)
    return 0;
#if SIZE_MAX > UINT32_MAX
  if (cost > UINT32_MAX)
    return 0;
#endif
  return (uint32_t)cost;
}

uint32_t TC_RSA_encode_v15_work(const TC_RSA_v15_options* options, size_t modulus_bytes)
{
  tc_hash_info info;
  if (!options || !tc_hash_info_get(options->hash, &info) ||
      !tc_rsa_supported_modulus_size(modulus_bytes))
    return 0;
  return (uint32_t)modulus_bytes;
}

uint32_t TC_RSA_encode_pss_work(const TC_RSA_pss_options* options, size_t modulus_bytes)
{
  size_t cost = 0;
  if (!options || !tc_rsa_supported_modulus_size(modulus_bytes))
    return 0;
  /* Sequence the cost call before reading cost: argument order is unspecified. */
  const TC_RSA_result result = tc_rsa_pss_cost(modulus_bytes, modulus_bytes * 8 - 1, options->hash,
                                               options->mgf_hash, options->salt_length, &cost);
  return tc_rsa_work_value(result, cost);
}

uint32_t TC_RSA_oaep_work(const TC_RSA_oaep_options* options, size_t modulus_bytes)
{
  size_t cost = 0;
  if (!options || !tc_rsa_supported_modulus_size(modulus_bytes))
    return 0;
  const TC_RSA_result result = tc_rsa_oaep_cost(modulus_bytes, options->hash, options->mgf_hash,
                                                options->label.length, &cost);
  return tc_rsa_work_value(result, cost);
}

uint32_t TC_RSA_public_work(const TC_RSA_public_key* key)
{
  if (tc_rsa_public_key_check(key) != TC_RSA_OK)
    return 0;
  return tc_rsa_public_cost(key->modulus.length, key->exponent.length, 0);
}

uint32_t TC_RSA_private_work(const TC_RSA_private_key* key, size_t attempts)
{
  if (!key || !attempts || tc_rsa_public_key_check(&key->public_key) != TC_RSA_OK)
    return 0;
  const size_t length = key->public_key.modulus.length;
  const uint32_t base =
      tc_rsa_private_base_cost(length, key->public_key.exponent.length, key->crt != NULL);
  const uint32_t attempt = tc_rsa_blinding_cost(length);
  if (attempts > (UINT32_MAX - base) / attempt)
    return 0;
  return base + (uint32_t)attempts * attempt;
}

/* Strip leading zero octets so a factor or CRT value fits half the modulus
 * width. Returns 0 for an empty field, one longer than the modulus, or a
 * value wider than length / 2 octets. */
static int tc_rsa_half_width_field(TC_bytes* field, size_t length)
{
  const size_t prime_length = length / 2;
  if (!field->length || field->length > length)
    return 0;
  while (field->length > prime_length && !*field->data) {
    ++field->data;
    --field->length;
  }
  return field->length <= prime_length;
}

TC_RSA_result tc_rsa_private_view_init(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
                                       tc_rsa_private_view* view)
{
  const TC_RSA_result status = tc_rsa_public_key_check(&key->public_key);
  if (status != TC_RSA_OK)
    return status;
  const size_t length = key->public_key.modulus.length;
  tc_rsa_private_view checked = {&key->public_key, key->d,    key->p,    key->q,
                                 crt != NULL,      {NULL, 0}, {NULL, 0}, {NULL, 0}};
  if (!checked.d.length || checked.d.length > length)
    return TC_RSA_INVALID;
  /* FIPS 186-5 A.1.3: each prime has half the modulus length. */
  if (!tc_rsa_half_width_field(&checked.p, length) || !tc_rsa_half_width_field(&checked.q, length))
    return TC_RSA_INVALID;
  if (crt) {
    /* RFC 8017 section 3.2: dP, dQ and qInv are residues modulo p - 1, q - 1
     * and p, so each fits half the modulus. */
    checked.dp = crt->dp;
    checked.dq = crt->dq;
    checked.q_inverse = crt->q_inverse;
    if (!tc_rsa_half_width_field(&checked.dp, length) ||
        !tc_rsa_half_width_field(&checked.dq, length) ||
        !tc_rsa_half_width_field(&checked.q_inverse, length))
      return TC_RSA_INVALID;
  }
  *view = checked;
  return TC_RSA_OK;
}

static void tc_rsa_storage_fail(tc_rsa_storage* storage)
{
  tc_pki_storage_plan_fail(&storage->plan, TC_TLV_ARGUMENT);
}

void tc_rsa_storage_workspace(tc_rsa_storage* storage, const TC_RSA_workspace* workspace)
{
  if (!workspace || (uintptr_t)workspace->words % sizeof(TC_RSA_word)) {
    tc_rsa_storage_fail(storage);
    return;
  }
  TC_PKI_PLAN_WRITE(&storage->plan, workspace->words, workspace->capacity);
}

void tc_rsa_storage_begin(tc_rsa_storage* storage, const TC_RSA_workspace* workspace)
{
  storage->workspace = workspace;
  tc_pki_storage_plan_begin(&storage->plan, storage->writes, TC_RSA_STORAGE_WRITES, SIZE_MAX);
  tc_rsa_storage_workspace(storage, workspace);
}

/* A NULL object fails in the plan because its size is nonzero. */
void tc_rsa_storage_write(tc_rsa_storage* storage, const void* data, size_t size)
{
  tc_pki_storage_plan_write(&storage->plan, data, 1, size);
}

void tc_rsa_storage_output(tc_rsa_storage* storage, TC_buffer output)
{
  if (!output.data)
    tc_rsa_storage_fail(storage);
  tc_pki_storage_plan_write(&storage->plan, output.data, output.capacity, 1);
}

void tc_rsa_storage_seal(tc_rsa_storage* storage)
{
  tc_pki_storage_plan_seal(&storage->plan);
  tc_rsa_storage_input(storage, storage->workspace, sizeof *storage->workspace);
}

void tc_rsa_storage_input(tc_rsa_storage* storage, const void* data, size_t size)
{
  tc_pki_storage_plan_input(&storage->plan, data, 1, size);
}

void tc_rsa_storage_span(tc_rsa_storage* storage, TC_bytes span)
{
  tc_pki_storage_plan_input_span(&storage->plan, span);
}

void tc_rsa_storage_required(tc_rsa_storage* storage, TC_bytes span)
{
  if (!span.data)
    tc_rsa_storage_fail(storage);
  tc_rsa_storage_span(storage, span);
}

void tc_rsa_storage_public_key(tc_rsa_storage* storage, const TC_RSA_public_key* key)
{
  tc_rsa_storage_input(storage, key, sizeof *key);
  if (!key)
    return;
  tc_rsa_storage_required(storage, key->modulus);
  tc_rsa_storage_required(storage, key->exponent);
}

void tc_rsa_storage_private_key(tc_rsa_storage* storage, const TC_RSA_private_key* key)
{
  tc_rsa_storage_input(storage, key, sizeof *key);
  if (!key)
    return;
  tc_rsa_storage_required(storage, key->public_key.modulus);
  tc_rsa_storage_required(storage, key->public_key.exponent);
  tc_rsa_storage_required(storage, key->d);
  tc_rsa_storage_required(storage, key->p);
  tc_rsa_storage_required(storage, key->q);
}

void tc_rsa_storage_crt(tc_rsa_storage* storage, const TC_RSA_crt* crt)
{
  tc_rsa_storage_input(storage, crt, sizeof *crt);
  if (!crt)
    return;
  tc_rsa_storage_required(storage, crt->dp);
  tc_rsa_storage_required(storage, crt->dq);
  tc_rsa_storage_required(storage, crt->q_inverse);
}

TC_RSA_result tc_rsa_storage_finish(const tc_rsa_storage* storage)
{
  return tc_pki_storage_plan_finish(&storage->plan, NULL) == TC_TLV_OK ? TC_RSA_OK
                                                                       : TC_RSA_ARGUMENT;
}
#endif
