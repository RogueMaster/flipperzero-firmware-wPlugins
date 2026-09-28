/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_cvc.h>
#if TC_ENABLE_PIV_CVC && TC_ENABLE_X509
#include "pki_storage_internal.h"
#include "pki_extensions_internal.h"
#include "pki_key_internal.h"
#if TC_ENABLE_SHA1
#include <tiny_crypto/hash.h>
#endif
#include <string.h>

enum { ISSUER_BYTES = 8, CARD_UUID_BYTES = 16, MAX_CURVE_OID_BYTES = 8,
  DER_OID_HEADER_BYTES = 2, POINT_WORK_PER_BIT = 8 };

static TC_X509_signature_result parse_result(TC_TLV_result result)
{
  switch (result) {
    case TC_TLV_OK: return TC_X509_SIGNATURE_VALID;
    case TC_TLV_ARGUMENT: return TC_X509_SIGNATURE_ERROR;
    case TC_TLV_LIMIT: return TC_X509_SIGNATURE_LIMIT;
    case TC_TLV_UNSUPPORTED: return TC_X509_SIGNATURE_UNSUPPORTED;
    default: return TC_X509_SIGNATURE_INVALID;
  }
}

static TC_TLV_result storage_check(const TC_PIV_CVC_chain_request* request,
    const TC_TLV_limits* limits, const TC_X509_signature_provider* provider,
    TC_EC_workspace* points, size_t* work, TC_PIV_CVC* out)
{
  if (!request || !request->signer || !limits || !provider || !points || !work || !out ||
      !request->card.data || !request->card.length ||
      (request->card_uuid.length && request->card_uuid.length != CARD_UUID_BYTES)) return TC_TLV_ARGUMENT;
  TC_bytes writes[3];
  if (tc_pki_storage_span(points,1,sizeof *points,&writes[0]) != TC_TLV_OK ||
      tc_pki_storage_span(work,1,sizeof *work,&writes[1]) != TC_TLV_OK ||
      tc_pki_storage_span(out,1,sizeof *out,&writes[2]) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  const TC_X509_public_key* key = &request->signer->public_key;
  const TC_bytes reads[] = {
    {(const uint8_t*)request,sizeof *request}, {(const uint8_t*)limits,sizeof *limits},
    {(const uint8_t*)provider,sizeof *provider}, {(const uint8_t*)request->signer,sizeof *request->signer},
    request->card, request->intermediate, request->card_uuid, request->signer->extensions,
    request->signer->encoded, key->algorithm.oid, key->algorithm.parameters,
    key->key, key->modulus, key->exponent, key->curve_oid
  };
  size_t checks = sizeof reads / sizeof *reads * 3 + 3;
  for (size_t i = 0; i < 3; ++i)
    if (tc_pki_storage_input(writes,i,writes[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < sizeof reads / sizeof *reads; ++i)
    if (tc_pki_storage_input(writes,3,reads[i],&checks) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  return TC_TLV_OK;
}

static TC_X509_signature_result check_point(const TC_PIV_CVC* cvc, TC_EC_curve curve,
    TC_EC_workspace* points, size_t* work)
{
#if TC_ENABLE_EC
  unsigned bits;
  switch (curve) {
#if TC_EC_ENABLE_P256
    case TC_EC_P256: bits = 256; break;
#endif
#if TC_EC_ENABLE_P384
    case TC_EC_P384: bits = 384; break;
#endif
    default: return TC_X509_SIGNATURE_UNSUPPORTED;
  }
  if (cvc->key_bits != bits) return TC_X509_SIGNATURE_INVALID;
  if (tc_x509_path_charge(work,bits * POINT_WORK_PER_BIT) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  return TC_EC_validate_public_key(curve,cvc->public_key.data,cvc->public_key.length,points) == TC_OK ?
      TC_X509_SIGNATURE_VALID : TC_X509_SIGNATURE_INVALID;
#else
  (void)cvc; (void)curve; (void)points; (void)work;
  return TC_X509_SIGNATURE_UNSUPPORTED;
#endif
}

static TC_X509_signature_result intermediate_subject(const TC_PIV_CVC* cvc, size_t* work)
{
#if TC_ENABLE_SHA1
  uint8_t digest[TC_SHA1_DIGESTLEN];
  if (tc_x509_path_charge(work,cvc->public_key.length + ISSUER_BYTES) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  if (TC_SHA1_digest(cvc->public_key.data,cvc->public_key.length,digest) != TC_OK)
    return TC_X509_SIGNATURE_ERROR;
  const int matched = !memcmp(digest,cvc->subject.data,ISSUER_BYTES);
  TC_secure_zero(digest,sizeof digest);
  return matched ? TC_X509_SIGNATURE_VALID : TC_X509_SIGNATURE_INVALID;
#else
  (void)cvc; (void)work;
  return TC_X509_SIGNATURE_UNSUPPORTED;
#endif
}

TC_X509_signature_result TC_PIV_CVC_chain_verify(const TC_PIV_CVC_chain_request* request,
    const TC_TLV_limits* limits, const TC_X509_signature_provider* provider,
    TC_EC_workspace* point_workspace, size_t* work, TC_PIV_CVC* out)
{
  TC_TLV_result parsed = storage_check(request,limits,provider,point_workspace,work,out);
  if (parsed != TC_TLV_OK) return parse_result(parsed);
  if (request->curve != TC_EC_P256 && request->curve != TC_EC_P384)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (request->card.length > limits->max_input || request->intermediate.length > limits->max_input ||
      request->signer->extensions.length > limits->max_input) return TC_X509_SIGNATURE_LIMIT;
  TC_PIV_CVC card, intermediate;
  if (tc_x509_path_charge(work,request->card.length) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
  parsed = TC_PIV_CVC_read(request->card.data,request->card.length,&card);
  if (parsed != TC_TLV_OK) return parse_result(parsed);
  if (card.role != TC_PIV_CVC_CARD_APPLICATION || (request->card_uuid.length &&
      memcmp(card.subject.data,request->card_uuid.data,CARD_UUID_BYTES))) return TC_X509_SIGNATURE_INVALID;
  TC_bytes issuer;
  parsed = tc_pki_subject_key_identifier(request->signer,limits,work,&issuer);
  if (parsed != TC_TLV_OK) return parse_result(parsed);
  if (issuer.length < ISSUER_BYTES) return TC_X509_SIGNATURE_INVALID;
  const TC_X509_public_key* signing_key = &request->signer->public_key;
  TC_X509_public_key intermediate_key = {0};
  uint8_t curve_parameters[DER_OID_HEADER_BYTES + MAX_CURVE_OID_BYTES];
  TC_X509_signature_result result;
  if (request->intermediate.length) {
    if (tc_x509_path_charge(work,request->intermediate.length) != TC_TLV_OK) return TC_X509_SIGNATURE_LIMIT;
    parsed = TC_PIV_CVC_read(request->intermediate.data,request->intermediate.length,&intermediate);
    if (parsed != TC_TLV_OK) return parse_result(parsed);
    if (intermediate.role != TC_PIV_CVC_INTERMEDIATE ||
        memcmp(intermediate.issuer.data,issuer.data,ISSUER_BYTES) ||
        memcmp(card.issuer.data,intermediate.subject.data,ISSUER_BYTES)) return TC_X509_SIGNATURE_INVALID;
    result = intermediate_subject(&intermediate,work);
    if (result != TC_X509_SIGNATURE_VALID) return result;
    result = TC_X509_signature_verify_message(&intermediate.signed_data,1,&intermediate.signature_algorithm,
        intermediate.signature,signing_key,provider,work);
    if (result != TC_X509_SIGNATURE_VALID) return result;
    result = check_point(&intermediate,request->curve,point_workspace,work);
    if (result != TC_X509_SIGNATURE_VALID) return result;
    intermediate_key.type = TC_KEY_EC;
    intermediate_key.algorithm.oid = tc_pki_ec_public_key_oid();
    /* The signature provider accepts DER curve parameters; CVC framing is BER. */
    curve_parameters[0] = 6;
    curve_parameters[1] = (uint8_t)intermediate.curve_oid.length;
    memcpy(curve_parameters + DER_OID_HEADER_BYTES,intermediate.curve_oid.data,intermediate.curve_oid.length);
    intermediate_key.algorithm.parameters = (TC_bytes){curve_parameters,
      intermediate.curve_oid.length + DER_OID_HEADER_BYTES};
    intermediate_key.curve = request->curve;
    intermediate_key.bits = intermediate.key_bits;
    intermediate_key.key = intermediate.public_key;
    intermediate_key.curve_oid = intermediate.curve_oid;
    signing_key = &intermediate_key;
  } else if (memcmp(card.issuer.data,issuer.data,ISSUER_BYTES)) return TC_X509_SIGNATURE_INVALID;
  result = TC_X509_signature_verify_message(&card.signed_data,1,&card.signature_algorithm,
      card.signature,signing_key,provider,work);
  if (result != TC_X509_SIGNATURE_VALID) return result;
  result = check_point(&card,request->curve,point_workspace,work);
  if (result == TC_X509_SIGNATURE_VALID) *out = card;
  return result;
}
#endif
