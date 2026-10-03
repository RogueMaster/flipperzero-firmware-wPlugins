/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* CMS signer discovery and path validation: candidate certificates from the
 * envelope and held sources, signer path construction and CRL revocation.
 * Standards: RFC 5652 section 5, RFC 5280 section 6.
 * Configuration: TC_ENABLE_CMS_VALIDATION.
 * Limitations: revocation uses CRL evidence. validation.h composes these
 * steps with a shared arena.
 * Contracts: docs/api.md. Guides: docs/cms.md, docs/validation.md. */
#ifndef TINY_CRYPTO_CMS_VALIDATION_H_
#define TINY_CRYPTO_CMS_VALIDATION_H_
#include <tiny_crypto/cms.h>
#include <tiny_crypto/x509_path.h>
#include <tiny_crypto/x509_revocation.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Signer path policy. path governs the signer certificate path. verification
 * governs envelope parsing, signed attributes and the SignerInfo signature. The
 * candidate limits bound the embedded and external certificates examined. */
typedef struct {
  TC_X509_path_options path;
  size_t max_candidates, max_candidate_bytes;
  TC_CMS_verification_policy verification;
} TC_CMS_path_options;

enum { TC_CMS_SIGNED_DIGEST_BYTES = 64 };

typedef struct {
  TC_X509_path_workspace validation;
  TC_X509_search_workspace search;
  TC_bytes* certificates;
  size_t certificate_capacity;
  uint8_t* signature;
  size_t signature_capacity;
  /* At least TC_CMS_SIGNED_DIGEST_BYTES, separate from inputs and other scratch.
   * Cleared after each signer search. */
  uint8_t* signed_digest;
  size_t signed_digest_capacity;
} TC_CMS_path_workspace;

/* One signer whose path is built.
 * - signer comes from a SignerInfo reader and retains stable input.
 * - content_type and digest are the eContentType OID contents and the content
 *   digest under SignerInfo.digest_algorithm.
 * - certificates holds the SignedData certificates field, or is empty.
 * - signer_certificate, when set, is the required DER signer certificate.
 *   Leave it empty to discover the signer from certificates and the source. */
typedef struct {
  const TC_CMS_signer_info* signer;
  TC_bytes content_type, digest, certificates, signer_certificate;
} TC_CMS_signer_path_request;

/* One SignedData message.
 * - encoded is the complete ContentInfo, and signer_index selects a SignerInfo.
 * - expected_type is the required eContentType OID contents.
 * - detached_content holds detached_count spans hashed in array order. Empty
 *   spans are allowed, and zero spans are an empty message. Attached content
 *   requires zero detached spans.
 * - signer_certificate follows TC_CMS_signer_path_request. */
typedef struct {
  TC_bytes encoded;
  size_t signer_index;
  TC_bytes expected_type;
  const TC_bytes* detached_content;
  size_t detached_count;
  TC_bytes signer_certificate;
} TC_CMS_validation_request;

#if TC_ENABLE_CMS_VALIDATION
/* Find the signer certificate of one parsed SignerInfo, verify the signature
 * with it and build a trusted path (RFC 5652 section 5.3, RFC 5280 section
 * 6). Embedded certificates precede source candidates. The SignerInfo
 * identifier selects candidates: issuer and serial for version 1, subject key
 * identifier for version 3. A pivSigner-DN attribute must equal the
 * certificate subject. Trust comes from source anchors alone. Unsigned
 * attributes, including countersignatures, stay unauthenticated.
 * - request, source, options and all their bytes stay stable during the call.
 *   They may overlap each other and must be disjoint from workspace arrays,
 *   work and out.
 * - workspace: validation and search scratch sized as in x509_path.h,
 *   certificates holds one span per indexed candidate, signature holds a
 *   signature split across BER chunks, and signed_digest holds at least
 *   TC_CMS_SIGNED_DIGEST_BYTES. signed_digest is wiped before return.
 * - On VALID, out follows TC_X509_path_build: out->path borrows the
 *   workspace search path and the policy spans borrow validation scratch.
 *   Copy the path and policy span arrays before reusing the workspace. The
 *   certificate and source bytes they point to stay in place.
 *
 * Work: one unit per storage comparison, the candidate scan, one unit per
 * candidate, each signature attempt and each path build. The source callbacks
 * charge their own reads.
 * Returns VALID with out written. ERROR for NULL arguments, an empty content
 * type or digest, a signer_certificate span with NULL data and a length or
 * the reverse, a short signed_digest, a source with a NULL candidate or anchor
 * callback and a nonzero count, an unknown policy value or overlap, with all
 * state unchanged.
 * LIMIT for exhausted work, parsing, candidate or workspace capacities.
 * UNSUPPORTED when the best candidate failed on an unsupported algorithm or
 * feature. INVALID when no candidate yields a valid signature and path. out
 * changes only on VALID. */
TC_X509_path_status TC_CMS_signer_path_build(const TC_CMS_signer_path_request* request,
                                             const TC_X509_store_source* source,
                                             const TC_CMS_path_options* options,
                                             const TC_CMS_path_workspace* workspace, size_t* work,
                                             TC_X509_search_report* out);

/* Parse SignedData, check its content type and the signer's digest listing,
 * hash the attached or detached content, then run TC_CMS_signer_path_build
 * for the selected signer (RFC 5652 sections 5.1 to 5.6).
 * - request->encoded is the complete ContentInfo. Its signer_index selects a
 *   SignerInfo and expected_type is the required eContentType OID contents.
 * - Attached content is hashed from the OCTET STRING. Detached content is
 *   hashed from request->detached_content in array order. The content digest
 *   uses a stack buffer and hash context that are wiped on return.
 * Storage, borrowing, workspace and work rules match
 * TC_CMS_signer_path_build: out->path and the policy spans borrow workspace
 * scratch, so copy them before reusing the workspace. Work also covers the
 * envelope parse, the version and signer passes and the hashed content bytes.
 * Returns VALID with out written. ERROR for the argument errors of
 * TC_CMS_signer_path_build, an empty envelope or expected type, NULL
 * detached spans with a count, an expected type with malformed OID
 * contents, or detached spans for attached content. The last two are found
 * after the storage check charges work. LIMIT as in TC_CMS_signer_path_build.
 * UNSUPPORTED for a digest outside SHA-1 and SHA-2 or one disabled in this
 * build. INVALID for a malformed envelope, a missing signer, a content type
 * other than expected_type, an unlisted digest and the path failures of
 * TC_CMS_signer_path_build. out changes only on VALID. */
TC_X509_path_status TC_CMS_signed_data_path_build(const TC_CMS_validation_request* request,
                                                  const TC_X509_store_source* source,
                                                  const TC_CMS_path_options* options,
                                                  const TC_CMS_path_workspace* workspace,
                                                  size_t* work, TC_X509_search_report* out);
#endif

typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_path_options* signer_policy;
  size_t max_candidate_bytes;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} TC_CMS_revocation_policy;

typedef struct {
  const TC_CMS_path_workspace* path;
  TC_bytes* held_path;
  size_t path_capacity;
  uint8_t* crl_states;
  size_t crl_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
  TC_X509_revocation_scope* scopes;
  size_t scope_capacity;
  /* Borrowed signer path and policy views. Capacity matches path scratch. */
  TC_bytes* signer_path;
  size_t signer_path_capacity;
  TC_bytes* signer_policies;
  size_t signer_policy_capacity;
} TC_CMS_credential_workspace;

#if TC_ENABLE_CMS_VALIDATION
/* Run TC_CMS_signed_data_path_build and check every path member against the
 * CRL index with TC_X509_path_check_revocation (RFC 5280 sections 6.3 and
 * 5). CRL signer paths use revocation->signer_policy with the selected
 * source and anchor, so every supplied policy field affects the result. CRL
 * freshness uses signer_policy->at and its clock skew with no age bound. This
 * workflow takes CRL evidence only.
 * - workspace->path is the TC_CMS_path_workspace. held_path holds at least
 *   path->search.capacity spans and crl_states one byte per index record.
 *   nodes, scopes, signer_path and signer_policies follow
 *   TC_X509_revocation_workspace. TC_validation_workspace_init sizes all of
 *   them from one arena.
 * - Request, source, options, revocation, index and all their bytes stay
 *   stable and disjoint from every workspace array and work.
 *
 * Work: one unit per storage comparison, charged before any parsing, then the
 * path build and the revocation check.
 * Returns VALID when the signature, path and unrevoked status all hold.
 * REVOKED when a path member is revoked. UNAVAILABLE when no current CRL
 * covers a member. UNSUPPORTED for an unsupported algorithm or CRL feature,
 * including an unsupported CRL in the index. ERROR for NULL arguments or
 * workspace arrays, invalid policy values, a path time that differs from
 * signer_policy->at or overlap, with work unchanged. LIMIT before any work for
 * a held_path or signer_path capacity below path->search.capacity, a
 * crl_states or scopes capacity below the index count, no nodes, or a
 * signer_policies capacity below path->validation.policy_capacity, and later
 * for exhausted work or capacities. INVALID for a malformed message or failed
 * signature, path or CRL check. */
TC_credential_status TC_CMS_credential_validate(const TC_CMS_validation_request* request,
                                                const TC_X509_store_source* source,
                                                const TC_CMS_path_options* options,
                                                const TC_CMS_revocation_policy* revocation,
                                                const TC_CMS_credential_workspace* workspace,
                                                size_t* work);
#endif

#ifdef __cplusplus
}
#endif
#endif
