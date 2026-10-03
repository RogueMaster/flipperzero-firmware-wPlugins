/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "x509_ocsp.h"
#include <string.h>

/* Bounds for responses of a few kilobytes that embed one delegate. The depth
 * matches the frames in ExampleX509Workspace. */
static const TC_TLV_limits ocsp_limits = {8192, 8192, 512, 16};

TC_TLV_result example_ocsp_request(TC_bytes certificate, const TC_X509_trust_anchor* issuer,
                                   const uint8_t nonce[EXAMPLE_OCSP_NONCE_LENGTH],
                                   ExampleX509Workspace* storage, TC_buffer encoded, size_t* length)
{
  if (!storage || !nonce || !length)
    return TC_TLV_ARGUMENT;
  const TC_X509_path_workspace workspace = example_x509_workspace(storage);
  const TC_X509_ocsp_encode_request request = {
      certificate, issuer, TC_HASH_SHA256, {nonce, EXAMPLE_OCSP_NONCE_LENGTH}, &ocsp_limits};
  size_t work = EXAMPLE_OCSP_WORK_LIMIT;
  return TC_X509_ocsp_request_encode(&request, &workspace, &work, encoded, length);
}

ExampleOcspStatus example_ocsp_check(const ExampleOcspCheck* check, ExampleX509Workspace* storage,
                                     TC_X509_ocsp_report* result)
{
  if (!check || !storage || !result || !check->verifier)
    return EXAMPLE_OCSP_ERROR;
  const TC_X509_path_workspace workspace = example_x509_workspace(storage);
  TC_X509_ocsp_verify_request request;
  memset(&request, 0, sizeof request);
  request.response = check->response;
  request.certificate = check->certificate;
  request.expected_nonce = check->nonce;
  request.issuer = check->issuer;
  request.certificates = check->delegates;
  /* Five minutes of skew. A response without nextUpdate is accepted for one
   * day after its thisUpdate. */
  request.time = (TC_X509_revocation_time){check->at, 300, 86400};
  request.max_responses = 16;
  request.max_certificates = 4;
  request.parsing = &ocsp_limits;
  request.signatures = check->verifier;
  size_t work = EXAMPLE_OCSP_WORK_LIMIT;
  const TC_TLV_result status = TC_X509_ocsp_response_verify(&request, &workspace, &work, result);
  if (status != TC_TLV_OK) {
    memset(storage, 0, sizeof *storage);
    memset(result, 0, sizeof *result);
  }
  switch (status) {
  case TC_TLV_OK:
    if (result->responder_certificate.data && !result->responder_nocheck)
      return EXAMPLE_OCSP_CHECK_RESPONDER;
    return result->status == TC_X509_REVOCATION_REVOKED ? EXAMPLE_OCSP_REVOKED : EXAMPLE_OCSP_GOOD;
  case TC_TLV_UNSUPPORTED:
    return EXAMPLE_OCSP_NO_DECISION;
  case TC_TLV_INVALID:
    return EXAMPLE_OCSP_REJECTED;
  case TC_TLV_LIMIT:
    return EXAMPLE_OCSP_LIMIT;
  default:
    return EXAMPLE_OCSP_ERROR;
  }
}
