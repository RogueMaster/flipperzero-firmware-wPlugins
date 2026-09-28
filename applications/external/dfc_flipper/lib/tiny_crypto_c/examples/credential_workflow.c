/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_workflow.h"
#include <string.h>
#include <tiny_crypto/piv_oid.h>

enum { EXAMPLE_PRINTED_CONTAINER = 0x3001 };

static ExampleCredentialVerdict
credential_verdict(TC_credential_status status) {
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

static ExampleCredentialVerdict
card_identifiers(const TC_X509_validation_result *card,
                 TC_PIV_card_profile profile, ExampleCredentialCardKey card_key,
                 int twic_reader_policy, TC_bytes card_guid,
                 const TC_validation_context *context, size_t *work,
                 TC_PIV_card_identifiers *out) {
  const TC_X509_path_workspace *storage = &context->workspace->path->validation;
  if (card->certificate.extensions.length > *work)
    return EXAMPLE_CREDENTIAL_LIMIT;
  *work -= card->certificate.extensions.length;
  TC_TLV_reader extensions;
  TC_TLV_result status = TC_X509_extensions_init(
      &extensions, card->certificate.extensions.data,
      card->certificate.extensions.length, &context->options->parsing);
  if (status != TC_TLV_OK)
    goto done;
  static const uint8_t san_oid[] = {0x55, 0x1d, 17};
  TC_X509_extension extension;
  int found = 0;
  while ((status = TC_X509_extension_next(&extensions, &extension)) ==
         TC_TLV_OK) {
    if (extension.oid.length != sizeof san_oid ||
        memcmp(extension.oid.data, san_oid, sizeof san_oid))
      continue;
    if (found) {
      status = TC_TLV_INVALID;
      break;
    }
    if (card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION)
      status = twic_reader_policy
                   ? TC_TWIC_authentication_identifiers_read(
                         extension.value, card_guid, &context->options->parsing,
                         storage->frames, storage->frame_capacity, work, out)
                   : TC_PIV_authentication_identifiers_read(
                         extension.value, card_guid, &context->options->parsing,
                         storage->frames, storage->frame_capacity, work, out);
    else
      status = profile == TC_PIV_CARD
                   ? TC_PIV_card_identifiers_read(
                         extension.value, profile, &context->options->parsing,
                         storage->frames, storage->frame_capacity, work, out)
                   : TC_TWIC_card_identifiers_read(
                         extension.value, profile, &context->options->parsing,
                         storage->frames, storage->frame_capacity, work, out);
    if (status != TC_TLV_OK)
      break;
    found = 1;
  }
  if (status == TC_TLV_END)
    status = found ? TC_TLV_OK : TC_TLV_INVALID;
done:
  if (status == TC_TLV_OK)
    return EXAMPLE_CREDENTIAL_VALID;
  if (status == TC_TLV_LIMIT)
    return EXAMPLE_CREDENTIAL_LIMIT;
  if (status == TC_TLV_UNSUPPORTED)
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  return status == TC_TLV_ARGUMENT ? EXAMPLE_CREDENTIAL_ERROR
                                   : EXAMPLE_CREDENTIAL_INVALID;
}

static ExampleCredentialVerdict tlv_verdict(TC_TLV_result status) {
  if (status == TC_TLV_LIMIT)
    return EXAMPLE_CREDENTIAL_LIMIT;
  if (status == TC_TLV_UNSUPPORTED)
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  return status == TC_TLV_ARGUMENT ? EXAMPLE_CREDENTIAL_ERROR
                                   : EXAMPLE_CREDENTIAL_INVALID;
}

static int bytes_equal(TC_bytes first, TC_bytes second) {
  return first.length == second.length &&
         (!first.length || !memcmp(first.data, second.data, first.length));
}

static int printed_is_authenticated(const TC_PIV_security_result *security,
                                    TC_bytes printed) {
  for (size_t i = 0; i < security->count; ++i) {
    const TC_PIV_security_data *object = &security->objects[i];
    if (object->container != EXAMPLE_PRINTED_CONTAINER)
      continue;
    return object->count == 1 && bytes_equal(object->parts[0], printed);
  }
  return 0;
}

static int biometric_formats(const ExampleCredentialValidationRequest *request,
                             unsigned *available) {
  if ((request->biometric_count && !request->biometrics) ||
      request->biometric_count > 3)
    return 0;
  unsigned formats = 0;
  for (size_t i = 0; i < request->biometric_count; ++i) {
    const ExampleCredentialBiometricInput *input = &request->biometrics[i];
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

static int evidence_available(const ExampleCredentialValidationRequest *request,
                              unsigned *available) {
  const unsigned supported = EXAMPLE_CREDENTIAL_REQUIRE_SECURITY |
                             EXAMPLE_CREDENTIAL_REQUIRE_UNSIGNED_CHUID |
                             EXAMPLE_CREDENTIAL_REQUIRE_PRINTED |
                             EXAMPLE_CREDENTIAL_REQUIRE_FINGERPRINT |
                             EXAMPLE_CREDENTIAL_REQUIRE_FACE |
                             EXAMPLE_CREDENTIAL_REQUIRE_IRIS;
  if (request->required_objects & ~supported)
    return 0;
  if ((request->security.encoded.length && !request->security.encoded.data) ||
      (request->security.unsigned_chuid.length &&
       !request->security.unsigned_chuid.data) ||
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
card_policy(const ExampleCredentialValidationRequest *request,
            const TC_validation_context *context, size_t *work,
            TC_validation_options *out) {
  *out = *context->options;
  const TC_PIV_oid_profile oids = request->profile == TC_PIV_CARD
                                      ? TC_PIV_OIDS_ONLY
                                      : TC_PIV_OIDS_TWIC_COMPATIBLE;
  if (out->certificate.purpose.length) {
    if (TC_PIV_oid_identify(out->certificate.purpose, oids) !=
        TC_PIV_OID_CARD_AUTHENTICATION)
      return EXAMPLE_CREDENTIAL_ERROR;
  }
  if (request->profile != TC_PIV_CARD)
    out->certificate.purpose = (TC_bytes){NULL, 0};
  if (!out->certificate.purpose.length) {
    if (!context->workspace->path)
      return EXAMPLE_CREDENTIAL_ERROR;
    const TC_X509_path_workspace *storage =
        &context->workspace->path->validation;
    if (request->certificate.length > *work / 2)
      return EXAMPLE_CREDENTIAL_LIMIT;
    *work -= request->certificate.length * 2;
    TC_X509_workspace parser = {storage->frames, storage->frame_capacity,
                                storage->oids, storage->oid_capacity};
    TC_X509_certificate certificate;
    TC_TLV_result status =
        TC_X509_read(request->certificate.data, request->certificate.length,
                     &out->parsing, &parser, &certificate);
    if (status != TC_TLV_OK)
      return tlv_verdict(status);
    static const uint8_t eku_oid[] = {0x55, 0x1d, 37};
    TC_TLV_reader extensions;
    status =
        TC_X509_extensions_init(&extensions, certificate.extensions.data,
                                certificate.extensions.length, &out->parsing);
    if (status != TC_TLV_OK)
      return tlv_verdict(status);
    TC_X509_extension extension;
    while ((status = TC_X509_extension_next(&extensions, &extension)) ==
           TC_TLV_OK) {
      if (extension.oid.length != sizeof eku_oid ||
          memcmp(extension.oid.data, eku_oid, sizeof eku_oid))
        continue;
      size_t count;
      status = TC_X509_extended_key_usage_read(
          extension.value.data, extension.value.length, storage->oids,
          storage->oid_capacity, &count);
      if (status != TC_TLV_OK)
        return tlv_verdict(status);
      for (size_t i = 0; i < count; ++i) {
        if (TC_PIV_oid_identify(storage->oids[i], oids) !=
            TC_PIV_OID_CARD_AUTHENTICATION)
          continue;
        if (out->certificate.purpose.length)
          return EXAMPLE_CREDENTIAL_INVALID;
        out->certificate.purpose = storage->oids[i];
      }
    }
    if (status != TC_TLV_END)
      return tlv_verdict(status);
    if (!out->certificate.purpose.length)
      return EXAMPLE_CREDENTIAL_INVALID;
  }
  out->certificate.key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  out->certificate.flags |= TC_X509_PATH_REQUIRE_KEY_USAGE |
                            TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                            TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  return EXAMPLE_CREDENTIAL_VALID;
}

static ExampleCredentialVerdict
cancellation_check(const ExampleCredentialValidationRequest *request,
                   TC_bytes fascn) {
  TC_TWIC_CCL_result status =
      TC_TWIC_CCL_check_freshness(&request->ccl->metadata, &request->freshness);
  if (status == TC_TWIC_CCL_STALE)
    return EXAMPLE_CREDENTIAL_STALE;
  if (status != TC_TWIC_CCL_OK)
    return EXAMPLE_CREDENTIAL_ERROR;
  int listed;
  status = TC_TWIC_CCL_snapshot_contains(request->ccl, fascn,
                                         request->ccl_reads, &listed);
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
example_credential_validate(const ExampleCredentialValidationRequest *request,
                            const TC_validation_context *card_context,
                            const TC_validation_context *content_context,
                            size_t *work,
                            ExampleCredentialValidationResult *out) {
  const int piv = request && request->profile == TC_PIV_CARD;
  unsigned available;
  if (!request || !card_context || !card_context->options ||
      !card_context->workspace || !content_context ||
      !content_context->options || !content_context->workspace || !work ||
      !out || !request->certificate.data || !request->certificate.length ||
      !request->chuid.data || !request->chuid.length ||
      (piv ? request->ccl || request->ccl_reads || request->freshness.now ||
                 request->freshness.max_age ||
                 request->freshness.minimum_publication
           : !request->ccl) ||
      !request->proof || !evidence_available(request, &available) ||
      (request->allow_legacy_rsa1024 != 0 &&
       request->allow_legacy_rsa1024 != 1) ||
      (request->rsa_padding != EXAMPLE_CARD_RSA_V15 &&
       request->rsa_padding != EXAMPLE_CARD_RSA_PSS) ||
      (request->profile != TC_PIV_CARD &&
       request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD) ||
      (request->card_key != EXAMPLE_CREDENTIAL_CARD_AUTHENTICATION &&
       request->card_key != EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) ||
      (request->twic_reader_policy != 0 && request->twic_reader_policy != 1) ||
      (request->card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION && !piv) ||
      (request->twic_reader_policy &&
       request->card_key != EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) ||
      (request->profile == TC_PIV_CARD
           ? request->chuid_profile != TC_CHUID_PROFILE_PIV &&
                 request->chuid_profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP
           : request->chuid_profile != TC_CHUID_PROFILE_TWIC_SIGNED))
    return EXAMPLE_CREDENTIAL_ERROR;
  if ((available & request->required_objects) != request->required_objects)
    return EXAMPLE_CREDENTIAL_UNAVAILABLE;

  int order;
  int64_t evaluated;
  if (TC_X509_time_compare(&card_context->options->at,
                           &content_context->options->at,
                           &order) != TC_TLV_OK ||
      order ||
      (!piv &&
       (TC_X509_time_to_unix(&card_context->options->at, &evaluated) !=
            TC_TLV_OK ||
        evaluated < 0 || request->freshness.now != (uint64_t)evaluated)))
    return EXAMPLE_CREDENTIAL_ERROR;

  ExampleCredentialValidationResult accepted = {0};
  TC_validation_options card_options;
  ExampleCredentialVerdict verdict =
      card_policy(request, card_context, work, &card_options);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;
  TC_validation_context constrained_card = *card_context;
  constrained_card.options = &card_options;
  TC_credential_status status = TC_X509_validate(
      request->certificate, &constrained_card, work, &accepted.card);
  verdict = credential_verdict(status);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;
  TC_bytes card_guid = {NULL, 0};
  if (request->card_key == EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION) {
    TC_PIV_CHUID structural;
    const TC_TLV_result parsed = TC_PIV_CHUID_read_profile(
        request->chuid.data, request->chuid.length, request->chuid_encoding,
        request->chuid_profile, &structural);
    if (parsed != TC_TLV_OK)
      return tlv_verdict(parsed);
    card_guid = structural.card_uuid;
  }
  verdict =
      card_identifiers(&accepted.card, request->profile, request->card_key,
                       request->twic_reader_policy, card_guid,
                       &constrained_card, work, &accepted.identifiers);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;
  if (!piv) {
    verdict = cancellation_check(request, accepted.identifiers.fascn);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
  }

  const ExampleCardKeyPolicy key_policy = {
      request->profile, TC_KEY_USAGE_DIGITAL_SIGNATURE,
      request->allow_legacy_rsa1024, request->rsa_padding};
  ExampleCardKeyParameters key_parameters;
  switch (example_card_key_parameters_select(
      &accepted.card.certificate.public_key, &key_policy, &key_parameters)) {
  case EXAMPLE_CARD_KEY_POLICY_OK:
    break;
  case EXAMPLE_CARD_KEY_POLICY_INVALID:
    return EXAMPLE_CREDENTIAL_INVALID;
  case EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED:
    return EXAMPLE_CREDENTIAL_UNSUPPORTED;
  default:
    return EXAMPLE_CREDENTIAL_ERROR;
  }

  const TC_status proof = request->proof(
      request->proof_context, request->profile, request->card_key,
      &accepted.card.certificate.public_key, &key_parameters.challenge);
  if (proof == TC_MISMATCH)
    return EXAMPLE_CREDENTIAL_PROOF_FAILED;
  if (proof != TC_OK)
    return EXAMPLE_CREDENTIAL_ERROR;

  const TC_PIV_CHUID_validation_request chuid = {
      request->chuid,
      request->chuid_encoding,
      request->profile,
      request->chuid_profile,
      request->twic_reader_policy,
      &accepted.identifiers,
      &accepted.card.certificate.not_after};
  status =
      TC_PIV_CHUID_validate(&chuid, content_context, work, &accepted.chuid);
  verdict = credential_verdict(status);
  if (verdict != EXAMPLE_CREDENTIAL_VALID)
    return verdict;

  if (request->security.encoded.length) {
    const TC_PIV_security_validation_request security = {
        request->security.encoded,
        request->security.encoding,
        request->profile,
        accepted.chuid.signer,
        &accepted.card.certificate.not_after,
        request->security.objects,
        request->security.count};
    const TC_PIV_security_validation_workspace workspace = {
        request->security.content.data, request->security.content.capacity};
    status = TC_PIV_security_validate(&security, content_context, &workspace,
                                      work, &accepted.security);
    verdict = credential_verdict(status);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
    accepted.has_security = 1;
    if (request->security.printed.length) {
      if (!printed_is_authenticated(&accepted.security,
                                    request->security.printed))
        return EXAMPLE_CREDENTIAL_INVALID;
      const TC_PIV_printed_profile printed_profile =
          piv ? TC_PIV_PRINTED_PROFILE_PIV : TC_PIV_PRINTED_PROFILE_TWIC;
      TC_TLV_result printed_status = TC_PIV_printed_read(
          request->security.printed, TC_PIV_PRINTED_CONTENTS, printed_profile,
          &accepted.printed);
      if (printed_status != TC_TLV_OK)
        return tlv_verdict(printed_status);
      int current;
      printed_status = TC_PIV_printed_expiration_check(
          &accepted.printed, accepted.chuid.object.expiration,
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
          request->security.unsigned_chuid,
          request->security.unsigned_chuid_encoding, request->profile,
          &accepted.identifiers};
      status = TC_TWIC_unsigned_CHUID_validate(
          &unsigned_chuid, &accepted.security, content_context, work);
      verdict = credential_verdict(status);
      if (verdict != EXAMPLE_CREDENTIAL_VALID)
        return verdict;
    }
  } else if (request->security.unsigned_chuid.length ||
             request->security.printed.length)
    return EXAMPLE_CREDENTIAL_ERROR;

  for (size_t i = 0; i < request->biometric_count; ++i) {
    const ExampleCredentialBiometricInput *input = &request->biometrics[i];
    const TC_PIV_biometric_validation_request biometric = {
        input->encoded,
        request->profile,
        accepted.chuid.object.fascn,
        accepted.chuid.object.card_uuid,
        accepted.chuid.signer,
        &accepted.card.certificate.not_after,
        input->signature_profile,
        input->format,
        input->require_current};
    status = TC_PIV_biometric_validate(&biometric, content_context, work);
    verdict = credential_verdict(status);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
  }
  if (!piv) {
    verdict = cancellation_check(request, accepted.identifiers.fascn);
    if (verdict != EXAMPLE_CREDENTIAL_VALID)
      return verdict;
  }
  *out = accepted;
  return EXAMPLE_CREDENTIAL_VALID;
}
