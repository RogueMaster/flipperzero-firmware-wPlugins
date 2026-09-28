/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_validate.h"
#include <string.h>

ExampleTWICResult
example_twic_cancellation_check(const TC_TWIC_CCL_snapshot *ccl,
                                const TC_TWIC_CCL_freshness_policy *freshness,
                                size_t max_reads, TC_bytes fascn) {
  if (!ccl)
    return EXAMPLE_TWIC_UNAVAILABLE;
  if (!freshness || !fascn.data || fascn.length != TC_TWIC_CCL_FASCN_BYTES)
    return EXAMPLE_TWIC_ERROR;
  TC_TWIC_CCL_result status =
      TC_TWIC_CCL_check_freshness(&ccl->metadata, freshness);
  int listed = 0;
  if (status == TC_TWIC_CCL_OK)
    status = TC_TWIC_CCL_snapshot_contains(ccl, fascn, max_reads, &listed);
  switch (status) {
  case TC_TWIC_CCL_OK:
    return listed ? EXAMPLE_TWIC_CANCELLED : EXAMPLE_TWIC_AUTHENTICATED;
  case TC_TWIC_CCL_STALE:
    return EXAMPLE_TWIC_STALE;
  case TC_TWIC_CCL_UNAVAILABLE:
    return EXAMPLE_TWIC_UNAVAILABLE;
  case TC_TWIC_CCL_LIMIT:
    return EXAMPLE_TWIC_LIMIT;
  default:
    return EXAMPLE_TWIC_ERROR;
  }
}

TC_TLV_result example_read_card_identity(
    TC_bytes encoded, TC_PIV_card_profile profile, const TC_TLV_limits *limits,
    ExampleX509Workspace *storage, size_t *work, ExampleCardIdentity *out) {
  if (!limits || !storage || !work || !out || !encoded.data || !encoded.length)
    return TC_TLV_ARGUMENT;
  /* Reserve the certificate and extension scans before using unmetered readers.
   */
  if (encoded.length > *work / 2)
    return TC_TLV_LIMIT;
  *work -= encoded.length * 2;
  TC_X509_workspace parser = {
      storage->frames, sizeof storage->frames / sizeof *storage->frames,
      storage->oids, sizeof storage->oids / sizeof *storage->oids};
  TC_X509_certificate certificate;
  TC_TLV_result status =
      TC_X509_read(encoded.data, encoded.length, limits, &parser, &certificate);
  if (status != TC_TLV_OK)
    return status;
  ExampleCardIdentity identity = {0};
  identity.expiration = certificate.not_after;
  TC_TLV_reader extensions;
  status = TC_X509_extensions_init(&extensions, certificate.extensions.data,
                                   certificate.extensions.length, limits);
  if (status != TC_TLV_OK)
    return status;
  static const uint8_t san_oid[] = {0x55, 0x1d, 17};
  TC_X509_extension extension;
  int found = 0;
  while ((status = TC_X509_extension_next(&extensions, &extension)) ==
         TC_TLV_OK) {
    if (extension.oid.length != sizeof san_oid ||
        memcmp(extension.oid.data, san_oid, sizeof san_oid))
      continue;
    if (found)
      return TC_TLV_INVALID;
    status = profile == TC_PIV_CARD
                 ? TC_PIV_card_identifiers_read(
                       extension.value, profile, limits, storage->frames,
                       parser.frame_capacity, work, &identity.identifiers)
                 : TC_TWIC_card_identifiers_read(
                       extension.value, profile, limits, storage->frames,
                       parser.frame_capacity, work, &identity.identifiers);
    if (status != TC_TLV_OK)
      return status;
    found = 1;
  }
  if (status != TC_TLV_END)
    return status;
  if (!found)
    return TC_TLV_INVALID;
  *out = identity;
  return TC_TLV_OK;
}

ExampleTWICResult
example_twic_authenticate(ExampleCardIO *io, const ExampleTWICRequest *request,
                          TC_random_fn random, void *random_context,
                          ExampleTWICWorkspace *workspace, size_t *work) {
  if (!io || !io->transmit || !request || !request->certificate.data ||
      !request->certificate.length || !request->trust || !request->path ||
      !random || !workspace || !work ||
      (request->allow_rsa1024 != 0 && request->allow_rsa1024 != 1) ||
      (request->rsa_padding != EXAMPLE_CARD_RSA_V15 &&
       request->rsa_padding != EXAMPLE_CARD_RSA_PSS))
    return EXAMPLE_TWIC_ERROR;
  if (request->profile != TC_TWIC_LEGACY_CARD &&
      request->profile != TC_TWIC_NEXGEN_CARD)
    return EXAMPLE_TWIC_UNSUPPORTED;
  if (TC_PIV_oid_identify(request->path->purpose,
                          TC_PIV_OIDS_TWIC_COMPATIBLE) !=
      TC_PIV_OID_CARD_AUTHENTICATION)
    return EXAMPLE_TWIC_ERROR;

  ExampleTWICResult result = EXAMPLE_TWIC_ERROR;
  TC_X509_path_options options = *request->path;
  int64_t seconds;
  if (TC_X509_time_to_unix(&options.at, &seconds) != TC_TLV_OK || seconds < 0)
    goto cleanup;
  const TC_TWIC_CCL_freshness_policy freshness = {
      (uint64_t)seconds, request->ccl_max_age,
      request->ccl_minimum_publication};
  if (options.max_work > *work)
    options.max_work = *work;
  options.key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  options.flags |= TC_X509_PATH_REQUIRE_KEY_USAGE |
                   TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                   TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  const TC_X509_path_workspace validation =
      example_x509_workspace(&workspace->certificate.validation);
  const TC_X509_search_workspace search =
      example_x509_search_workspace(&workspace->certificate);
  TC_X509_search_result path;
  const TC_X509_path_status checked =
      TC_X509_path_build(request->certificate, request->trust, &options,
                         &validation, &search, &path);
  if (checked != TC_X509_PATH_VALID) {
    /* Failed path validation does not expose its consumed-work count. */
    *work -= options.max_work;
    switch (checked) {
    case TC_X509_PATH_INVALID:
      result = EXAMPLE_TWIC_INVALID;
      break;
    case TC_X509_PATH_UNSUPPORTED:
      result = EXAMPLE_TWIC_UNSUPPORTED;
      break;
    case TC_X509_PATH_LIMIT:
      result = EXAMPLE_TWIC_LIMIT;
      break;
    default:
      break;
    }
    goto cleanup;
  }
  if (path.validation.work_used > *work)
    goto cleanup;
  *work -= path.validation.work_used;
  ExampleCardIdentity identity;
  const TC_TLV_result parsed = example_read_card_identity(
      request->certificate, request->profile, &options.parsing,
      &workspace->certificate.validation, work, &identity);
  if (parsed != TC_TLV_OK) {
    result = parsed == TC_TLV_LIMIT      ? EXAMPLE_TWIC_LIMIT
             : parsed == TC_TLV_ARGUMENT ? EXAMPLE_TWIC_ERROR
                                         : EXAMPLE_TWIC_INVALID;
    goto cleanup;
  }
  result = example_twic_cancellation_check(
      request->ccl, &freshness, request->ccl_reads, identity.identifiers.fascn);
  if (result != EXAMPLE_TWIC_AUTHENTICATED)
    goto cleanup;
  const ExampleCardKeyPolicy policy = {
      request->profile, TC_KEY_USAGE_DIGITAL_SIGNATURE, request->allow_rsa1024,
      request->rsa_padding};
  switch (example_card_check_key(io, EXAMPLE_CARD_KEY_CARD_AUTHENTICATION,
                                 &path.validation.public_key, &policy,
                                 &options.signatures, random, random_context,
                                 &workspace->challenge, work)) {
  case EXAMPLE_CARD_KEY_VERIFIED:
    /* A publication during card I/O supersedes the held cancellation list. */
    result = example_twic_cancellation_check(request->ccl, &freshness,
                                             request->ccl_reads,
                                             identity.identifiers.fascn);
    break;
  case EXAMPLE_CARD_KEY_INVALID:
    result = EXAMPLE_TWIC_INVALID;
    break;
  case EXAMPLE_CARD_KEY_UNSUPPORTED:
    result = EXAMPLE_TWIC_UNSUPPORTED;
    break;
  case EXAMPLE_CARD_KEY_LIMIT:
    result = EXAMPLE_TWIC_LIMIT;
    break;
  case EXAMPLE_CARD_KEY_TRANSPORT:
    result = EXAMPLE_TWIC_TRANSPORT;
    break;
  default:
    result = EXAMPLE_TWIC_ERROR;
    break;
  }
cleanup:
  TC_secure_zero(workspace, sizeof *workspace);
  return result;
}
