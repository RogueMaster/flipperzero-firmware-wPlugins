/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_CRL_INTERNAL_H_
#define TC_X509_CRL_INTERNAL_H_
#include <tiny_crypto/x509_revocation.h>
#include "pki_tree_internal.h"
#include "pki_names_internal.h"
#include "pki_distribution_internal.h"

typedef TC_X509_crl tc_x509_crl;
/* Compare authenticated signed content, including source-backed records. */
TC_TLV_result tc_x509_crl_content_equal(const tc_x509_crl* left,
    const tc_x509_crl* right, size_t* work, int* equal);
typedef struct {
  TC_bytes algorithm, signature, version, inner_algorithm, issuer;
  TC_bytes this_update, next_update, extensions;
} tc_x509_crl_fields;

/* Validate bounded encoded metadata. Returned spans borrow fields; entry syntax,
 * complete signed bytes and authentication are handled by the enclosing reader. */
TC_TLV_result tc_x509_crl_metadata_read(const tc_x509_crl_fields* fields,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, tc_x509_crl* out);
typedef struct {
  TC_bytes serial, extensions;
  TC_X509_time revoked_at;
  int serial_negative;
} tc_x509_crl_entry;

typedef enum {
  TC_X509_CRL_CURRENT, TC_X509_CRL_FUTURE, TC_X509_CRL_STALE,
  TC_X509_CRL_NO_NEXT_UPDATE
} tc_x509_crl_freshness;
/* Current means thisUpdate <= at < nextUpdate, with no implicit clock skew.
 * Missing nextUpdate never establishes freshness. Invalid dates or a reversed
 * interval return INVALID. Output changes only on OK; inputs/output disjoint. */
TC_TLV_result tc_x509_crl_fresh_at(const tc_x509_crl* crl,
    const TC_X509_time* at, tc_x509_crl_freshness* out);
/* RFC 5280 6.3.3(f): KeyUsage, when present, must permit cRLSign.
 * Does not require cA or establish signer trust. Certificate is already parsed;
 * its critical extensions/path must be validated separately. Input, work and
 * output are disjoint; output changes only on OK. */
TC_TLV_result tc_x509_crl_signer_usage(const TC_X509_certificate* signer,
    const TC_TLV_limits* limits, size_t* work, int* authorized);
/* Verify issuer linkage and signature directly with the selected trust anchor.
 * Uses the anchor key's algorithm restrictions. Scope, freshness and CRL policy
 * remain separate. This authenticates this CRL, not a signer certificate.
 * Parsed inputs/provider state must be disjoint from scratch and work. */
TC_X509_signature_result tc_x509_crl_anchor_check(const tc_x509_crl* crl,
    const TC_X509_trust_anchor* anchor, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work);
/* Check issuer/subject linkage, cRLSign and the CRL signature using signer's
 * SPKI. Signer path, AKID binding, CRL scope and freshness are separate checks.
 * Parsed inputs/provider context must be disjoint from workspace and work. */
TC_X509_signature_result tc_x509_crl_signer_check(const tc_x509_crl* crl,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work);
/* Digest covers the exact source TBS encoding. Check issuer linkage, cRLSign,
 * algorithm/key restrictions and signature; signer trust and CRL scope follow. */
TC_X509_signature_result tc_x509_crl_signer_digest_check(const tc_x509_crl* crl,
    TC_hash_algorithm hash, TC_bytes digest, const TC_X509_certificate* signer,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* names, size_t* work);
/* Verify the CRL signature and build its signer's path to the selected anchor
 * from the same held source snapshot as the certificate path. options contain
 * signer policy, not the certificate holder's EKU/purpose. cRLSign is added to
 * required usage; absent KU remains allowed unless explicitly required.
 * No CRL scope/freshness or signer revocation check. Parsed signer metadata
 * must match its unchanged encoded bytes. All inputs, scratch, work and output
 * are disjoint. Work is shared/consumed on failure; out changes only on VALID.
 * Result spans borrow inputs/path workspace; anchor_index uses source indexing. */
TC_X509_path_status tc_x509_crl_signer_validate(const tc_x509_crl* crl,
    const TC_X509_certificate* signer, const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work, TC_X509_search_result* out);

typedef TC_X509_crl_distribution tc_x509_crl_distribution;
enum {
  TC_CRL_EXT_NUMBER = TC_X509_CRL_EXT_NUMBER, TC_CRL_EXT_DELTA = TC_X509_CRL_EXT_DELTA,
  TC_CRL_EXT_AUTHORITY = TC_X509_CRL_EXT_AUTHORITY, TC_CRL_EXT_DISTRIBUTION = TC_X509_CRL_EXT_DISTRIBUTION,
  TC_CRL_EXT_FRESHEST = TC_X509_CRL_EXT_FRESHEST, TC_CRL_EXT_ISSUER_ALT = TC_X509_CRL_EXT_ISSUER_ALT
};
typedef TC_X509_crl_extensions tc_x509_crl_extension_info;
/* Parse into provisional storage after the caller has checked overlap. */
TC_TLV_result tc_x509_crl_record_read(TC_bytes encoded, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_bytes* oids, size_t oid_capacity,
    TC_X509_crl_record* record);
enum {
  TC_CRL_ENTRY_REASON = 1u << 0, TC_CRL_ENTRY_INVALIDITY = 1u << 1,
  TC_CRL_ENTRY_ISSUER = 1u << 2
};
typedef struct {
  TC_bytes issuer, unknown_critical_oid;
  TC_X509_time invalidity_date;
  unsigned reason, present, critical;
} tc_x509_crl_entry_info;
typedef struct {
  TC_bytes name, names;
} tc_x509_crl_entry_issuer;
typedef struct {
  TC_TLV_reader entries;
  const tc_x509_crl_extension_info* extensions;
  tc_x509_crl_entry_issuer issuer;
  unsigned version;
} tc_x509_crl_revoked_reader;
typedef struct {
  tc_x509_crl_entry entry;
  tc_x509_crl_entry_info extensions;
  tc_x509_crl_entry_issuer issuer;
} tc_x509_crl_revoked_entry;
/* Resolve entry extensions and issuer inheritance after entry syntax checks. */
TC_TLV_result tc_x509_crl_entry_resolve(const tc_x509_crl_entry* entry,
    const tc_x509_crl_extension_info* extensions, const tc_x509_crl_entry_issuer* issuer,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_revoked_entry* out);
/* Policy-aware iteration with issuer inheritance. Default issuer.name is a
 * borrowed DER Name; explicit issuer.names holds GeneralNames contents instead.
 * CRL bytes and extension metadata must remain unchanged while views are in use.
 * Parsed inputs and writable storage are disjoint. */
TC_TLV_result tc_x509_crl_revoked_init(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_x509_crl_revoked_reader* out);
/* Reader and output are unchanged on failure or END. Work/OID scratch and
 * frames are provisional. An explicit issuer must contain a directoryName. */
TC_TLV_result tc_x509_crl_revoked_next(tc_x509_crl_revoked_reader* reader,
    const tc_pki_tree_workspace* tree, TC_bytes* oids, size_t capacity,
    tc_x509_crl_revoked_entry* out);
/* Match parsed serial/issuer fields against a resolved entry. Explicit issuer
 * DNs require the certificate's encoding (5.3.3); the default issuer uses Name
 * comparison. Output changes only on OK; inputs and scratch are disjoint. */
TC_TLV_result tc_x509_crl_entry_matches(const tc_x509_crl_revoked_entry* entry,
    const TC_X509_certificate* certificate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched);
typedef TC_X509_crl_match tc_x509_crl_match;
typedef TC_X509_crl_target tc_x509_crl_serial_query;
/* Accumulate one query match; duplicate issuer/serial entries are invalid. */
TC_TLV_result tc_x509_crl_match_update(const tc_x509_crl_revoked_entry* entry,
    const tc_x509_crl_serial_query* query, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    tc_x509_crl_match* match);
typedef struct {
  const tc_x509_crl* base;
  const tc_x509_crl_extension_info* base_info;
  const tc_x509_crl* delta;
  const tc_x509_crl_extension_info* delta_info;
} tc_x509_crl_selected;
/* Check pair compatibility, signatures under one key, and the signer's path to
 * the target anchor. Does not inspect entries, scope or CRL freshness.
 * Signer-path revocation is separate. Parsed inputs and writable storage are
 * disjoint. Work/scratch are provisional; out changes only on OK and borrows
 * the signer path. Source records remain stable for the operation. */
TC_TLV_result tc_x509_crl_selected_validate(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work, TC_X509_search_result* out);
/* Check pairing, authenticate both CRLs with one signer's key, then resolve
 * their entries. Omit both delta pointers for a complete CRL alone. The caller
 * must establish signer trust, freshness and scope before using this result.
 * Parsed inputs and scratch/output are disjoint. Output changes only on OK;
 * work and scratch are consumed on failure. */
TC_TLV_result tc_x509_crl_selected_find(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const TC_X509_certificate* certificate,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, tc_x509_crl_match* out);
/* Combine lookups from an already validated compatible pair; delta may be NULL.
 * A matching delta overrides the base, including removeFromCRL. A cleared or
 * absent match still needs complete reason coverage before good status is known.
 * Dates are retained as reported, not interpreted as a historical-status rule.
 * Input/output disjoint; output changes only on OK. */
TC_TLV_result tc_x509_crl_combine(const tc_x509_crl_match* base,
    const tc_x509_crl_match* delta, tc_x509_crl_match* out);
/* Scan all entries and reject duplicate issuer/serial matches. A missing match
 * does not establish good status; signature, scope and freshness are separate.
 * Parsed/disjoint inputs, provisional scratch/work, output only on OK. */
TC_TLV_result tc_x509_crl_find(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, TC_bytes* oids, size_t capacity,
    tc_x509_crl_match* out);
/* Base/delta pairing (5.2.4, 6.3.3(c)): issuer, IDP, AKID and number range.
 * AKIDs may be absent on both sides; signatures must still bind both CRLs to
 * the same validated key. No freshness check. Parsed/disjoint input contract;
 * compatible changes only on OK, scratch/work are provisional. */
TC_TLV_result tc_x509_crl_delta_compatible(const tc_x509_crl* base,
    const tc_x509_crl_extension_info* base_info, const tc_x509_crl* delta,
    const tc_x509_crl_extension_info* delta_info, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* compatible);
/* Compare issuer names and issuingDistributionPoint encodings/presence.
 * Key identity, authority hints, extension policy and freshness are separate.
 * Parsed/disjoint inputs; equal changes only on OK; scratch/work provisional. */
TC_TLV_result tc_x509_crl_scope_equal(const tc_x509_crl* left,
    const tc_x509_crl_extension_info* left_info, const tc_x509_crl* right,
    const tc_x509_crl_extension_info* right_info, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* equal);
/* Compare validated nonnegative DER INTEGER contents, including sign padding.
 * Output changes only on OK; work is consumed on failure. */
TC_TLV_result tc_x509_crl_number_compare(TC_bytes left, TC_bytes right,
    size_t* work, int* order);
/* Entry extension metadata. Missing reason defaults to unspecified (0);
 * present distinguishes an absent value. Issuer borrows GeneralNames contents.
 * Output changes only on OK; OID scratch/work are provisional. */
TC_TLV_result tc_x509_crl_entry_info_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_entry_info* out);
/* Entry criticality and CRL-type rules. Unknown critical extensions return
 * UNSUPPORTED. Issuer-name validation/inheritance is a separate step. */
TC_TLV_result tc_x509_crl_entry_policy(const tc_x509_crl_extension_info* crl,
    const tc_x509_crl_entry_info* entry);
/* Decode CRL-level extensions in one pass, retaining presence and criticality.
 * Unknown critical OIDs are reported, not accepted as understood. Borrowed spans;
 * output changes only on OK, OID scratch/work are provisional. */
TC_TLV_result tc_x509_crl_extension_info_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_extension_info* out);
/* Processing rules for decoded extensions, not an issuer-conformance audit.
 * Unknown critical extensions return UNSUPPORTED; wrong criticality or a delta
 * carrying FreshestCRL returns INVALID. Absence of AKID/CRLNumber is not rejected
 * here. Delta compatibility, key binding and entry policy remain separate. */
TC_TLV_result tc_x509_crl_extension_policy(const tc_x509_crl_extension_info* info);
/* DER IssuingDistributionPoint. Absent name/reasons leave that dimension unrestricted.
 * Returned name spans borrow input. Scope matching and criticality are separate. */
TC_TLV_result tc_x509_crl_distribution_read(TC_bytes encoded,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_x509_crl_distribution* out);

/* RFC 5280 6.3.3(b)(1), issuer linkage only, not a revocation verdict.
 * Inputs are parsed views; a missing IDP is NULL. All writable storage is
 * disjoint from inputs. matched changes only on OK; scratch/work are provisional. */
TC_TLV_result tc_x509_crl_issuer_matches(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched);
/* Distribution-name part of scope matching (6.3.3(b)(2)(i)). Same storage rules.
 * An empty point matches the certificate issuer DN as an issuer-wide fallback.
 * Issuer alternative names require a separate fullName point.
 * Issuer linkage, certificate type and reason coverage must also be checked. */
TC_TLV_result tc_x509_crl_name_matches(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, int* matched);

typedef TC_X509_revocation_status tc_x509_crl_status;
typedef TC_X509_crl_evidence tc_x509_crl_evidence;
typedef struct {
  const TC_X509_certificate* certificate;
  const tc_pki_distribution_point* point;
  int certificate_ca;
} tc_x509_crl_query;
/* Process selected CRLs for one distribution point and proposed signer.
 * Checks effective freshness/scope, both signatures and the signer path to the
 * target certificate's anchor, then accumulates reasons and resolved entries.
 * A delta supplies the effective update times (RFC 5280 5.2.4).
 * END means no new eligible reasons or an already determined status.
 * options hold signer policy; certificate_ca is the target's validated cA.
 * Signer-path revocation is separate. The source is the target path's held
 * snapshot. Inputs, scratch, work, evidence and out are disjoint; parsed views
 * must match unchanged encodings. Evidence/out change only on OK; scratch/work
 * are provisional. out borrows the signer path and records total work used. */
TC_TLV_result tc_x509_crl_process(const tc_x509_crl_selected* selected,
    const TC_X509_certificate* signer, const tc_x509_crl_query* query,
    const TC_X509_store_source* source, size_t anchor_index,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const TC_X509_search_workspace* search, size_t* work,
    tc_x509_crl_evidence* evidence, TC_X509_search_result* out);
/* Apply an authenticated pair: effective freshness/scope, entry lookup and
 * reason coverage. Caller has checked pair compatibility, both signatures and
 * signer trust. Those checks are not repeated; signer-path revocation is separate.
 * Stable parsed inputs; disjoint scratch/evidence. Evidence changes only on OK;
 * work and scratch are provisional. END means no new eligible coverage. */
TC_TLV_result tc_x509_crl_apply(const tc_x509_crl_selected* selected,
    const tc_x509_crl_query* query, const TC_X509_time* at, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t oid_capacity, tc_x509_crl_evidence* evidence);
/* Zero-initialize evidence. Add only selected, authenticated, current and
 * applicable CRLs; combine base/delta results first. Only newly covered reasons
 * contribute (RFC 5280 6.3.3(e)). END means status is already determined.
 * Errors leave evidence unchanged. Match may alias evidence.revocation. */
TC_TLV_result tc_x509_crl_evidence_add(tc_x509_crl_evidence* evidence,
    uint16_t reasons, const tc_x509_crl_match* match);
/* Partial coverage without a revocation remains UNDETERMINED.
 * Input/output disjoint; output changes only on OK. */
TC_TLV_result tc_x509_crl_evidence_status(const tc_x509_crl_evidence* evidence,
    tc_x509_crl_status* out);
/* Compare reason coverage and reported revocation fields, ignoring unused dates.
 * Inputs are parsed evidence; equal changes only on OK. */
TC_TLV_result tc_x509_crl_evidence_equal(const tc_x509_crl_evidence* left,
    const tc_x509_crl_evidence* right, int* equal);
/* Combine issuer/name linkage, public-key certificate type and reason masks.
 * certificate_ca is the validated basicConstraints cA value (0 if absent).
 * Zero means no coverage. Nonzero is not a signature, freshness or trust verdict.
 * Parsed/disjoint input contract above applies; output changes only on OK. */
TC_TLV_result tc_x509_crl_scope_reasons(const tc_x509_crl* crl,
    const tc_x509_crl_distribution* idp, const tc_pki_distribution_point* point,
    TC_bytes certificate_issuer, int certificate_ca, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names, uint16_t* out);
typedef struct {
  tc_x509_crl_freshness freshness;
  uint16_t reasons;
} tc_x509_crl_coverage;
/* Eligible reasons at the requested time, after extension-policy and scope
 * checks. Noncurrent CRLs contribute zero. Authenticate the CRL before adding
 * these reasons to coverage; a delta also needs a validated compatible base.
 * Parsed inputs/scratch/out are disjoint. Output changes only on OK. */
TC_TLV_result tc_x509_crl_coverage_at(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_time* at,
    const tc_pki_distribution_point* point, TC_bytes certificate_issuer, int certificate_ca,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, tc_x509_crl_coverage* out);

/* Scalar outputs change only on OK; input/output must be disjoint.
 * Nonnegative INTEGER contents include sign padding, with no width truncation. */
TC_TLV_result tc_x509_crl_number_read(TC_bytes encoded, TC_bytes* out);
/* RFC 5280 CRLReason values: 0..6 and 8..10. */
TC_TLV_result tc_x509_crl_reason_read(TC_bytes encoded, unsigned* out);
TC_TLV_result tc_x509_crl_invalidity_date_read(TC_bytes encoded, TC_X509_time* out);

/* DER CertificateList and revoked-entry fields. Extension collections retain
 * their SEQUENCE encodings for separate validation. No trust/freshness checks.
 * Input and writable ranges must be disjoint; outputs change only on OK. */
TC_TLV_result tc_x509_crl_read(TC_bytes input, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, tc_x509_crl* out);
/* Initialize from crl.revoked. The absent list produces an empty reader. */
TC_TLV_result tc_x509_crl_entries_init(TC_bytes encoded, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, TC_TLV_reader* out);
/* Reader and out are unchanged on failure or END; work/scratch are provisional. */
TC_TLV_result tc_x509_crl_entry_next(TC_TLV_reader* reader, unsigned version,
    const tc_pki_tree_workspace* tree, tc_x509_crl_entry* out);
/* Check CRL and entry extension wrappers, embedded DER, and duplicate OIDs.
 * Checks numbers, reasons, dates, AKIDs, issuer names and issuing distribution points.
 * Reuses OID scratch per list. Critical flags, cross-field rules and other
 * extension semantics remain separate from this structural check. */
TC_TLV_result tc_x509_crl_extensions_check(const tc_x509_crl* crl,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity);
#endif
