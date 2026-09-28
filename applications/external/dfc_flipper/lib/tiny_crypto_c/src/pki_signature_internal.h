/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_SIGNATURE_INTERNAL_H_
#define TC_PKI_SIGNATURE_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include "pki_hash_internal.h"
#include "pki_tree_internal.h"
#include "pki_key_internal.h"

static inline TC_TLV_result tc_pki_pss_resolve_profile(TC_bytes encoded,
    TC_TLV_profile profile, const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_signature_algorithm* out)
{
  tc_pki_pss_parameters parameters;
  TC_signature_algorithm parsed = {TC_SIGNATURE_RSA_PSS,TC_HASH_UNKNOWN,TC_HASH_UNKNOWN,0};
  TC_TLV_result result;
  if (!out || (profile != TC_TLV_DER && profile != TC_TLV_BER) ||
      (profile == TC_TLV_BER && !tree)) return TC_TLV_ARGUMENT;
  result = tree ? tc_pki_pss_read_profile(encoded,profile,limits,tree,&parameters) :
      tc_pki_pss_read(encoded,&parameters);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_hash_algorithm_profile(&parameters.hash,profile,&parsed.hash);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_hash_algorithm_profile(&parameters.mgf_hash,profile,&parsed.mgf_hash);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_uint32_contents(parameters.salt_length.data,parameters.salt_length.length,&parsed.salt_length);
  if (result == TC_TLV_OK) *out = parsed;
  return result;
}

static inline TC_TLV_result tc_pki_pss_resolve(TC_bytes encoded,
    TC_signature_algorithm* out)
{ return tc_pki_pss_resolve_profile(encoded,TC_TLV_DER,NULL,NULL,out); }

/* Parameters and key type come from a successfully parsed key. */
static inline TC_TLV_result tc_pki_signature_usage_check(const TC_signature_algorithm* signature,
    TC_key_type type, TC_bytes parameters)
{
  if (!signature) return TC_TLV_ARGUMENT;
  switch (signature->scheme) {
    case TC_SIGNATURE_ECDSA: return type == TC_KEY_EC ? TC_TLV_OK : TC_TLV_INVALID;
    case TC_SIGNATURE_RSA_V15: return type == TC_KEY_RSA ? TC_TLV_OK : TC_TLV_INVALID;
    case TC_SIGNATURE_RSA_PSS:
      if (type != TC_KEY_RSA && type != TC_KEY_RSA_PSS) return TC_TLV_INVALID;
      if (type == TC_KEY_RSA_PSS && parameters.length) {
        TC_signature_algorithm restriction;
        TC_TLV_result result = tc_pki_pss_resolve(parameters,&restriction);
        if (result != TC_TLV_OK) return result;
        /* RFC 4055 section 3.3: the key's salt length is a minimum. */
        if (signature->hash != restriction.hash || signature->mgf_hash != restriction.mgf_hash ||
            signature->salt_length < restriction.salt_length) return TC_TLV_INVALID;
      }
      return TC_TLV_OK;
    default: return TC_TLV_UNSUPPORTED;
  }
}

/* Certificate, CMS and imported private keys share usage restrictions. */
static inline TC_TLV_result tc_pki_signature_key_check(const TC_signature_algorithm* signature,
    const TC_X509_public_key* key)
{
  return key ? tc_pki_signature_usage_check(signature,key->type,key->algorithm.parameters) :
      TC_TLV_ARGUMENT;
}


/* Decode signature and hash parameters before selecting a signer. Inputs have
 * validated spans and are disjoint from out; out changes only on OK. */
static inline TC_TLV_result tc_pki_signature_algorithm_read(const TC_DER_algorithm* algorithm,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_signature_algorithm* out)
{
  static const uint8_t rsa[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1};
  static const uint8_t ec[] = {0x2a,0x86,0x48,0xce,0x3d,4};
  TC_signature_algorithm parsed = {TC_SIGNATURE_ECDSA,TC_HASH_UNKNOWN,TC_HASH_UNKNOWN,0};
  TC_TLV_result result;
  if (!algorithm || !out || (profile != TC_TLV_DER && profile != TC_TLV_BER) ||
      (profile == TC_TLV_BER && !tree)) return TC_TLV_ARGUMENT;
  TC_bytes oid = algorithm->oid;
  if (oid.length == 9 && !memcmp(oid.data,rsa,sizeof rsa)) {
    if (oid.data[8] == 10) {
      result = tc_pki_pss_resolve_profile(algorithm->parameters,profile,limits,tree,&parsed);
      if (result != TC_TLV_OK) return result;
    } else {
      switch (oid.data[8]) {
        case 5: parsed.hash = TC_HASH_SHA1; break;
        case 14: parsed.hash = TC_HASH_SHA224; break;
        case 11: parsed.hash = TC_HASH_SHA256; break;
        case 12: parsed.hash = TC_HASH_SHA384; break;
        case 13: parsed.hash = TC_HASH_SHA512; break;
        default: return TC_TLV_UNSUPPORTED;
      }
      if (algorithm->parameters.length &&
          tc_pki_null(algorithm->parameters,profile) != TC_TLV_OK)
        return TC_TLV_INVALID;
      parsed.scheme = TC_SIGNATURE_RSA_V15;
    }
  } else if ((oid.length == 7 || oid.length == 8) && !memcmp(oid.data,ec,sizeof ec)) {
    if (algorithm->parameters.length) return TC_TLV_INVALID;
    if (oid.length == 7 && oid.data[6] == 1) parsed.hash = TC_HASH_SHA1;
    else if (oid.length == 8 && oid.data[6] == 3 && oid.data[7] >= 1 && oid.data[7] <= 4)
      parsed.hash = (TC_hash_algorithm)(TC_HASH_SHA224 + oid.data[7] - 1);
    else return TC_TLV_UNSUPPORTED;
  } else return TC_TLV_UNSUPPORTED;
  *out = parsed;
  return TC_TLV_OK;
}
/* Apply the selected key's type and algorithm restrictions. */
static inline TC_TLV_result tc_pki_signature_resolve_profile(const TC_DER_algorithm* algorithm,
    const TC_X509_public_key* key, TC_TLV_profile profile, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_signature_algorithm* out)
{
  if (!key || !out) return TC_TLV_ARGUMENT;
  TC_signature_algorithm parsed;
  TC_TLV_result result = tc_pki_signature_algorithm_read(algorithm,profile,limits,tree,&parsed);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_signature_key_check(&parsed,key);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}
static inline TC_TLV_result tc_pki_signature_resolve(const TC_DER_algorithm* algorithm,
    const TC_X509_public_key* key, TC_signature_algorithm* out)
{ return tc_pki_signature_resolve_profile(algorithm,key,TC_TLV_DER,NULL,NULL,out); }
#endif
