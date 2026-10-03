/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* X.509 certification path validation against an explicit trust anchor, and
 * path discovery from a certificate store.
 * Standards: RFC 5280 section 6, RFC 5937.
 * Configuration: TC_ENABLE_X509_PATH.
 * Limitations: revocation is in x509_revocation.h.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/x509-path.md. */
#ifndef TINY_CRYPTO_X509_PATH_H_
#define TINY_CRYPTO_X509_PATH_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_store.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned storage elements. Their fields are managed by the validator. */
typedef struct {
  TC_bytes oid;
  size_t depth;
  int alive, mapped;
} TC_X509_policy_node;
typedef struct {
  size_t parent, child;
} TC_X509_policy_edge;
typedef struct {
  size_t node;
  TC_bytes oid;
} TC_X509_policy_expected;

/* One path certificate's validation-relevant extensions. The validator fills
 * a summary with one metered walk over the certificate's extensions, and every
 * later pass of the same validation reads it. Callers provide the storage
 * through TC_X509_path_workspace.summaries, and the validator owns every
 * field. The layout is public so applications can size and place the array.
 *
 * The nine slots cover keyUsage, subjectAltName, basicConstraints,
 * nameConstraints, certificatePolicies, policyMappings, policyConstraints,
 * extendedKeyUsage and inhibitAnyPolicy. */
enum { TC_X509_EXTENSION_SUMMARY_SLOTS = 9 };
typedef struct {
  /* extnValue contents per slot, borrowed from the certificate DER. */
  TC_bytes values[TC_X509_EXTENSION_SUMMARY_SLOTS];
  /* Decoded fixed-form values, valid when the matching slot is present. */
  TC_X509_basic_constraints basic;
  TC_X509_policy_constraints policy_constraints;
  uint32_t inhibit_any;       /* SkipCerts */
  uint16_t present, critical; /* one bit per slot */
  uint16_t key_usage;         /* TC_KEY_USAGE_* bits */
  uint8_t unknown_critical;   /* a critical extension outside the slots */
  uint8_t self_issued;        /* intermediate whose subject matches its issuer */
  uint8_t ready;              /* set once the walk completes */
} TC_X509_extension_summary;

typedef struct {
  /* One frame per constructed nesting level of the deepest object parsed. */
  TC_TLV_frames frames;
  /* Shared by extension decoding, policy decoding and EKU checks. */
  TC_bytes* oids;
  size_t oid_capacity;
  TC_X509_name_workspace names;
  TC_X509_policy_node* nodes;
  size_t node_capacity;
  TC_X509_policy_edge* edges;
  size_t edge_capacity;
  TC_X509_policy_expected* expected;
  size_t expected_capacity;
  TC_X509_policy_mapping* mappings;
  size_t mapping_capacity;
  TC_bytes* policies;
  size_t policy_capacity;
  /* One parsed view per path entry. Views borrow the input DER. */
  TC_X509_certificate* certificates;
  size_t certificate_capacity;
  /* One extension summary per path entry, filled by the first pass and
   * read by the later ones. Fewer entries than the path returns LIMIT. */
  TC_X509_extension_summary* summaries;
  size_t summary_capacity;
} TC_X509_path_workspace;

/* Arguments must be arrays. The shorter name buffer sets the limit.
 * Use as an initializer in C or C++: TC_X509_path_workspace w = ...; */
#define TC_X509_PATH_ARRAY_COUNT_(a) (sizeof(a) / sizeof((a)[0]))
#define TC_X509_PATH_WORKSPACE_INIT(frames_, oids_, left_, right_, matched_, nodes_, edges_,       \
                                    expected_, mappings_, policies_, certificates_, summaries_)    \
  {{(frames_), TC_X509_PATH_ARRAY_COUNT_(frames_)},                                                \
   (oids_),                                                                                        \
   TC_X509_PATH_ARRAY_COUNT_(oids_),                                                               \
   {(left_), (right_),                                                                             \
    TC_X509_PATH_ARRAY_COUNT_(left_) < TC_X509_PATH_ARRAY_COUNT_(right_)                           \
        ? TC_X509_PATH_ARRAY_COUNT_(left_)                                                         \
        : TC_X509_PATH_ARRAY_COUNT_(right_),                                                       \
    (matched_), TC_X509_PATH_ARRAY_COUNT_(matched_)},                                              \
   (nodes_),                                                                                       \
   TC_X509_PATH_ARRAY_COUNT_(nodes_),                                                              \
   (edges_),                                                                                       \
   TC_X509_PATH_ARRAY_COUNT_(edges_),                                                              \
   (expected_),                                                                                    \
   TC_X509_PATH_ARRAY_COUNT_(expected_),                                                           \
   (mappings_),                                                                                    \
   TC_X509_PATH_ARRAY_COUNT_(mappings_),                                                           \
   (policies_),                                                                                    \
   TC_X509_PATH_ARRAY_COUNT_(policies_),                                                           \
   (certificates_),                                                                                \
   TC_X509_PATH_ARRAY_COUNT_(certificates_),                                                       \
   (summaries_),                                                                                   \
   TC_X509_PATH_ARRAY_COUNT_(summaries_)}

/* Element counts for a workspace laid out in one arena. name_scalars sizes
 * both name buffers. path sizes the certificate views and the extension
 * summaries, one entry per path certificate. A zero policy count leaves that
 * array NULL, and validation returns LIMIT when a policy array runs out.
 * The policy tree always holds its anyPolicy root, so validation with zero
 * policy_nodes returns LIMIT. */
typedef struct {
  size_t frames, oids, name_scalars, name_attributes;
  size_t policy_nodes, policy_edges, policy_expected, policy_mappings, policies;
  size_t path;
} TC_X509_path_capacity;

/* An array of this union meets TC_X509_path_workspace_alignment, for static
 * arena storage. Size the array with TC_X509_path_workspace_size. */
typedef union {
  TC_TLV_frame frame;
  TC_bytes span;
  TC_X509_policy_node node;
  TC_X509_policy_edge edge;
  TC_X509_policy_expected expected;
  TC_X509_policy_mapping mapping;
  size_t count;
  uint32_t scalar;
} TC_X509_path_storage;

#if TC_ENABLE_X509_PATH
/* Return the byte alignment that TC_X509_path_workspace_init requires of the
 * arena. Every array starts at a multiple of it. */
size_t TC_X509_path_workspace_alignment(void);
/* Write the arena bytes for capacity to bytes, including the padding that
 * aligns each array. The size is exact for an arena that meets
 * TC_X509_path_workspace_alignment. Charges no work.
 * Returns OK with bytes written. ARGUMENT for NULL arguments or a zero frames,
 * oids, name_scalars, name_attributes or path count. LIMIT when the size
 * overflows size_t. bytes changes only on OK. */
TC_result TC_X509_path_workspace_size(const TC_X509_path_capacity* capacity, size_t* bytes);
/* Partition arena into the twelve workspace arrays and write their pointers
 * and capacities to out. Arrays follow the TC_X509_path_workspace field order
 * and are mutually disjoint. The workspace borrows arena, so keep it alive and
 * reserved for one validation at a time. Initialization leaves the arena
 * bytes untouched. capacity, arena and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL arguments, an arena that is
 * misaligned for TC_X509_path_workspace_alignment or wraps the address space,
 * overlap, or an invalid capacity as in TC_X509_path_workspace_size. LIMIT
 * for an arena smaller than TC_X509_path_workspace_size reports or a size
 * overflow. Failure leaves out and arena unchanged. For fixed array locations,
 * use TC_X509_PATH_WORKSPACE_INIT or fill the fields directly. */
TC_result TC_X509_path_workspace_init(const TC_X509_path_capacity* capacity, TC_buffer arena,
                                      TC_X509_path_workspace* out);
#endif

enum {
  TC_X509_PATH_REQUIRE_EXPLICIT_POLICY = 1u,
  TC_X509_PATH_INHIBIT_MAPPING = 2u,
  TC_X509_PATH_INHIBIT_ANY_POLICY = 4u,
  TC_X509_PATH_REQUIRE_KEY_USAGE = 8u,
  TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE = 16u,
  /* Require an explicit purpose match when a certificate contains EKU. */
  TC_X509_PATH_INHIBIT_ANY_PURPOSE = 32u
};
typedef struct {
  /* Validation time. Certificate validity periods are widened by
   * clock_skew_seconds on both sides to tolerate clock differences. */
  TC_X509_time at;
  uint32_t clock_skew_seconds;
  TC_TLV_limits parsing;
  size_t max_certificates, max_input, max_work;
  TC_X509_signature_provider signatures;
  /* Acceptable policy OIDs as DER contents. An empty list is the RFC 5280
   * default user-initial-policy-set, {anyPolicy}. */
  const TC_bytes* initial_policies;
  size_t initial_policy_count;
  TC_X509_name_constraints anchor_names;
  TC_bytes purpose;
  uint16_t key_usage;
  unsigned flags;
} TC_X509_path_options;

typedef TC_result TC_X509_path_status;
#define TC_X509_PATH_VALID TC_RESULT_OK
#define TC_X509_PATH_INVALID TC_RESULT_INVALID
#define TC_X509_PATH_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_X509_PATH_LIMIT TC_RESULT_LIMIT
#define TC_X509_PATH_ERROR TC_RESULT_ERROR
typedef struct {
  TC_X509_public_key public_key;
  const TC_bytes* policies;
  size_t policy_count, work_used;
} TC_X509_path_report;

/* Search scratch fields are managed by the library. */
typedef struct {
  size_t candidate, anchor, bytes;
  TC_bytes issuer;
} TC_X509_search_frame;
typedef struct {
  TC_bytes* path;
  TC_X509_search_frame* frames;
  size_t capacity;
} TC_X509_search_workspace;
typedef struct {
  const TC_bytes* path;
  size_t count, anchor_index;
  TC_X509_path_report validation;
} TC_X509_search_report;

#if TC_ENABLE_X509_PATH
/* Construct and validate a path from target to an explicit source anchor
 * (RFC 5280 section 6.1, with discovery as in RFC 4158). Candidates come only
 * from source. The depth-first search compares issuer and subject Names,
 * skips repeated encodings and validates each complete candidate path with
 * TC_X509_path_validate_with_anchor. Check revocation separately.
 * - Source callbacks keep returned records stable and separate from both
 *   workspaces and out. Hold the source snapshot until all result use
 *   finishes. Workspace arrays are mutually disjoint and separate from input
 *   storage.
 * - search holds capacity path spans and frames. validation.certificates and
 *   summaries need one entry per attempted path certificate.
 *   options.max_certificates and max_input bound each path.
 * - On VALID, out->path borrows a suffix of search.path, anchor-issued first
 *   and target last. anchor_index names the selected source anchor. Key and
 *   policy lifetimes match TC_X509_path_validate. Copy the path and policy
 *   span arrays before reusing their workspaces. DER bytes stay in place.
 *
 * Work: options.max_work bounds one units counter across all branches. It
 * covers the storage comparisons, candidate reads, Name comparisons, cycle
 * checks and each validation. out->validation.work_used reports the units
 * spent.
 * Returns VALID with out written. ERROR for NULL arguments, unknown flags, a
 * source with a NULL candidate or anchor callback and a nonzero count, overlap,
 * a callback that returns an empty record or an argument error found while
 * searching.
 * LIMIT when max_work, search capacity or a path bound runs out, or when
 * that was the most severe candidate failure. UNSUPPORTED and INVALID
 * report the most severe failure among the attempted candidates. out changes
 * only on VALID. Scratch may change on any result. */
TC_X509_path_status TC_X509_path_build(TC_bytes target, const TC_X509_store_source* source,
                                       const TC_X509_path_options* options,
                                       const TC_X509_path_workspace* validation,
                                       const TC_X509_search_workspace* search,
                                       TC_X509_search_report* out);

/* Validate an ordered chain against one trust anchor (RFC 5280 section
 * 6.1): anchor-issued certificate first and target last, with the anchor
 * excluded. Checks signatures, validity at options.at widened by
 * clock_skew_seconds, CA status and path length, key usage, names and name
 * constraints, the policy tree, extended key usage and critical extensions.
 * Path discovery and revocation checks are separate steps. options.signatures
 * provides the verifier for the algorithms in use.
 * - DER and option spans are borrowed and unchanged during the call. Result
 *   key bytes borrow the target DER. Policy spans also borrow issuer DER or
 *   initial_policies. Keep those buffers and workspace.policies alive while
 *   the result is used.
 * - workspace.certificates and summaries need at least count entries. The
 *   policy arrays bound the policy tree, and oids bounds the extensions of
 *   one certificate. Workspace arrays are mutually disjoint and disjoint from
 *   inputs and out. The provider context is separate from workspace and out.
 *
 * Work: options.max_work bounds a local units counter for the storage
 * comparisons, chain bytes, extension walks, Name and policy processing and
 * each signature. out->work_used reports the units spent.
 * Returns VALID with out written. ERROR for NULL arguments, unknown flags,
 * an invalid options.at, initial policies or a purpose with malformed OID
 * contents, or overlap. The options are checked before any work. INVALID
 * for an empty chain, an empty certificate or any failed check. LIMIT for count above max_certificates or workspace
 * capacity, before any work, and for exhausted max_work, max_input, parsing
 * limits or workspace capacities. UNSUPPORTED for an unsupported algorithm,
 * critical extension or name form. out changes only on VALID. Workspace and
 * provider state may change on any result. */
TC_X509_path_status TC_X509_path_validate(const TC_bytes* chain, size_t count,
                                          const TC_X509_trust_anchor* anchor,
                                          const TC_X509_path_options* options,
                                          const TC_X509_path_workspace* workspace,
                                          TC_X509_path_report* out);
/* Validate against one explicit store anchor, including its path controls
 * (RFC 5937 section 3). anchor->policy_set, policy_flags, names and path_len
 * add their constraints to options. The anchor and its borrowed spans stay
 * stable throughout validation, and result policy spans may also borrow
 * anchor->policy_set. Other rules, work and statuses match
 * TC_X509_path_validate, with these additions. INVALID for an x509_unusable
 * anchor or a CertPathControls duplicate in anchor->extensions. UNSUPPORTED
 * for an unimplemented critical anchor extension, or for a path control in
 * the anchor's extension spans that its record fields omit (see
 * TC_X509_store_anchor). ERROR for unknown policy_flags or replaced_controls
 * bits. */
TC_X509_path_status TC_X509_path_validate_with_anchor(const TC_bytes* chain, size_t count,
                                                      const TC_X509_store_anchor* anchor,
                                                      const TC_X509_path_options* options,
                                                      const TC_X509_path_workspace* workspace,
                                                      TC_X509_path_report* out);
#endif
#ifdef __cplusplus
}
#endif
#endif
