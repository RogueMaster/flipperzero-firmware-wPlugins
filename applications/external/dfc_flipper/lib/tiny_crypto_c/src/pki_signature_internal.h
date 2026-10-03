/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_SIGNATURE_INTERNAL_H_
#define TC_PKI_SIGNATURE_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include "pki_hash_internal.h"
#include "pki_tree_internal.h"
#include "pki_key_internal.h"
#include "pki_signature_oid_internal.h"

static inline TC_TLV_result tc_pki_pss_resolve_profile(TC_bytes encoded, TC_TLV_profile profile,
                                                       const TC_TLV_limits* limits,
                                                       const tc_pki_tree_workspace* tree,
                                                       TC_signature_algorithm* out)
{
  tc_pki_pss_parameters parameters;
  TC_signature_algorithm parsed = {TC_SIGNATURE_RSA_PSS, TC_HASH_UNKNOWN, TC_HASH_UNKNOWN, 0};
  TC_TLV_result result;
  if (!out || (profile != TC_TLV_DER && profile != TC_TLV_BER) || (profile == TC_TLV_BER && !tree))
    return TC_TLV_ARGUMENT;
  result = tree ? tc_pki_pss_read_profile(encoded, profile, limits, tree, &parameters)
                : tc_pki_pss_read(encoded, &parameters);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_hash_algorithm_profile(&parameters.hash, profile, &parsed.hash);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_hash_algorithm_profile(&parameters.mgf_hash, profile, &parsed.mgf_hash);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_uint32_contents(parameters.salt_length, &parsed.salt_length);
  if (result == TC_TLV_OK)
    *out = parsed;
  return result;
}

static inline TC_TLV_result tc_pki_pss_resolve(TC_bytes encoded, TC_signature_algorithm* out)
{
  return tc_pki_pss_resolve_profile(encoded, TC_TLV_DER, NULL, NULL, out);
}

/* Parameters and key type come from a successfully parsed key. */
static inline TC_TLV_result tc_pki_signature_usage_check(const TC_signature_algorithm* signature,
                                                         TC_key_type type, TC_bytes parameters)
{
  if (!signature)
    return TC_TLV_ARGUMENT;
  switch (signature->scheme) {
  case TC_SIGNATURE_ECDSA:
    return type == TC_KEY_EC ? TC_TLV_OK : TC_TLV_INVALID;
  case TC_SIGNATURE_RSA_V15:
    return type == TC_KEY_RSA ? TC_TLV_OK : TC_TLV_INVALID;
  case TC_SIGNATURE_RSA_PSS:
    if (type != TC_KEY_RSA && type != TC_KEY_RSA_PSS)
      return TC_TLV_INVALID;
    if (type == TC_KEY_RSA_PSS && parameters.length) {
      TC_signature_algorithm restriction;
      TC_TLV_result result = tc_pki_pss_resolve(parameters, &restriction);
      if (result != TC_TLV_OK)
        return result;
      /* RFC 4055 section 3.3: the key's salt length is a minimum. */
      if (signature->hash != restriction.hash || signature->mgf_hash != restriction.mgf_hash ||
          signature->salt_length < restriction.salt_length)
        return TC_TLV_INVALID;
    }
    return TC_TLV_OK;
  default:
    return TC_TLV_UNSUPPORTED;
  }
}

/* Certificate, CMS and imported private keys share usage restrictions. */
static inline TC_TLV_result tc_pki_signature_key_check(const TC_signature_algorithm* signature,
                                                       const TC_X509_public_key* key)
{
  return key ? tc_pki_signature_usage_check(signature, key->type, key->algorithm.parameters)
             : TC_TLV_ARGUMENT;
}

/* Decode signature and hash parameters before selecting a signer. Inputs have
 * validated spans and are disjoint from out. out changes only on OK. */
static inline TC_TLV_result tc_pki_signature_algorithm_read(const TC_DER_algorithm* algorithm,
                                                            TC_TLV_profile profile,
                                                            const TC_TLV_limits* limits,
                                                            const tc_pki_tree_workspace* tree,
                                                            TC_signature_algorithm* out)
{
  TC_signature_algorithm parsed = {TC_SIGNATURE_ECDSA, TC_HASH_UNKNOWN, TC_HASH_UNKNOWN, 0};
  TC_TLV_result result;
  if (!algorithm || !out || (profile != TC_TLV_DER && profile != TC_TLV_BER) ||
      (profile == TC_TLV_BER && !tree))
    return TC_TLV_ARGUMENT;
  tc_pki_signature_oid_info info = tc_pki_signature_oid_classify(algorithm->oid);
  if (info.kind == TC_PKI_SIGNATURE_RSA_PSS) {
    result = tc_pki_pss_resolve_profile(algorithm->parameters, profile, limits, tree, &parsed);
    if (result != TC_TLV_OK)
      return result;
  } else if ((info.kind == TC_PKI_SIGNATURE_RSA_V15 && info.hash != TC_HASH_UNKNOWN) ||
             info.kind == TC_PKI_SIGNATURE_ECDSA) {
    if (tc_pki_signature_parameters_check(info.kind, algorithm->parameters, profile) != TC_TLV_OK)
      return TC_TLV_INVALID;
    parsed.scheme = info.kind == TC_PKI_SIGNATURE_ECDSA ? TC_SIGNATURE_ECDSA : TC_SIGNATURE_RSA_V15;
    parsed.hash = info.hash;
  } else
    return TC_TLV_UNSUPPORTED;
  *out = parsed;
  return TC_TLV_OK;
}
/* Apply the selected key's type and algorithm restrictions. */
static inline TC_TLV_result
tc_pki_signature_resolve_profile(const TC_DER_algorithm* algorithm, const TC_X509_public_key* key,
                                 TC_TLV_profile profile, const TC_TLV_limits* limits,
                                 const tc_pki_tree_workspace* tree, TC_signature_algorithm* out)
{
  if (!key || !out)
    return TC_TLV_ARGUMENT;
  TC_signature_algorithm parsed;
  TC_TLV_result result = tc_pki_signature_algorithm_read(algorithm, profile, limits, tree, &parsed);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_signature_key_check(&parsed, key);
  if (result != TC_TLV_OK)
    return result;
  *out = parsed;
  return TC_TLV_OK;
}
static inline TC_TLV_result tc_pki_signature_resolve(const TC_DER_algorithm* algorithm,
                                                     const TC_X509_public_key* key,
                                                     TC_signature_algorithm* out)
{
  return tc_pki_signature_resolve_profile(algorithm, key, TC_TLV_DER, NULL, NULL, out);
}
#endif
