/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PIV and TWIC credential validators: CHUID, biometric and unsigned TWIC
 * CHUID. Each validator checks request storage,
 * verifies the signed object, builds and validates the signer path and maps
 * the outcome to TC_credential_status. The Security Object validators are in
 * credential_security.c, the content signer and CVC validators in
 * credential_signer.c. */
#include <string.h>
#include <tiny_crypto/credential.h>
#include <tiny_crypto/piv_biometric.h>
#include <tiny_crypto/piv_cms.h>

#if TC_ENABLE_CREDENTIAL
#include "internal.h"
#include "pki_budget_internal.h"
#include "credential_session_internal.h"
#include "credential_status_internal.h"
#include "credential_policy_internal.h"
#include "x509_time_internal.h"

/* SP 800-76-2 section 9.3: a biometric signed with the CHUID key omits the
 * certificate, so an embedded certificate must carry a different key. Returns
 * INVALID for the same RSA modulus and exponent or the same EC curve and x
 * coordinate with the same y parity. */
static TC_TLV_result biometric_signer_distinct(const TC_X509_public_key* embedded,
                                               const TC_X509_public_key* chuid, size_t* work)
{
  const TC_X509_public_key* keys[2] = {embedded, chuid};
  TC_bytes parts[2][2];
  uint8_t parity[2] = {0, 0};
  for (size_t i = 0; i < 2; ++i) {
    const TC_X509_public_key* key = keys[i];
    if (key->type == TC_KEY_RSA || key->type == TC_KEY_RSA_PSS) {
      parts[i][0] = key->modulus;
      parts[i][1] = key->exponent;
    } else if (key->type == TC_KEY_EC) {
      if (!key->bits)
        return TC_TLV_UNSUPPORTED;
      const TC_bytes point = key->key;
      parity[i] = (point.data[0] == 4 ? point.data[point.length - 1] : point.data[0]) & 1u;
      parts[i][0] = key->curve_oid;
      parts[i][1] = (TC_bytes){point.data + 1, (key->bits + 7u) / 8u};
    } else
      return TC_TLV_UNSUPPORTED;
  }
  if ((keys[0]->type == TC_KEY_EC) != (keys[1]->type == TC_KEY_EC) || parity[0] != parity[1])
    return TC_TLV_OK;
  for (size_t i = 0; i < 2; ++i) {
    if (parts[0][i].length != parts[1][i].length)
      return TC_TLV_OK;
    if (tc_pki_work_charge(work, parts[0][i].length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (memcmp(parts[0][i].data, parts[1][i].data, parts[0][i].length))
      return TC_TLV_OK;
  }
  return TC_TLV_INVALID;
}

TC_credential_status TC_PIV_CHUID_validate(const TC_PIV_CHUID_validation_request* request,
                                           const TC_validation_context* context, size_t* work,
                                           TC_PIV_CHUID_report* out)
{
  tc_credential_session session;
  if (!request || !request->encoded.data || !request->encoded.length || !request->card ||
      !request->card_expiration || !context || !work || !out ||
      (request->twic_reader_policy != 0 && request->twic_reader_policy != 1) ||
      (request->profile != TC_PIV_CARD && request->twic_reader_policy) ||
      (request->chuid_profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP &&
       request->chuid_profile != (request->profile == TC_PIV_CARD
                                      ? TC_CHUID_PROFILE_PIV
                                      : TC_CHUID_PROFILE_TWIC_SIGNED)) ||
      !tc_credential_session_open(&session, context, request->profile))
    return TC_CREDENTIAL_ERROR;
  const int strict_piv = session.piv && !request->twic_reader_policy;
  const TC_PIV_oid_profile oids =
      request->twic_reader_policy ? TC_PIV_OIDS_TWIC_COMPATIBLE : session.oids;

  const TC_bytes inputs[] = {
      request->encoded,
      {(const uint8_t*)request, sizeof *request},
      {(const uint8_t*)request->card, sizeof *request->card},
      {(const uint8_t*)request->card_expiration, sizeof *request->card_expiration},
      request->card->fascn,
      request->card->uuid_urn,
      request->card->fascn_oid};
  const tc_credential_storage storage = {
      inputs, sizeof inputs / sizeof *inputs, out, sizeof *out, NULL, 0, NULL, 0};
  TC_TLV_result parsed = tc_credential_session_bind(&session, context, &storage, work);
  if (parsed == TC_TLV_OK)
    parsed = tc_credential_session_input(&session, request->encoded, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_TLV_limits* limits = &session.policy.path.parsing;

  TC_PIV_CHUID chuid;
  parsed = TC_PIV_CHUID_read(request->encoded, request->encoding, request->chuid_profile, &chuid);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);

  int current, matched;
  parsed =
      tc_credential_chuid_expiration_check(chuid.expiration, &session.policy.path.at, &current);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!current)
    return TC_CREDENTIAL_INVALID;
  parsed = strict_piv ? TC_PIV_card_identifiers_match(request->card, chuid.fascn, chuid.card_uuid,
                                                      work, &matched)
                      : TC_TWIC_card_identifiers_match(request->card, chuid.fascn, chuid.card_uuid,
                                                       work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;

  TC_PIV_CMS_object object;
  session.policy.verification.attribute_oids = tc_credential_attribute_oids(oids);
  parsed = TC_PIV_CMS_read(chuid.signature, TC_PIV_CMS_CHUID, &session.policy.verification, limits,
                           tc_credential_frames(context), work, &object);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = TC_PIV_CMS_identifiers_match(&object, TC_PIV_CMS_CHUID, chuid.fascn, chuid.card_uuid,
                                        limits, tc_credential_frames(context), work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;
  TC_X509_certificate signer;
  parsed = tc_credential_signer_read(object.certificate, limits, tc_credential_scratch(context),
                                     work, &signer);
  if (parsed == TC_TLV_OK)
    parsed = tc_credential_signer_policy(
        &signer, session.piv, request->twic_reader_policy || !session.piv, request->card_expiration,
        &session.policy.path, tc_credential_scratch(context), work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);

  const TC_CMS_validation_request cms = {chuid.signature,      0, object.envelope.content_type,
                                         chuid.signed_content, 2, object.certificate};
  TC_credential_status status =
      tc_credential_session_verify(&session, context, &cms, &object, work);
  if (status == TC_CREDENTIAL_VALID) {
    const TC_PIV_CHUID_report result = {chuid, object.certificate, context->options->at,
                                        request->profile, session.revocation_checked};
    *out = result;
  }
  return status;
}

/* Check the CBEFF header against the request and parse the record under the
 * card's profile. The FASC-N comes from the accepted CHUID. */
static TC_credential_status
biometric_contents_check(const TC_PIV_biometric_validation_request* request,
                         const TC_validation_context* context, TC_PIV_CBEFF* cbeff,
                         TC_PIV_CBEFF_metadata* metadata)
{
  TC_TLV_result parsed = TC_PIV_CBEFF_read(request->encoded, cbeff);
  if (parsed == TC_TLV_OK)
    parsed = TC_PIV_CBEFF_metadata_read(request->encoded, metadata);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (TC_PIV_CBEFF_format_identify(metadata) != request->format)
    return TC_CREDENTIAL_INVALID;
  if (request->require_current) {
    int current;
    if (tc_x509_time_window(&context->options->at, 0, &metadata->valid_from, &metadata->valid_until,
                            &current) != TC_TLV_OK ||
        !current)
      return TC_CREDENTIAL_INVALID;
  }
  if (request->format == TC_PIV_CBEFF_FINGERPRINT_TEMPLATE) {
    TC_PIV_fingerprint_record fingerprint;
    parsed = TC_PIV_fingerprint_read(cbeff->record, &fingerprint);
  } else {
    TC_PIV_face_record face;
    const TC_PIV_face_profile face_profile =
        request->profile == TC_PIV_CARD ? TC_PIV_FACE_PROFILE_PIV : TC_PIV_FACE_PROFILE_TWIC;
    parsed = TC_PIV_face_read(cbeff->record, face_profile, &face);
  }
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_bytes fascn = request->chuid->object.fascn;
  if (cbeff->fascn.length != fascn.length || memcmp(cbeff->fascn.data, fascn.data, fascn.length))
    return TC_CREDENTIAL_INVALID;
  return TC_CREDENTIAL_VALID;
}

TC_credential_status TC_PIV_biometric_validate(const TC_PIV_biometric_validation_request* request,
                                               const TC_validation_context* context, size_t* work,
                                               TC_PIV_biometric_report* out)
{
  tc_credential_session session;
  if (!request || !request->encoded.data || !request->encoded.length ||
      (request->signature_profile != TC_PIV_CMS_BIOMETRIC &&
       request->signature_profile != TC_PIV_CMS_BIOMETRIC_LEGACY) ||
      (request->format != TC_PIV_CBEFF_FINGERPRINT_TEMPLATE &&
       request->format != TC_PIV_CBEFF_FACE_IMAGE && request->format != TC_PIV_CBEFF_IRIS_IMAGE) ||
      !request->card_expiration || !work || !out ||
      !tc_credential_session_open(&session, context, request->profile) ||
      !tc_credential_chuid_bound(request->chuid, request->profile, context))
    return TC_CREDENTIAL_ERROR;
  if (request->format == TC_PIV_CBEFF_IRIS_IMAGE)
    return TC_CREDENTIAL_UNSUPPORTED;
  const TC_PIV_CHUID_report* chuid = request->chuid;
  const TC_bytes inputs[] = {
      request->encoded,
      chuid->object.fascn,
      chuid->object.card_uuid,
      chuid->signer,
      {(const uint8_t*)request, sizeof *request},
      {(const uint8_t*)chuid, sizeof *chuid},
      {(const uint8_t*)request->card_expiration, sizeof *request->card_expiration}};
  const tc_credential_storage storage = {
      inputs, sizeof inputs / sizeof *inputs, out, sizeof *out, NULL, 0, NULL, 0};
  TC_TLV_result parsed = tc_credential_session_bind(&session, context, &storage, work);
  if (parsed == TC_TLV_OK)
    parsed = tc_credential_session_input(&session, request->encoded, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_TLV_limits* limits = &session.policy.path.parsing;
  const TC_X509_path_workspace* scratch = tc_credential_scratch(context);

  TC_PIV_CBEFF cbeff;
  TC_PIV_CBEFF_metadata metadata;
  TC_credential_status status = biometric_contents_check(request, context, &cbeff, &metadata);
  if (status != TC_CREDENTIAL_VALID)
    return status;
  TC_PIV_CMS_object object;
  int matched = 0;
  session.policy.verification.attribute_oids = tc_credential_attribute_oids(session.oids);
  parsed =
      TC_PIV_CMS_read(cbeff.signature, request->signature_profile, &session.policy.verification,
                      limits, tc_credential_frames(context), work, &object);
  if (parsed == TC_TLV_OK)
    parsed = TC_PIV_CMS_identifiers_match(&object, request->signature_profile, chuid->object.fascn,
                                          chuid->object.card_uuid, limits,
                                          tc_credential_frames(context), work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;

  /* Parse each certificate once. The views borrow certificate bytes, so the
   * CHUID signer view survives the second parse. */
  TC_X509_certificate chuid_signer, embedded;
  const TC_X509_certificate* signer = &chuid_signer;
  parsed = tc_credential_signer_read(chuid->signer, limits, scratch, work, &chuid_signer);
  if (parsed == TC_TLV_OK && object.certificate.length) {
    parsed = tc_credential_signer_read(object.certificate, limits, scratch, work, &embedded);
    if (parsed == TC_TLV_OK)
      parsed = biometric_signer_distinct(&embedded.public_key, &chuid_signer.public_key, work);
    signer = &embedded;
  }
  if (parsed == TC_TLV_OK)
    parsed =
        tc_credential_signer_policy(signer, session.piv, !session.piv, request->card_expiration,
                                    &session.policy.path, scratch, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_CMS_validation_request cms = {cbeff.signature,       0, object.envelope.content_type,
                                         &cbeff.signed_content, 1, signer->encoded};
  status = tc_credential_session_verify(&session, context, &cms, &object, work);
  if (status == TC_CREDENTIAL_VALID) {
    const TC_PIV_biometric_report result = {
        request->format,           metadata,         cbeff.record,
        signer->encoded,           request->profile, context->options->at,
        session.revocation_checked};
    *out = result;
  }
  return status;
}

static TC_TLV_result security_object_equals(const TC_PIV_security_report* security,
                                            uint16_t container, TC_bytes expected, size_t* work,
                                            int* matched)
{
  const TC_PIV_security_data* selected = NULL;
  for (size_t i = 0; i < security->count; ++i)
    if (security->objects[i].container == container)
      selected = &security->objects[i];
  if (!selected) {
    *matched = 0;
    return TC_TLV_OK;
  }
  if (selected->count && !selected->parts)
    return TC_TLV_ARGUMENT;
  size_t offset = 0;
  for (size_t i = 0; i < selected->count; ++i) {
    const TC_bytes part = selected->parts[i];
    if ((!part.data && part.length) || part.length > expected.length - offset) {
      *matched = 0;
      return TC_TLV_OK;
    }
    if (tc_pki_work_charge(work, part.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    if (part.length && memcmp(part.data, expected.data + offset, part.length)) {
      *matched = 0;
      return TC_TLV_OK;
    }
    offset += part.length;
  }
  *matched = offset == expected.length;
  return TC_TLV_OK;
}

/* Validate borrowed inventory ranges before charging the caller's counter. */
static TC_TLV_result
unsigned_chuid_storage(const TC_TWIC_unsigned_CHUID_validation_request* request,
                       const TC_validation_context* context, size_t* work)
{
  const TC_PIV_security_report* security = request->security;
  /* The only write is the caller's work counter. */
  TC_bytes counter;
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, &counter, 1, *work);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_seal(&plan);
  const TC_bytes fields[] = {request->encoded, request->card->fascn, request->card->fascn_oid,
                             request->card->uuid_urn, security->signer};
  TC_PKI_PLAN_INPUT(&plan, request, 1);
  TC_PKI_PLAN_INPUT(&plan, request->card, 1);
  TC_PKI_PLAN_INPUT(&plan, security, 1);
  TC_PKI_PLAN_INPUT(&plan, context, 1);
  TC_PKI_PLAN_INPUT(&plan, context->options, 1);
  tc_pki_storage_plan_input_spans(&plan, fields, sizeof fields / sizeof *fields);
  TC_PKI_PLAN_INPUT(&plan, security->objects, security->count);
  for (size_t i = 0; plan.status == TC_TLV_OK && i < security->count; ++i) {
    const TC_PIV_security_data* object = &security->objects[i];
    TC_PKI_PLAN_INPUT(&plan, object->parts, object->count);
    tc_pki_storage_plan_input_spans(&plan, object->parts, object->count);
  }
  return tc_pki_storage_plan_finish(&plan, work);
}

TC_credential_status
TC_TWIC_unsigned_CHUID_validate(const TC_TWIC_unsigned_CHUID_validation_request* request,
                                const TC_validation_context* context, size_t* work)
{
  if (!request || !request->encoded.data || !request->encoded.length || !request->card ||
      !request->security ||
      (request->profile != TC_TWIC_LEGACY_CARD && request->profile != TC_TWIC_NEXGEN_CARD) ||
      !context || !context->options || !work || !request->security->objects ||
      !request->security->count || request->security->count > TC_LDS_MAX_GROUPS ||
      !tc_credential_result_current(request->security->profile, &request->security->at,
                                    request->profile, context))
    return TC_CREDENTIAL_ERROR;
  TC_TLV_result parsed = unsigned_chuid_storage(request, context, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (request->encoded.length > context->options->parsing.max_input)
    return TC_CREDENTIAL_LIMIT;
  int matched;
  parsed = security_object_equals(request->security, TC_TWIC_UNSIGNED_CHUID_CONTAINER,
                                  request->encoded, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;
  TC_PIV_CHUID chuid;
  parsed = TC_PIV_CHUID_read(request->encoded, request->encoding, TC_CHUID_PROFILE_TWIC_UNSIGNED,
                             &chuid);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  int current;
  parsed = tc_credential_chuid_expiration_check(chuid.expiration, &context->options->at, &current);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!current)
    return TC_CREDENTIAL_INVALID;
  parsed =
      TC_TWIC_card_identifiers_match(request->card, chuid.fascn, chuid.card_uuid, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  return matched ? TC_CREDENTIAL_VALID : TC_CREDENTIAL_INVALID;
}

#endif
