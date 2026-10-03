/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CMS_SIGNATURE_INTERNAL_H_
#define TC_CMS_SIGNATURE_INTERNAL_H_
#include "cms_base_internal.h"
#include "pki_signature_internal.h"
#include "pki_key_internal.h"

typedef struct {
  TC_hash_algorithm content_hash;
  TC_signature_algorithm signature;
} tc_cms_signature_algorithm;

/* Resolve parsed SignerInfo algorithms. Hashing and signature validation follow.
 * Input spans are valid and disjoint from out. out changes only on success. */
static inline TC_TLV_result tc_cms_signature_resolve_policy(
    const TC_CMS_signer_info* signer, const TC_X509_public_key* key, TC_TLV_profile profile,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_CMS_rsa_parameters rsa_parameters, tc_cms_signature_algorithm* out)
{
  tc_cms_signature_algorithm parsed = {TC_HASH_UNKNOWN,
                                       {TC_SIGNATURE_RSA_V15, TC_HASH_UNKNOWN, TC_HASH_UNKNOWN, 0}};
  TC_TLV_result result;
  if (!signer || !key || !out || !tc_cms_rsa_parameters_valid(rsa_parameters) ||
      (profile != TC_TLV_DER && profile != TC_TLV_BER) || (profile == TC_TLV_BER && !tree) ||
      (tree && (!tree->work || !limits)))
    return TC_TLV_ARGUMENT;
  if (tree) {
    const TC_bytes fields[] = {signer->digest_algorithm.oid, signer->digest_algorithm.parameters,
                               signer->signature_algorithm.oid,
                               signer->signature_algorithm.parameters, key->algorithm.parameters};
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i)
      if (tc_pki_work_charge(tree->work, fields[i].length) != TC_TLV_OK)
        return TC_TLV_LIMIT;
  }
  result = tc_pki_hash_algorithm_profile(&signer->digest_algorithm, profile, &parsed.content_hash);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_equal(signer->signature_algorithm.oid, tc_pki_rsa_encryption_oid())) {
    /* RFC 3370: rsaEncryption takes its hash from digestAlgorithm. */
    if (signer->signature_algorithm.parameters.length ||
        rsa_parameters == TC_CMS_RSA_PARAMETERS_NULL)
      if (tc_pki_null(signer->signature_algorithm.parameters, profile) != TC_TLV_OK)
        return TC_TLV_INVALID;
    parsed.signature.hash = parsed.content_hash;
    result = tc_pki_signature_key_check(&parsed.signature, key);
  } else {
    result = tc_pki_signature_resolve_profile(&signer->signature_algorithm, key, profile, limits,
                                              tree, &parsed.signature);
    if (result != TC_TLV_OK)
      return result;
    /* RFC 4056 permits distinct content and signedAttrs hashes for PSS.
     * Without attributes there is only one digest to verify. */
    if (parsed.signature.hash != parsed.content_hash &&
        (parsed.signature.scheme != TC_SIGNATURE_RSA_PSS || !signer->signed_attributes.length))
      return TC_TLV_INVALID;
  }
  if (result == TC_TLV_OK)
    *out = parsed;
  return result;
}
#endif
