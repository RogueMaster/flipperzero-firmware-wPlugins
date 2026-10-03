/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_validate.h"
#include <string.h>

ExampleTWICResult example_twic_cancellation_check(const TC_TWIC_CCL_snapshot* ccl,
                                                  const TC_TWIC_CCL_freshness_policy* freshness,
                                                  size_t max_reads, TC_bytes fascn)
{
  if (!ccl)
    return EXAMPLE_TWIC_UNAVAILABLE;
  if (!freshness || !fascn.data || fascn.length != TC_TWIC_CCL_FASCN_BYTES)
    return EXAMPLE_TWIC_ERROR;
  TC_TWIC_CCL_result status = TC_TWIC_CCL_check_freshness(&ccl->metadata, freshness);
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

TC_TLV_result example_read_card_identity(TC_bytes encoded, TC_PIV_card_profile profile,
                                         const TC_TLV_limits* limits, ExampleX509Workspace* storage,
                                         size_t* work, ExampleCardIdentity* out)
{
  if (!limits || !storage || !work || !out || !encoded.data || !encoded.length)
    return TC_TLV_ARGUMENT;
  /* Reserve the certificate and extension scans before using unmetered readers.
   */
  if (encoded.length > *work / 2)
    return TC_TLV_LIMIT;
  *work -= encoded.length * 2;
  const TC_X509_workspace parser = {
      {storage->frames, sizeof storage->frames / sizeof *storage->frames},
      storage->oids,
      sizeof storage->oids / sizeof *storage->oids};
  TC_X509_certificate certificate;
  TC_TLV_result status = TC_X509_read(encoded, limits, &parser, &certificate);
  if (status != TC_TLV_OK)
    return status;
  ExampleCardIdentity identity;
  memset(&identity, 0, sizeof identity);
  identity.certificate = certificate;
  identity.expiration = certificate.not_after;
  TC_TLV_reader extensions;
  status = TC_X509_extensions_init(&extensions, certificate.extensions, limits);
  if (status != TC_TLV_OK)
    return status;
  static const uint8_t san_oid[] = {0x55, 0x1d, 17};
  TC_X509_extension extension;
  int found = 0;
  while ((status = TC_X509_extension_next(&extensions, &extension)) == TC_TLV_OK) {
    if (extension.oid.length != sizeof san_oid ||
        memcmp(extension.oid.data, san_oid, sizeof san_oid))
      continue;
    if (found)
      return TC_TLV_INVALID;
    status = profile == TC_PIV_CARD
                 ? TC_PIV_card_identifiers_read(extension.value, profile, limits, parser.frames,
                                                work, &identity.identifiers)
                 : TC_TWIC_card_identifiers_read(extension.value, profile, limits, parser.frames,
                                                 work, &identity.identifiers);
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

/* The key proof results as workflow results. A card status is a failed
 * proof. */
static ExampleTWICResult proof_result(TC_PIV_result result)
{
  switch (result) {
  case TC_PIV_OK:
    return EXAMPLE_TWIC_AUTHENTICATED;
  case TC_PIV_INVALID:
  case TC_PIV_CARD_STATUS:
    return EXAMPLE_TWIC_INVALID;
  case TC_PIV_UNSUPPORTED:
    return EXAMPLE_TWIC_UNSUPPORTED;
  case TC_PIV_LIMIT:
    return EXAMPLE_TWIC_LIMIT;
  default:
    return EXAMPLE_TWIC_ERROR;
  }
}

ExampleTWICResult example_twic_authenticate(TC_PIV_link* link, const ExampleTWICRequest* request,
                                            TC_random_source random,
                                            ExampleTWICWorkspace* workspace, size_t* work)
{
  if (!link || !request || !request->certificate.data || !request->certificate.length ||
      !request->trust || !request->path || !random.fill || !workspace || !work)
    return EXAMPLE_TWIC_ERROR;
  if (request->profile != TC_TWIC_LEGACY_CARD && request->profile != TC_TWIC_NEXGEN_CARD)
    return EXAMPLE_TWIC_UNSUPPORTED;
  if (TC_PIV_oid_identify(request->path->purpose, TC_PIV_OIDS_TWIC_COMPATIBLE) !=
      TC_PIV_OID_CARD_AUTHENTICATION)
    return EXAMPLE_TWIC_ERROR;

  ExampleTWICResult result = EXAMPLE_TWIC_ERROR;
  TC_X509_path_options options = *request->path;
  int64_t seconds;
  if (TC_X509_time_to_unix(&options.at, &seconds) != TC_TLV_OK || seconds < 0)
    goto cleanup;
  const TC_TWIC_CCL_freshness_policy freshness = {(uint64_t)seconds, request->ccl_max_age,
                                                  request->ccl_minimum_publication};
  if (options.max_work > *work)
    options.max_work = *work;
  options.key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  options.flags |= TC_X509_PATH_REQUIRE_KEY_USAGE | TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                   TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  const TC_X509_path_workspace validation =
      example_x509_workspace(&workspace->certificate.validation);
  const TC_X509_search_workspace search = example_x509_search_workspace(&workspace->certificate);
  TC_X509_search_report path;
  const TC_X509_path_status checked = TC_X509_path_build(request->certificate, request->trust,
                                                         &options, &validation, &search, &path);
  if (checked != TC_X509_PATH_VALID) {
    /* Failed path validation reports no work count, so charge the full budget. */
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
  const TC_TLV_result parsed =
      example_read_card_identity(request->certificate, request->profile, &options.parsing,
                                 &workspace->certificate.validation, work, &identity);
  if (parsed != TC_TLV_OK) {
    result = parsed == TC_TLV_LIMIT      ? EXAMPLE_TWIC_LIMIT
             : parsed == TC_TLV_ARGUMENT ? EXAMPLE_TWIC_ERROR
                                         : EXAMPLE_TWIC_INVALID;
    goto cleanup;
  }
  result = example_twic_cancellation_check(request->ccl, &freshness, request->ccl_reads,
                                           identity.identifiers.fascn);
  if (result != EXAMPLE_TWIC_AUTHENTICATED)
    goto cleanup;
  /* TC_PIV_key_prove applies the SP 800-78-5 and TWIC key policy to the
   * validated certificate. */
  const TC_PIV_key_proof_request proof = {
      &identity.certificate,
      {request->profile, options.at, request->rsa_padding, request->allow_rsa1024},
      TC_PIV_KEY_CARD_AUTHENTICATION};
  TC_work_budget budget = {*work > UINT32_MAX ? UINT32_MAX : (uint32_t)*work};
  const uint32_t before = budget.remaining;
  result = proof_result(
      TC_PIV_key_prove(link, &proof, random, &options.signatures, &workspace->proof, &budget));
  *work -= before - budget.remaining;
  /* A publication during card I/O supersedes the held cancellation list. */
  if (result == EXAMPLE_TWIC_AUTHENTICATED)
    result = example_twic_cancellation_check(request->ccl, &freshness, request->ccl_reads,
                                             identity.identifiers.fascn);
cleanup:
  TC_secure_zero(workspace, sizeof *workspace);
  return result;
}
