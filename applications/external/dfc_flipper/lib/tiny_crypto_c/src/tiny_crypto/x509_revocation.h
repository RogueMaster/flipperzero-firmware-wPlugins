/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Revocation checking for a validated path from CRLs and OCSP responses.
 * The status and time types are shared with x509_ocsp.h.
 * Standards: RFC 5280 sections 5 and 6.3, RFC 6960.
 * Configuration: TC_ENABLE_X509_REVOCATION, with OCSP evidence from
 * TC_ENABLE_X509_OCSP.
 * Limitations: CMS and credential validation use CRL evidence only.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/x509-revocation.md. */
#ifndef TINY_CRYPTO_X509_REVOCATION_H_
#define TINY_CRYPTO_X509_REVOCATION_H_

#include <tiny_crypto/x509_crl.h>
#include <tiny_crypto/x509_path.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Revocation status shared by CRL and OCSP evidence. UNDETERMINED means no
 * evidence covers the certificate. */
typedef enum {
  TC_X509_REVOCATION_UNDETERMINED,
  TC_X509_REVOCATION_GOOD,
  TC_X509_REVOCATION_REVOKED
} TC_X509_revocation_status;

/* Freshness rule shared by CRLs and OCSP responses. at must pass
 * TC_X509_time_check. Evidence is current when:
 *   thisUpdate <= at + clock_skew_seconds,
 *   nextUpdate, when present, is later than at - clock_skew_seconds, and
 *   thisUpdate is at most max_age_seconds before at - clock_skew_seconds.
 * max_age_seconds of zero sets no age bound. A CRL without nextUpdate is never
 * current. An OCSP response without nextUpdate needs a nonzero max_age_seconds.
 * Both values are seconds. */
typedef struct {
  TC_X509_time at;
  uint32_t clock_skew_seconds;
  uint32_t max_age_seconds;
} TC_X509_revocation_time;

/* ReasonFlags bits 1..8. Bit 0 is unused. */
enum { TC_X509_CRL_ALL_REASONS = 0x1fe };
typedef struct {
  uint16_t reasons;
  TC_X509_crl_match revocation;
} TC_X509_crl_evidence;
/* Provisional workspace entries. Fields are managed by the resolver. Each
 * entry uses two size_t links for bounded dependency lookup. */
typedef struct {
  TC_bytes certificate;
  TC_X509_revocation_status status;
  /* Internal hash chains. Keep all entries until the operation completes. */
  size_t hash_next, hash_head;
} TC_X509_revocation_node;
/* One caller-owned slot per indexed CRL, managed during revocation checks. */
typedef struct {
  size_t representative, next, head;
} TC_X509_revocation_scope;
typedef enum {
  TC_X509_CRL_COMPLETE_ONLY,
  TC_X509_CRL_DELTA_IF_AVAILABLE,
  TC_X509_CRL_DELTA_REQUIRED
} TC_X509_crl_delta_policy;
typedef enum { TC_X509_CRL_ORDER_NUMBER, TC_X509_CRL_ORDER_THIS_UPDATE } TC_X509_crl_order_policy;

/* Optional OCSP evidence, one DER OCSPResponse per path member. responses is
 * NULL with count zero, or holds exactly one span per chain entry in chain
 * order. An empty span means no response for that member. Each response is
 * verified with TC_X509_ocsp_response_verify against the member's issuer
 * (the selected anchor for chain[0], chain[i - 1] otherwise), without a
 * nonce. Delegate candidates come from the options source. max_responses and
 * max_certificates bound each response as in TC_X509_ocsp_verify_request. */
typedef struct {
  const TC_bytes* responses;
  size_t count;
  size_t max_responses, max_certificates;
} TC_X509_revocation_ocsp;

typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_store_source* source;
  const TC_X509_path_options* signer_policy;
  size_t anchor_index, max_candidate_bytes;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
  /* Evaluation time and freshness limits for CRLs and OCSP responses. */
  TC_X509_revocation_time time;
  TC_X509_revocation_ocsp ocsp;
} TC_X509_revocation_options;
typedef struct {
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
  uint8_t* states;
  size_t state_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
  TC_X509_revocation_scope* scopes;
  size_t scope_capacity;
  TC_bytes* signer_path;
  size_t signer_path_capacity;
  TC_bytes* signer_policies;
  size_t signer_policy_capacity;
} TC_X509_revocation_workspace;
typedef struct {
  TC_X509_revocation_status status;
  size_t certificate_index;
  TC_X509_crl_evidence evidence;
} TC_X509_revocation_report;

#if TC_ENABLE_X509_REVOCATION
/* Check a previously validated path, anchor-issued first, anchor excluded.
 * Keep the selected anchor, time, CRL index and source snapshot fixed. Source
 * candidates supply CRL signers, their paths and OCSP delegates. signer_policy
 * validates CRL signers and is distinct from the holder's purpose/usage
 * policy. options->time sets the freshness of every CRL and OCSP response,
 * and its at must equal signer_policy->at. max_candidate_bytes bounds the
 * candidate collection.
 *
 * An authenticated REVOKED response settles the member. GOOD from a delegate
 * without id-pkix-ocsp-nocheck requires the CRL index to prove the delegate
 * unrevoked (RFC 6960 section 4.2.2.2.1). After GOOD, current CRLs covering
 * the member are still checked and revocation wins. A member falls back to
 * CRLs when its response is malformed, unauthorized, stale, UNKNOWN or
 * unavailable, or when GOOD lacks the required delegate proof. A build
 * without TC_ENABLE_X509_OCSP uses CRLs for every member.
 *
 * states needs one byte per indexed CRL. nodes covers the path, OCSP
 * delegates without nocheck and distinct signer dependencies. Each node is
 * 2 size_t larger for lookup links. scopes needs one slot per indexed CRL.
 * Verified nodes and scope groups are reused only within this call.
 * signer_path needs search->capacity entries. signer_policies needs
 * validation->policy_capacity entries. OCSP verification uses the validation
 * workspace. Spans borrow stable source bytes and stay in scratch until this
 * call returns. Inputs, OCSP responses, metadata, workspace arrays, work and
 * out must be disjoint. Save path spans outside search scratch before
 * calling. Work and scratch are provisional.
 *
 * OK means a determined result: callers must inspect status. REVOKED includes
 * the member index and evidence. For OCSP evidence, revocation.revoked_at is
 * the revocationTime and reason the CRLReason, or unspecified when absent.
 * GOOD uses SIZE_MAX and zero evidence.
 * ARGUMENT: a required pointer is NULL, options->time.at fails
 *   TC_X509_time_check or differs from signer_policy->at, ocsp.count is
 *   neither zero nor count, or OCSP responses are supplied with a zero
 *   max_responses.
 * UNSUPPORTED: a member has no accepted OCSP response and no CRL evidence,
 *   the evidence depends on a cycle, or a candidate CRL for an unsettled
 *   member is unsupported, such as one with an unknown critical extension.
 * INVALID: a member has no accepted OCSP response, and its candidate CRLs
 *   failed as invalid data with none unsupported. Causes include a CRL
 *   signature that fails verification, no signer candidate with a valid path to
 *   the anchor, a revoked CRL signer, conflicting CRLs in one scope and
 *   malformed CRL entries.
 * LIMIT: signer_path or signer_policies below the required capacity, before
 *   any work, or exhausted work, storage or a parsing limit.
 * Failures leave out unchanged.
 *
 * Work: the OCSP verifications, CRL candidate reads and entry lookups, CRL
 * signer path builds and signature checks, and dependency lookups. */
TC_TLV_result TC_X509_path_check_revocation(const TC_bytes* chain, size_t count,
                                            const TC_X509_revocation_options* options,
                                            const TC_X509_revocation_workspace* workspace,
                                            size_t* work, TC_X509_revocation_report* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
