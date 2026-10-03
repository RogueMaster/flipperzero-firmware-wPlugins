/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_X509_OCSP_H_
#define EXAMPLE_X509_OCSP_H_
#include "x509_workspace.h"
#include <tiny_crypto/x509_ocsp.h>
#ifdef __cplusplus
extern "C" {
#endif

/* A 32-byte nonce is the RFC 9654 minimum. The request capacity covers a
 * SHA-256 CertID, a 20-byte serial number and that nonce. Each SD 33
 * response uses under 100000 work units with the native provider, and the
 * work limit leaves room for four delegate candidates. */
enum {
  EXAMPLE_OCSP_NONCE_LENGTH = 32,
  EXAMPLE_OCSP_REQUEST_CAPACITY = 192,
  EXAMPLE_OCSP_WORK_LIMIT = 1000000
};

typedef enum {
  /* Authenticated and fresh. result holds the times. */
  EXAMPLE_OCSP_GOOD,
  /* Authenticated and fresh. result holds revocation_time and the reason. */
  EXAMPLE_OCSP_REVOKED,
  /* GOOD or REVOKED from a delegate without id-pkix-ocsp-nocheck. Check
   * result->responder_certificate against a CRL before relying on the
   * status (RFC 6960 4.2.2.2.1). */
  EXAMPLE_OCSP_CHECK_RESPONDER,
  /* No decision: an unknown status, a responder error such as tryLater, or
   * an unsupported algorithm or critical extension. Try another source. */
  EXAMPLE_OCSP_NO_DECISION,
  /* Malformed, stale, unauthorized, a nonce mismatch or no entry for the
   * certificate. */
  EXAMPLE_OCSP_REJECTED,
  /* A parsing, workspace or work limit was reached. */
  EXAMPLE_OCSP_LIMIT,
  /* A missing input or a signature provider failure. */
  EXAMPLE_OCSP_ERROR
} ExampleOcspStatus;

/* certificate and issuer come from an already validated path. nonce is the
 * value sent in the request, or empty when the response may be pre-produced
 * or stapled. delegates optionally lists responder certificates. Every span
 * stays borrowed and unchanged through the call and while the result's
 * responder_certificate is used. */
typedef struct {
  TC_bytes certificate;
  const TC_X509_trust_anchor* issuer;
  TC_bytes nonce;
  TC_bytes response;
  TC_X509_time at;
  const TC_X509_signature_provider* verifier;
  const TC_X509_store_source* delegates;
} ExampleOcspCheck;

/* Encode a request with a SHA-256 CertID and the caller's random nonce.
 * encoded must be at least the size reported by a first sizing query.
 * EXAMPLE_OCSP_REQUEST_CAPACITY suffices for ordinary serial numbers. On
 * LIMIT, *length holds the required size, or 0 when work ran out. */
TC_TLV_result example_ocsp_request(TC_bytes certificate, const TC_X509_trust_anchor* issuer,
                                   const uint8_t nonce[EXAMPLE_OCSP_NONCE_LENGTH],
                                   ExampleX509Workspace* storage, TC_buffer encoded,
                                   size_t* length);

/* Verify one response and map every library status. result is written on
 * GOOD, REVOKED and CHECK_RESPONDER and zeroed after any other verification
 * result. A NULL check, storage, result or verifier returns ERROR before
 * verification and leaves result unchanged. storage holds parser views only
 * and is wiped after every verification failure. */
ExampleOcspStatus example_ocsp_check(const ExampleOcspCheck* check, ExampleX509Workspace* storage,
                                     TC_X509_ocsp_report* result);

#ifdef __cplusplus
}
#endif
#endif
