/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_VERIFY_INTERNAL_H_
#define TC_PKI_VERIFY_INTERNAL_H_
#include "pki_signature_internal.h"
#include <tiny_crypto/ec.h>
#include <tiny_crypto/rsa.h>

/* Covers two scalar multiplies and inversions; not a cycle count. */
enum { TC_PKI_ECDSA_WORK_PER_BIT = 64 };

/* Inputs/metadata are stable and disjoint from both workspaces. The caller
 * resolves algorithms and charges hashing/DER bytes before this operation.
 * Only the selected algorithm's workspace is required. */
static inline TC_X509_signature_result tc_pki_verify_digest(
    const TC_signature_algorithm* algorithm, const TC_X509_public_key* key,
    TC_bytes digest, TC_bytes signature, TC_ECDSA_workspace* ec,
    const TC_RSA_workspace* rsa, size_t max_work)
{
  tc_hash_info hash;
  if (!algorithm || !key) return TC_X509_SIGNATURE_ERROR;
  if (!tc_hash_info_get(algorithm->hash,&hash)) return TC_X509_SIGNATURE_UNSUPPORTED;
  if (!digest.data || digest.length != hash.digest_length) return TC_X509_SIGNATURE_ERROR;
  if (algorithm->scheme == TC_SIGNATURE_ECDSA) {
#if TC_ENABLE_EC
    TC_DER_signature_pair pair;
    uint8_t raw[2 * TC_EC_MAX_BYTES];
    size_t width;
    TC_status result;
    switch (key->curve) {
#if TC_EC_ENABLE_P192
      case TC_EC_P192: width = 24; break;
#endif
#if TC_EC_ENABLE_P256
      case TC_EC_P256: width = 32; break;
#endif
#if TC_EC_ENABLE_P384
      case TC_EC_P384: width = 48; break;
#endif
      default: return TC_X509_SIGNATURE_UNSUPPORTED;
    }
    if (!ec) return TC_X509_SIGNATURE_ERROR;
    /* Reserve fixed public work for two scalar multiplies and inversions. */
    if (max_work < width * 8 * TC_PKI_ECDSA_WORK_PER_BIT) return TC_X509_SIGNATURE_LIMIT;
    if (TC_DER_ecdsa_signature(signature.data,signature.length,&pair) != TC_TLV_OK ||
        pair.r.length > width || pair.s.length > width) return TC_X509_SIGNATURE_INVALID;
    memset(raw,0,sizeof raw);
    memcpy(raw + width - pair.r.length,pair.r.data,pair.r.length);
    memcpy(raw + 2 * width - pair.s.length,pair.s.data,pair.s.length);
    result = TC_ECDSA_verify_digest(key->curve,key->key.data,key->key.length,
        digest.data,digest.length,raw,2 * width,ec);
    TC_secure_zero(raw,sizeof raw);
    return result == TC_OK ? TC_X509_SIGNATURE_VALID :
      result == TC_MISMATCH ? TC_X509_SIGNATURE_INVALID : TC_X509_SIGNATURE_ERROR;
#else
    (void)ec;
    return TC_X509_SIGNATURE_UNSUPPORTED;
#endif
  }
#if TC_ENABLE_RSA
  TC_RSA_public_key public_key = {key->modulus,key->exponent};
  TC_RSA_result result;
  TC_work_budget budget = {
    max_work > UINT32_MAX ? UINT32_MAX : (uint32_t)max_work
  };
  if (algorithm->scheme == TC_SIGNATURE_RSA_V15) {
    const TC_RSA_v15_options options = {algorithm->hash};
    result = TC_RSA_verify_v15_digest(&public_key,&options,digest,signature,rsa,&budget);
  } else if (algorithm->scheme == TC_SIGNATURE_RSA_PSS) {
    const TC_RSA_pss_options options = {
      algorithm->hash,algorithm->mgf_hash,algorithm->salt_length
    };
    result = TC_RSA_verify_pss_digest(&public_key,&options,digest,signature,rsa,&budget);
  }
  else return TC_X509_SIGNATURE_UNSUPPORTED;
  switch (result) {
    case TC_RSA_OK: return TC_X509_SIGNATURE_VALID;
    case TC_RSA_INVALID: return TC_X509_SIGNATURE_INVALID;
    case TC_RSA_LIMIT: return TC_X509_SIGNATURE_LIMIT;
    case TC_RSA_UNSUPPORTED: return TC_X509_SIGNATURE_UNSUPPORTED;
    default: return TC_X509_SIGNATURE_ERROR;
  }
#else
  (void)digest; (void)signature; (void)rsa; (void)max_work;
  return TC_X509_SIGNATURE_UNSUPPORTED;
#endif
}
#endif
