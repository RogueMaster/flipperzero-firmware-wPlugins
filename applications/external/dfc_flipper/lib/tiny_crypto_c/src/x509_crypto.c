/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509_crypto.h>
#if TC_ENABLE_X509
#include "pki_verify_internal.h"
#include "pki_storage_internal.h"
#include "hash_dispatch_internal.h"
#include "x509_path_internal.h"

static TC_X509_signature_result native_status(TC_TLV_result result)
{
  return result == TC_TLV_LIMIT ? TC_X509_SIGNATURE_LIMIT :
    result == TC_TLV_UNSUPPORTED ? TC_X509_SIGNATURE_UNSUPPORTED :
    result == TC_TLV_ARGUMENT ? TC_X509_SIGNATURE_ERROR : TC_X509_SIGNATURE_INVALID;
}

static TC_X509_signature_result native_storage(const TC_X509_native_workspace* workspace,
    const TC_bytes* message, size_t count, TC_bytes algorithm_metadata,
    const TC_bytes* algorithm_fields, size_t field_count, TC_bytes signature,
    const TC_X509_public_key* key, size_t* work)
{
  TC_bytes message_storage, ec_storage, rsa_storage;
  size_t budget;
  TC_TLV_result checked;
  if (!workspace || !key || !work || (count && !message)) return TC_X509_SIGNATURE_ERROR;
  budget = *work;
  if (tc_pki_storage_span(message,count,sizeof *message,&message_storage) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->ec,workspace->ec ? 1 : 0,sizeof *workspace->ec,&ec_storage) != TC_TLV_OK ||
      tc_pki_storage_span(workspace->rsa ? workspace->rsa->words : NULL,
          workspace->rsa ? workspace->rsa->capacity : 0,sizeof(TC_RSA_word),&rsa_storage) != TC_TLV_OK)
    return TC_X509_SIGNATURE_ERROR;
  const TC_bytes writes[] = {ec_storage,rsa_storage,{(const uint8_t*)work,sizeof *work}};
  const TC_bytes metadata[] = {
    message_storage, {(const uint8_t*)workspace,sizeof *workspace},
    {(const uint8_t*)workspace->rsa,workspace->rsa ? sizeof *workspace->rsa : 0},
    algorithm_metadata, {(const uint8_t*)key,sizeof *key}
  };
  const TC_bytes fields[] = {
    key->algorithm.oid, key->algorithm.parameters,
    key->key, key->modulus, key->exponent, key->curve_oid, signature
  };
  const size_t write_count = sizeof writes / sizeof *writes;
  for (size_t i = 0; i < write_count; ++i)
    for (size_t j = i + 1; j < write_count; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data,writes[i].length,writes[j].data,writes[j].length))
        return TC_X509_SIGNATURE_ERROR;
  for (size_t i = 0; i < sizeof metadata / sizeof *metadata; ++i) {
    checked = tc_pki_storage_input(writes,write_count,metadata[i],&budget);
    if (checked != TC_TLV_OK) return native_status(checked);
  }
  for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i) {
    checked = tc_pki_storage_input(writes,write_count,fields[i],&budget);
    if (checked != TC_TLV_OK) return native_status(checked);
  }
  for (size_t i = 0; i < field_count; ++i) {
    checked = tc_pki_storage_input(writes,write_count,algorithm_fields[i],&budget);
    if (checked != TC_TLV_OK) return native_status(checked);
    if (tc_x509_path_charge(&budget,algorithm_fields[i].length) != TC_TLV_OK)
      return TC_X509_SIGNATURE_LIMIT;
  }
  for (size_t i = 0; i < count; ++i) {
    checked = tc_pki_storage_input(writes,write_count,message[i],&budget);
    if (checked != TC_TLV_OK) return native_status(checked);
    if (tc_x509_path_charge(&budget,message[i].length) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  }
  /* Charge encoded fields before parsing OIDs, PSS parameters and signatures. */
  for (size_t i = 0; i < sizeof fields / sizeof *fields; ++i)
    if (tc_x509_path_charge(&budget,fields[i].length) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  *work = budget;
  return TC_X509_SIGNATURE_VALID;
}

static TC_X509_signature_result native_verify_digest(void* context, TC_bytes digest,
    const TC_signature_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* key, size_t* work)
{
  const TC_X509_native_workspace* workspace = context;
  TC_X509_signature_result result;
  TC_TLV_result checked;
  if (!algorithm) return TC_X509_SIGNATURE_ERROR;
  result = native_storage(workspace,&digest,1,
      (TC_bytes){(const uint8_t*)algorithm,sizeof *algorithm},NULL,0,signature,key,work);
  if (result != TC_X509_SIGNATURE_VALID) return result;
  checked = tc_pki_signature_key_check(algorithm,key);
  if (checked != TC_TLV_OK) return native_status(checked);
  if (tc_x509_path_charge(work,workspace->signature_work) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  return tc_pki_verify_digest(algorithm,key,digest,signature,workspace->ec,workspace->rsa,
      workspace->signature_work);
}

static TC_X509_signature_result native_verify(void* context, const TC_bytes* message,
    size_t count, const TC_DER_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* key, size_t* work)
{
  const TC_X509_native_workspace* workspace = context;
  TC_signature_algorithm selected;
  tc_hash_workspace hash_workspace;
  tc_hash_info hash;
  enum { MAX_DIGEST_BYTES = 64 };
  uint8_t digest[MAX_DIGEST_BYTES];
  TC_X509_signature_result result;
  TC_TLV_result checked;
  if (!algorithm) return TC_X509_SIGNATURE_ERROR;
  const TC_bytes fields[] = {algorithm->oid,algorithm->parameters};
  result = native_storage(workspace,message,count,
      (TC_bytes){(const uint8_t*)algorithm,sizeof *algorithm},fields,
      sizeof fields / sizeof *fields,signature,key,work);
  if (result != TC_X509_SIGNATURE_VALID) return result;
  checked = tc_pki_signature_resolve(algorithm,key,&selected);
  if (checked != TC_TLV_OK) return native_status(checked);
  if (!tc_hash_available(selected.hash) || !tc_hash_info_get(selected.hash,&hash))
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (tc_x509_path_charge(work,workspace->signature_work) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  if (tc_hash_digest_parts(selected.hash,message,count,digest,&hash_workspace) != TC_OK)
    return TC_X509_SIGNATURE_ERROR;
  result = tc_pki_verify_digest(&selected,key,(TC_bytes){digest,hash.digest_length},
      signature,workspace->ec,workspace->rsa,workspace->signature_work);
  TC_secure_zero(digest,sizeof digest);
  return result;
}

TC_X509_signature_provider TC_X509_native_provider(const TC_X509_native_workspace* workspace)
{
  TC_X509_signature_provider provider = {native_verify,(void*)workspace,native_verify_digest};
  return provider;
}
#endif
