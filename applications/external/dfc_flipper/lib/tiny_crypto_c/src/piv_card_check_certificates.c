/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Card certificates: the path purpose of each key, path and revocation
 * evidence, and the subjectAltName identifiers, for the card check and for
 * retained certificates. */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "internal.h"
#include "piv_card_check_internal.h"
#include "validation_internal.h"
#include <string.h>

enum { GUID_BYTES = 16 };

/* Key references of SP 800-73-5 Part 1 Table 5. */
enum {
  KEY_PIV_AUTHENTICATION = 0x9a,
  KEY_DIGITAL_SIGNATURE = 0x9c,
  KEY_MANAGEMENT = 0x9d,
  KEY_CARD_AUTHENTICATION = 0x9e
};

static const uint8_t san_oid[] = {0x55, 0x1d, 17};
static const uint8_t eku_oid[] = {0x55, 0x1d, 37};

static int oid_equal(TC_bytes oid, const uint8_t* expected, size_t length)
{
  return oid.length == length && !memcmp(oid.data, expected, length);
}

static TC_PIV_oid_profile oid_profile(TC_PIV_card_profile profile)
{
  return profile == TC_PIV_CARD ? TC_PIV_OIDS_ONLY : TC_PIV_OIDS_TWIC_COMPATIBLE;
}

/* A context purpose is empty or names card authentication for profile. */
static int context_purpose_valid(const TC_validation_options* options, TC_PIV_card_profile profile)
{
  return !options->certificate.purpose.length ||
         TC_PIV_oid_identify(options->certificate.purpose, oid_profile(profile)) ==
             TC_PIV_OID_CARD_AUTHENTICATION;
}

/* Select the Card Authentication purpose: the context purpose on a PIV
 * card, otherwise the one PIV or TWIC card authentication EKU of the
 * certificate (SP 800-73-5 Part 1 section 3.1.4, TWIC Part 2 v5 section 6).
 * Charges twice the certificate length for the scan. */
static TC_TLV_result card_authentication_purpose(TC_bytes encoded, TC_PIV_card_profile profile,
                                                 const TC_validation_context* context, size_t* work,
                                                 TC_validation_options* options)
{
  const TC_PIV_oid_profile oids = oid_profile(profile);
  if (profile != TC_PIV_CARD)
    options->certificate.purpose = (TC_bytes){NULL, 0};
  if (!options->certificate.purpose.length) {
    const TC_X509_path_workspace* storage = &context->workspace->path->validation;
    if (encoded.length > *work / 2)
      return TC_TLV_LIMIT;
    *work -= encoded.length * 2;
    TC_X509_workspace parser = {storage->frames, storage->oids, storage->oid_capacity};
    TC_X509_certificate certificate;
    TC_TLV_result status = TC_X509_read(encoded, &options->parsing, &parser, &certificate);
    TC_TLV_reader extensions = {0};
    if (status == TC_TLV_OK)
      status = TC_X509_extensions_init(&extensions, certificate.extensions, &options->parsing);
    TC_X509_extension extension;
    while (status == TC_TLV_OK &&
           (status = TC_X509_extension_next(&extensions, &extension)) == TC_TLV_OK) {
      if (!oid_equal(extension.oid, eku_oid, sizeof eku_oid))
        continue;
      size_t count = 0;
      status = TC_X509_extended_key_usage_read(extension.value, &options->parsing, storage->oids,
                                               storage->oid_capacity, &count);
      for (size_t i = 0; status == TC_TLV_OK && i < count; ++i) {
        if (TC_PIV_oid_identify(storage->oids[i], oids) != TC_PIV_OID_CARD_AUTHENTICATION)
          continue;
        if (options->certificate.purpose.length)
          status = TC_TLV_INVALID;
        else
          options->certificate.purpose = storage->oids[i];
      }
    }
    if (status != TC_TLV_END)
      return status;
    if (!options->certificate.purpose.length)
      return TC_TLV_INVALID;
  }
  options->certificate.key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
  options->certificate.flags |= TC_X509_PATH_REQUIRE_KEY_USAGE |
                                TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
                                TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  return TC_TLV_OK;
}

/* The path policy of key under the card context. 9E follows
 * card_authentication_purpose. 9A and 9C need digitalSignature and no EKU
 * purpose. 9D keeps the context policy without a purpose. */
static TC_TLV_result key_policy(uint8_t key, TC_bytes encoded, TC_PIV_card_profile profile,
                                const TC_validation_context* context, size_t* work,
                                TC_validation_options* options)
{
  *options = *context->options;
  if (key == KEY_CARD_AUTHENTICATION)
    return card_authentication_purpose(encoded, profile, context, work, options);
  options->certificate.purpose = (TC_bytes){NULL, 0};
  options->certificate.flags &= ~(unsigned)TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE;
  if (key == KEY_PIV_AUTHENTICATION || key == KEY_DIGITAL_SIGNATURE) {
    options->certificate.key_usage |= TC_KEY_USAGE_DIGITAL_SIGNATURE;
    options->certificate.flags |= TC_X509_PATH_REQUIRE_KEY_USAGE;
  }
  return TC_TLV_OK;
}

/* Read the identifiers of the certificate's one subjectAltName. 9E uses the
 * card identifier rules of profile, 9A the PIV Authentication rules against
 * card_guid (SP 800-73-5 Part 1 sections 3.1.3, 3.1.4 and 3.4). Charges the
 * extension bytes, then the reader. */
static TC_TLV_result san_identifiers(const TC_X509_certificate* certificate,
                                     const TC_PIV_card_certificate_request* request,
                                     const TC_validation_context* context, size_t* work,
                                     TC_PIV_card_identifiers* out)
{
  const TC_TLV_limits* limits = &context->options->parsing;
  const TC_TLV_frames frames = context->workspace->path->validation.frames;
  if (certificate->extensions.length > *work)
    return TC_TLV_LIMIT;
  *work -= certificate->extensions.length;
  TC_TLV_reader extensions;
  TC_TLV_result status = TC_X509_extensions_init(&extensions, certificate->extensions, limits);
  TC_X509_extension extension;
  int found = 0;
  while (status == TC_TLV_OK &&
         (status = TC_X509_extension_next(&extensions, &extension)) == TC_TLV_OK) {
    if (!oid_equal(extension.oid, san_oid, sizeof san_oid))
      continue;
    if (found)
      return TC_TLV_INVALID;
    if (request->key_reference == KEY_PIV_AUTHENTICATION)
      status = request->twic_reader_policy
                   ? TC_TWIC_authentication_identifiers_read(extension.value, request->card_guid,
                                                             limits, frames, work, out)
                   : TC_PIV_authentication_identifiers_read(extension.value, request->card_guid,
                                                            limits, frames, work, out);
    else
      status = request->profile == TC_PIV_CARD
                   ? TC_PIV_card_identifiers_read(extension.value, request->profile, limits, frames,
                                                  work, out)
                   : TC_TWIC_card_identifiers_read(extension.value, request->profile, limits,
                                                   frames, work, out);
    found = 1;
  }
  if (status == TC_TLV_END)
    return found ? TC_TLV_OK : TC_TLV_INVALID;
  return status;
}

static int request_valid(const TC_PIV_card_certificate_request* request)
{
  const uint8_t key = request->key_reference;
  if (!request->encoded.data || !request->encoded.length ||
      (request->profile != TC_PIV_CARD && request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD) ||
      request->twic_reader_policy > 1)
    return 0;
  if (key == KEY_CARD_AUTHENTICATION)
    return !request->twic_reader_policy;
  return key == KEY_PIV_AUTHENTICATION && request->profile == TC_PIV_CARD &&
         request->card_guid.data && request->card_guid.length == GUID_BYTES;
}

TC_credential_status
TC_PIV_card_certificate_validate(const TC_PIV_card_certificate_request* request,
                                 const TC_validation_context* context, size_t* work,
                                 TC_PIV_card_certificate_report* out)
{
  if (!request || !work || !out || !context || !context->options || !context->workspace ||
      !context->workspace->path || !request_valid(request) ||
      !context_purpose_valid(context->options, request->profile) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, request, sizeof *request) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, request->encoded.data,
                                   request->encoded.length) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, work, sizeof *work) ||
      !tc_internal_ranges_disjoint(work, sizeof *work, request, sizeof *request))
    return TC_CREDENTIAL_ERROR;
  TC_validation_options options;
  TC_TLV_result status = key_policy(request->key_reference, request->encoded, request->profile,
                                    context, work, &options);
  if (status != TC_TLV_OK)
    return status == TC_TLV_LIMIT         ? TC_CREDENTIAL_LIMIT
           : status == TC_TLV_UNSUPPORTED ? TC_CREDENTIAL_UNSUPPORTED
                                          : TC_CREDENTIAL_INVALID;
  TC_validation_context constrained = *context;
  constrained.options = &options;
  TC_PIV_card_certificate_report result;
  const TC_credential_status validated =
      TC_X509_validate(request->encoded, &constrained, work, &result.certificate);
  if (validated != TC_CREDENTIAL_VALID)
    return validated;
  status =
      san_identifiers(&result.certificate.certificate, request, context, work, &result.identifiers);
  if (status != TC_TLV_OK)
    return status == TC_TLV_LIMIT         ? TC_CREDENTIAL_LIMIT
           : status == TC_TLV_UNSUPPORTED ? TC_CREDENTIAL_UNSUPPORTED
           : status == TC_TLV_ARGUMENT    ? TC_CREDENTIAL_ERROR
                                          : TC_CREDENTIAL_INVALID;
  *out = result;
  return TC_CREDENTIAL_VALID;
}

/* ---- Card check phases ---- */

static const struct {
  uint8_t key;
  uint16_t container;
} slots[TC_PIV_CARD_CERTIFICATES] = {
    {KEY_PIV_AUTHENTICATION, 0x0101},
    {KEY_DIGITAL_SIGNATURE, 0x0100},
    {KEY_MANAGEMENT, 0x0102},
    {KEY_CARD_AUTHENTICATION, 0x0500},
};

TC_TLV_result tc_piv_check_certificate_decode(tc_piv_check_run* run, const TC_PIV_object* object,
                                              TC_PIV_certificate_profile profile,
                                              const TC_validation_context* context,
                                              TC_PIV_certificate* container,
                                              TC_X509_certificate* out)
{
  TC_buffer* storage = &run->workspace->certificates;
  const TC_buffer der = {storage->data ? storage->data + run->certificate_used : NULL,
                         storage->capacity - run->certificate_used};
  TC_TLV_result status =
      TC_PIV_certificate_decode(object->encoded, profile, context->options->parsing.max_input,
                                &run->workspace->gzip, run->work, der, container);
  if (status != TC_TLV_OK)
    return status;
  if (container->certificate.data == der.data)
    run->certificate_used += container->certificate.length;
  const TC_X509_path_workspace* scratch = &context->workspace->path->validation;
  TC_X509_workspace parser = {scratch->frames, scratch->oids, scratch->oid_capacity};
  return TC_X509_read(container->certificate, &context->options->parsing, &parser, out);
}

/* Entries of one certificate slot, appended in this order. */
typedef struct {
  TC_PIV_check path, revocation, identifiers;
  int with_identifiers;
} slot_checks;

static void slot_add(tc_piv_check_run* run, const slot_checks* checks)
{
  tc_piv_check_add(run, &checks->path);
  tc_piv_check_add(run, &checks->revocation);
  if (checks->with_identifiers)
    tc_piv_check_add(run, &checks->identifiers);
}

static void slot_dependency(slot_checks* checks)
{
  tc_piv_check_not_checkable(&checks->revocation, TC_PIV_REASON_DEPENDENCY);
  tc_piv_check_not_checkable(&checks->identifiers, TC_PIV_REASON_DEPENDENCY);
}

/* Map the TLV result of an identifier read. */
static TC_credential_status tlv_credential(TC_TLV_result status)
{
  switch (status) {
  case TC_TLV_OK:
    return TC_CREDENTIAL_VALID;
  case TC_TLV_LIMIT:
    return TC_CREDENTIAL_LIMIT;
  case TC_TLV_UNSUPPORTED:
    return TC_CREDENTIAL_UNSUPPORTED;
  case TC_TLV_ARGUMENT:
    return TC_CREDENTIAL_ERROR;
  default:
    return TC_CREDENTIAL_INVALID;
  }
}

/* CERTIFICATE_IDENTIFIERS of a usable 9E or 9A certificate. 9E records the
 * card identifiers. 9A reads against the CHUID GUID and matches its FASC-N
 * too. */
static void slot_identifiers(tc_piv_check_run* run, size_t slot,
                             const TC_X509_validation_report* validated, TC_PIV_check* check)
{
  TC_PIV_card_report* report = run->report;
  const TC_validation_context* context = run->request->card;
  TC_PIV_card_certificate_request request = {
      validated->certificate.encoded, report->profile, slots[slot].key, 0, {NULL, 0}};
  TC_PIV_card_identifiers identifiers;
  if (slots[slot].key == KEY_PIV_AUTHENTICATION) {
    if (!report->has_chuid) {
      tc_piv_check_not_checkable(check, TC_PIV_REASON_DEPENDENCY);
      return;
    }
    request.card_guid = report->chuid.object.card_uuid;
    request.twic_reader_policy = run->twic_piv;
  }
  TC_TLV_result status =
      san_identifiers(&validated->certificate, &request, context, run->work, &identifiers);
  int matched = 1;
  if (status == TC_TLV_OK && slots[slot].key == KEY_PIV_AUTHENTICATION)
    status = (run->twic_piv ? TC_TWIC_card_identifiers_match : TC_PIV_card_identifiers_match)(
        &identifiers, report->chuid.object.fascn, report->chuid.object.card_uuid, run->work,
        &matched);
  if (!tc_piv_check_status(run, check,
                           status == TC_TLV_OK && !matched ? TC_CREDENTIAL_INVALID
                                                           : tlv_credential(status)))
    return;
  if (check->outcome == TC_PIV_CHECK_PASSED && slots[slot].key == KEY_CARD_AUTHENTICATION) {
    report->card = identifiers;
    report->card_expiration = validated->certificate.not_after;
    report->has_card = 1;
  }
}

/* CERTIFICATE_PATH, REVOCATION and, for 9E and 9A, CERTIFICATE_IDENTIFIERS
 * of one slot. The path check accepts missing evidence, and REVOCATION
 * reports it. */
static void slot_check(tc_piv_check_run* run, size_t slot)
{
  if (run->result != TC_PIV_OK)
    return;
  const TC_PIV_card_check_request* request = run->request;
  const uint8_t key = slots[slot].key;
  const TC_PIV_object* object =
      tc_piv_check_object(request->inventory, TC_PIV_KIND_CERTIFICATE, key);
  if (!object)
    return;
  const uint16_t container = object->info->container;
  slot_checks checks = {tc_piv_check_make(TC_PIV_CHECK_CERTIFICATE_PATH, container, key),
                        tc_piv_check_make(TC_PIV_CHECK_REVOCATION, container, key),
                        tc_piv_check_make(TC_PIV_CHECK_CERTIFICATE_IDENTIFIERS, container, key),
                        key == KEY_CARD_AUTHENTICATION || key == KEY_PIV_AUTHENTICATION};
  if (object->state != TC_PIV_OBJECT_PRESENT) {
    tc_piv_check_unread(run, &checks.path, object, 0);
    slot_dependency(&checks);
    slot_add(run, &checks);
    return;
  }
  const TC_validation_context* context = request->card;
  const TC_PIV_certificate_profile profile =
      request->inventory->link.application == TC_PIV_APPLICATION_TWIC ? TC_PIV_CERTIFICATE_TWIC
                                                                      : TC_PIV_CERTIFICATE_SLOT;
  TC_PIV_certificate container_fields;
  TC_X509_certificate parsed;
  TC_validation_options options;
  TC_TLV_result decoded =
      tc_piv_check_certificate_decode(run, object, profile, context, &container_fields, &parsed);
  if (decoded == TC_TLV_OK)
    decoded = key_policy(key, container_fields.certificate, run->report->profile, context,
                         run->work, &options);
  if (decoded != TC_TLV_OK) {
    if (tc_piv_check_tlv(run, &checks.path, decoded)) {
      slot_dependency(&checks);
      slot_add(run, &checks);
    }
    return;
  }
  TC_validation_context constrained = *context;
  constrained.options = &options;
  tc_cms_revocation_evidence evidence;
  memset(&evidence, 0, sizeof evidence);
  evidence.evidence_optional = 1;
  if (request->ocsp) {
    evidence.ocsp = request->ocsp->responses[slot];
    evidence.ocsp_max_responses = request->ocsp->max_responses;
    evidence.ocsp_max_certificates = request->ocsp->max_certificates;
  }
  TC_X509_validation_report validated;
  int path_valid = 0;
  const TC_credential_status status = tc_x509_validate_evidence(
      container_fields.certificate, &constrained, &evidence, run->work, &validated, &path_valid);
  if (!path_valid) {
    if (tc_piv_check_status(run, &checks.path, status)) {
      slot_dependency(&checks);
      slot_add(run, &checks);
    }
    return;
  }
  const int covered = status == TC_CREDENTIAL_VALID && validated.revocation_checked;
  if (!tc_piv_check_status(run, &checks.revocation,
                           status == TC_CREDENTIAL_VALID && !covered ? TC_CREDENTIAL_UNAVAILABLE
                                                                     : status))
    return;
  /* The card context's evidence policy decides whether an unchecked
   * certificate is usable for identifiers and key proofs. */
  const int usable =
      covered || (status == TC_CREDENTIAL_VALID &&
                  context->options->revocation == TC_VALIDATION_REVOCATION_WHEN_AVAILABLE);
  if (usable) {
    run->report->certificates[slot] = validated.certificate;
    run->report->certificate_valid[slot] = 1;
    if (checks.with_identifiers)
      slot_identifiers(run, slot, &validated, &checks.identifiers);
  } else
    tc_piv_check_not_checkable(&checks.identifiers, TC_PIV_REASON_DEPENDENCY);
  slot_add(run, &checks);
}

void tc_piv_check_card_certificate(tc_piv_check_run* run)
{
  slot_check(run, TC_PIV_CARD_SLOT_CARD_AUTHENTICATION);
}

void tc_piv_check_key_certificates(tc_piv_check_run* run)
{
  slot_check(run, TC_PIV_CARD_SLOT_PIV_AUTHENTICATION);
  slot_check(run, TC_PIV_CARD_SLOT_DIGITAL_SIGNATURE);
  slot_check(run, TC_PIV_CARD_SLOT_KEY_MANAGEMENT);
}

int tc_piv_check_card_purpose_valid(const TC_validation_options* options,
                                    TC_PIV_card_profile profile)
{
  return context_purpose_valid(options, profile);
}
#endif
