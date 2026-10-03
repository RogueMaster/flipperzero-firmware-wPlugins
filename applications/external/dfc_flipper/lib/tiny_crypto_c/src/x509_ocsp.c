/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RFC 6960 OCSP response verification and request encoding. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_OCSP
#include <tiny_crypto/x509_ocsp.h>
#include "pki_internal.h"
#include "pki_tree_internal.h"
#include "pki_source_internal.h"
#include "pki_extensions_internal.h"
#include "pki_status_internal.h"
#include "pki_crl_reason_internal.h"
#include "hash_dispatch_internal.h"
#include "x509_path_internal.h"
#include "x509_time_internal.h"
#include "tlv_internal.h"
#include "internal.h"
#include <string.h>

static const uint8_t basic_oid[] = {0x2b, 6, 1, 5, 5, 7, 0x30, 1, 1};
static const uint8_t nonce_oid[] = {0x2b, 6, 1, 5, 5, 7, 0x30, 1, 2};
static const uint8_t nocheck_oid[] = {0x2b, 6, 1, 5, 5, 7, 0x30, 1, 5};
static const uint8_t ocsp_signing_oid[] = {0x2b, 6, 1, 5, 5, 7, 3, 9};

/* CertStatus as read from a SingleResponse (RFC 6960 4.2.1). */
typedef enum { OCSP_GOOD, OCSP_REVOKED, OCSP_UNKNOWN } ocsp_cert_status;

typedef struct {
  TC_bytes tbs, signature, responder, embedded, nonce;
  TC_DER_algorithm algorithm;
  TC_X509_ocsp_report result;
  ocsp_cert_status status;
  int responder_by_key, matched, nonce_present;
} ocsp_response;

/* Every reader below applies the caller's parsing limits to its own level.
 * basic_response bounds the whole BasicOCSPResponse tree once before them. */
static TC_TLV_result sequence(TC_bytes encoded, const TC_TLV_limits* limits, TC_TLV_reader* reader)
{
  TC_bytes contents;
  if (TC_DER_sequence(encoded, &contents) != TC_TLV_OK)
    return TC_TLV_INVALID;
  return TC_TLV_reader_init(reader, contents, TC_TLV_DER, limits);
}

static TC_TLV_result contents_reader(TC_bytes contents, const TC_TLV_limits* limits,
                                     TC_TLV_reader* reader)
{
  return TC_TLV_reader_init(reader, contents, TC_TLV_DER, limits);
}

static int oid_is(TC_bytes oid, const uint8_t* expected, size_t length)
{
  return oid.length == length && !memcmp(oid.data, expected, length);
}

static TC_TLV_result time_value(const TC_TLV_element* element, TC_X509_time* out)
{
  return tc_pki_tag(element, 0x18) ? tc_x509_time_value(element, out) : TC_TLV_INVALID;
}

static TC_hash_algorithm hash_algorithm(TC_bytes oid)
{
  const TC_hash_algorithm supported[] = {TC_HASH_SHA1, TC_HASH_SHA256};
  for (size_t i = 0; i < sizeof supported / sizeof *supported; ++i) {
    tc_hash_info info;
    if (tc_hash_info_get(supported[i], &info) && tc_pki_equal(oid, info.oid))
      return supported[i];
  }
  return TC_HASH_UNKNOWN;
}

static TC_TLV_result digest(TC_hash_algorithm algorithm, TC_bytes bytes, uint8_t out[64],
                            size_t* work)
{
  tc_hash_info info;
  TC_hash_context context;
  if (!tc_hash_info_get(algorithm, &info) || !tc_hash_available(algorithm))
    return TC_TLV_UNSUPPORTED;
  if (bytes.length > *work)
    return TC_TLV_LIMIT;
  *work -= bytes.length;
  return tc_hash_digest_parts(algorithm, &bytes, 1, out, &context) == TC_OK ? TC_TLV_OK
                                                                            : TC_TLV_ARGUMENT;
}

static TC_TLV_result cert_id_matches(TC_bytes encoded, const TC_X509_certificate* certificate,
                                     const TC_X509_ocsp_verify_request* request, size_t* work,
                                     int* matched)
{
  const TC_X509_trust_anchor* issuer = request->issuer;
  TC_TLV_reader reader;
  TC_TLV_element field;
  TC_DER_algorithm algorithm;
  TC_bytes name_hash, key_hash, serial;
  TC_hash_algorithm hash;
  tc_hash_info info;
  uint8_t computed[64];
  int negative;
  TC_TLV_result status = sequence(encoded, request->parsing, &reader);
  if (status != TC_TLV_OK || tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK ||
      TC_DER_algorithm_identifier(field.encoded, &algorithm) != TC_TLV_OK)
    return TC_TLV_INVALID;
  hash = hash_algorithm(algorithm.oid);
  if (hash == TC_HASH_UNKNOWN || !tc_hash_available(hash))
    return TC_TLV_UNSUPPORTED;
  if (algorithm.parameters.length && TC_DER_null(algorithm.parameters) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_field(&reader, 4, &field) != TC_TLV_OK)
    return TC_TLV_INVALID;
  name_hash = field.value;
  if (tc_pki_field(&reader, 4, &field) != TC_TLV_OK)
    return TC_TLV_INVALID;
  key_hash = field.value;
  if (tc_pki_field(&reader, 2, &field) != TC_TLV_OK ||
      TC_DER_integer(field.encoded, &serial, &negative) != TC_TLV_OK || negative ||
      !tc_pki_end(&reader) || !tc_hash_info_get(hash, &info) ||
      name_hash.length != info.digest_length || key_hash.length != info.digest_length)
    return TC_TLV_INVALID;
  *matched = 0;
  if (!tc_pki_equal(serial, certificate->serial))
    return TC_TLV_OK;
  status = digest(hash, issuer->name, computed, work);
  if (status != TC_TLV_OK)
    return status;
  if (memcmp(name_hash.data, computed, info.digest_length))
    return TC_TLV_OK;
  status = digest(hash, issuer->public_key.key, computed, work);
  if (status == TC_TLV_OK)
    *matched = !memcmp(key_hash.data, computed, info.digest_length);
  return status;
}

/* Extension check state. Only responseExtensions may carry a nonce. */
typedef struct {
  const TC_TLV_limits* limits;
  int allow_nonce, nonce_present;
  TC_bytes nonce;
} ocsp_extensions;

/* RFC 9654 2.1: the nonce extnValue is an OCTET STRING of 1..128 octets and
 * the extension is non-critical. Unknown critical extensions are
 * unsupported (RFC 5280 4.2). tc_pki_extensions_visit rejects empty lists and
 * duplicate OIDs. */
static TC_TLV_result ocsp_extension(void* context, const TC_X509_extension* extension)
{
  ocsp_extensions* state = context;
  TC_TLV_element value;
  if (!oid_is(extension->oid, nonce_oid, sizeof nonce_oid))
    return extension->critical ? TC_TLV_UNSUPPORTED : TC_TLV_OK;
  if (!state->allow_nonce || extension->critical ||
      TC_TLV_read(extension->value, TC_TLV_DER, state->limits, &value) != TC_TLV_OK ||
      !tc_pki_tag(&value, 4) || value.encoded.length != extension->value.length ||
      !value.value.length || value.value.length > 128)
    return TC_TLV_INVALID;
  state->nonce = value.value;
  state->nonce_present = 1;
  return TC_TLV_OK;
}

/* wrapper holds the contents of an [1] EXPLICIT Extensions field. */
static TC_TLV_result extensions(TC_bytes wrapper, const TC_X509_ocsp_verify_request* request,
                                const TC_X509_path_workspace* workspace, size_t* work,
                                ocsp_extensions* state)
{
  TC_TLV_reader outer;
  TC_TLV_element list;
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, work};
  if (contents_reader(wrapper, request->parsing, &outer) != TC_TLV_OK ||
      tc_pki_field(&outer, 0x30, &list) != TC_TLV_OK || !tc_pki_end(&outer))
    return TC_TLV_INVALID;
  return tc_pki_extensions_visit(list.encoded, request->parsing, &tree, workspace->oids,
                                 workspace->oid_capacity, ocsp_extension, state);
}

/* revokedInfo: revocationTime GeneralizedTime, revocationReason [0] EXPLICIT
 * CRLReason OPTIONAL (RFC 6960 4.2.1). removeFromCRL appears only in delta
 * CRLs (RFC 5280 5.3.1), so a response that carries it is INVALID. */
static TC_TLV_result revoked_info(TC_bytes contents, const TC_TLV_limits* limits,
                                  TC_X509_ocsp_report* result)
{
  TC_TLV_reader revoked;
  TC_TLV_element field;
  if (contents_reader(contents, limits, &revoked) != TC_TLV_OK ||
      tc_pki_field(&revoked, 0x18, &field) != TC_TLV_OK ||
      time_value(&field, &result->revocation_time) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (!tc_pki_end(&revoked)) {
    if (tc_pki_field(&revoked, 0xa0, &field) != TC_TLV_OK ||
        tc_pki_crl_reason_read(field.value, &result->reason) != TC_TLV_OK ||
        result->reason == TC_PKI_CRL_REASON_REMOVE)
      return TC_TLV_INVALID;
    result->has_reason = 1;
  }
  return tc_pki_end(&revoked) ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result single_response(TC_bytes encoded, const TC_X509_certificate* certificate,
                                     const TC_X509_ocsp_verify_request* request,
                                     const TC_X509_path_workspace* workspace, size_t* work,
                                     ocsp_response* parsed)
{
  TC_TLV_reader reader;
  TC_TLV_element field;
  TC_X509_ocsp_report result = {0};
  ocsp_cert_status cert_status;
  int matched;
  TC_TLV_result status = sequence(encoded, request->parsing, &reader);
  if (status != TC_TLV_OK || tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK)
    return TC_TLV_INVALID;
  status = cert_id_matches(field.encoded, certificate, request, work, &matched);
  if (status != TC_TLV_OK)
    return status;
  if (TC_TLV_next(&reader, &field) != TC_TLV_OK || field.header.tag_length != 1)
    return TC_TLV_INVALID;
  switch (field.header.tag[0]) {
  case 0x80:
    if (field.value.length)
      return TC_TLV_INVALID;
    cert_status = OCSP_GOOD;
    break;
  case 0x82:
    if (field.value.length)
      return TC_TLV_INVALID;
    cert_status = OCSP_UNKNOWN;
    break;
  case 0xa1:
    status = revoked_info(field.value, request->parsing, &result);
    if (status != TC_TLV_OK)
      return status;
    cert_status = OCSP_REVOKED;
    break;
  default:
    return TC_TLV_UNSUPPORTED;
  }
  if (tc_pki_field(&reader, 0x18, &field) != TC_TLV_OK ||
      time_value(&field, &result.this_update) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (!tc_pki_end(&reader)) {
    TC_TLV_reader date;
    TC_TLV_reader probe = reader;
    if (TC_TLV_next(&probe, &field) != TC_TLV_OK)
      return TC_TLV_INVALID;
    if (tc_pki_tag(&field, 0xa0)) {
      reader = probe;
      if (contents_reader(field.value, request->parsing, &date) != TC_TLV_OK ||
          tc_pki_field(&date, 0x18, &field) != TC_TLV_OK || !tc_pki_end(&date) ||
          time_value(&field, &result.next_update) != TC_TLV_OK)
        return TC_TLV_INVALID;
      result.has_next_update = 1;
    }
  }
  if (!tc_pki_end(&reader)) {
    if (tc_pki_field(&reader, 0xa1, &field) != TC_TLV_OK)
      return TC_TLV_INVALID;
    ocsp_extensions single = {request->parsing, 0, 0, {NULL, 0}};
    status = extensions(field.value, request, workspace, work, &single);
    if (status != TC_TLV_OK)
      return status;
  }
  if (!tc_pki_end(&reader))
    return TC_TLV_INVALID;
  if (matched) {
    if (parsed->matched)
      return TC_TLV_INVALID;
    parsed->matched = 1;
    parsed->status = cert_status;
    parsed->result.this_update = result.this_update;
    parsed->result.next_update = result.next_update;
    parsed->result.has_next_update = result.has_next_update;
    parsed->result.revocation_time = result.revocation_time;
    parsed->result.reason = result.reason;
    parsed->result.has_reason = result.has_reason;
  }
  return TC_TLV_OK;
}

static TC_TLV_result response_data(TC_bytes encoded, const TC_X509_certificate* certificate,
                                   const TC_X509_ocsp_verify_request* request,
                                   const TC_X509_path_workspace* workspace, size_t* work,
                                   ocsp_response* parsed)
{
  TC_TLV_reader reader, responses;
  TC_TLV_element field;
  TC_TLV_result status = sequence(encoded, request->parsing, &reader);
  if (status != TC_TLV_OK)
    return status;
  TC_TLV_reader probe = reader;
  if (TC_TLV_next(&probe, &field) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (tc_pki_tag(&field, 0xa0)) {
    TC_TLV_reader version;
    if (contents_reader(field.value, request->parsing, &version) != TC_TLV_OK ||
        tc_pki_field(&version, 2, &field) != TC_TLV_OK || !tc_pki_end(&version) ||
        field.value.length != 1 || field.value.data[0] != 0)
      return TC_TLV_UNSUPPORTED;
    if (TC_TLV_next(&probe, &field) != TC_TLV_OK)
      return TC_TLV_INVALID;
  }
  reader = probe;
  if (field.header.tag_length != 1)
    return TC_TLV_INVALID;
  if (tc_pki_tag(&field, 0xa1)) {
    TC_TLV_reader name;
    TC_TLV_element value;
    if (contents_reader(field.value, request->parsing, &name) != TC_TLV_OK ||
        tc_pki_field(&name, 0x30, &value) != TC_TLV_OK || !tc_pki_end(&name))
      return TC_TLV_INVALID;
    parsed->responder = value.encoded;
    parsed->responder_by_key = 0;
  } else if (tc_pki_tag(&field, 0xa2)) {
    TC_TLV_reader key;
    TC_TLV_element value;
    if (contents_reader(field.value, request->parsing, &key) != TC_TLV_OK ||
        tc_pki_field(&key, 4, &value) != TC_TLV_OK || !tc_pki_end(&key) || value.value.length != 20)
      return TC_TLV_INVALID;
    parsed->responder = value.value;
    parsed->responder_by_key = 1;
  } else
    return TC_TLV_INVALID;
  if (tc_pki_field(&reader, 0x18, &field) != TC_TLV_OK ||
      time_value(&field, &parsed->result.produced_at) != TC_TLV_OK ||
      tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK ||
      contents_reader(field.value, request->parsing, &responses) != TC_TLV_OK)
    return TC_TLV_INVALID;
  size_t count = 0;
  while ((status = TC_TLV_next(&responses, &field)) == TC_TLV_OK) {
    if (++count > request->max_responses)
      return TC_TLV_LIMIT;
    if (!tc_pki_tag(&field, 0x30))
      return TC_TLV_INVALID;
    status = single_response(field.encoded, certificate, request, workspace, work, parsed);
    if (status != TC_TLV_OK)
      return status;
  }
  if (status != TC_TLV_END || !count || !parsed->matched)
    return TC_TLV_INVALID;
  if (!tc_pki_end(&reader)) {
    if (tc_pki_field(&reader, 0xa1, &field) != TC_TLV_OK)
      return TC_TLV_INVALID;
    ocsp_extensions response = {request->parsing, 1, 0, {NULL, 0}};
    status = extensions(field.value, request, workspace, work, &response);
    if (status != TC_TLV_OK)
      return status;
    parsed->nonce = response.nonce;
    parsed->nonce_present = response.nonce_present;
  }
  return tc_pki_end(&reader) ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result basic_response(TC_bytes encoded, const TC_X509_certificate* certificate,
                                    const TC_X509_ocsp_verify_request* request,
                                    const TC_X509_path_workspace* workspace, size_t* work,
                                    ocsp_response* parsed)
{
  TC_TLV_reader reader;
  TC_TLV_element field;
  unsigned unused;
  /* The outer walk stops at the responseBytes OCTET STRING. Walk the whole
   * BasicOCSPResponse under the caller limits and charge its bytes to work. */
  const tc_pki_tree_workspace tree = {workspace->frames.data, workspace->frames.capacity, work};
  TC_TLV_result status =
      tc_pki_tree_open(encoded, 0x30, TC_TLV_DER, request->parsing, &tree, &reader);
  if (status != TC_TLV_OK)
    return status;
  if (tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK)
    return TC_TLV_INVALID;
  parsed->tbs = field.encoded;
  status = response_data(field.encoded, certificate, request, workspace, work, parsed);
  if (status != TC_TLV_OK)
    return status;
  if (tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK ||
      TC_DER_algorithm_identifier(field.encoded, &parsed->algorithm) != TC_TLV_OK ||
      tc_pki_field(&reader, 3, &field) != TC_TLV_OK ||
      TC_DER_bit_string(field.encoded, &parsed->signature, &unused) != TC_TLV_OK || unused ||
      !parsed->signature.length)
    return TC_TLV_INVALID;
  if (!tc_pki_end(&reader)) {
    TC_TLV_reader embedded;
    if (tc_pki_field(&reader, 0xa0, &field) != TC_TLV_OK ||
        contents_reader(field.value, request->parsing, &embedded) != TC_TLV_OK ||
        tc_pki_field(&embedded, 0x30, &field) != TC_TLV_OK || !tc_pki_end(&embedded))
      return TC_TLV_INVALID;
    parsed->embedded = field.value;
  }
  return tc_pki_end(&reader) ? TC_TLV_OK : TC_TLV_INVALID;
}

/* OCSPResponseStatus values (RFC 6960 4.2.1). */
enum {
  OCSP_SUCCESSFUL = 0,
  OCSP_INTERNAL_ERROR = 2,
  OCSP_TRY_LATER = 3,
  OCSP_SIG_REQUIRED = 5,
  OCSP_UNAUTHORIZED = 6
};

static TC_TLV_result parse_response(const TC_X509_ocsp_verify_request* request,
                                    const TC_X509_certificate* certificate,
                                    const TC_X509_path_workspace* workspace, size_t* work,
                                    ocsp_response* parsed)
{
  TC_TLV_reader reader, bytes;
  TC_TLV_element field;
  TC_bytes oid;
  TC_TLV_result status = sequence(request->response, request->parsing, &reader);
  if (status != TC_TLV_OK || tc_pki_field(&reader, 0x0a, &field) != TC_TLV_OK ||
      field.value.length != 1)
    return TC_TLV_INVALID;
  const unsigned response_status = field.value.data[0];
  if (response_status != OCSP_SUCCESSFUL) {
    /* Unsigned error responses carry no status for the certificate. */
    if (!tc_pki_end(&reader))
      return TC_TLV_INVALID;
    return response_status == OCSP_INTERNAL_ERROR || response_status == OCSP_TRY_LATER ||
                   response_status == OCSP_SIG_REQUIRED || response_status == OCSP_UNAUTHORIZED
               ? TC_TLV_UNSUPPORTED
               : TC_TLV_INVALID;
  }
  if (tc_pki_field(&reader, 0xa0, &field) != TC_TLV_OK || !tc_pki_end(&reader) ||
      contents_reader(field.value, request->parsing, &reader) != TC_TLV_OK ||
      tc_pki_field(&reader, 0x30, &field) != TC_TLV_OK || !tc_pki_end(&reader) ||
      contents_reader(field.value, request->parsing, &bytes) != TC_TLV_OK ||
      tc_pki_field(&bytes, 6, &field) != TC_TLV_OK || TC_DER_oid(field.encoded, &oid) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (!oid_is(oid, basic_oid, sizeof basic_oid))
    return TC_TLV_UNSUPPORTED;
  if (tc_pki_field(&bytes, 4, &field) != TC_TLV_OK || !tc_pki_end(&bytes))
    return TC_TLV_INVALID;
  return basic_response(field.value, certificate, request, workspace, work, parsed);
}

/* Freshness of a parsed response (RFC 6960 3.2 items 5 and 6, 4.2.2.1) under
 * the shared revocation time rule. producedAt may be at most skew ahead of
 * at. A response without nextUpdate is fresh only under a nonzero
 * max_age_seconds. Response times that cannot be converted are INVALID. */
static TC_TLV_result time_check(const TC_X509_ocsp_verify_request* request,
                                const TC_X509_ocsp_report* result)
{
  int64_t now, produced;
  tc_x509_freshness freshness;
  if (TC_X509_time_to_unix(&request->time.at, &now) != TC_TLV_OK ||
      TC_X509_time_to_unix(&result->produced_at, &produced) != TC_TLV_OK ||
      produced > now + (int64_t)request->time.clock_skew_seconds)
    return TC_TLV_INVALID;
  if (tc_x509_freshness_at(&request->time, &result->this_update,
                           result->has_next_update ? &result->next_update : NULL,
                           &freshness) != TC_TLV_OK)
    return TC_TLV_INVALID;
  if (freshness == TC_X509_FRESH_CURRENT ||
      (freshness == TC_X509_FRESH_NO_NEXT_UPDATE && request->time.max_age_seconds))
    return TC_TLV_OK;
  return TC_TLV_INVALID;
}

static TC_TLV_result name_matches(TC_bytes left, TC_bytes right,
                                  const TC_X509_ocsp_verify_request* request,
                                  const TC_X509_path_workspace* workspace, size_t* work,
                                  int* matched)
{
  return TC_X509_name_equal(left, right, request->parsing, &workspace->names, work, matched);
}

/* A byKey ResponderID is the SHA-1 hash of the responder's subjectPublicKey
 * BIT STRING value (RFC 6960 4.2.1). The build requires SHA-1 for OCSP. */
static TC_TLV_result responder_matches(const ocsp_response* response, TC_bytes name, TC_bytes key,
                                       const TC_X509_ocsp_verify_request* request,
                                       const TC_X509_path_workspace* workspace, size_t* work,
                                       int* matched)
{
  if (!response->responder_by_key)
    return name_matches(response->responder, name, request, workspace, work, matched);
  uint8_t computed[64];
  TC_TLV_result status = digest(TC_HASH_SHA1, key, computed, work);
  if (status == TC_TLV_OK)
    *matched = !memcmp(response->responder.data, computed, 20);
  return status;
}

/* INVALID when the signature does not verify under key. */
static TC_TLV_result verify_signature(const ocsp_response* response, const TC_X509_public_key* key,
                                      const TC_X509_ocsp_verify_request* request, size_t* work)
{
  const TC_bytes message = response->tbs;
  return tc_pki_signature_status(TC_X509_signature_verify_message(
      &message, 1, &response->algorithm, response->signature, key, request->signatures, work));
}

/* RFC 6960 4.2.2.2: the delegate is issued directly by the CA that issued
 * the certificate and carries id-kp-OCSPSigning. Validate it as a
 * one-certificate path below the issuer with that purpose, which also checks
 * its signature, validity with skew, digitalSignature in a present key usage
 * and critical extensions. anyExtendedKeyUsage does not authorize it. */
static TC_TLV_result delegate_path(TC_bytes encoded, const TC_X509_ocsp_verify_request* request,
                                   const TC_X509_path_workspace* workspace, size_t* work,
                                   TC_X509_public_key* key)
{
  TC_X509_path_options options;
  TC_X509_path_report validated;
  memset(&options, 0, sizeof options);
  options.at = request->time.at;
  options.clock_skew_seconds = request->time.clock_skew_seconds;
  options.parsing = *request->parsing;
  options.max_certificates = 1;
  options.max_input = request->parsing->max_input;
  options.signatures = *request->signatures;
  options.purpose = (TC_bytes){ocsp_signing_oid, sizeof ocsp_signing_oid};
  options.key_usage = TC_KEY_USAGE_DIGITAL_SIGNATURE;
  options.flags = TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE | TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  TC_X509_path_status status = tc_x509_path_validate_budget(&encoded, 1, request->issuer, &options,
                                                            workspace, work, &validated);
  if (status == TC_X509_PATH_VALID)
    *key = validated.public_key;
  return tc_x509_path_result_status(status);
}

/* id-pkix-ocsp-nocheck has a NULL value (RFC 6960 4.2.2.2.1). The path
 * validation before this call already rejected duplicate extensions. */
static TC_TLV_result delegate_nocheck(const TC_X509_certificate* signer,
                                      const TC_TLV_limits* limits, size_t* work, int* nocheck)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  int found = 0;
  TC_TLV_result status = tc_pki_extensions_init(&reader, signer, limits, work);
  if (status != TC_TLV_OK)
    return status;
  while ((status = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    if (!oid_is(extension.oid, nocheck_oid, sizeof nocheck_oid))
      continue;
    if (TC_DER_null(extension.value) != TC_TLV_OK)
      return TC_TLV_INVALID;
    found = 1;
  }
  if (status != TC_TLV_END)
    return status;
  *nocheck = found;
  return TC_TLV_OK;
}

/* Delegate candidates come from the response certs field, then the store.
 * max_certificates bounds the candidates examined across both sources. */
typedef struct {
  size_t examined;
  int nocheck;
  TC_bytes certificate;
} delegate_search;

/* Try one candidate as a delegated responder. OK with search->certificate
 * set means the candidate is authorized and signed the response. A candidate
 * that is malformed, unauthorized or unsupported leaves the search running.
 * LIMIT and ARGUMENT stop it. */
static TC_TLV_result try_candidate(TC_bytes encoded, const ocsp_response* response,
                                   const TC_X509_ocsp_verify_request* request,
                                   const TC_X509_path_workspace* workspace, size_t* work,
                                   delegate_search* search)
{
  if (search->examined++ >= request->max_certificates)
    return TC_TLV_LIMIT;
  TC_X509_workspace parser = {workspace->frames, workspace->oids, workspace->oid_capacity};
  TC_X509_certificate signer;
  TC_X509_public_key key;
  int matches = 0, nocheck = 0;
  TC_TLV_result status = TC_X509_read(encoded, request->parsing, &parser, &signer);
  if (status == TC_TLV_OK)
    status = responder_matches(response, signer.subject, signer.public_key.key, request, workspace,
                               work, &matches);
  if (status == TC_TLV_OK && !matches)
    return TC_TLV_OK;
  if (status == TC_TLV_OK)
    status = delegate_path(encoded, request, workspace, work, &key);
  if (status == TC_TLV_OK)
    status = delegate_nocheck(&signer, request->parsing, work, &nocheck);
  if (status == TC_TLV_OK)
    status = verify_signature(response, &key, request, work);
  if (status == TC_TLV_OK) {
    search->certificate = encoded;
    search->nocheck = nocheck;
  }
  return status == TC_TLV_LIMIT || status == TC_TLV_ARGUMENT ? status : TC_TLV_OK;
}

static TC_TLV_result authorize_response(ocsp_response* response,
                                        const TC_X509_ocsp_verify_request* request,
                                        const TC_X509_path_workspace* workspace, size_t* work)
{
  int matches;
  TC_TLV_result status =
      responder_matches(response, request->issuer->name, request->issuer->public_key.key, request,
                        workspace, work, &matches);
  if (status != TC_TLV_OK)
    return status;
  if (matches) {
    /* A signature that does not verify under the issuer key may still come
     * from a delegate with the same ResponderID. */
    status = verify_signature(response, &request->issuer->public_key, request, work);
    if (status != TC_TLV_INVALID)
      return status;
  }
  delegate_search search = {0, 0, {NULL, 0}};
  if (response->embedded.data) {
    TC_TLV_reader embedded;
    TC_TLV_element element;
    status = contents_reader(response->embedded, request->parsing, &embedded);
    if (status != TC_TLV_OK)
      return status;
    while (!search.certificate.data && (status = TC_TLV_next(&embedded, &element)) == TC_TLV_OK) {
      if (!tc_pki_tag(&element, 0x30))
        return TC_TLV_INVALID;
      status = try_candidate(element.encoded, response, request, workspace, work, &search);
      if (status != TC_TLV_OK)
        return status;
    }
    if (!search.certificate.data && status != TC_TLV_END)
      return status;
  }
  const TC_X509_store_source* store = request->certificates;
  for (size_t i = 0; store && !search.certificate.data && i < store->candidate_count; ++i) {
    TC_bytes candidate;
    status = tc_pki_source_candidate(store, i, work, &candidate);
    if (status != TC_TLV_OK)
      return status;
    status = try_candidate(candidate, response, request, workspace, work, &search);
    if (status != TC_TLV_OK)
      return status;
  }
  if (!search.certificate.data)
    return TC_TLV_INVALID;
  response->result.responder_certificate = search.certificate;
  response->result.responder_nocheck = search.nocheck;
  return TC_TLV_OK;
}

static int trust_anchor_present(const TC_X509_trust_anchor* issuer)
{
  return issuer && issuer->name.data && issuer->public_key.key.data;
}

/* Verify after the entry checks. out is written only on OK. */
static TC_TLV_result verify_response(const TC_X509_ocsp_verify_request* request,
                                     const TC_X509_path_workspace* workspace, size_t* work,
                                     TC_X509_ocsp_report* out)
{
  if (request->response.length > *work)
    return TC_TLV_LIMIT;
  *work -= request->response.length;
  TC_TLV_result status =
      TC_TLV_walk(request->response, TC_TLV_DER, request->parsing, workspace->frames, NULL, NULL);
  if (status != TC_TLV_OK)
    return status;
  TC_X509_workspace parser = {workspace->frames, workspace->oids, workspace->oid_capacity};
  TC_X509_certificate certificate;
  status = TC_X509_read(request->certificate, request->parsing, &parser, &certificate);
  if (status != TC_TLV_OK)
    return status;
  int matches;
  status =
      name_matches(certificate.issuer, request->issuer->name, request, workspace, work, &matches);
  if (status != TC_TLV_OK || !matches)
    return status == TC_TLV_OK ? TC_TLV_INVALID : status;
  ocsp_response parsed;
  memset(&parsed, 0, sizeof parsed);
  status = parse_response(request, &certificate, workspace, work, &parsed);
  if (status != TC_TLV_OK)
    return status;
  if (request->expected_nonce.length &&
      (!parsed.nonce_present || !tc_pki_equal(request->expected_nonce, parsed.nonce)))
    return TC_TLV_INVALID;
  status = time_check(request, &parsed.result);
  if (status == TC_TLV_OK)
    status = authorize_response(&parsed, request, workspace, work);
  if (status != TC_TLV_OK)
    return status;
  /* An authenticated unknown status gives no revocation decision. */
  if (parsed.status == OCSP_UNKNOWN)
    return TC_TLV_UNSUPPORTED;
  parsed.result.status =
      parsed.status == OCSP_REVOKED ? TC_X509_REVOCATION_REVOKED : TC_X509_REVOCATION_GOOD;
  *out = parsed.result;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_ocsp_response_verify(const TC_X509_ocsp_verify_request* request,
                                           const TC_X509_path_workspace* workspace, size_t* work,
                                           TC_X509_ocsp_report* out)
{
  if (!request || !workspace || !work || !out || !request->response.data ||
      !request->certificate.data || !trust_anchor_present(request->issuer) || !request->parsing ||
      !request->signatures || !workspace->frames.data || !workspace->oids ||
      !request->max_responses ||
      (request->certificates && request->certificates->candidate_count &&
       !request->certificates->candidate) ||
      (request->expected_nonce.length &&
       (!request->expected_nonce.data || request->expected_nonce.length < 32 ||
        request->expected_nonce.length > 128)) ||
      TC_X509_time_check(&request->time.at) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  TC_TLV_result status = verify_response(request, workspace, work, out);
  if (status != TC_TLV_OK)
    memset(out, 0, sizeof *out);
  return status;
}

/* Size of one element with a one-byte tag. Request contents stay far below
 * the FFFFFF bound of the shared header writer. request_sizes checks the
 * outer size, which bounds every inner one. */
static size_t der_size(size_t content)
{
  return tc_tlv_header_size(1, content) + content;
}

static uint8_t* der_header(uint8_t* out, uint8_t tag, size_t content)
{
  return tc_tlv_header_write(out, &tag, 1, content);
}

static uint8_t* der_bytes(uint8_t* out, uint8_t tag, TC_bytes bytes)
{
  out = der_header(out, tag, bytes.length);
  memcpy(out, bytes.data, bytes.length);
  return out + bytes.length;
}

/* Content and total sizes of one OCSPRequest (RFC 6960 4.1.1) with a single
 * Request and an optional RFC 9654 nonce requestExtension. */
typedef struct {
  size_t algorithm_content, cert_id_content, cert_id, request, extension_content, extension,
      extensions, tbs_content, tbs, total;
} request_layout;

static request_layout request_sizes(const tc_hash_info* info, size_t serial_length,
                                    size_t nonce_length)
{
  request_layout layout = {0};
  /* AlgorithmIdentifier { OID, NULL }. */
  layout.algorithm_content = der_size(info->oid.length) + 2;
  layout.cert_id_content = der_size(layout.algorithm_content) + 2 * der_size(info->digest_length) +
                           der_size(serial_length);
  layout.cert_id = der_size(layout.cert_id_content);
  layout.request = der_size(layout.cert_id);
  size_t extensions_wrapper = 0;
  if (nonce_length) {
    layout.extension_content = der_size(sizeof nonce_oid) + der_size(der_size(nonce_length));
    layout.extension = der_size(layout.extension_content);
    layout.extensions = der_size(layout.extension);
    extensions_wrapper = der_size(layout.extensions);
  }
  layout.tbs_content = der_size(layout.request) + extensions_wrapper;
  layout.tbs = der_size(layout.tbs_content);
  layout.total = tc_tlv_header_size(1, layout.tbs) ? der_size(layout.tbs) : 0;
  return layout;
}

TC_TLV_result TC_X509_ocsp_request_encode(const TC_X509_ocsp_encode_request* request,
                                          const TC_X509_path_workspace* workspace, size_t* work,
                                          TC_buffer encoded, size_t* length)
{
  if (!request)
    return TC_TLV_ARGUMENT;
  const TC_bytes certificate = request->certificate;
  const TC_X509_trust_anchor* issuer = request->issuer;
  const TC_hash_algorithm hash = request->hash;
  const TC_bytes nonce = request->nonce;
  const TC_TLV_limits* parsing = request->parsing;
  tc_hash_info info;
  if (!certificate.data || !trust_anchor_present(issuer) || !parsing || !workspace ||
      !workspace->frames.data || !workspace->oids || !work || !length ||
      (encoded.capacity && !encoded.data) ||
      (nonce.length && (!nonce.data || nonce.length < 32 || nonce.length > 128)))
    return TC_TLV_ARGUMENT;
  if (!tc_internal_ranges_disjoint(encoded.data, encoded.capacity, certificate.data,
                                   certificate.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, issuer->name.data,
                                   issuer->name.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, issuer->public_key.key.data,
                                   issuer->public_key.key.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, nonce.data, nonce.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, request, sizeof *request) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, length, sizeof *length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.capacity, work, sizeof *work))
    return TC_TLV_ARGUMENT;
  /* Every later failure reports no encoding, except a short buffer. */
  *length = 0;
  if ((hash != TC_HASH_SHA1 && hash != TC_HASH_SHA256) || !tc_hash_info_get(hash, &info) ||
      !tc_hash_available(hash))
    return TC_TLV_UNSUPPORTED;
  TC_X509_workspace parser = {workspace->frames, workspace->oids, workspace->oid_capacity};
  TC_X509_certificate target;
  TC_TLV_result status = TC_X509_read(certificate, parsing, &parser, &target);
  if (status != TC_TLV_OK)
    return status;
  int matched;
  status =
      TC_X509_name_equal(target.issuer, issuer->name, parsing, &workspace->names, work, &matched);
  if (status != TC_TLV_OK || !matched)
    return status == TC_TLV_OK ? TC_TLV_INVALID : status;
  /* The serial is bounded by the certificate, so the sizes cannot overflow.
   * A request above the writer's FFFFFF length bound reports no layout. */
  const request_layout layout = request_sizes(&info, target.serial.length, nonce.length);
  if (!layout.total)
    return TC_TLV_UNSUPPORTED;
  if (layout.total > encoded.capacity) {
    *length = layout.total;
    return TC_TLV_LIMIT;
  }

  uint8_t name_hash[64], key_hash[64];
  if (info.digest_length > sizeof name_hash)
    return TC_TLV_UNSUPPORTED;
  status = digest(hash, issuer->name, name_hash, work);
  if (status == TC_TLV_OK)
    status = digest(hash, issuer->public_key.key, key_hash, work);
  if (status != TC_TLV_OK)
    return status;
  uint8_t* cursor = der_header(encoded.data, 0x30, layout.tbs);
  cursor = der_header(cursor, 0x30, layout.tbs_content);
  cursor = der_header(cursor, 0x30, layout.request);
  cursor = der_header(cursor, 0x30, layout.cert_id);
  cursor = der_header(cursor, 0x30, layout.cert_id_content);
  cursor = der_header(cursor, 0x30, layout.algorithm_content);
  cursor = der_bytes(cursor, 6, info.oid);
  *cursor++ = 5;
  *cursor++ = 0;
  cursor = der_bytes(cursor, 4, (TC_bytes){name_hash, info.digest_length});
  cursor = der_bytes(cursor, 4, (TC_bytes){key_hash, info.digest_length});
  cursor = der_bytes(cursor, 2, target.serial);
  if (nonce.length) {
    cursor = der_header(cursor, 0xa2, layout.extensions);
    cursor = der_header(cursor, 0x30, layout.extension);
    cursor = der_header(cursor, 0x30, layout.extension_content);
    cursor = der_bytes(cursor, 6, (TC_bytes){nonce_oid, sizeof nonce_oid});
    cursor = der_header(cursor, 4, der_size(nonce.length));
    cursor = der_bytes(cursor, 4, nonce);
  }
  *length = (size_t)(cursor - encoded.data);
  return TC_TLV_OK;
}

#endif
