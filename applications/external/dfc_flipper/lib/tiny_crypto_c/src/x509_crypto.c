/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509_crypto.h>
#if TC_ENABLE_X509
#include "pki_signature_internal.h"
#include "pki_status_internal.h"
#include <tiny_crypto/ec.h>
#include <tiny_crypto/rsa.h>
#include "pki_storage_internal.h"
#include "hash_dispatch_internal.h"
#include "x509_path_internal.h"

/* Abstract work units for two scalar multiplies and inversions. */
enum { TC_PKI_ECDSA_WORK_PER_BIT = 64 };

/* One digest-signature check. Inputs and metadata are stable and disjoint
 * from both workspaces. The caller resolves the algorithm and charges hashing
 * and DER bytes first. Only the selected algorithm's workspace is required.
 * max_work is the fixed work reserved for the public-key operation. */
typedef struct {
  const TC_signature_algorithm* algorithm;
  const TC_X509_public_key* key;
  TC_bytes digest, signature;
  TC_ECDSA_workspace* ec;
  const TC_RSA_workspace* rsa;
  size_t max_work;
} digest_check;

#if TC_ENABLE_EC
static TC_X509_signature_result verify_ecdsa(const digest_check* check)
{
  TC_DER_signature_pair pair;
  uint8_t raw[2 * TC_EC_MAX_BYTES];
  const TC_X509_public_key* key = check->key;
  const size_t width = TC_EC_coordinate_bytes(key->curve);
  if (!width)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!check->ec)
    return TC_X509_SIGNATURE_ERROR;
  /* Reserve fixed public work for two scalar multiplies and inversions. */
  if (check->max_work < width * 8 * TC_PKI_ECDSA_WORK_PER_BIT)
    return TC_X509_SIGNATURE_LIMIT;
  if (TC_DER_ecdsa_signature(check->signature, &pair) != TC_TLV_OK || pair.r.length > width ||
      pair.s.length > width)
    return TC_X509_SIGNATURE_INVALID;
  memset(raw, 0, sizeof raw);
  memcpy(raw + width - pair.r.length, pair.r.data, pair.r.length);
  memcpy(raw + 2 * width - pair.s.length, pair.s.data, pair.s.length);
  /* The PKI budget above covers the EC operation's own units. */
  TC_work_budget budget = {TC_EC_operation_work(key->curve, TC_EC_OPERATION_VERIFY)};
  const TC_EC_result verified = TC_ECDSA_verify_digest(
      key->curve, key->key, check->digest, (TC_bytes){raw, 2 * width}, check->ec, &budget);
  TC_secure_zero(raw, sizeof raw);
  switch (verified) {
  case TC_EC_OK:
    return TC_X509_SIGNATURE_VALID;
  case TC_EC_INVALID:
    return TC_X509_SIGNATURE_INVALID;
  case TC_EC_LIMIT:
    return TC_X509_SIGNATURE_LIMIT;
  case TC_EC_UNSUPPORTED:
    return TC_X509_SIGNATURE_UNSUPPORTED;
  default:
    return TC_X509_SIGNATURE_ERROR;
  }
}
#endif

#if TC_ENABLE_RSA
static TC_X509_signature_result verify_rsa(const digest_check* check)
{
  const TC_signature_algorithm* algorithm = check->algorithm;
  TC_RSA_public_key public_key = {check->key->modulus, check->key->exponent};
  TC_RSA_result result;
  TC_work_budget budget = {check->max_work > UINT32_MAX ? UINT32_MAX : (uint32_t)check->max_work};
  if (algorithm->scheme == TC_SIGNATURE_RSA_V15) {
    const TC_RSA_v15_options options = {algorithm->hash};
    result = TC_RSA_verify_v15_digest(&public_key, &options, check->digest, check->signature,
                                      check->rsa, &budget);
  } else {
    const TC_RSA_pss_options options = {algorithm->hash, algorithm->mgf_hash,
                                        algorithm->salt_length};
    result = TC_RSA_verify_pss_digest(&public_key, &options, check->digest, check->signature,
                                      check->rsa, &budget);
  }
  switch (result) {
  case TC_RSA_OK:
    return TC_X509_SIGNATURE_VALID;
  case TC_RSA_INVALID:
    return TC_X509_SIGNATURE_INVALID;
  case TC_RSA_LIMIT:
    return TC_X509_SIGNATURE_LIMIT;
  case TC_RSA_UNSUPPORTED:
    return TC_X509_SIGNATURE_UNSUPPORTED;
  default:
    return TC_X509_SIGNATURE_ERROR;
  }
}
#endif

/* Dispatch on the scheme. A scheme whose family is disabled is UNSUPPORTED. */
static TC_X509_signature_result tc_pki_verify_digest(const digest_check* check)
{
  tc_hash_info hash;
  if (!check->algorithm || !check->key)
    return TC_X509_SIGNATURE_ERROR;
  if (!tc_hash_info_get(check->algorithm->hash, &hash))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!check->digest.data || check->digest.length != hash.digest_length)
    return TC_X509_SIGNATURE_ERROR;
  switch (check->algorithm->scheme) {
#if TC_ENABLE_EC
  case TC_SIGNATURE_ECDSA:
    return verify_ecdsa(check);
#endif
#if TC_ENABLE_RSA
  case TC_SIGNATURE_RSA_V15:
  case TC_SIGNATURE_RSA_PSS:
    return verify_rsa(check);
#endif
  default:
    return TC_X509_SIGNATURE_UNSUPPORTED;
  }
}

/* Inputs of one native signature check. message holds count parts.
 * algorithm is the caller's algorithm record, and fields are the encoded spans
 * it references. */
typedef struct {
  const TC_bytes* message;
  size_t count;
  TC_bytes algorithm;
  const TC_bytes* fields;
  size_t field_count;
  TC_bytes signature;
  const TC_X509_public_key* key;
} native_inputs;

/* Check that workspace storage is disjoint from every input and charge the
 * encoded input bytes. */
static TC_X509_signature_result native_storage(const TC_X509_native_workspace* workspace,
                                               const native_inputs* in, size_t* work)
{
  const TC_bytes* message = in->message;
  const size_t count = in->count;
  const TC_bytes* algorithm_fields = in->fields;
  const size_t field_count = in->field_count;
  const TC_X509_public_key* key = in->key;
  TC_bytes writes[3];
  tc_pki_storage_plan plan;
  size_t budget;
  if (!workspace || !key || !work || (count && !message))
    return TC_X509_SIGNATURE_ERROR;
  const TC_bytes fields[] = {key->algorithm.oid, key->algorithm.parameters,
                             key->key,           key->modulus,
                             key->exponent,      key->curve_oid,
                             in->signature};
  tc_pki_storage_plan_begin(&plan, writes, 3, *work);
  TC_PKI_PLAN_WRITE(&plan, workspace->ec, workspace->ec ? 1 : 0);
  tc_pki_storage_plan_write(&plan, workspace->rsa ? workspace->rsa->words : NULL,
                            workspace->rsa ? workspace->rsa->capacity : 0, sizeof(TC_RSA_word));
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, message, count);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  TC_PKI_PLAN_INPUT(&plan, workspace->rsa, workspace->rsa ? 1 : 0);
  tc_pki_storage_plan_input_span(&plan, in->algorithm);
  TC_PKI_PLAN_INPUT(&plan, key, 1);
  tc_pki_storage_plan_input_spans(&plan, fields, sizeof fields / sizeof *fields);
  tc_pki_storage_plan_input_spans(&plan, algorithm_fields, field_count);
  tc_pki_storage_plan_input_spans(&plan, message, count);
  TC_TLV_result checked = tc_pki_storage_plan_finish(&plan, &budget);
  if (checked != TC_TLV_OK)
    return tc_pki_signature_error(checked);
  /* Charge encoded bytes before parsing OIDs, PSS parameters and signatures. */
  for (size_t i = 0; i < field_count; ++i)
    if (tc_pki_work_charge(&budget, algorithm_fields[i].length) != TC_TLV_OK)
      return TC_X509_SIGNATURE_LIMIT;
  for (size_t i = 0; i < count; ++i)
    if (tc_pki_work_charge(&budget, message[i].length) != TC_TLV_OK)
      return TC_X509_SIGNATURE_LIMIT;
  for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i)
    if (tc_pki_work_charge(&budget, fields[i].length) != TC_TLV_OK)
      return TC_X509_SIGNATURE_LIMIT;
  *work = budget;
  return TC_X509_SIGNATURE_VALID;
}

static TC_X509_signature_result native_verify_digest(void* context, TC_bytes digest,
                                                     const TC_signature_algorithm* algorithm,
                                                     TC_bytes signature,
                                                     const TC_X509_public_key* key, size_t* work)
{
  const TC_X509_native_workspace* workspace = context;
  TC_X509_signature_result result;
  TC_TLV_result checked;
  if (!algorithm)
    return TC_X509_SIGNATURE_ERROR;
  const native_inputs inputs = {
      &digest, 1, {(const uint8_t*)algorithm, sizeof *algorithm}, NULL, 0, signature, key};
  result = native_storage(workspace, &inputs, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  checked = tc_pki_signature_key_check(algorithm, key);
  if (checked != TC_TLV_OK)
    return tc_pki_signature_error(checked);
  if (tc_pki_work_charge(work, workspace->signature_work) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  const digest_check check = {
      algorithm, key, digest, signature, workspace->ec, workspace->rsa, workspace->signature_work};
  return tc_pki_verify_digest(&check);
}

static TC_X509_signature_result native_verify(void* context, const TC_bytes* message, size_t count,
                                              const TC_DER_algorithm* algorithm, TC_bytes signature,
                                              const TC_X509_public_key* key, size_t* work)
{
  const TC_X509_native_workspace* workspace = context;
  TC_signature_algorithm selected;
  TC_hash_context hash_workspace;
  tc_hash_info hash;
  enum { MAX_DIGEST_BYTES = 64 };
  uint8_t digest[MAX_DIGEST_BYTES];
  TC_X509_signature_result result;
  TC_TLV_result checked;
  if (!algorithm)
    return TC_X509_SIGNATURE_ERROR;
  const TC_bytes fields[] = {algorithm->oid, algorithm->parameters};
  const native_inputs inputs = {message,
                                count,
                                {(const uint8_t*)algorithm, sizeof *algorithm},
                                fields,
                                sizeof fields / sizeof *fields,
                                signature,
                                key};
  result = native_storage(workspace, &inputs, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  checked = tc_pki_signature_resolve(algorithm, key, &selected);
  if (checked != TC_TLV_OK)
    return tc_pki_signature_error(checked);
  if (!tc_hash_available(selected.hash) || !tc_hash_info_get(selected.hash, &hash))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (tc_pki_work_charge(work, workspace->signature_work) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  if (tc_hash_digest_parts(selected.hash, message, count, digest, &hash_workspace) != TC_OK)
    return TC_X509_SIGNATURE_ERROR;
  const digest_check check = {&selected,
                              key,
                              {digest, hash.digest_length},
                              signature,
                              workspace->ec,
                              workspace->rsa,
                              workspace->signature_work};
  result = tc_pki_verify_digest(&check);
  TC_secure_zero(digest, sizeof digest);
  return result;
}

TC_X509_signature_provider TC_X509_native_provider(const TC_X509_native_workspace* workspace)
{
  TC_X509_signature_provider provider = {native_verify, (void*)workspace, native_verify_digest};
  return provider;
}
#endif
