/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CMS_INTERNAL_H_
#define TC_CMS_INTERNAL_H_
#include <tiny_crypto/cms_validation.h>
#include "cms_base_internal.h"
#include "pki_tree_internal.h"
#include "pki_source_internal.h"
#include "x509_revocation_internal.h"
#include "pki_candidate_internal.h"

typedef enum {
  TC_CMS_OTHER_CERTIFICATE,
  TC_CMS_OTHER_REVOCATION
} tc_cms_other_kind;
typedef struct {
  TC_bytes format, value;
} tc_cms_other_format;
/* RFC 5652 OtherCertificateFormat/OtherRevocationInfoFormat, implicitly tagged.
 * The value is required and retained verbatim; format-specific validation is
 * the application's responsibility. Input/scratch/out must be disjoint;
 * spans borrow input and out changes only on OK. */
TC_TLV_result tc_cms_other_format_read(TC_bytes encoded, tc_cms_other_kind kind,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, tc_cms_other_format* out);

/* Match parsed SignerInfo and certificate views. Inputs must come from their
 * schema readers. A match identifies a candidate, not a trusted signer.
 * Missing SKI is a mismatch. Callers preflight disjoint input metadata/spans,
 * name/tree scratch and matched. matched changes only on OK. */
TC_TLV_result tc_cms_signer_matches(const tc_cms_signer_info* signer, TC_TLV_profile profile,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* names, const tc_pki_tree_workspace* tree, int* matched);

typedef struct {
  TC_TLV_reader embedded;
  size_t external_index, remaining, bytes_left;
} tc_cms_collection;
typedef struct {
  tc_cms_collection collection;
  const TC_X509_store_source* external;
} tc_cms_candidates;
typedef struct {
  TC_bytes encoded;
  tc_cms_certificate_kind kind;
} tc_cms_certificate_choice;

/* Shared validation engine. Additional metadata spans preserve caller-owned
 * configuration alias checks when public options are adapted on the stack. */
TC_credential_status tc_cms_credential_validate_with_metadata(
    const TC_CMS_validation_request* request,
    const TC_X509_store_source* source, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation,
    const TC_CMS_credential_workspace* workspace, size_t* work,
    const TC_bytes* metadata, size_t metadata_count);
/* Embedded [0] CertificateSet followed by external candidate records. Limits
 * bound total records and bytes, including the embedded collection framing.
 * Records/order and source metadata must stay stable throughout iteration.
 * OtherCertificateFormat requires its OID and value. Remaining choice schemas
 * and SignedData version consistency are separate checks.
 * Callers preflight disjoint inputs, source results, scratch and outputs. */
TC_TLV_result tc_cms_candidates_init(TC_bytes embedded, const TC_X509_store_source* external,
    size_t max_candidates, size_t max_bytes, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_cms_candidates* out);
TC_TLV_result tc_cms_candidates_next(tc_cms_candidates* reader, const tc_pki_tree_workspace* tree,
    tc_cms_certificate_choice* out);

typedef struct {
  tc_cms_collection collection;
  const tc_pki_record_source* external;
} tc_cms_revocations;
typedef enum { TC_CMS_REVOCATION_CRL, TC_CMS_REVOCATION_OTHER } tc_cms_revocation_kind;
typedef struct {
  TC_bytes encoded;
  tc_cms_revocation_kind kind;
} tc_cms_revocation_choice;
/* Embedded [1] RevocationInfoChoices followed by external DER CRL records.
 * OtherRevocationInfoFormat receives its OID/value schema check. CRL schema,
 * signature, scope, freshness and trust checks follow selection.
 * Shared record/byte/work bounds and borrowed-storage rules match candidates.
 * Source snapshots and bytes remain stable. Callers check source-record overlap
 * with scratch before parsing. Reader/out change only on OK; work is provisional. */
TC_TLV_result tc_cms_revocations_init(TC_bytes embedded, const tc_pki_record_source* external,
    size_t max_records, size_t max_bytes, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_cms_revocations* out);
TC_TLV_result tc_cms_revocations_next(tc_cms_revocations* reader,
    const tc_pki_tree_workspace* tree, tc_cms_revocation_choice* out);

/* Decode CRL headers and CRL-level extensions once into caller-owned records.
 * Other revocation formats consume traversal bounds and increment other_count.
 * Retain policy failures for selection; malformed schemas/source errors stop
 * indexing. Entry extensions are checked later during authenticated processing.
 * Index spans borrow stable record bytes. Checks disjoint records, OID/tree
 * scratch, work and output against input/metadata and returned source records.
 * Callers guard any additional writable storage used by subsequent stages.
 * Index/OID scratch and work are provisional; reader and out remain unchanged
 * on failure. Empty input permits NULL/0 storage. No signatures are checked. */
TC_TLV_result tc_cms_crl_index_init(const tc_cms_revocations* reader,
    const tc_pki_tree_workspace* tree, TC_bytes* oids, size_t oid_capacity,
    TC_X509_crl_record* records, size_t capacity, TC_X509_crl_index* out);
typedef struct {
  const TC_bytes* certificates;
  size_t count;
  const TC_X509_store_source* external;
} tc_cms_path_source;
/* Index X.509 choice encodings from an initialized candidate iterator. Other
 * certificate choices still consume record/byte limits but need no index slot.
 * Certificate schemas are checked when path discovery reads a candidate.
 * Each external candidate is fetched once. Anchors retain external indexing.
 * Index spans borrow record bytes; keep them, context and external snapshot
 * stable until all path results expire. Inputs, index, context and outputs are
 * disjoint from each other and tree scratch/work. Index is provisional on
 * failure; candidates/context/out remain unchanged. */
TC_TLV_result tc_cms_path_source_init(const tc_cms_candidates* candidates,
    const tc_pki_tree_workspace* tree, TC_bytes* index, size_t capacity,
    tc_cms_path_source* context, TC_X509_store_source* out);
/* Yield each matching X.509 candidate; other certificate formats are skipped.
 * scratch holds the parsed candidate on OK and may change on any result.
 * Reader advances on OK/END; out changes only on OK. Other failures preserve
 * reader/out, but consume work and may change callback state. No trust checks. */
TC_TLV_result tc_cms_signer_candidate_next(tc_cms_candidates* reader,
    const tc_cms_signer_info* signer, TC_TLV_profile profile,
    const TC_X509_name_workspace* names, const tc_pki_tree_workspace* tree,
    TC_X509_workspace* parser, TC_X509_certificate* scratch, TC_bytes* out);
/* Match the signer ID, authenticate the supplied content digest, and find a
 * valid path to an explicit source anchor. Reuse one digest across candidates.
 * Path options control time, usage, policies and signature providers. Embedded
 * intermediates must be present in path_source; revocation is separate.
 * Caller checks all input/metadata/source spans against scratch, work and out.
 * Tree, signature and validation may share frames across processing phases.
 * Candidate/source bytes stay stable through result use. Search scratch may
 * change on failure; out changes only on VALID and borrows the selected path.
 * Work includes all candidate attempts; the candidate iterator is unchanged. */
TC_X509_path_status tc_cms_signer_find(const tc_cms_candidates* candidates,
    const TC_CMS_signer_info* signer, TC_bytes content_type, TC_bytes digest,
    TC_CMS_attribute_encoding encoding, const TC_X509_store_source* path_source,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_CMS_signature_workspace* signature, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, TC_X509_search_result* out);
/* Shared parsed-certificate iterator. NULL filter yields every X.509 record.
 * Filters consume bounded work, return
 * OK with matched=0/1, and leave the certificate/limits unchanged. Context
 * must be disjoint from parser scratch, reader, output and work. */
TC_TLV_result tc_cms_x509_candidate_next(tc_cms_candidates* reader,
    tc_pki_candidate_filter filter, const void* context, const tc_pki_tree_workspace* tree,
    TC_X509_workspace* parser, TC_X509_certificate* scratch, TC_bytes* out);
/* Select CRL signer candidates by subject, authority hints and cRLSign usage.
 * Same iterator/storage contract as above. Signature, path, scope and freshness
 * checks remain separate. Unsupported authority-name matching is reported. */
TC_TLV_result tc_cms_crl_signer_candidate_next(tc_cms_candidates* reader,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const TC_X509_name_workspace* names, const tc_pki_tree_workspace* tree,
    TC_X509_workspace* parser, TC_X509_certificate* scratch, TC_bytes* out);
/* Find a candidate with a valid CRL signature and path to the selected anchor.
 * Candidates remain unchanged; failed candidate paths do not end the search.
 * Unresolved limits/algorithms are retained if no candidate succeeds. Malformed
 * candidate records and source read errors stop iteration.
 * path_source must provide needed intermediates, including embedded ones, and
 * retain the target certificate's anchor snapshot. Scope/freshness and signer
 * revocation are separate. Parsed inputs/source data are stable and disjoint
 * from all scratch/work/out; tree and validation may share frame storage.
 * Work covers all attempts, out changes only on VALID and borrows path storage. */
TC_X509_path_status tc_cms_crl_signer_find(const tc_cms_candidates* candidates,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    TC_X509_search_result* out);
/* Process selected CRLs, retrying proposed signers with the same bounded search.
 * Subject, authority hints and cRLSign filter candidates before full processing.
 * END means terminal evidence or no new eligible reasons. INVALID means no
 * candidate succeeded; unresolved limits/algorithms retain their own status.
 * Source/record errors stop search. Evidence/out change only on OK; candidates
 * stay unchanged, work covers all attempts. Same disjoint/stable storage rules
 * as signer_find, with evidence also separate from inputs and scratch.
 * CRL selection and signer-path revocation remain separate. */
TC_TLV_result tc_cms_crl_process(const tc_cms_candidates* candidates,
    const tc_x509_crl_selected* selected, const tc_x509_crl_query* query,
    const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    tc_x509_crl_evidence* evidence, TC_X509_search_result* out);
/* Process one indexed complete CRL with signer retry and explicit delta policy.
 * Authenticate the base/path once per signer attempt, choose a current signed
 * delta, then apply entries. REQUIRED returns END if no usable delta is found;
 * IF_AVAILABLE falls back to the complete CRL, which must itself be current.
 * Same borrowed/disjoint storage and result rules as crl_process. Index and
 * candidates stay unchanged. Signer-path revocation remains separate. */
TC_TLV_result tc_cms_crl_index_process(const tc_cms_candidates* candidates,
    const TC_X509_crl_index* index, size_t base, TC_X509_crl_delta_policy delta_policy,
    const tc_x509_crl_query* query, const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    tc_x509_crl_evidence* evidence, TC_X509_search_result* out);
/* Process all indexed scopes for one distribution point. check is required.
 * OK publishes new evidence, which may still have incomplete reason coverage.
 * END means no contribution or terminal input evidence. Failures preserve
 * evidence; scratch is provisional. Invalid/unsupported alternatives are tried
 * until status is determined, otherwise their failure is returned. LIMIT and
 * source errors stop processing. Inputs/source snapshot remain stable.
 * Records sharing issuer and IDP are ranked across validated signer keys.
 * This internal runner does not itself implement the callback's revocation work. */
TC_TLV_result tc_cms_crl_point_process(const tc_cms_candidates* candidates,
    const TC_X509_crl_index* index, TC_X509_crl_delta_policy delta_policy,
    TC_X509_crl_order_policy order_policy, const tc_x509_crl_query* query,
    const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    uint8_t* states, size_t capacity, const tc_x509_crl_path_check* check,
    tc_x509_crl_evidence* evidence);
/* Process an encoded CRLDistributionPoints value, then query->point as fallback
 * if coverage is incomplete. An absent value uses only the fallback. The caller
 * supplies the target's validated CA flag and an issuer-wide fallback point.
 * The whole list is checked before processing. Same storage, callback and
 * transactional evidence rules as point_process; one budget covers all points. */
TC_TLV_result tc_cms_crl_points_process(const tc_cms_candidates* candidates,
    const TC_X509_crl_index* index, TC_X509_crl_delta_policy delta_policy,
    TC_X509_crl_order_policy order_policy, const tc_x509_crl_query* query,
    TC_bytes points, const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    uint8_t* states, size_t capacity, const tc_x509_crl_path_check* check,
    tc_x509_crl_evidence* evidence);
/* Read BasicConstraints, CRLDistributionPoints and issuerAltName from a target
 * whose path has already been validated to anchor_index. Try listed points,
 * then issuer DN and alternative names while coverage remains incomplete.
 * Extension uniqueness/framing and relevant values are checked here; other
 * extension policy belongs to path validation. Same rules as points_process. */
TC_TLV_result tc_cms_crl_certificate_process(const tc_cms_candidates* candidates,
    const TC_X509_crl_index* index, TC_X509_crl_delta_policy delta_policy,
    TC_X509_crl_order_policy order_policy, const TC_X509_certificate* certificate,
    const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    uint8_t* states, size_t capacity, const tc_x509_crl_path_check* check,
    tc_x509_crl_evidence* evidence);
typedef struct {
  const tc_cms_candidates* candidates;
  const TC_X509_crl_index* index;
  const TC_X509_store_source* source;
  const TC_X509_path_options* options;
  size_t anchor_index;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} tc_cms_crl_resolution;
/* Resolve a previously validated target's revocation and signer dependencies.
 * options specify CRL signer policy, not the target holder's EKU/purpose.
 * Sources, policy and time stay fixed. Nodes borrow certificate encodings and
 * are provisional scratch. One shared budget covers all retries; no recursion.
 * OK publishes a determined status. Ungrounded cycles/missing evidence return
 * UNSUPPORTED. Every failure preserves out. All input/output/scratch disjoint. */
TC_TLV_result tc_cms_crl_resolve(const TC_X509_certificate* target,
    const tc_cms_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace,
    tc_x509_crl_evidence* out);
/* Check a previously validated path, anchor-issued first, target last.
 * The anchor is excluded. Hold chain spans outside search/validation scratch.
 * A revoked member supplies its index and evidence; an unrevoked path reports
 * SIZE_MAX and zero evidence. All members share one budget and source snapshot.
 * Nodes retain proven dependencies across members, but not across calls; capacity
 * must cover the path and all distinct signer dependencies visited during it.
 * Resolution policy and disjoint storage rules match tc_cms_crl_resolve.
 * Failure preserves out; OK reports REVOKED or UNREVOKED, never UNDETERMINED. */
TC_TLV_result tc_cms_crl_path_resolve(const TC_bytes* chain, size_t count,
    const tc_cms_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace,
    TC_X509_revocation_result* out);
/* Find a trusted signer for the reference CRL, then select/apply its scope.
 * The reference may be complete or delta; its signature identifies the signer.
 * Scopes with no new reason coverage return END before signer discovery.
 * State storage needs one byte per indexed CRL and is reset for each signer.
 * Reference verification is reused during selection. Evidence/out change only
 * on OK; states and other scratch are provisional. Preflight checks metadata,
 * used input spans and writable ranges; source records are guarded on return.
 * Opaque source/provider contexts must remain separate from writable storage.
 * Tree/path frames may share an array; partial overlaps are rejected.
 * Candidate/index views stay unchanged. Signer-path revocation is separate. */
TC_TLV_result tc_cms_crl_scope_process(const tc_cms_candidates* candidates,
    const TC_X509_crl_index* index, size_t reference, TC_X509_crl_delta_policy delta_policy,
    TC_X509_crl_order_policy order_policy, const tc_x509_crl_query* query,
    const TC_X509_store_source* path_source, size_t anchor_index,
    const TC_X509_path_options* options, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    uint8_t* states, size_t capacity, tc_x509_crl_evidence* evidence, TC_X509_search_result* out);
#endif
