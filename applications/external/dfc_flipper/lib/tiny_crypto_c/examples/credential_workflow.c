/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_workflow.h"
#include <string.h>

enum { EXAMPLE_PRINTED_CONTAINER = 0x3001 };

static ExampleCredentialVerdict credential_verdict(TC_credential_status status)
{
  switch (status) {
  case TC_CREDENTIAL_VALID:
    return EXAMPLE_CREDENTIAL_VALID;
  case TC_CREDENTIAL_INVALID:
    return EXAMPLE_CREDENTIAL_INVALID;
  case TC_CREDENTIAL_REVOKED:
    return EXAMPLE_CREDENTIAL_REVOKED;
  case TC_CREDENTIAL_UNAVAILABLE:
    return EXAMPLE_CREDENTIAL_UNAVAILABLE;
  case TC_CREDENTIAL_UNSUPPORTED:
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  case TC_CREDENTIAL_LIMIT:
    return EXAMPLE_CREDENTIAL_LIMIT;
  default:
    return EXAMPLE_CREDENTIAL_ERROR;
  }
}

static ExampleCredentialVerdict tlv_verdict(TC_TLV_result status)
{
  if (status == TC_TLV_LIMIT)
    return EXAMPLE_CREDENTIAL_LIMIT;
  if (status == TC_TLV_UNSUPPORTED)
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  return status == TC_TLV_ARGUMENT ? EXAMPLE_CREDENTIAL_ERROR : EXAMPLE_CREDENTIAL_INVALID;
}

static int bytes_equal(TC_bytes first, TC_bytes second)
{
  return first.length == second.length &&
         (!first.length || !memcmp(first.data, second.data, first.length));
}

static int printed_is_authenticated(const TC_PIV_security_report* security, TC_bytes printed)
{
  for (size_t i = 0; i < security->count; ++i) {
    const TC_PIV_security_data* object = &security->objects[i];
    if (object->container != EXAMPLE_PRINTED_CONTAINER)
      continue;
    return object->count == 1 && bytes_equal(object->parts[0], printed);
  }
  return 0;
}

static int biometric_formats(const ExampleCredentialValidationRequest* request, unsigned* available)
{
  if ((request->biometric_count && !request->biometrics) ||
      request->biometric_count > EXAMPLE_CREDENTIAL_BIOMETRICS)
    return 0;
  unsigned formats = 0;
  for (size_t i = 0; i < request->biometric_count; ++i) {
    const ExampleCredentialBiometricInput* input = &request->biometrics[i];
    unsigned bit;
    if (!input->encoded.data || !input->encoded.length)
      return 0;
    switch (input->format) {
    case TC_PIV_CBEFF_FINGERPRINT_TEMPLATE:
      bit = 1u;
      *available |= EXAMPLE_CREDENTIAL_REQUIRE_FINGERPRINT;
      break;
    case TC_PIV_CBEFF_FACE_IMAGE:
      bit = 2u;
      *available |= EXAMPLE_CREDENTIAL_REQUIRE_FACE;
      break;
    case TC_PIV_CBEFF_IRIS_IMAGE:
      bit = 4u;
      *available |= EXAMPLE_CREDENTIAL_REQUIRE_IRIS;
      break;
    default:
      return 0;
    }
    if (formats & bit)
      return 0;
    formats |= bit;
  }
  return 1;
}

static int evidence_available(const ExampleCredentialValidationRequest* request,
                              unsigned* available)
{
  const unsigned supported =
      EXAMPLE_CREDENTIAL_REQUIRE_SECURITY | EXAMPLE_CREDENTIAL_REQUIRE_UNSIGNED_CHUID |
      EXAMPLE_CREDENTIAL_REQUIRE_PRINTED | EXAMPLE_CREDENTIAL_REQUIRE_FINGERPRINT |
      EXAMPLE_CREDENTIAL_REQUIRE_FACE | EXAMPLE_CREDENTIAL_REQUIRE_IRIS;
  if (request->required_objects & ~supported)
    return 0;
  if ((request->security.encoded.length && !request->security.encoded.data) ||
      (request->security.unsigned_chuid.length && !request->security.unsigned_chuid.data) ||
      (request->security.printed.length && !request->security.printed.data) ||
      (request->security.count && !request->security.objects) ||
      (request->security.content.capacity && !request->security.content.data) ||
      (!request->security.encoded.length &&
       (request->security.count || request->security.unsigned_chuid.length ||
        request->security.printed.length)))
    return 0;
  *available = 0;
  if (request->security.encoded.length)
    *available |= EXAMPLE_CREDENTIAL_REQUIRE_SECURITY;
  if (request->security.unsigned_chuid.length)
    *available |= EXAMPLE_CREDENTIAL_REQUIRE_UNSIGNED_CHUID;
  if (request->security.printed.length)
    *available |= EXAMPLE_CREDENTIAL_REQUIRE_PRINTED;
  return biometric_formats(request, available);
}

static ExampleCredentialVerdict
cancellation_check(const ExampleCredentialValidationRequest* request, TC_bytes fascn)
{
  TC_TWIC_CCL_result status =
      TC_TWIC_CCL_check_freshness(&request->ccl->metadata, &request->freshness);
  if (status == TC_TWIC_CCL_STALE)
    return EXAMPLE_CREDENTIAL_STALE;
  if (status != TC_TWIC_CCL_OK)
    return EXAMPLE_CREDENTIAL_ERROR;
  int listed;
  status = TC_TWIC_CCL_snapshot_contains(request->ccl, fascn, request->ccl_reads, &listed);
  if (status == TC_TWIC_CCL_STALE)
    return EXAMPLE_CREDENTIAL_STALE;
  if (status == TC_TWIC_CCL_UNAVAILABLE)
    return EXAMPLE_CREDENTIAL_UNAVAILABLE;
  if (status == TC_TWIC_CCL_LIMIT)
    return EXAMPLE_CREDENTIAL_LIMIT;
  if (status != TC_TWIC_CCL_OK)
    return EXAMPLE_CREDENTIAL_ERROR;
  return listed ? EXAMPLE_CREDENTIAL_CANCELLED : EXAMPLE_CREDENTIAL_VALID;
}

ExampleCredentialVerdict
example_credential_validate(const ExampleCredentialValidationRequest* request,
                            const TC_validation_context* card_context,
                            const TC_validation_context* content_context, size_t* work,
                            ExampleCredentialValidationResult* out)
{
  const int piv = request && request->profile == TC_PIV_CARD;
  unsigned available;
  if (!request || !card_context || !card_context->options || !card_context->workspace ||
      !content_context || !content_context->options || !content_context->workspace || !work ||
      !out || !request->certificate.data || !request->certificate.length || !request->chuid.data ||
      !request->chuid.length ||
      (piv ? request->ccl || request->ccl_reads || request->freshness.now ||
                 request->freshness.max_age || request->freshness.minimum_publication
           : !request->ccl) ||
      !request->proof || !evidence_available(request, &available) ||
      (request->profile != TC_PIV_CARD && request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD) ||
      (request->card_key != EXAMPLE_CREDENTIAL_CARD_AUTHENTICATION &&
       request->card_key != EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) ||
      (request->twic_reader_policy != 0 && request->twic_reader_policy != 1) ||
      (request->card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION && !piv) ||
      (request->twic_reader_policy && request->card_key != EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) ||
      (request->profile == TC_PIV_CARD
           ? request->chuid_profile != TC_CHUID_PROFILE_PIV &&
                 request->chuid_profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP
           : request->chuid_profile != TC_CHUID_PROFILE_TWIC_SIGNED))
    return EXAMPLE_CREDENTIAL_ERROR;
  if ((available & request->required_objects) != request->required_objects)
    return EXAMPLE_CREDENTIAL_UNAVAILABLE;

  int order;
  int64_t evaluated;
  if (TC_X509_time_compare(&card_context->options->at, &content_context->options->at, &order) !=
          TC_TLV_OK ||
      order ||
      (!piv && (TC_X509_time_to_unix(&card_context->options->at, &evaluated) != TC_TLV_OK ||
                evaluated < 0 || request->freshness.now != (uint64_t)evaluated)))
    return EXAMPLE_CREDENTIAL_ERROR;

  ExampleCredentialValidationResult accepted = {0};
  TC_bytes card_guid = {NULL, 0};
  if (request->card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) {
    TC_PIV_CHUID structural;
    const TC_TLV_result parsed = TC_PIV_CHUID_read(request->chuid, request->chuid_encoding,
                                                   request->chuid_profile, &structural);
    if (parsed != TC_TLV_OK)
      return tlv_verdict(parsed);
    card_guid = structural.card_uuid;
  }
  /* The library applies the card key's path purpose and reads the
   * subjectAltName identifiers. */
  const TC_PIV_card_certificate_request card = {
      request->certificate, request->profile,
      request->card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION ? TC_PIV_KEY_PIV_AUTHENTICATION
                                                                 : TC_PIV_KEY_CARD_AUTHENTICATION,
      (uint8_t)request->twic_reader_policy, card_guid};
  TC_PIV_card_certificate_report validated;
  TC_credential_status status =
      TC_PIV_card_certificate_validate(&card, card_context, work, &validated);
  ExampleCredentialVerdict verdict = credential_verdict(status);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;
  accepted.card = validated.certificate;
  accepted.identifiers = validated.identifiers;
  if (!piv) {
    verdict = cancellation_check(request, accepted.identifiers.fascn);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
  }

  const TC_PIV_key_policy key_policy = {request->profile, card_context->options->at,
                                        request->rsa_padding, request->allow_legacy_rsa1024};
  TC_PIV_key_parameters key_parameters;
  switch (TC_PIV_key_parameters_select(&accepted.card.certificate, &key_policy, &key_parameters)) {
  case TC_PIV_OK:
    break;
  case TC_PIV_INVALID:
    return EXAMPLE_CREDENTIAL_INVALID;
  case TC_PIV_UNSUPPORTED:
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  default:
    return EXAMPLE_CREDENTIAL_ERROR;
  }

  const TC_status proof =
      request->proof(request->proof_context, request->profile, request->card_key,
                     &accepted.card.certificate.public_key, &key_parameters.challenge);
  if (proof == TC_MISMATCH)
    return EXAMPLE_CREDENTIAL_PROOF_FAILED;
  if (proof != TC_OK)
    return EXAMPLE_CREDENTIAL_ERROR;

  const TC_PIV_CHUID_validation_request chuid = {request->chuid,
                                                 request->chuid_encoding,
                                                 request->profile,
                                                 request->chuid_profile,
                                                 request->twic_reader_policy,
                                                 &accepted.identifiers,
                                                 &accepted.card.certificate.not_after};
  status = TC_PIV_CHUID_validate(&chuid, content_context, work, &accepted.chuid);
  verdict = credential_verdict(status);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;

  if (request->security.encoded.length) {
    const TC_PIV_security_validation_request security = {
        request->security.encoded, request->security.encoding,           request->profile,
        &accepted.chuid,           &accepted.card.certificate.not_after, request->security.objects,
        request->security.count};
    const TC_PIV_security_validation_workspace workspace = {request->security.content.data,
                                                            request->security.content.capacity};
    status =
        TC_PIV_security_validate(&security, content_context, &workspace, work, &accepted.security);
    verdict = credential_verdict(status);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
    accepted.has_security = 1;
    if (request->security.printed.length) {
      if (!printed_is_authenticated(&accepted.security, request->security.printed))
        return EXAMPLE_CREDENTIAL_INVALID;
      const TC_PIV_printed_profile printed_profile =
          piv ? TC_PIV_PRINTED_PROFILE_PIV : TC_PIV_PRINTED_PROFILE_TWIC;
      TC_TLV_result printed_status = TC_PIV_printed_read(
          request->security.printed, TC_PIV_PRINTED_CONTENTS, printed_profile, &accepted.printed);
      if (printed_status != TC_TLV_OK)
        return tlv_verdict(printed_status);
      int current;
      printed_status =
          TC_PIV_printed_expiration_check(&accepted.printed, accepted.chuid.object.expiration,
                                          &content_context->options->at, &current);
      if (printed_status != TC_TLV_OK)
        return tlv_verdict(printed_status);
      if (!current)
        return EXAMPLE_CREDENTIAL_INVALID;
      accepted.has_printed = 1;
    }
    if (request->security.unsigned_chuid.length) {
      if (piv)
        return EXAMPLE_CREDENTIAL_ERROR;
      const TC_TWIC_unsigned_CHUID_validation_request unsigned_chuid = {
          request->security.unsigned_chuid, request->security.unsigned_chuid_encoding,
          request->profile, &accepted.identifiers, &accepted.security};
      status = TC_TWIC_unsigned_CHUID_validate(&unsigned_chuid, content_context, work);
      verdict = credential_verdict(status);
      if (verdict != EXAMPLE_CREDENTIAL_VALID)
        return verdict;
    }
  } else if (request->security.unsigned_chuid.length || request->security.printed.length)
    return EXAMPLE_CREDENTIAL_ERROR;

  for (size_t i = 0; i < request->biometric_count; ++i) {
    const ExampleCredentialBiometricInput* input = &request->biometrics[i];
    const TC_PIV_biometric_validation_request biometric = {
        input->encoded,           request->profile,
        &accepted.chuid,          &accepted.card.certificate.not_after,
        input->signature_profile, input->format,
        input->require_current};
    status = TC_PIV_biometric_validate(&biometric, content_context, work, &accepted.biometrics[i]);
    verdict = credential_verdict(status);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
    accepted.biometric_count = i + 1;
  }
  if (!piv) {
    verdict = cancellation_check(request, accepted.identifiers.fascn);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
  }
  *out = accepted;
  return EXAMPLE_CREDENTIAL_VALID;
}
