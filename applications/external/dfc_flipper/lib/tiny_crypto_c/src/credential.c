/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "internal.h"
#include "pki_source_internal.h"
#include "validation_internal.h"
#include <string.h>
#include <tiny_crypto/credential.h>
#include <tiny_crypto/piv_biometric.h>
#include <tiny_crypto/piv_cms.h>

#if TC_ENABLE_CREDENTIAL

static TC_TLV_result
signing_policy_present(TC_bytes encoded_extensions, TC_bytes required,
                       const TC_TLV_limits *limits,
                       const TC_X509_path_workspace *storage) {
  static const uint8_t policies_oid[] = {0x55, 0x1d, 0x20};
  TC_TLV_reader extensions;
  TC_X509_extension extension;
  TC_TLV_result status = TC_X509_extensions_init(
      &extensions, encoded_extensions.data, encoded_extensions.length, limits);
  if (status != TC_TLV_OK)
    return status;
  int found = 0;
  while ((status = TC_X509_extension_next(&extensions, &extension)) ==
         TC_TLV_OK) {
    if (extension.oid.length != sizeof policies_oid ||
        memcmp(extension.oid.data, policies_oid, sizeof policies_oid))
      continue;
    TC_X509_policy_reader policies;
    TC_X509_policy policy;
    status = TC_X509_policies_init(&policies, extension.value.data,
                                   extension.value.length, limits,
                                   storage->oids, storage->oid_capacity);
    if (status != TC_TLV_OK)
      return status;
    while ((status = TC_X509_policy_next(&policies, &policy)) == TC_TLV_OK) {
      if (policy.oid.length == required.length &&
          !memcmp(policy.oid.data, required.data, required.length))
        found = 1;
    }
    if (status != TC_TLV_END)
      return status;
  }
  return status == TC_TLV_END ? (found ? TC_TLV_OK : TC_TLV_INVALID) : status;
}

static TC_TLV_result
content_signing_purpose(TC_bytes extensions, int piv,
                        TC_X509_path_options *policy,
                        const TC_X509_path_workspace *storage) {
  static const uint8_t eku_oid[] = {0x55, 0x1d, 37};
  TC_TLV_reader reader;
  TC_TLV_result status = TC_X509_extensions_init(
      &reader, extensions.data, extensions.length, &policy->parsing);
  if (status != TC_TLV_OK)
    return status;
  TC_X509_extension extension;
  while ((status = TC_X509_extension_next(&reader, &extension)) == TC_TLV_OK) {
    if (extension.oid.length != sizeof eku_oid ||
        memcmp(extension.oid.data, eku_oid, sizeof eku_oid))
      continue;
    size_t count;
    status = TC_X509_extended_key_usage_read(
        extension.value.data, extension.value.length, storage->oids,
        storage->oid_capacity, &count);
    if (status != TC_TLV_OK)
      return status;
    for (size_t i = 0; i < count; ++i) {
      if (TC_PIV_oid_identify(storage->oids[i],
                              piv ? TC_PIV_OIDS_ONLY
                                  : TC_PIV_OIDS_TWIC_COMPATIBLE) !=
          TC_PIV_OID_CONTENT_SIGNING)
        continue;
      if (policy->purpose.length)
        return TC_TLV_INVALID;
      policy->purpose = storage->oids[i];
    }
  }
  if (status != TC_TLV_END)
    return status;
  return policy->purpose.length ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result piv_content_signer_policy(
    const TC_X509_certificate *signer, const TC_X509_time *card_expiration,
    TC_X509_path_options *policy, const TC_X509_path_workspace *storage) {
  static const uint8_t signing_policy[] = {0x60, 0x86, 0x48, 1, 0x65,
                                           3,    2,    1,    3, 39};
  static const TC_bytes required_policy = {signing_policy,
                                           sizeof signing_policy};
  TC_TLV_result status = signing_policy_present(
      signer->extensions, required_policy, &policy->parsing, storage);
  if (status != TC_TLV_OK)
    return status;
  if (card_expiration) {
    int order;
    status = TC_X509_time_compare(&signer->not_after, card_expiration, &order);
    if (status != TC_TLV_OK)
      return status;
    if (order < 0)
      return TC_TLV_INVALID;
  }
  policy->initial_policies = &required_policy;
  policy->initial_policy_count = 1;
  policy->flags |= TC_X509_PATH_REQUIRE_EXPLICIT_POLICY;
  return TC_TLV_OK;
}

static TC_TLV_result
content_signer_policy(TC_bytes certificate, int piv,
                      const TC_X509_time *card_expiration,
                      TC_X509_path_options *policy,
                      const TC_X509_path_workspace *storage, size_t *work) {
  if (!piv && policy->purpose.length)
    policy->purpose = (TC_bytes){NULL, 0};
  if (piv || !policy->purpose.length) {
    if (certificate.length > *work / 3)
      return TC_TLV_LIMIT;
    *work -= certificate.length * 3;
    TC_X509_workspace parser = {storage->frames, storage->frame_capacity,
                                storage->oids, storage->oid_capacity};
    TC_X509_certificate signer;
    TC_TLV_result status = TC_X509_read(certificate.data, certificate.length,
                                        &policy->parsing, &parser, &signer);
    if (status != TC_TLV_OK)
      return status;
    if (!policy->purpose.length) {
      status = content_signing_purpose(signer.extensions, piv, policy, storage);
      if (status != TC_TLV_OK)
        return status;
    }
    if (piv) {
      status =
          piv_content_signer_policy(&signer, card_expiration, policy, storage);
      if (status != TC_TLV_OK)
        return status;
    }
  }
  policy->key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  policy->flags |= TC_X509_PATH_REQUIRE_KEY_USAGE |
                   TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                   TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  return TC_TLV_OK;
}

static TC_TLV_result chuid_expiration_check(TC_bytes expiration,
                                            const TC_X509_time *at,
                                            int *valid) {
  if (!at || !valid || !expiration.data || expiration.length != 8)
    return TC_TLV_ARGUMENT;
  TC_X509_time expires = {0};
  for (size_t i = 0; i < 4; ++i)
    expires.year = expires.year * 10 + expiration.data[i] - '0';
  expires.month =
      (uint8_t)((expiration.data[4] - '0') * 10 + expiration.data[5] - '0');
  expires.day =
      (uint8_t)((expiration.data[6] - '0') * 10 + expiration.data[7] - '0');
  expires.hour = 23;
  expires.minute = 59;
  expires.second = 59;
  int order;
  TC_TLV_result result = TC_X509_time_compare(at, &expires, &order);
  if (result == TC_TLV_OK)
    *valid = order <= 0;
  return result;
}

static int credential_profile(TC_PIV_card_profile profile,
                              const TC_validation_options *options, int *piv,
                              TC_PIV_oid_profile *oids) {
  if (!options || !piv || !oids ||
      (profile != TC_PIV_CARD && profile != TC_TWIC_LEGACY_CARD &&
       profile != TC_TWIC_NEXGEN_CARD))
    return 0;
  *piv = profile == TC_PIV_CARD;
  *oids = *piv ? TC_PIV_OIDS_ONLY : TC_PIV_OIDS_TWIC_COMPATIBLE;
  return (!options->certificate.purpose.data &&
          !options->certificate.purpose.length) ||
         TC_PIV_oid_identify(options->certificate.purpose, *oids) ==
             TC_PIV_OID_CONTENT_SIGNING;
}

static TC_TLV_result
biometric_distinct_signer(TC_bytes embedded, TC_bytes chuid,
                          const TC_TLV_limits *limits,
                          const TC_X509_path_workspace *storage, size_t *work) {
  TC_X509_certificate certificate;
  TC_X509_public_key keys[2];
  const TC_bytes encoded[] = {embedded, chuid};
  TC_X509_workspace parser = {storage->frames, storage->frame_capacity,
                              storage->oids, storage->oid_capacity};
  for (size_t i = 0; i < 2; ++i) {
    if (encoded[i].length > *work)
      return TC_TLV_LIMIT;
    *work -= encoded[i].length;
    TC_TLV_result result = TC_X509_read(encoded[i].data, encoded[i].length,
                                        limits, &parser, &certificate);
    if (result != TC_TLV_OK)
      return result;
    keys[i] = certificate.public_key;
  }
  TC_bytes parts[2][2];
  uint8_t parity[2] = {0, 0};
  for (size_t i = 0; i < 2; ++i) {
    if (keys[i].type == TC_KEY_RSA || keys[i].type == TC_KEY_RSA_PSS) {
      parts[i][0] = keys[i].modulus;
      parts[i][1] = keys[i].exponent;
    } else if (keys[i].type == TC_KEY_EC) {
      if (!keys[i].bits)
        return TC_TLV_UNSUPPORTED;
      const TC_bytes point = keys[i].key;
      parity[i] =
          (point.data[0] == 4 ? point.data[point.length - 1] : point.data[0]) &
          1u;
      parts[i][0] = keys[i].curve_oid;
      parts[i][1] = (TC_bytes){point.data + 1, (keys[i].bits + 7u) / 8u};
    } else
      return TC_TLV_UNSUPPORTED;
  }
  if ((keys[0].type == TC_KEY_EC) != (keys[1].type == TC_KEY_EC) ||
      parity[0] != parity[1])
    return TC_TLV_OK;
  for (size_t i = 0; i < 2; ++i) {
    if (parts[0][i].length != parts[1][i].length)
      return TC_TLV_OK;
    if (parts[0][i].length > *work)
      return TC_TLV_LIMIT;
    *work -= parts[0][i].length;
    if (memcmp(parts[0][i].data, parts[1][i].data, parts[0][i].length))
      return TC_TLV_OK;
  }
  return TC_TLV_INVALID;
}

TC_credential_status
TC_PIV_CHUID_validate(const TC_PIV_CHUID_validation_request *request,
                      const TC_validation_context *context, size_t *work,
                      TC_PIV_CHUID_result *out) {
  TC_CMS_path_options policy;
  TC_X509_path_options crl_policy;
  TC_CMS_revocation_policy revocation;
  if (!request || !request->encoded.data || !request->encoded.length ||
      !request->card || !request->card_expiration || !context || !work ||
      !out ||
      !tc_validation_policies(context, &policy, &crl_policy, &revocation) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, request,
                                   sizeof *request) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, request->encoded.data,
                                   request->encoded.length) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, work, sizeof *work) ||
      (request->profile != TC_PIV_CARD &&
       request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD) ||
      (request->twic_reader_policy != 0 && request->twic_reader_policy != 1) ||
      (request->profile != TC_PIV_CARD && request->twic_reader_policy) ||
      (request->profile == TC_PIV_CARD
           ? request->chuid_profile != TC_CHUID_PROFILE_PIV &&
                 request->chuid_profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP
           : request->chuid_profile != TC_CHUID_PROFILE_TWIC_SIGNED))
    return TC_CREDENTIAL_ERROR;

  const TC_CMS_credential_workspace *workspace = context->workspace;

  int piv;
  TC_PIV_oid_profile oids;
  if (!credential_profile(request->profile, context->options, &piv, &oids))
    return TC_CREDENTIAL_ERROR;
  const int strict_piv = piv && !request->twic_reader_policy;
  if (request->twic_reader_policy)
    oids = TC_PIV_OIDS_TWIC_COMPATIBLE;

  TC_bytes writes[TC_VALIDATION_WRITES];
  const TC_bytes inputs[] = {
      request->encoded,
      {(const uint8_t *)request, sizeof *request},
      {(const uint8_t *)request->card, sizeof *request->card},
      {(const uint8_t *)request->card_expiration,
       sizeof *request->card_expiration},
      request->card->fascn,
      request->card->uuid_urn,
      request->card->fascn_oid};
  TC_TLV_result checked =
      tc_validation_storage(context, inputs, sizeof inputs / sizeof *inputs,
                            work, out, sizeof *out, writes);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  tc_pki_source_guard guard = {context->trust.certificates, writes,
                               TC_VALIDATION_WRITES};
  const TC_X509_store_source source = {
      &guard, guard.source->candidate_count, guard.source->anchor_count,
      tc_pki_source_guard_candidate, tc_pki_source_guard_anchor};

  const TC_TLV_limits *limits = &policy.path.parsing;
  const TC_X509_path_workspace *storage = &workspace->path->validation;
  if (request->encoded.length > limits->max_input ||
      request->encoded.length > *work)
    return TC_CREDENTIAL_LIMIT;
  *work -= request->encoded.length;

  TC_PIV_CHUID chuid;
  TC_TLV_result parsed = TC_PIV_CHUID_read_profile(
      request->encoded.data, request->encoded.length, request->encoding,
      request->chuid_profile, &chuid);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);

  int current, matched;
  parsed = chuid_expiration_check(chuid.expiration, &policy.path.at, &current);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!current)
    return TC_CREDENTIAL_INVALID;
  parsed =
      strict_piv
          ? TC_PIV_card_identifiers_match(request->card, chuid.fascn,
                                          chuid.card_uuid, work, &matched)
          : TC_TWIC_card_identifiers_match(request->card, chuid.fascn,
                                           chuid.card_uuid, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;

  TC_PIV_CMS_object object;
  parsed = TC_PIV_CMS_read(chuid.signature, TC_PIV_CMS_CHUID, oids,
                           policy.attributes, limits, storage->frames,
                           storage->frame_capacity, work, &object);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = TC_PIV_CMS_identifiers_match(
      &object, TC_PIV_CMS_CHUID, chuid.fascn, chuid.card_uuid, limits,
      storage->frames, storage->frame_capacity, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;
  parsed = content_signer_policy(object.certificate, strict_piv,
                                 request->card_expiration, &policy.path,
                                 storage, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);

  const TC_CMS_validation_request cms = {
      chuid.signature,      0, object.envelope.content_type,
      chuid.signed_content, 2, object.certificate};
  TC_credential_status status = TC_CMS_credential_validate(
      &cms, &source, &policy, &revocation, workspace, work);
  if (status == TC_CREDENTIAL_VALID) {
    const TC_PIV_CHUID_result result = {chuid, object.certificate,
                                        context->options->at, request->profile};
    *out = result;
  }
  return status;
}

TC_credential_status
TC_PIV_biometric_validate(const TC_PIV_biometric_validation_request *request,
                          const TC_validation_context *context, size_t *work) {
  enum { FASCN_BYTES = 25, GUID_BYTES = 16 };
  TC_CMS_path_options policy;
  TC_X509_path_options crl_policy;
  TC_CMS_revocation_policy revocation;
  if (!request || !request->encoded.data || !request->encoded.length ||
      (request->signature_profile != TC_PIV_CMS_BIOMETRIC &&
       request->signature_profile != TC_PIV_CMS_BIOMETRIC_LEGACY) ||
      (request->format != TC_PIV_CBEFF_FINGERPRINT_TEMPLATE &&
       request->format != TC_PIV_CBEFF_FACE_IMAGE &&
       request->format != TC_PIV_CBEFF_IRIS_IMAGE) ||
      !request->fascn.data || request->fascn.length != FASCN_BYTES ||
      !request->guid.data || request->guid.length != GUID_BYTES ||
      !request->chuid_signer.data || !request->chuid_signer.length ||
      !request->card_expiration || !work ||
      !tc_validation_policies(context, &policy, &crl_policy, &revocation))
    return TC_CREDENTIAL_ERROR;

  int piv, matched;
  TC_PIV_oid_profile oids;
  if (!credential_profile(request->profile, context->options, &piv, &oids))
    return TC_CREDENTIAL_ERROR;
  if (request->format == TC_PIV_CBEFF_IRIS_IMAGE)
    return TC_CREDENTIAL_UNSUPPORTED;
  TC_bytes writes[TC_VALIDATION_WRITES];
  const TC_bytes inputs[] = {request->encoded,
                             request->fascn,
                             request->guid,
                             request->chuid_signer,
                             {(const uint8_t *)request, sizeof *request},
                             {(const uint8_t *)request->card_expiration,
                              sizeof *request->card_expiration}};
  TC_TLV_result checked = tc_validation_storage(
      context, inputs, sizeof inputs / sizeof *inputs, work, NULL, 0, writes);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  tc_pki_source_guard guard = {context->trust.certificates, writes,
                               TC_VALIDATION_WRITES};
  const TC_X509_store_source source = {
      &guard, guard.source->candidate_count, guard.source->anchor_count,
      tc_pki_source_guard_candidate, tc_pki_source_guard_anchor};
  const TC_TLV_limits *limits = &policy.path.parsing;
  const TC_X509_path_workspace *storage = &context->workspace->path->validation;
  if (request->encoded.length > limits->max_input ||
      request->encoded.length > *work)
    return TC_CREDENTIAL_LIMIT;
  *work -= request->encoded.length;

  TC_PIV_CBEFF cbeff;
  TC_PIV_CBEFF_metadata metadata;
  TC_PIV_CMS_object object;
  TC_TLV_result parsed = TC_PIV_CBEFF_read(request->encoded, &cbeff);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = TC_PIV_CBEFF_metadata_read(request->encoded, &metadata);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_PIV_CBEFF_format format = TC_PIV_CBEFF_format_identify(&metadata);
  if (format != request->format)
    return TC_CREDENTIAL_INVALID;
  if (request->require_current) {
    int order;
    if (TC_X509_time_compare(&context->options->at, &metadata.valid_from,
                             &order) != TC_TLV_OK ||
        order < 0 ||
        TC_X509_time_compare(&context->options->at, &metadata.valid_until,
                             &order) != TC_TLV_OK ||
        order > 0)
      return TC_CREDENTIAL_INVALID;
  }
  if (format == TC_PIV_CBEFF_FINGERPRINT_TEMPLATE) {
    TC_PIV_fingerprint_record fingerprint;
    parsed = TC_PIV_fingerprint_read(cbeff.record, &fingerprint);
    if (parsed != TC_TLV_OK)
      return tc_validation_status(parsed);
  } else if (format == TC_PIV_CBEFF_FACE_IMAGE) {
    TC_PIV_face_record face;
    const TC_PIV_face_profile face_profile = request->profile == TC_PIV_CARD
                                                 ? TC_PIV_FACE_PROFILE_PIV
                                                 : TC_PIV_FACE_PROFILE_TWIC;
    parsed = TC_PIV_face_read(cbeff.record, face_profile, &face);
    if (parsed != TC_TLV_OK)
      return tc_validation_status(parsed);
  }
  if (memcmp(cbeff.fascn.data, request->fascn.data, request->fascn.length))
    return TC_CREDENTIAL_INVALID;
  parsed = TC_PIV_CMS_read(cbeff.signature, request->signature_profile, oids,
                           policy.attributes, limits, storage->frames,
                           storage->frame_capacity, work, &object);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = TC_PIV_CMS_identifiers_match(
      &object, request->signature_profile, request->fascn, request->guid,
      limits, storage->frames, storage->frame_capacity, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;
  if (object.certificate.length) {
    parsed = biometric_distinct_signer(
        object.certificate, request->chuid_signer, limits, storage, work);
    if (parsed != TC_TLV_OK)
      return tc_validation_status(parsed);
  }
  const TC_bytes certificate =
      object.certificate.length ? object.certificate : request->chuid_signer;
  parsed = content_signer_policy(certificate, piv, request->card_expiration,
                                 &policy.path, storage, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_CMS_validation_request cms = {
      cbeff.signature,       0, object.envelope.content_type,
      &cbeff.signed_content, 1, certificate};
  return TC_CMS_credential_validate(&cms, &source, &policy, &revocation,
                                    context->workspace, work);
}

TC_credential_status
TC_PIV_security_validate(const TC_PIV_security_validation_request *request,
                         const TC_validation_context *context,
                         const TC_PIV_security_validation_workspace *workspace,
                         size_t *work, TC_PIV_security_result *out) {
  TC_CMS_path_options policy;
  TC_X509_path_options crl_policy;
  TC_CMS_revocation_policy revocation;
  if (!request || !request->encoded.data || !request->encoded.length ||
      !request->chuid_signer.data || !request->chuid_signer.length ||
      !request->card_expiration || !request->objects || !request->count ||
      request->count > TC_LDS_MAX_GROUPS ||
      !tc_validation_policies(context, &policy, &crl_policy, &revocation) ||
      !workspace || !workspace->content || !workspace->content_capacity ||
      !work || !out ||
      (request->encoding != TC_PIV_SECURITY_CONTENTS &&
       request->encoding != TC_PIV_SECURITY_CONTAINER))
    return request && request->count > TC_LDS_MAX_GROUPS ? TC_CREDENTIAL_LIMIT
                                                         : TC_CREDENTIAL_ERROR;

  int piv;
  TC_PIV_oid_profile oids;
  if (!credential_profile(request->profile, context->options, &piv, &oids))
    return TC_CREDENTIAL_ERROR;
  if (request->count < 2)
    return TC_CREDENTIAL_INVALID;
  for (size_t i = 0; i < request->count; ++i) {
    if (!request->objects[i].parts || !request->objects[i].count)
      return TC_CREDENTIAL_ERROR;
    for (size_t j = 0; j < i; ++j)
      if (request->objects[i].container == request->objects[j].container)
        return TC_CREDENTIAL_INVALID;
  }
  enum { SECURITY_WRITES = TC_VALIDATION_WRITES + 1 };
  TC_bytes writes[SECURITY_WRITES];
  const TC_bytes inputs[] = {request->encoded,
                             request->chuid_signer,
                             {(const uint8_t *)request, sizeof *request},
                             {(const uint8_t *)workspace, sizeof *workspace},
                             {(const uint8_t *)request->card_expiration,
                              sizeof *request->card_expiration},
                             {(const uint8_t *)request->objects,
                              request->count * sizeof *request->objects},
                             {workspace->content, workspace->content_capacity}};
  const size_t initial_work = *work;
  TC_TLV_result stored =
      tc_validation_storage(context, inputs, sizeof inputs / sizeof *inputs,
                            work, out, sizeof *out, writes);
  if (stored != TC_TLV_OK)
    return tc_validation_status(stored);
  size_t budget = *work;
  writes[TC_VALIDATION_WRITES] =
      (TC_bytes){workspace->content, workspace->content_capacity};
  /* The content decoder writes only after all signature and inventory inputs
   * have been checked for overlap with its buffer. */
  for (size_t i = 0; i + 1 < sizeof inputs / sizeof *inputs; ++i) {
    stored = tc_pki_storage_input(&writes[TC_VALIDATION_WRITES], 1, inputs[i],
                                  &budget);
    if (stored != TC_TLV_OK)
      break;
  }
  for (size_t i = 0; stored == TC_TLV_OK && i < request->count; ++i) {
    TC_bytes parts;
    stored = tc_pki_storage_span(request->objects[i].parts,
                                 request->objects[i].count,
                                 sizeof *request->objects[i].parts, &parts);
    if (stored == TC_TLV_OK)
      stored = tc_pki_storage_input(writes, SECURITY_WRITES, parts, &budget);
    for (size_t j = 0; stored == TC_TLV_OK && j < request->objects[i].count;
         ++j)
      stored = tc_pki_storage_input(writes, SECURITY_WRITES,
                                    request->objects[i].parts[j], &budget);
  }
  if (stored != TC_TLV_OK) {
    *work = initial_work;
    return tc_validation_status(stored);
  }
  *work = budget;
  tc_pki_source_guard guard = {context->trust.certificates, writes,
                               SECURITY_WRITES};
  const TC_X509_store_source source = {
      &guard, guard.source->candidate_count, guard.source->anchor_count,
      tc_pki_source_guard_candidate, tc_pki_source_guard_anchor};
  const TC_TLV_limits *limits = &policy.path.parsing;
  const TC_X509_path_workspace *storage = &context->workspace->path->validation;
  if (request->encoded.length > limits->max_input ||
      request->encoded.length > *work)
    return TC_CREDENTIAL_LIMIT;
  *work -= request->encoded.length;

  TC_PIV_security_object container;
  TC_PIV_CMS_object object;
  TC_LDS_security_object lds;
  TC_TLV_result parsed =
      TC_PIV_security_read(request->encoded, request->encoding, &container);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = TC_PIV_CMS_read(container.cms, TC_PIV_CMS_SECURITY, oids,
                           policy.attributes, limits, storage->frames,
                           storage->frame_capacity, work, &object);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  parsed = content_signer_policy(request->chuid_signer, piv,
                                 request->card_expiration, &policy.path,
                                 storage, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_CMS_validation_request cms = {
      container.cms, 0, object.envelope.content_type,
      NULL,          0, request->chuid_signer};
  TC_credential_status result = TC_CMS_credential_validate(
      &cms, &source, &policy, &revocation, context->workspace, work);
  if (result != TC_CREDENTIAL_VALID)
    return result;
  parsed = TC_LDS_read_content(
      object.envelope.content, limits, storage->frames, storage->frame_capacity,
      work, workspace->content, workspace->content_capacity, &lds);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (container.groups != lds.groups)
    return TC_CREDENTIAL_INVALID;

  uint16_t checked = 0;
  for (size_t i = 0; i < request->count; ++i) {
    unsigned group;
    int matched;
    parsed = TC_PIV_security_group_find(&container,
                                        request->objects[i].container, &group);
    if (parsed == TC_TLV_END)
      return TC_CREDENTIAL_INVALID;
    if (parsed != TC_TLV_OK)
      return tc_validation_status(parsed);
    const uint16_t bit = (uint16_t)(1u << (group - 1));
    if (checked & bit)
      return TC_CREDENTIAL_INVALID;
    parsed = TC_LDS_hash_check(
        &lds, group, request->objects[i].parts, request->objects[i].count,
        limits, storage->frames, storage->frame_capacity, work, &matched);
    if (parsed != TC_TLV_OK)
      return tc_validation_status(parsed);
    if (!matched)
      return TC_CREDENTIAL_INVALID;
    checked |= bit;
  }
  if (checked != lds.groups)
    return TC_CREDENTIAL_INVALID;
  const TC_PIV_security_result accepted = {
      request->objects, request->count, request->chuid_signer, request->profile,
      context->options->at};
  *out = accepted;
  return TC_CREDENTIAL_VALID;
}

static TC_TLV_result
security_object_equals(const TC_PIV_security_result *security,
                       uint16_t container, TC_bytes expected, size_t *work,
                       int *matched) {
  const TC_PIV_security_data *selected = NULL;
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
    if (part.length > *work)
      return TC_TLV_LIMIT;
    *work -= part.length;
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
unsigned_chuid_storage(const TC_TWIC_unsigned_CHUID_validation_request *request,
                       const TC_PIV_security_result *security,
                       const TC_validation_context *context, size_t *work) {
  TC_bytes counter;
  TC_TLV_result status = tc_pki_storage_span(work, 1, sizeof *work, &counter);
  if (status != TC_TLV_OK)
    return status;
  size_t budget = *work;
#define INPUT(pointer, count)                                                  \
  do {                                                                         \
    TC_bytes span;                                                             \
    status =                                                                   \
        tc_pki_storage_span((pointer), (count), sizeof *(pointer), &span);     \
    if (status != TC_TLV_OK)                                                   \
      return status;                                                           \
    status = tc_pki_storage_input(&counter, 1, span, &budget);                 \
    if (status != TC_TLV_OK)                                                   \
      return status;                                                           \
  } while (0)
  INPUT(request, 1);
  INPUT(request->encoded.data, request->encoded.length);
  INPUT(request->card, 1);
  INPUT(request->card->fascn.data, request->card->fascn.length);
  INPUT(request->card->fascn_oid.data, request->card->fascn_oid.length);
  INPUT(request->card->uuid_urn.data, request->card->uuid_urn.length);
  INPUT(security, 1);
  INPUT(security->signer.data, security->signer.length);
  INPUT(context, 1);
  INPUT(context->options, 1);
  INPUT(security->objects, security->count);
  for (size_t i = 0; i < security->count; ++i) {
    const TC_PIV_security_data *object = &security->objects[i];
    INPUT(object->parts, object->count);
    for (size_t j = 0; j < object->count; ++j)
      INPUT(object->parts[j].data, object->parts[j].length);
  }
#undef INPUT
  *work = budget;
  return TC_TLV_OK;
}

TC_credential_status TC_TWIC_unsigned_CHUID_validate(
    const TC_TWIC_unsigned_CHUID_validation_request *request,
    const TC_PIV_security_result *security,
    const TC_validation_context *context, size_t *work) {
  if (!request || !request->encoded.data || !request->encoded.length ||
      !request->card || !security || request->profile != security->profile ||
      (request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD) ||
      !context || !context->options || !work || !security->objects ||
      !security->count || security->count > TC_LDS_MAX_GROUPS)
    return TC_CREDENTIAL_ERROR;
  int order;
  if (TC_X509_time_compare(&context->options->at, &security->at, &order) !=
          TC_TLV_OK ||
      order)
    return TC_CREDENTIAL_ERROR;
  TC_TLV_result parsed =
      unsigned_chuid_storage(request, security, context, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (request->encoded.length > context->options->parsing.max_input)
    return TC_CREDENTIAL_LIMIT;
  int matched;
  parsed = security_object_equals(security, TC_TWIC_UNSIGNED_CHUID_CONTAINER,
                                  request->encoded, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!matched)
    return TC_CREDENTIAL_INVALID;
  TC_PIV_CHUID chuid;
  parsed = TC_PIV_CHUID_read_profile(request->encoded.data,
                                     request->encoded.length, request->encoding,
                                     TC_CHUID_PROFILE_TWIC_UNSIGNED, &chuid);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  int current;
  parsed =
      chuid_expiration_check(chuid.expiration, &context->options->at, &current);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  if (!current)
    return TC_CREDENTIAL_INVALID;
  parsed = TC_TWIC_card_identifiers_match(request->card, chuid.fascn,
                                          chuid.card_uuid, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  return matched ? TC_CREDENTIAL_VALID : TC_CREDENTIAL_INVALID;
}

#if TC_ENABLE_PIV_CVC
TC_credential_status
TC_PIV_CVC_validate(const TC_PIV_CVC_validation_request *request,
                    const TC_validation_context *context,
                    TC_EC_workspace *point, size_t *work, TC_PIV_CVC *out) {
  TC_CMS_path_options policy;
  TC_X509_path_options crl;
  TC_CMS_revocation_policy revocation;
  if (!request || !request->card.data || !request->card.length ||
      !request->signer_certificate.data ||
      !request->signer_certificate.length || !point || !work || !out ||
      !tc_validation_policies(context, &policy, &crl, &revocation))
    return TC_CREDENTIAL_ERROR;
  int piv;
  TC_PIV_oid_profile oids;
  if (!credential_profile(request->profile, context->options, &piv, &oids))
    return TC_CREDENTIAL_ERROR;
  TC_bytes writes[TC_VALIDATION_WRITES];
  const TC_bytes inputs[] = {request->card,
                             request->intermediate,
                             request->expected_uuid,
                             request->signer_certificate,
                             {(const uint8_t *)request, sizeof *request}};
  TC_TLV_result checked =
      tc_validation_storage(context, inputs, sizeof inputs / sizeof *inputs,
                            work, out, sizeof *out, writes);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  checked = content_signer_policy(request->signer_certificate, piv, NULL,
                                  &policy.path,
                                  &context->workspace->path->validation, work);
  if (checked != TC_TLV_OK)
    return tc_validation_status(checked);
  TC_validation_options options = *context->options;
  options.certificate = (TC_validation_certificate_policy){
      policy.path.initial_policies, policy.path.initial_policy_count,
      policy.path.anchor_names,     policy.path.purpose,
      policy.path.key_usage,        policy.path.flags};
  tc_pki_source_guard guard = {context->trust.certificates, writes,
                               TC_VALIDATION_WRITES};
  const TC_X509_store_source source = {
      &guard, guard.source->candidate_count, guard.source->anchor_count,
      tc_pki_source_guard_candidate, tc_pki_source_guard_anchor};
  TC_validation_context signer_context = *context;
  signer_context.options = &options;
  signer_context.trust.certificates = &source;
  TC_X509_validation_result signer;
  TC_credential_status status = TC_X509_validate(
      request->signer_certificate, &signer_context, work, &signer);
  if (status != TC_CREDENTIAL_VALID)
    return status;
  const TC_PIV_CVC_chain_request chain = {request->card, request->intermediate,
                                          request->expected_uuid,
                                          request->curve, &signer.certificate};
  switch (TC_PIV_CVC_chain_verify(&chain, &options.parsing, &options.signatures,
                                  point, work, out)) {
  case TC_X509_SIGNATURE_VALID:
    return TC_CREDENTIAL_VALID;
  case TC_X509_SIGNATURE_INVALID:
    return TC_CREDENTIAL_INVALID;
  case TC_X509_SIGNATURE_LIMIT:
    return TC_CREDENTIAL_LIMIT;
  case TC_X509_SIGNATURE_UNSUPPORTED:
    return TC_CREDENTIAL_UNSUPPORTED;
  default:
    return TC_CREDENTIAL_ERROR;
  }
}
#endif
#endif
