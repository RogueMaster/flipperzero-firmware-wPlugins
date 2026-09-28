/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_VALIDATE_H_
#define EXAMPLE_CREDENTIAL_VALIDATE_H_
#include "credential_auth.h"
#include "x509_workspace.h"
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/twic_ccl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EXAMPLE_TWIC_AUTHENTICATED,
  EXAMPLE_TWIC_INVALID,
  EXAMPLE_TWIC_CANCELLED,
  EXAMPLE_TWIC_STALE,
  EXAMPLE_TWIC_UNAVAILABLE,
  EXAMPLE_TWIC_UNSUPPORTED,
  EXAMPLE_TWIC_LIMIT,
  EXAMPLE_TWIC_TRANSPORT,
  EXAMPLE_TWIC_ERROR
} ExampleTWICResult;

typedef struct {
  /* Slot 9E DER and a held source of issuer candidates and trusted anchors. */
  TC_bytes certificate;
  const TC_X509_store_source *trust;
  const TC_X509_path_options *path;
  TC_PIV_card_profile profile;
  int allow_rsa1024;
  const TC_TWIC_CCL_snapshot *ccl;
  uint64_t ccl_max_age, ccl_minimum_publication;
  size_t ccl_reads;
  /* RSA challenge encoding; EC keys use their curve's hash policy. */
  ExampleCardRSAPadding rsa_padding;
} ExampleTWICRequest;

/* Certificate checks finish before the card challenge uses this storage. */
typedef union {
  ExampleX509SearchWorkspace certificate;
  ExampleCardKeyWorkspace challenge;
} ExampleTWICWorkspace;

typedef struct {
  TC_PIV_card_identifiers identifiers;
  TC_X509_time expiration;
} ExampleCardIdentity;

/* Check one held CCL snapshot for freshness and exact FASC-N cancellation.
 * A superseded snapshot returns STALE. */
ExampleTWICResult
example_twic_cancellation_check(const TC_TWIC_CCL_snapshot *ccl,
                                const TC_TWIC_CCL_freshness_policy *freshness,
                                size_t max_reads, TC_bytes fascn);

/* Read identity metadata from slot 9E DER. Returned spans borrow encoded.
 * Caller supplies the validated certificate and its selected card profile.
 * Inputs, work, storage and out must be disjoint. out changes only on OK. */
TC_TLV_result example_read_card_identity(
    TC_bytes encoded, TC_PIV_card_profile profile, const TC_TLV_limits *limits,
    ExampleX509Workspace *storage, size_t *work, ExampleCardIdentity *out);

/* TWIC Part 2 section 7.5 active-card authentication on a selected application.
 * NEXGEN uses TWIC; Legacy uses its PIV application. Hold the reader
 * transaction, trust source, certificate and acquired CCL snapshot throughout
 * the call. path supplies the evaluation time, policy and crypto provider. Its
 * purpose must be a registered PIV/TWIC card-authentication OID. Required
 * KU/EKU checks are added here. CCL freshness uses that same time, converted to
 * Unix seconds. This workflow requires an evaluation time on or after
 * 1970-01-01 UTC. The application controls clock accuracy, maximum transaction
 * duration and access authorization. AUTHENTICATED applies to this evaluation
 * instant.
 *
 * Work is shared across certificate validation, identifier reading and proof;
 * ccl_reads bounds each of the two index lookups separately. A failure ends the
 * operation. Cancellation/status failures before proof send no card command.
 * Workspace, work, mutable contexts and inputs must be disjoint. Keep scratch
 * outside small task stacks; it is cleared after processing. No PIN is used. */
ExampleTWICResult
example_twic_authenticate(ExampleCardIO *io, const ExampleTWICRequest *request,
                          TC_random_fn random, void *random_context,
                          ExampleTWICWorkspace *workspace, size_t *work);

#ifdef __cplusplus
}
#endif
#endif
