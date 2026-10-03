/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* OCSP request encoding and response verification for one certificate,
 * including stapled responses and delegated responders.
 * Standards: RFC 6960 sections 4.1 and 4.2, RFC 9654 for nonces.
 * Configuration: TC_ENABLE_X509_OCSP, which requires X.509 path support and
 * SHA-1. A byKey ResponderID is always a SHA-1 key hash (RFC 6960 section
 * 4.2.1).
 * Limitations: status and freshness use the shared revocation model of
 * x509_revocation.h, and TC_X509_path_check_revocation consumes responses
 * for a whole path. Before trusting GOOD, the caller establishes the
 * revocation status of a delegate without id-pkix-ocsp-nocheck.
 * Contracts: docs/api.md. Guides: docs/x509-ocsp.md,
 * docs/x509-revocation.md. */
#ifndef TINY_CRYPTO_X509_OCSP_H_
#define TINY_CRYPTO_X509_OCSP_H_

#include <tiny_crypto/x509_revocation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /* TC_X509_REVOCATION_GOOD or TC_X509_REVOCATION_REVOKED. */
  TC_X509_revocation_status status;
  TC_X509_time produced_at, this_update, next_update;
  int has_next_update;
  /* revocationTime of a REVOKED status. */
  TC_X509_time revocation_time;
  /* CRLReason of a REVOKED status (RFC 5280 5.3.1), valid when has_reason. */
  unsigned reason;
  int has_reason;
  /* DER certificate of the delegated responder that signed the response
   * (RFC 6960 4.2.2.2). It is {NULL, 0} when the issuer signed directly.
   * The span borrows request->response or a store record. */
  TC_bytes responder_certificate;
  /* Nonzero when that delegate carries id-pkix-ocsp-nocheck. Zero for a
   * delegate means the caller must establish the delegate's own revocation
   * status before relying on the result (RFC 6960 4.2.2.2.1). */
  int responder_nocheck;
} TC_X509_ocsp_report;

typedef struct {
  /* A complete DER OCSPResponse and the certificate it should cover. */
  TC_bytes response;
  TC_bytes certificate;
  /* Empty, or the 32..128 byte nonce sent in the request (RFC 9654 2.1).
   * When present, the response must echo it. */
  TC_bytes expected_nonce;
  /* The issuer name and key must come from the already validated path. */
  const TC_X509_trust_anchor* issuer;
  /* Optional untrusted delegate candidates, searched after the response's
   * own certs field. */
  const TC_X509_store_source* certificates;
  /* Evaluation time and freshness (see TC_X509_revocation_time). producedAt
   * must also be at most clock_skew_seconds after at. A response without
   * nextUpdate needs a nonzero max_age_seconds. */
  TC_X509_revocation_time time;
  /* max_responses bounds the SingleResponses read and must be nonzero.
   * max_certificates bounds the delegate candidates examined, from the certs
   * field and the store together. Zero examines none, which suffices for a
   * response signed by the issuer. */
  size_t max_responses, max_certificates;
  /* Applied to the OCSPResponse, the nested BasicOCSPResponse, the target
   * certificate and every delegate candidate. max_input bounds the response
   * and each certificate. */
  const TC_TLV_limits* parsing;
  const TC_X509_signature_provider* signatures;
} TC_X509_ocsp_verify_request;

#if TC_ENABLE_X509_OCSP
/* Verify a complete DER OCSPResponse, including one received by stapling
 * (RFC 6960 section 4.2). Response, certificate, store records and issuer
 * remain borrowed and unchanged during the call and while
 * out->responder_certificate is used. The certificate path and issuer must
 * already be trusted by the caller. work and out must be disjoint from the
 * request, its bytes and the workspace.
 *
 * workspace supplies frames, oids and names for parsing, extension checks
 * and Name comparison. A delegate is validated as a one-certificate path
 * below the issuer, which also uses certificates and summaries (one entry
 * each) and the policy arrays. frames need one entry per constructed nesting
 * level of the deepest object parsed. oids bounds the extensions of each
 * extension list.
 *
 * A response is accepted when the issuer signed it, or when a delegate
 * signed it that matches the ResponderID and validates as a path below the
 * issuer at time.at with time.clock_skew_seconds, with id-kp-OCSPSigning in
 * its extended key usage and digitalSignature in a present key usage
 * (RFC 6960 4.2.2.2). The result names that delegate and reports
 * id-pkix-ocsp-nocheck. Before trusting GOOD, the caller establishes the
 * delegate's own revocation status, using responder_nocheck for RFC 6960
 * 4.2.2.2.1. An authenticated REVOKED status remains revocation evidence.
 *
 * OK: the response is authenticated, fresh and echoes a present nonce, and
 *   out->status is GOOD or REVOKED.
 * ARGUMENT: a required pointer or issuer span is NULL, a store with
 *   candidates has no candidate callback, max_responses is zero, the nonce
 *   length is outside 32..128, or time.at fails TC_X509_time_check. Checked
 *   before any parsing, with out unchanged. Later, a store callback failure
 *   other than LIMIT or UNSUPPORTED, or a signature provider error, is
 *   ARGUMENT.
 * UNSUPPORTED: an authenticated unknown status, an unsuccessful
 *   responseStatus other than malformedRequest (internalError, tryLater,
 *   sigRequired, unauthorized), or an unknown response type, CertID hash,
 *   version, critical extension or signature algorithm.
 * INVALID: malformed DER, a malformedRequest responseStatus, an empty or
 *   duplicate extension list entry, no SingleResponse for the certificate,
 *   a duplicate one, a wrong issuer, a nonce mismatch, stale or future
 *   times, a removeFromCRL reason, or no authorized signer.
 * LIMIT: work, a parsing limit, workspace capacity, max_responses or
 *   max_certificates is exceeded.
 * After the argument checks, every result other than OK zeroes out. Work is
 * charged for the response and BasicOCSPResponse bytes, extensions,
 * hashing, Name comparison, delegate path validation and signature
 * verification. Work and scratch may change on every result. */
TC_TLV_result TC_X509_ocsp_response_verify(const TC_X509_ocsp_verify_request* request,
                                           const TC_X509_path_workspace* workspace, size_t* work,
                                           TC_X509_ocsp_report* out);
#endif

/* One OCSPRequest for a single certificate. issuer names and holds the key
 * of the certificate's issuer. hash selects the CertID hash: SHA-256 is
 * recommended, and TC_HASH_SHA1 serves a legacy responder. A present nonce is
 * generated by the caller and must be 32..128 bytes (RFC 9654 2.1). */
typedef struct {
  TC_bytes certificate;
  const TC_X509_trust_anchor* issuer;
  TC_hash_algorithm hash;
  TC_bytes nonce;
  const TC_TLV_limits* parsing;
} TC_X509_ocsp_encode_request;

#if TC_ENABLE_X509_OCSP
/* Encode one unsigned OCSPRequest (RFC 6960 4.1.1) into encoded and write its
 * size to length. The certificate, issuer and nonce stay borrowed and unchanged
 * during the call and must be disjoint from encoded. workspace supplies frames,
 * oids and names to parse the certificate and compare its issuer.
 *
 * Sizing: pass encoded = {NULL, 0} to query the size. When encoded is too
 * small the result is LIMIT, *length holds the required size and encoded is
 * unchanged. The size is fixed by the hash, the serial number length and the
 * nonce length.
 *
 * OK: encoded[0..*length) holds the DER request.
 * ARGUMENT: a required pointer or issuer span is NULL, the nonce length is
 *   outside 32..128, or encoded overlaps an input, request, work or length.
 *   Outputs unchanged.
 * UNSUPPORTED: hash is other than SHA-1 or SHA-256, or is disabled, or the
 *   request would exceed 16 MiB (a length field above three octets).
 * INVALID: the certificate is malformed or its issuer differs from
 *   issuer->name.
 * LIMIT: encoded is too small, with *length set to the required size, or
 *   work or a parsing limit is exceeded, with *length set to 0.
 * After argument validation, failures other than a short buffer set *length
 * to 0 and leave encoded unchanged. Work is charged for Name comparison and
 * for hashing the issuer name and key. */
TC_TLV_result TC_X509_ocsp_request_encode(const TC_X509_ocsp_encode_request* request,
                                          const TC_X509_path_workspace* workspace, size_t* work,
                                          TC_buffer encoded, size_t* length);
#endif

#ifdef __cplusplus
}
#endif
#endif
