/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_CRL_INTERNAL_H_
#define TC_X509_CRL_INTERNAL_H_
#include <tiny_crypto/x509_revocation.h>
#include "pki_tree_internal.h"
#include "pki_names_internal.h"
#include "pki_distribution_internal.h"
#include "pki_crl_reason_internal.h"
#include "x509_time_internal.h"

/* Borrowed path state shared by CRL signer searches and dependency resolution.
 * tree->work is the operation's work budget. options validates CRL signer
 * paths. time sets CRL freshness. */
typedef struct {
  const TC_X509_store_source* source;
  size_t anchor_index;
  const TC_X509_path_options* options;
  const tc_pki_tree_workspace* tree;
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
  const TC_X509_revocation_time* time;
} tc_x509_crl_trust;

/* Decoding resources shared by CRL entry, scope and evidence checks. tree
 * holds the frames and work budget. oids holds up to oid_capacity extension
 * OIDs for duplicate detection and may be NULL/0 where no extensions are read.
 * All storage is caller scratch and may change on failure. */
typedef struct {
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  const TC_X509_name_workspace* names;
  TC_bytes* oids;
  size_t oid_capacity;
} tc_x509_crl_decode;

/* Compare authenticated signed content, including source-backed records. */
TC_TLV_result tc_x509_crl_content_equal(const TC_X509_crl* left, const TC_X509_crl* right,
                                        size_t* work, int* equal);
typedef struct {
  TC_bytes algorithm, signature, version, inner_algorithm, issuer;
  TC_bytes this_update, next_update, extensions;
} tc_x509_crl_fields;

/* Validate bounded encoded metadata. Returned spans borrow fields. Entry syntax,
 * complete signed bytes and authentication are handled by the enclosing reader. */
TC_TLV_result tc_x509_crl_metadata_read(const tc_x509_crl_fields* fields,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree, TC_X509_crl* out);
typedef struct {
  TC_bytes serial, extensions;
  TC_X509_time revoked_at;
  int serial_negative;
} tc_x509_crl_entry;

/* Freshness of one CRL under the shared revocation time rule (RFC 5280
 * section 6.3.3 (a)(2) with the skew and age bounds of
 * TC_X509_revocation_time). Missing nextUpdate never establishes freshness.
 * Invalid dates or a reversed interval return INVALID. Output changes only
 * on OK. Inputs/output are disjoint. */
TC_TLV_result tc_x509_crl_fresh_at(const TC_X509_crl* crl, const TC_X509_revocation_time* time,
                                   tc_x509_freshness* out);
/* RFC 5280 6.3.3(f): KeyUsage, when present, must permit cRLSign.
 * The certificate is already parsed. cA, signer trust, critical extensions and
 * the path are validated separately. Input, work and output are disjoint.
 * Output changes only on OK. */
TC_TLV_result tc_x509_crl_signer_usage(const TC_X509_certificate* signer,
                                       const TC_TLV_limits* limits, size_t* work, int* authorized);
/* Verify issuer linkage and signature directly with the selected trust anchor.
 * Uses the anchor key's algorithm restrictions. Scope, freshness and CRL policy
 * remain separate.
 * Parsed inputs/provider state must be disjoint from scratch and work. */
TC_X509_signature_result
tc_x509_crl_anchor_check(const TC_X509_crl* crl, const TC_X509_trust_anchor* anchor,
                         const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
                         const TC_X509_name_workspace* names, size_t* work);
/* Check issuer/subject linkage, cRLSign and the CRL signature using signer's
 * SPKI. Signer path, AKID binding, CRL scope and freshness are separate checks.
 * Parsed inputs/provider context must be disjoint from workspace and work. */
TC_X509_signature_result
tc_x509_crl_signer_check(const TC_X509_crl* crl, const TC_X509_certificate* signer,
                         const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
                         const TC_X509_name_workspace* names, size_t* work);
/* Verify the CRL signature and build its signer's path to the selected anchor
 * from the same held source snapshot as the certificate path. options contain
 * signer policy, including the signer's EKU/purpose. cRLSign is added to
 * required usage. A v3 signer certificate must carry keyUsage with cRLSign
 * (RFC 10007 section 4). v1 and v2 signers skip the keyUsage check.
 * CRL scope/freshness and signer revocation are separate. Parsed signer metadata
 * must match its unchanged encoded bytes. All inputs, scratch, work and output
 * are disjoint. Work is shared/consumed on failure. out changes only on VALID.
 * Result spans borrow inputs/path workspace. anchor_index uses source indexing. */
TC_X509_path_status tc_x509_crl_signer_validate(const TC_X509_crl* crl,
                                                const TC_X509_certificate* signer,
                                                const tc_x509_crl_trust* trust,
                                                TC_X509_search_report* out);

/* Parse into provisional storage after the caller has checked overlap. */
TC_TLV_result tc_x509_crl_record_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                      const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                      size_t oid_capacity, TC_X509_crl_record* record);
enum {
  TC_CRL_ENTRY_REASON = 1u << 0,
  TC_CRL_ENTRY_INVALIDITY = 1u << 1,
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
  const TC_X509_crl_extensions* extensions;
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
                                        const TC_X509_crl_extensions* extensions,
                                        const tc_x509_crl_entry_issuer* issuer,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                        size_t capacity, tc_x509_crl_revoked_entry* out);
/* Policy-aware iteration with issuer inheritance. Default issuer.name is a
 * borrowed DER Name; explicit issuer.names holds GeneralNames contents instead.
 * CRL bytes and extension metadata must remain unchanged while views are in use.
 * Parsed inputs and writable storage are disjoint. */
TC_TLV_result tc_x509_crl_revoked_init(const TC_X509_crl* crl,
                                       const TC_X509_crl_extensions* extensions,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       tc_x509_crl_revoked_reader* out);
/* Reader and output are unchanged on failure or END. Work/OID scratch and
 * frames are provisional. An explicit issuer must contain a directoryName. */
TC_TLV_result tc_x509_crl_revoked_next(tc_x509_crl_revoked_reader* reader,
                                       const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                       size_t capacity, tc_x509_crl_revoked_entry* out);
/* Accumulate one query match. Duplicate issuer/serial entries are invalid. */
TC_TLV_result tc_x509_crl_match_update(const tc_x509_crl_revoked_entry* entry,
                                       const TC_X509_crl_target* query, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names,
                                       TC_X509_crl_match* match);
typedef struct {
  const TC_X509_crl* base;
  const TC_X509_crl_extensions* base_info;
  const TC_X509_crl* delta;
  const TC_X509_crl_extensions* delta_info;
} tc_x509_crl_selected;
/* Combine lookups from an already validated compatible pair. delta may be NULL.
 * A matching delta overrides the base, including removeFromCRL. A cleared or
 * absent match still needs complete reason coverage before good status is known.
 * Dates are reported unchanged. Historical-status interpretation belongs to the caller.
 * Input/output are disjoint. Output changes only on OK. */
TC_TLV_result tc_x509_crl_combine(const TC_X509_crl_match* base, const TC_X509_crl_match* delta,
                                  TC_X509_crl_match* out);
/* Scan all entries and reject duplicate issuer/serial matches. A missing match
 * supports good status only after signature, scope and freshness checks.
 * Parsed/disjoint inputs, provisional scratch/work, output only on OK. */
TC_TLV_result tc_x509_crl_find(const TC_X509_crl* crl, const TC_X509_crl_extensions* extensions,
                               const TC_X509_certificate* certificate,
                               const tc_x509_crl_decode* decode, TC_X509_crl_match* out);
/* Base/delta pairing (5.2.4, 6.3.3(c)): issuer, IDP, AKID and number range.
 * AKIDs may be absent on both sides. Signatures must still bind both CRLs to
 * the same validated key. Freshness is checked separately. Parsed/disjoint input
 * contract. compatible changes only on OK. Scratch/work are provisional. */
TC_TLV_result tc_x509_crl_delta_compatible(const TC_X509_crl* base,
                                           const TC_X509_crl_extensions* base_info,
                                           const TC_X509_crl* delta,
                                           const TC_X509_crl_extensions* delta_info,
                                           const tc_x509_crl_decode* decode, int* compatible);
/* Compare issuer names and issuingDistributionPoint encodings/presence.
 * Key identity, authority hints, extension policy and freshness are separate.
 * Parsed/disjoint inputs. equal changes only on OK. Scratch/work are provisional. */
TC_TLV_result
tc_x509_crl_scope_equal(const TC_X509_crl* left, const TC_X509_crl_extensions* left_info,
                        const TC_X509_crl* right, const TC_X509_crl_extensions* right_info,
                        const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
                        const TC_X509_name_workspace* names, int* equal);
/* Compare validated nonnegative DER INTEGER contents, including sign padding.
 * Output changes only on OK; work is consumed on failure. */
TC_TLV_result tc_x509_crl_number_compare(TC_bytes left, TC_bytes right, size_t* work, int* order);
/* Entry extension metadata (RFC 5280 5.3). Missing reason defaults to
 * unspecified (0), and present distinguishes an absent value. Issuer borrows
 * GeneralNames contents. CRL-level extensions are unrecognized in this scope
 * and are never decoded. Output changes only on OK. OID scratch/work are
 * provisional. */
TC_TLV_result tc_x509_crl_entry_info_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                          const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                          size_t capacity, tc_x509_crl_entry_info* out);
/* Entry criticality and CRL-type rules. Unknown critical extensions return
 * UNSUPPORTED. Issuer-name validation/inheritance is a separate step. */
TC_TLV_result tc_x509_crl_entry_policy(const TC_X509_crl_extensions* crl,
                                       const tc_x509_crl_entry_info* entry);
/* Decode CRL-level extensions (RFC 5280 5.2) in one pass, retaining presence
 * and criticality. Entry extensions are unrecognized in this scope and are
 * never decoded. The first unrecognized critical OID is reported. Spans are
 * borrowed. Output changes only on OK. OID scratch/work are provisional. */
TC_TLV_result tc_x509_crl_extension_info_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                              const tc_pki_tree_workspace* tree, TC_bytes* oids,
                                              size_t capacity, TC_X509_crl_extensions* out);
/* Processing rules for decoded extensions. Unknown critical extensions return
 * UNSUPPORTED. Wrong criticality or a delta carrying FreshestCRL returns INVALID.
 * AKID and CRLNumber may be absent here. Delta compatibility, key binding and
 * entry policy remain separate. */
TC_TLV_result tc_x509_crl_extension_policy(const TC_X509_crl_extensions* info);
/* DER IssuingDistributionPoint. Absent name/reasons leave that dimension unrestricted.
 * Returned name spans borrow input. Scope matching and criticality are separate. */
TC_TLV_result tc_x509_crl_distribution_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                            const tc_pki_tree_workspace* tree,
                                            TC_X509_crl_distribution* out);

/* RFC 5280 6.3.3(b)(1), issuer linkage only.
 * Inputs are parsed views. A missing IDP is NULL. All writable storage is
 * disjoint from inputs. matched changes only on OK. Scratch/work are provisional. */
TC_TLV_result tc_x509_crl_issuer_matches(const TC_X509_crl* crl,
                                         const TC_X509_crl_distribution* idp,
                                         const tc_pki_distribution_point* point,
                                         TC_bytes certificate_issuer, const TC_TLV_limits* limits,
                                         const tc_pki_tree_workspace* tree,
                                         const TC_X509_name_workspace* names, int* matched);
/* Distribution-name part of scope matching (6.3.3(b)(2)(i)). Same storage rules.
 * An empty point matches the certificate issuer DN as an issuer-wide fallback.
 * Issuer alternative names require a separate fullName point.
 * Issuer linkage, certificate type and reason coverage must also be checked. */
TC_TLV_result tc_x509_crl_name_matches(const TC_X509_crl* crl, const TC_X509_crl_distribution* idp,
                                       const tc_pki_distribution_point* point,
                                       TC_bytes certificate_issuer, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names, int* matched);

typedef struct {
  const TC_X509_certificate* certificate;
  const tc_pki_distribution_point* point;
  int certificate_ca;
} tc_x509_crl_query;
/* Apply an authenticated pair: effective freshness/scope, entry lookup and
 * reason coverage. Caller has checked pair compatibility, both signatures and
 * signer trust. Those checks are not repeated. Signer-path revocation is separate.
 * Parsed inputs are stable. Scratch/evidence are disjoint. Evidence changes only on
 * OK. Work and scratch are provisional. END means no new eligible coverage. */
TC_TLV_result tc_x509_crl_apply(const tc_x509_crl_selected* selected,
                                const tc_x509_crl_query* query, const TC_X509_revocation_time* time,
                                const tc_x509_crl_decode* decode, TC_X509_crl_evidence* evidence);
/* Zero-initialize evidence. Add only selected, authenticated, current and
 * applicable CRLs. Combine base/delta results first. Only newly covered reasons
 * contribute (RFC 5280 6.3.3(e)). END means status is already determined.
 * Errors leave evidence unchanged. Match may alias evidence.revocation. */
TC_TLV_result tc_x509_crl_evidence_add(TC_X509_crl_evidence* evidence, uint16_t reasons,
                                       const TC_X509_crl_match* match);
/* Partial coverage without a revocation remains UNDETERMINED.
 * Input/output are disjoint. Output changes only on OK. */
TC_TLV_result tc_x509_crl_evidence_status(const TC_X509_crl_evidence* evidence,
                                          TC_X509_revocation_status* out);
/* Compare reason coverage and reported revocation fields, ignoring unused dates.
 * Inputs are parsed evidence. equal changes only on OK. */
TC_TLV_result tc_x509_crl_evidence_equal(const TC_X509_crl_evidence* left,
                                         const TC_X509_crl_evidence* right, int* equal);
/* Combine issuer/name linkage, public-key certificate type and reason masks.
 * certificate_ca is the validated basicConstraints cA value (0 if absent).
 * Zero means no coverage. Signature, freshness and trust checks are separate.
 * Parsed/disjoint input contract above applies. Output changes only on OK. */
TC_TLV_result tc_x509_crl_scope_reasons(const TC_X509_crl* crl, const TC_X509_crl_distribution* idp,
                                        const tc_pki_distribution_point* point,
                                        TC_bytes certificate_issuer, int certificate_ca,
                                        const tc_x509_crl_decode* decode, uint16_t* out);
typedef struct {
  tc_x509_freshness freshness;
  uint16_t reasons;
} tc_x509_crl_coverage;
/* Eligible reasons at the requested time, after extension-policy and scope
 * checks. Noncurrent CRLs contribute zero. Authenticate the CRL before adding
 * these reasons to coverage. A delta also needs a validated compatible base.
 * Parsed inputs/scratch/out are disjoint. Output changes only on OK. */
TC_TLV_result tc_x509_crl_coverage_at(const TC_X509_crl* crl,
                                      const TC_X509_crl_extensions* extensions,
                                      const TC_X509_revocation_time* time,
                                      const tc_pki_distribution_point* point,
                                      TC_bytes certificate_issuer, int certificate_ca,
                                      const tc_x509_crl_decode* decode, tc_x509_crl_coverage* out);

/* Scalar outputs change only on OK. Input/output must be disjoint.
 * Nonnegative INTEGER contents include sign padding, with no width truncation. */
TC_TLV_result tc_x509_crl_number_read(TC_bytes encoded, TC_bytes* out);
TC_TLV_result tc_x509_crl_invalidity_date_read(TC_bytes encoded, TC_X509_time* out);

/* DER CertificateList and revoked-entry fields. Extension collections retain
 * their SEQUENCE encodings for separate validation.
 * Input and writable ranges must be disjoint. Outputs change only on OK. */
TC_TLV_result tc_x509_crl_read(TC_bytes input, const TC_TLV_limits* limits,
                               const tc_pki_tree_workspace* tree, TC_X509_crl* out);
/* Initialize from crl.revoked. The absent list produces an empty reader. */
TC_TLV_result tc_x509_crl_entries_init(TC_bytes encoded, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, TC_TLV_reader* out);
/* Reader and out are unchanged on failure or END. Work/scratch are provisional. */
TC_TLV_result tc_x509_crl_entry_next(TC_TLV_reader* reader, unsigned version,
                                     const tc_pki_tree_workspace* tree, tc_x509_crl_entry* out);

/* Match a revoked entry against a serial and issuer query. */
TC_TLV_result tc_x509_crl_query_matches(const tc_x509_crl_revoked_entry* entry,
                                        const TC_X509_crl_target* certificate,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree,
                                        const TC_X509_name_workspace* names, int* matched);
/* Verify a selected CRL pair's signatures under signer. */
TC_TLV_result tc_x509_crl_selected_authenticate(const tc_x509_crl_selected* selected,
                                                const TC_X509_certificate* signer,
                                                const TC_X509_signature_provider* provider,
                                                const tc_x509_crl_decode* decode);
/* Look up certificate in an authenticated selected CRL pair. */
TC_TLV_result tc_x509_crl_selected_lookup(const tc_x509_crl_selected* selected,
                                          const TC_X509_certificate* certificate,
                                          const tc_x509_crl_decode* decode, TC_X509_crl_match* out);
/* The CRL signer key after issuer and usage checks. */
TC_TLV_result tc_x509_crl_signer_key(const TC_X509_crl* crl, const TC_X509_certificate* signer,
                                     const TC_TLV_limits* limits,
                                     const TC_X509_name_workspace* names, size_t* work,
                                     TC_X509_public_key* key);
/* Verify a CRL signature over a supplied digest. */
TC_X509_signature_result tc_x509_crl_digest_signature(const TC_X509_crl* crl,
                                                      TC_hash_algorithm hash, TC_bytes digest,
                                                      const TC_X509_public_key* key,
                                                      const TC_X509_signature_provider* provider,
                                                      size_t* work);
#endif
