/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_cvc.h>
#if TC_ENABLE_PIV_CVC && TC_ENABLE_X509
#include "pki_status_internal.h"
#include "pki_storage_internal.h"
#include "pki_extensions_internal.h"
#include "pki_key_internal.h"
#if TC_ENABLE_SHA1
#include <tiny_crypto/hash.h>
#endif
#include <string.h>

enum {
  ISSUER_BYTES = 8,
  CARD_UUID_BYTES = 16,
  MAX_CURVE_OID_BYTES = 8,
  DER_OID_HEADER_BYTES = 2,
  POINT_WORK_PER_BIT = 8
};

static TC_TLV_result storage_check(const TC_PIV_CVC_chain_request* request,
                                   const TC_TLV_limits* limits,
                                   const TC_X509_signature_provider* provider,
                                   TC_EC_workspace* points, size_t* work, TC_PIV_CVC* out)
{
  if (!request || !request->signer || !limits || !provider || !points || !work || !out ||
      !request->card.data || !request->card.length ||
      (request->card_uuid.length && request->card_uuid.length != CARD_UUID_BYTES))
    return TC_TLV_ARGUMENT;
  const TC_X509_public_key* key = &request->signer->public_key;
  const TC_bytes reads[] = {{(const uint8_t*)request, sizeof *request},
                            {(const uint8_t*)limits, sizeof *limits},
                            {(const uint8_t*)provider, sizeof *provider},
                            {(const uint8_t*)request->signer, sizeof *request->signer},
                            request->card,
                            request->intermediate,
                            request->card_uuid,
                            request->signer->extensions,
                            request->signer->encoded,
                            key->algorithm.oid,
                            key->algorithm.parameters,
                            key->key,
                            key->modulus,
                            key->exponent,
                            key->curve_oid};
  TC_bytes writes[3];
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, writes, 3, SIZE_MAX);
  TC_PKI_PLAN_WRITE(&plan, points, 1);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  tc_pki_storage_plan_input_spans(&plan, reads, sizeof reads / sizeof *reads);
  return tc_pki_storage_plan_finish(&plan, NULL) == TC_TLV_OK ? TC_TLV_OK : TC_TLV_ARGUMENT;
}

#if TC_ENABLE_EC
static TC_X509_signature_result check_point(const TC_PIV_CVC* cvc, TC_EC_curve curve,
                                            TC_EC_workspace* points, size_t* work)
{
  const size_t bits = 8 * TC_EC_coordinate_bytes(curve);
  if (!bits)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (cvc->key_bits != bits)
    return TC_X509_SIGNATURE_INVALID;
  if (tc_pki_work_charge(work, bits * POINT_WORK_PER_BIT) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  TC_work_budget budget = {TC_EC_operation_work(curve, TC_EC_OPERATION_VALIDATE)};
  return TC_EC_validate_public_key(curve, cvc->public_key, points, &budget) == TC_EC_OK
             ? TC_X509_SIGNATURE_VALID
             : TC_X509_SIGNATURE_INVALID;
}

#if TC_ENABLE_SHA1
/* SP 800-73-5 Part 2, section 4.1.5: an intermediate's subject is the first
 * eight bytes of SHA-1 over its public-key point. */
static TC_X509_signature_result intermediate_subject(const TC_PIV_CVC* cvc, size_t* work)
{
  uint8_t digest[TC_SHA1_DIGESTLEN];
  if (tc_pki_work_charge(work, cvc->public_key.length + ISSUER_BYTES) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  if (TC_SHA1_digest(cvc->public_key, digest) != TC_OK)
    return TC_X509_SIGNATURE_ERROR;
  const int matched = !memcmp(digest, cvc->subject.data, ISSUER_BYTES);
  TC_secure_zero(digest, sizeof digest);
  return matched ? TC_X509_SIGNATURE_VALID : TC_X509_SIGNATURE_INVALID;
}

/* An intermediate CVC signed under the content signer, with its point as the
 * signing key for the card CVC. curve_parameters backs key.algorithm. */
typedef struct {
  TC_X509_public_key key;
  uint8_t curve_parameters[DER_OID_HEADER_BYTES + MAX_CURVE_OID_BYTES];
} intermediate_link;

static TC_X509_signature_result verify_intermediate(const TC_PIV_CVC_chain_request* request,
                                                    const TC_X509_signature_provider* provider,
                                                    TC_EC_workspace* points, size_t* work,
                                                    TC_bytes issuer, const TC_PIV_CVC* card,
                                                    intermediate_link* link)
{
  TC_PIV_CVC intermediate;
  TC_X509_signature_result result;
  if (tc_pki_work_charge(work, request->intermediate.length) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  TC_TLV_result parsed = TC_PIV_CVC_read(request->intermediate, &intermediate);
  if (parsed != TC_TLV_OK)
    return tc_pki_signature_error(parsed);
  if (intermediate.role != TC_PIV_CVC_INTERMEDIATE ||
      memcmp(intermediate.issuer.data, issuer.data, ISSUER_BYTES) ||
      memcmp(card->issuer.data, intermediate.subject.data, ISSUER_BYTES))
    return TC_X509_SIGNATURE_INVALID;
  result = intermediate_subject(&intermediate, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  result = TC_X509_signature_verify_message(
      &intermediate.signed_data, 1, &intermediate.signature_algorithm, intermediate.signature,
      &request->signer->public_key, provider, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  result = check_point(&intermediate, request->curve, points, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  memset(&link->key, 0, sizeof link->key);
  link->key.type = TC_KEY_EC;
  link->key.algorithm.oid = tc_pki_ec_public_key_oid();
  /* The signature provider accepts DER curve parameters. CVC framing is BER. */
  link->curve_parameters[0] = 6;
  link->curve_parameters[1] = (uint8_t)intermediate.curve_oid.length;
  memcpy(link->curve_parameters + DER_OID_HEADER_BYTES, intermediate.curve_oid.data,
         intermediate.curve_oid.length);
  link->key.algorithm.parameters =
      (TC_bytes){link->curve_parameters, intermediate.curve_oid.length + DER_OID_HEADER_BYTES};
  link->key.curve = request->curve;
  link->key.bits = intermediate.key_bits;
  link->key.key = intermediate.public_key;
  link->key.curve_oid = intermediate.curve_oid;
  return TC_X509_SIGNATURE_VALID;
}
#endif

/* The caller validated storage, the curve and input sizes. */
static TC_X509_signature_result verify_chain(const TC_PIV_CVC_chain_request* request,
                                             const TC_TLV_limits* limits,
                                             const TC_X509_signature_provider* provider,
                                             TC_EC_workspace* points, size_t* work, TC_PIV_CVC* out)
{
  TC_PIV_CVC card;
  TC_bytes issuer;
  TC_X509_signature_result result;
  if (tc_pki_work_charge(work, request->card.length) != TC_TLV_OK)
    return TC_X509_SIGNATURE_LIMIT;
  TC_TLV_result parsed = TC_PIV_CVC_read(request->card, &card);
  if (parsed != TC_TLV_OK)
    return tc_pki_signature_error(parsed);
  if (card.role != TC_PIV_CVC_CARD_APPLICATION ||
      (request->card_uuid.length &&
       memcmp(card.subject.data, request->card_uuid.data, CARD_UUID_BYTES)))
    return TC_X509_SIGNATURE_INVALID;
  parsed = tc_pki_subject_key_identifier(request->signer, limits, work, &issuer);
  if (parsed != TC_TLV_OK)
    return tc_pki_signature_error(parsed);
  if (issuer.length < ISSUER_BYTES)
    return TC_X509_SIGNATURE_INVALID;
  const TC_X509_public_key* signing_key = &request->signer->public_key;
#if TC_ENABLE_SHA1
  intermediate_link link;
  if (request->intermediate.length) {
    result = verify_intermediate(request, provider, points, work, issuer, &card, &link);
    if (result != TC_X509_SIGNATURE_VALID)
      return result;
    signing_key = &link.key;
  }
#endif
  /* A direct card CVC names the content signer as its issuer. */
  if (!request->intermediate.length && memcmp(card.issuer.data, issuer.data, ISSUER_BYTES))
    return TC_X509_SIGNATURE_INVALID;
  result = TC_X509_signature_verify_message(&card.signed_data, 1, &card.signature_algorithm,
                                            card.signature, signing_key, provider, work);
  if (result != TC_X509_SIGNATURE_VALID)
    return result;
  result = check_point(&card, request->curve, points, work);
  if (result == TC_X509_SIGNATURE_VALID)
    *out = card;
  return result;
}
#endif

TC_X509_signature_result TC_PIV_CVC_chain_verify(const TC_PIV_CVC_chain_request* request,
                                                 const TC_TLV_limits* limits,
                                                 const TC_X509_signature_provider* provider,
                                                 TC_EC_workspace* point_workspace, size_t* work,
                                                 TC_PIV_CVC* out)
{
  TC_TLV_result parsed = storage_check(request, limits, provider, point_workspace, work, out);
  if (parsed != TC_TLV_OK)
    return tc_pki_signature_error(parsed);
  if (request->curve != TC_EC_P256 && request->curve != TC_EC_P384)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  if (request->card.length > limits->max_input ||
      request->intermediate.length > limits->max_input ||
      request->signer->extensions.length > limits->max_input)
    return TC_X509_SIGNATURE_LIMIT;
#if TC_ENABLE_EC
  /* The intermediate subject is a SHA-1 prefix of its public-key point. */
  if (request->intermediate.length && !TC_ENABLE_SHA1)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  return verify_chain(request, limits, provider, point_workspace, work, out);
#else
  /* Both CVC points need curve-membership checks. */
  return TC_X509_SIGNATURE_UNSUPPORTED;
#endif
}
#endif
