/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CMS_SIGNATURE_INTERNAL_H_
#define TC_CMS_SIGNATURE_INTERNAL_H_
#include "cms_base_internal.h"
#include "pki_signature_internal.h"

typedef struct {
  TC_hash_algorithm content_hash;
  TC_signature_algorithm signature;
} tc_cms_signature_algorithm;

static inline int tc_cms_rsa_parameters_valid(TC_CMS_rsa_parameters policy)
{ return policy == TC_CMS_RSA_PARAMETERS_NULL || policy == TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT; }

static inline int tc_cms_verification_policy_valid(TC_CMS_verification_policy policy)
{
  return (policy.attributes == TC_CMS_ATTRIBUTES_DER || policy.attributes == TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER) &&
      tc_cms_rsa_parameters_valid(policy.rsa_parameters);
}

/* Resolve parsed SignerInfo algorithms without hashing or signature validation.
 * Input spans are valid and disjoint from out; out changes only on success. */
static inline TC_TLV_result tc_cms_signature_resolve_policy(const tc_cms_signer_info* signer,
    const TC_X509_public_key* key, TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_CMS_rsa_parameters rsa_parameters,
    tc_cms_signature_algorithm* out)
{
  static const uint8_t rsa_encryption[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,1};
  tc_cms_signature_algorithm parsed = {TC_HASH_UNKNOWN,
    {TC_SIGNATURE_RSA_V15,TC_HASH_UNKNOWN,TC_HASH_UNKNOWN,0}};
  TC_TLV_result result;
  if (!signer || !key || !out ||
      !tc_cms_rsa_parameters_valid(rsa_parameters) ||
      (profile != TC_TLV_DER && profile != TC_TLV_BER) ||
      (profile == TC_TLV_BER && !tree) || (tree && (!tree->work || !limits))) return TC_TLV_ARGUMENT;
  if (tree) {
    const TC_bytes fields[] = {signer->digest_algorithm.oid,signer->digest_algorithm.parameters,
      signer->signature_algorithm.oid,signer->signature_algorithm.parameters,key->algorithm.parameters};
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i)
      if (tc_x509_path_charge(tree->work,fields[i].length) != TC_TLV_OK) return TC_TLV_LIMIT;
  }
  result = tc_pki_hash_algorithm_profile(&signer->digest_algorithm,profile,&parsed.content_hash);
  if (result != TC_TLV_OK) return result;
  if (tc_pki_equal(signer->signature_algorithm.oid,
      (TC_bytes){rsa_encryption,sizeof rsa_encryption})) {
    /* RFC 3370: rsaEncryption takes its hash from digestAlgorithm. */
    if (signer->signature_algorithm.parameters.length || rsa_parameters == TC_CMS_RSA_PARAMETERS_NULL)
      if (tc_pki_null(signer->signature_algorithm.parameters,profile) != TC_TLV_OK) return TC_TLV_INVALID;
    parsed.signature.hash = parsed.content_hash;
    result = tc_pki_signature_key_check(&parsed.signature,key);
  } else {
    result = tc_pki_signature_resolve_profile(&signer->signature_algorithm,key,profile,limits,tree,&parsed.signature);
    if (result != TC_TLV_OK) return result;
    /* RFC 4056 permits distinct content and signedAttrs hashes for PSS.
     * Without attributes there is only one digest to verify. */
    if (parsed.signature.hash != parsed.content_hash &&
        (parsed.signature.scheme != TC_SIGNATURE_RSA_PSS || !signer->signed_attributes.length))
      return TC_TLV_INVALID;
  }
  if (result == TC_TLV_OK) *out = parsed;
  return result;
}
static inline TC_TLV_result tc_cms_signature_resolve_profile(const tc_cms_signer_info* signer,
    const TC_X509_public_key* key, TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_cms_signature_algorithm* out)
{ return tc_cms_signature_resolve_policy(signer,key,profile,limits,tree,TC_CMS_RSA_PARAMETERS_NULL,out); }
static inline TC_TLV_result tc_cms_signature_resolve(const tc_cms_signer_info* signer,
    const TC_X509_public_key* key, tc_cms_signature_algorithm* out)
{ return tc_cms_signature_resolve_profile(signer,key,TC_TLV_DER,NULL,NULL,out); }
#endif
