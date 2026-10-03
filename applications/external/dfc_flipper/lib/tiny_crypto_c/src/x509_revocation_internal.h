/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_REVOCATION_INTERNAL_H_
#define TC_X509_REVOCATION_INTERNAL_H_
#include "x509_crl_internal.h"
#include "x509_path_internal.h"
#include "pki_candidate_internal.h"

static inline int tc_x509_crl_trust_valid(const tc_x509_crl_trust* trust)
{
  return trust && trust->source && trust->options && trust->tree && trust->tree->work &&
         trust->validation && trust->search && trust->time && trust->source->anchor &&
         trust->anchor_index < trust->source->anchor_count;
}

typedef TC_TLV_result (*tc_x509_crl_attempt)(const void* context,
                                             const TC_X509_certificate* candidate,
                                             const tc_x509_crl_trust* trust,
                                             TC_X509_search_report* out);

/* One CRL whose signer is searched. attempt validates each matching
 * candidate and receives context. */
typedef struct {
  const TC_X509_crl* crl;
  const TC_X509_crl_extensions* extensions;
  tc_x509_crl_attempt attempt;
  const void* context;
} tc_x509_crl_signer_query;

typedef TC_TLV_result (*tc_x509_crl_source_search_fn)(const void* candidates,
                                                      const TC_X509_store_source* external,
                                                      const tc_x509_crl_signer_query* query,
                                                      const tc_x509_crl_trust* trust,
                                                      TC_X509_search_report* out,
                                                      int* source_failed);

/* The adapter borrows its cursor snapshot and uses the operation's guarded source. */
typedef struct {
  const void* context;
  const TC_X509_store_source* external;
  tc_x509_crl_source_search_fn search;
} tc_x509_crl_candidate_source;

/* Candidate adapters describe their immutable cursor/source metadata here so
 * the shared operation can guard it with the rest of the borrowed inputs. */
typedef struct {
  tc_x509_crl_candidate_source candidates;
  const TC_bytes* metadata;
  size_t metadata_count;
} tc_x509_crl_operation_source;

TC_TLV_result tc_x509_crl_source_search(const void* candidates,
                                        const tc_x509_crl_signer_query* query,
                                        const tc_x509_crl_trust* trust, TC_X509_search_report* out,
                                        int* source_failed);

TC_TLV_result tc_x509_crl_store_source_search(const void* candidates,
                                              const TC_X509_store_source* external,
                                              const tc_x509_crl_signer_query* query,
                                              const tc_x509_crl_trust* trust,
                                              TC_X509_search_report* out, int* source_failed);

typedef struct {
  const TC_X509_crl* crl;
  const TC_X509_crl_extensions* extensions;
  const TC_X509_name_workspace* names;
} tc_x509_crl_filter;

TC_TLV_result tc_x509_crl_filter_match(const void* context, const TC_X509_certificate* candidate,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, int* matched);

/* Search a guarded cursor for a matching CRL signer and attempt validation.
 * Cursor and scratch are provisional; output is published on success. */
TC_TLV_result tc_x509_crl_search_candidates(void* cursor, tc_pki_candidate_next next,
                                            const tc_x509_crl_signer_query* query,
                                            const tc_x509_crl_trust* trust,
                                            TC_X509_search_report* out, int* source_failed);

static inline int tc_x509_crl_index_arguments(const TC_X509_crl_index* index,
                                              const tc_x509_crl_query* query,
                                              const tc_x509_crl_trust* trust,
                                              TC_X509_search_report* out)
{
  return index && (!index->count || index->records) && out && tc_x509_crl_trust_valid(trust) &&
         query && query->certificate && query->point &&
         (query->certificate_ca == 0 || query->certificate_ca == 1);
}

typedef struct {
  tc_x509_crl_selected selected;
  TC_X509_crl_evidence evidence;
  TC_TLV_result result;
  /* Scope checks passed; only signer dependencies remain unresolved. */
  int pending_only;
} tc_x509_crl_proposal;

/* Merge authenticated scope proposals using the configured freshness order. */
TC_TLV_result tc_x509_crl_proposal_merge(tc_x509_crl_proposal* chosen,
                                         const tc_x509_crl_proposal* candidate,
                                         TC_X509_crl_order_policy policy,
                                         const tc_pki_tree_workspace* tree);

/* Called after signature/path validation and scope selection, before publication.
 * selected borrows the effective base/delta pair, or is NULL when no pair could
 * be ranked. A ranked pair can still have failed scope/entry checks. Callback
 * success does not override those failures. The views remain live for this call.
 * VALID must include the signer's revocation dependencies. The callback keeps
 * path bytes unchanged and uses separate scratch. work may only decrease. */
typedef struct {
  void* context;
  TC_X509_path_status (*verify)(void* context, const TC_X509_search_report* path,
                                const tc_x509_crl_selected* selected, size_t* work);
} tc_x509_crl_path_check;

typedef struct {
  TC_bytes signer;
  TC_X509_search_report result;
  TC_bytes* path;
  size_t path_capacity;
  TC_bytes* policies;
  size_t policy_capacity;
  int valid;
} tc_x509_crl_signer_cache;

typedef struct {
  const TC_X509_crl_index* index;
  size_t reference;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
  const tc_x509_crl_query* query;
  uint8_t* states;
  size_t capacity;
  TC_X509_crl_evidence* evidence;
  const tc_x509_crl_path_check* check;
  tc_x509_crl_proposal* proposal;
  tc_x509_crl_proposal* unresolved;
  const TC_X509_revocation_scope* scopes;
  tc_x509_crl_signer_cache* signer_cache;
} tc_x509_crl_scope_processing;

/* Validate scope inputs before describing or using writable storage. */
TC_TLV_result tc_x509_crl_scope_arguments(const tc_x509_crl_scope_processing* processing,
                                          const tc_x509_crl_trust* trust, int all_scopes,
                                          TC_X509_search_report* out);

/* Run a signer attempt after the caller validates state and storage separation. */
TC_TLV_result tc_x509_crl_scope_attempt(const void* context, const TC_X509_certificate* signer,
                                        const tc_x509_crl_trust* trust, TC_X509_search_report* out);

typedef TC_TLV_result (*tc_x509_crl_search)(const void* candidates,
                                            const tc_x509_crl_signer_query* query,
                                            const tc_x509_crl_trust* trust,
                                            TC_X509_search_report* out, int* source_failed);

/* Signer candidates and the search that walks them. */
typedef struct {
  const void* candidates;
  tc_x509_crl_search search;
} tc_x509_crl_searcher;

TC_TLV_result tc_x509_crl_same_scope(const tc_x509_crl_scope_processing* processing, size_t other,
                                     const tc_x509_crl_trust* trust, int* same);
TC_TLV_result tc_x509_crl_scopes_index(const TC_X509_crl_index* index, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names,
                                       TC_X509_revocation_scope* slots, size_t capacity);
/* Search each reference key before ranking authenticated scope proposals.
 * Inputs and storage are validated by the caller. Search preserves work limits. */
TC_TLV_result tc_x509_crl_group(const tc_x509_crl_searcher* searcher,
                                const tc_x509_crl_scope_processing* processing,
                                const tc_x509_crl_trust* trust, int* source_failed,
                                tc_x509_crl_proposal* out);

typedef struct {
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  TC_bytes points;
  tc_pki_distribution_point alternative;
  int ca;
} tc_x509_crl_certificate_fields;

enum { CRL_PATH_OPTIONS, CRL_PATH_WORKSPACE, CRL_PATH_METADATA_COUNT };

/* The chain, optional OCSP responses and operation metadata stay live across
 * dependency searches. ocsp is NULL with ocsp_count zero, or holds one span
 * per chain entry. */
typedef struct {
  const TC_bytes* chain;
  size_t count;
  TC_X509_revocation_report* out;
  size_t dependency_count;
  TC_bytes metadata[CRL_PATH_METADATA_COUNT];
  const TC_bytes* ocsp;
  size_t ocsp_count;
  /* 1: a member without evidence gives UNDETERMINED in out. */
  uint8_t report_uncovered;
} tc_x509_crl_held_path;

/* Caller guards the path object before this checks its borrowed inputs. */
void tc_x509_crl_path_plan_inputs(tc_pki_storage_plan* plan, const tc_x509_crl_held_path* path);

/* Collect borrowed revocation fields through the checked extension visitor.
 * Zero-initialize fields and supply limits/tree before visiting extensions. */
TC_TLV_result tc_x509_crl_certificate_extension(void* context, const TC_X509_extension* extension);

/* Initialize a borrowed point reader after checking the whole list. A supplied
 * certificate replaces the initial fields. Scratch and work are provisional.
 * Fields and reader are published on success. Caller checks storage overlap. */
TC_TLV_result tc_x509_crl_points_init(tc_x509_crl_certificate_fields* fields,
                                      const TC_X509_certificate* certificate, TC_bytes* oids,
                                      size_t oid_capacity, TC_TLV_reader* reader);

/* Traverse checked points and issuer fallbacks using guarded candidate sources.
 * Caller validates policies, workspace separation and the complete point list.
 * Evidence and the single-scope path are published on success. Scratch is provisional. */
TC_TLV_result tc_x509_crl_scopes(const tc_x509_crl_searcher* searcher,
                                 const tc_x509_crl_scope_processing* input,
                                 const tc_x509_crl_trust* trust,
                                 const tc_x509_crl_certificate_fields* fields,
                                 TC_TLV_reader* point_reader, int all_scopes, int* source_failed,
                                 TC_X509_search_report* out);

/* Caller-owned scratch reused across signer dependency searches. */
typedef struct {
  const tc_pki_tree_workspace* tree;
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
  uint8_t* states;
  size_t state_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
  int indexed_dependencies;
  TC_X509_revocation_scope* scopes;
  size_t scope_capacity;
  tc_x509_crl_signer_cache* signer_cache;
  /* NULL: a CRL signer path member without evidence leaves the CRL
   * unresolved. Otherwise such a member is accepted and *uncovered_used is
   * set to 1 (tc_x509_path_revocation_coverage). */
  uint8_t* uncovered_used;
} tc_x509_crl_resolution_workspace;

/* Node status of a certificate that no indexed CRL covers. It is stable for
 * the operation, since the index and the evaluation time are fixed. The
 * value extends TC_X509_revocation_status inside the resolver only. */
enum { TC_X509_CRL_NODE_UNCOVERED = TC_X509_REVOCATION_REVOKED + 1 };

typedef struct {
  const tc_x509_crl_operation_source* candidates;
  const TC_X509_crl_index* index;
  const TC_X509_store_source* source;
  const TC_X509_path_options* options;
  size_t anchor_index;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
  /* CRL freshness time, checked by the public entry. */
  const TC_X509_revocation_time* time;
} tc_x509_crl_resolution;

typedef struct {
  const TC_X509_path_options* options;
  size_t anchor_index;
  const tc_x509_crl_resolution_workspace* workspace;
  const TC_X509_store_anchor* anchor;
  const TC_bytes* writes;
  size_t write_count;
  size_t count;
} tc_x509_crl_dependencies;

/* Parse a guarded dependency using the caller's reusable validation scratch. */
TC_TLV_result tc_x509_crl_dependency_read(const tc_x509_crl_dependencies* dependencies,
                                          size_t index, TC_X509_certificate* out);

enum { CRL_EXTRA_RESOLUTION, CRL_EXTRA_WORKSPACE, CRL_EXTRA_INPUT_COUNT };

typedef struct {
  TC_bytes nodes_storage, output_storage, inputs[CRL_EXTRA_INPUT_COUNT];
  const TC_X509_revocation_node* nodes;
  size_t count;
} tc_x509_crl_extra_storage;

/* Check the node array and borrowed bytes of extra as plan inputs. The caller
 * records extra itself as a write or input. */
void tc_x509_crl_extra_plan_inputs(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_extra_storage* extra);

/* Guard borrowed certificate bytes before adding a dependency. */
TC_TLV_result tc_x509_crl_dependency_add(tc_x509_crl_dependencies* dependencies,
                                         TC_bytes certificate, size_t* work, size_t* index);

/* Anchor signatures establish a root. Other signer paths need resolved
 * dependencies. A NULL selected (no current pair in scope) returns VALID, since
 * the attempt keeps its END or failure result and publishes nothing. */
TC_X509_path_status tc_x509_crl_dependencies_check(void* context, const TC_X509_search_report* path,
                                                   const tc_x509_crl_selected* selected,
                                                   size_t* work);

/* Reuse an encoded certificate's node or append a borrowed, undetermined node.
 * Caller guards certificate bytes against writable storage and keeps existing
 * nodes valid. Failures preserve nodes, count and index. Work is provisional. */
TC_TLV_result tc_x509_crl_dependency_find(TC_X509_revocation_node* nodes, size_t capacity,
                                          size_t* count, TC_bytes certificate, size_t* work,
                                          size_t* index);
/* Production resolver uses the node array as its own hash bucket storage. */
TC_TLV_result tc_x509_crl_dependency_find_indexed(TC_X509_revocation_node* nodes, size_t capacity,
                                                  size_t* count, TC_bytes certificate, size_t* work,
                                                  size_t* index);

/* Both effective records must verify with the anchor to bypass signer paths. */
TC_X509_signature_result tc_x509_crl_selected_anchor_check(
    const tc_x509_crl_selected* selected, const TC_X509_store_anchor* anchor,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* names, size_t* work);

/* Extend provisional dependency nodes and check each signer's current status.
 * Caller validates metadata storage. writes covers every writable operation span. */
TC_X509_path_status tc_x509_crl_dependencies_path(const TC_X509_search_report* path,
                                                  const tc_x509_crl_resolution_workspace* workspace,
                                                  size_t* count, const TC_bytes* writes,
                                                  size_t write_count, size_t* work);

typedef TC_TLV_result (*tc_x509_crl_node_evaluate)(void* context, size_t index,
                                                   TC_X509_crl_evidence* evidence, int* stop);
/* Retry pending nodes while evaluation discovers dependencies or resolves nodes.
 * Evaluators append within capacity and consume work. Nodes remain provisional;
 * OK publishes determined root evidence. stop marks an operation-wide failure.
 * An evaluator result of END (no indexed CRL applies) marks the node
 * TC_X509_CRL_NODE_UNCOVERED, and returns END at once for the root. */
TC_TLV_result tc_x509_crl_nodes_resolve(TC_X509_revocation_node* nodes, size_t capacity,
                                        size_t* count, size_t root, size_t* work,
                                        tc_x509_crl_node_evaluate evaluate, void* context,
                                        TC_X509_crl_evidence* out);

/* Resolve a guarded target, reusing proven dependencies within the held path.
 * Caller prepares the source/anchor and checks operation storage first. */
TC_TLV_result tc_x509_crl_resolve_dependencies(TC_bytes target,
                                               tc_x509_crl_dependencies* dependencies,
                                               tc_x509_crl_node_evaluate evaluate, void* context,
                                               tc_x509_crl_held_path* path,
                                               TC_X509_crl_evidence* out);

/* index is the member's position in the chain. */
typedef TC_TLV_result (*tc_x509_crl_certificate_resolve)(void* context, size_t index,
                                                         TC_bytes certificate,
                                                         TC_X509_crl_evidence* evidence);
/* Resolve an anchor-issued-first chain. resolve guards the whole held chain
 * before reading bytes or using scratch. It returns END, or OK with
 * incomplete evidence, for a member without evidence. With report_uncovered
 * zero such a member returns UNSUPPORTED at once. Otherwise the remaining
 * members are still resolved: a REVOKED member or a failure takes
 * precedence, and else out holds UNDETERMINED with the index of the first
 * uncovered member and zero evidence. out changes only on OK. */
TC_TLV_result tc_x509_crl_path_resolve(const TC_bytes* chain, size_t count,
                                       tc_x509_crl_certificate_resolve resolve, void* context,
                                       int report_uncovered, TC_X509_revocation_report* out);

/* TC_X509_path_check_revocation with uncovered members reported. A member
 * with no accepted OCSP response and no current indexed CRL in its scope
 * gives OK with out->status UNDETERMINED and out->certificate_index naming
 * the first such member, once every other member is settled. A REVOKED member takes
 * precedence. A CRL signer without evidence is accepted, and a member whose
 * evidence relied on one counts as uncovered unless it is REVOKED. A
 * candidate CRL that is unsupported or invalid, an unsupported CRL anywhere
 * in the index and a dependency cycle keep their UNSUPPORTED or INVALID
 * result. The validation layer uses this for its revocation evidence
 * policy. */
TC_TLV_result tc_x509_path_revocation_coverage(const TC_bytes* chain, size_t count,
                                               const TC_X509_revocation_options* options,
                                               const TC_X509_revocation_workspace* workspace,
                                               size_t* work, TC_X509_revocation_report* out);

enum {
  CRL_SCOPE_PATH = TC_X509_PATH_STORAGE_COUNT,
  CRL_SCOPE_SEARCH,
  CRL_SCOPE_TREE,
  CRL_SCOPE_STATES,
  CRL_SCOPE_EVIDENCE,
  CRL_SCOPE_RESULT,
  CRL_SCOPE_WORK,
  CRL_SCOPE_SCOPES,
  CRL_SCOPE_SIGNER_CACHE,
  CRL_SCOPE_SIGNER_PATH,
  CRL_SCOPE_SIGNER_POLICIES,
  CRL_SCOPE_NODES,
  CRL_SCOPE_OUTPUT,
  CRL_SCOPE_PATH_OUTPUT,
  CRL_SCOPE_WRITES
};

/* Which scopes a run consults. points, when set, is an explicit encoded
 * distribution point list. from_certificate also reads the certificate's own
 * CRLDistributionPoints. all_scopes gathers evidence from every scope in place
 * of stopping at the first path. */
typedef struct {
  const TC_bytes* points;
  int from_certificate;
  int all_scopes;
} tc_x509_crl_scope_selection;

/* Run a scope operation after tc_x509_crl_scope_arguments and the storage
 * preflight that sealed writes. The caller validates every argument once, so
 * this body repeats neither step. Sources and input bytes stay fixed.
 * source_failed accumulates callback failures across dependency searches.
 * out receives the signer path on OK. Its work_used is left to the caller. */
TC_TLV_result tc_x509_crl_scope_run(const tc_x509_crl_candidate_source* candidates,
                                    const tc_x509_crl_scope_processing* processing,
                                    const tc_x509_crl_trust* trust,
                                    const tc_x509_crl_scope_selection* selection,
                                    const TC_bytes writes[CRL_SCOPE_WRITES], int* source_failed,
                                    TC_X509_search_report* out);

/* Validate, preflight and run one scope through a prepared candidate adapter.
 * On OK for a single scope, out->validation.work_used covers the whole call. */
TC_TLV_result tc_x509_crl_scope_execute(const tc_x509_crl_operation_source* candidates,
                                        const tc_x509_crl_scope_processing* processing,
                                        const tc_x509_crl_trust* trust,
                                        const tc_x509_crl_scope_selection* selection,
                                        TC_X509_search_report* out);

/* Resolve one parsed target through the shared, non-recursive dependency
 * operation. Validates the resolution and runs one storage preflight. */
TC_TLV_result tc_x509_crl_resolve(const TC_X509_certificate* target,
                                  const tc_x509_crl_resolution* resolution,
                                  const tc_x509_crl_resolution_workspace* workspace,
                                  TC_X509_crl_evidence* out);
/* Resolve every member of a held path. One validation and storage preflight
 * cover all members and dependency nodes, and proven nodes are reused. */
TC_TLV_result tc_x509_crl_path_operation(tc_x509_crl_held_path* held,
                                         const tc_x509_crl_resolution* resolution,
                                         const tc_x509_crl_resolution_workspace* workspace);

/* Record the dependency-node, output and held-path result writes in the
 * CRL_SCOPE_NODES through CRL_SCOPE_PATH_OUTPUT slots. Absent storage records
 * an empty span so slot numbers stay fixed. */
void tc_x509_crl_scope_plan_outputs(tc_pki_storage_plan* plan,
                                    const tc_x509_crl_extra_storage* extra,
                                    const tc_x509_crl_held_path* path);
/* Record validation, search and scope writes in CRL_SCOPE_* slot order through
 * CRL_SCOPE_SIGNER_POLICIES. The caller appends the operation outputs. */
void tc_x509_crl_scope_plan_writes(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_scope_processing* processing,
                                   const tc_x509_crl_trust* trust, TC_X509_search_report* out);

/* Check all borrowed record fields against a sealed plan. The caller checks
 * the index object and record array before this visits their byte spans. */
void tc_x509_crl_index_plan_inputs(tc_pki_storage_plan* plan, const TC_X509_crl_index* index);

/* Check shared scope, query and policy metadata and their borrowed bytes. */
void tc_x509_crl_scope_plan_inputs(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_scope_processing* processing,
                                   const tc_x509_crl_trust* trust);

/* Match issuer, authority identifiers and cRLSign usage on parsed inputs.
 * Signature and path checks follow candidate selection. matched changes on OK.
 * Name/tree scratch and work are separate from inputs and output. */
TC_TLV_result tc_x509_crl_candidate_matches(const TC_X509_crl* crl,
                                            const TC_X509_crl_extensions* extensions,
                                            const TC_X509_certificate* candidate,
                                            const TC_TLV_limits* limits,
                                            const TC_X509_name_workspace* names,
                                            const tc_pki_tree_workspace* tree, int* matched);

typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_certificate* signer;
  const TC_X509_signature_provider* provider;
  const TC_TLV_limits* limits;
  const TC_X509_name_workspace* names;
  uint8_t* states;
  size_t capacity;
  const TC_X509_revocation_scope* scopes;
} tc_x509_crl_signature_cache;
/* One CRL scope evaluation: a signer's signature cache, the delta and order
 * policies, the validation time, tree scratch with its work budget, and
 * entry-extension OID scratch. Every field is borrowed for the call. */
typedef struct {
  const tc_x509_crl_signature_cache* cache;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
  const TC_X509_revocation_time* time;
  const tc_pki_tree_workspace* tree;
  TC_bytes* oids;
  size_t oid_capacity;
} tc_x509_crl_scope_context;
/* One byte per indexed record, scoped to this signer/provider and stable input
 * snapshot. Cache valid/invalid signatures only. Limits and provider errors can
 * be retried.
 * Reinitialize when the signer, provider policy or indexed records change.
 * Caller keeps metadata/record bytes immutable and disjoint from states/work/out
 * and name scratch. Initialization charges one work unit per record. */
TC_TLV_result
tc_x509_crl_signature_cache_init(const TC_X509_crl_index* index, const TC_X509_certificate* signer,
                                 const TC_X509_signature_provider* provider,
                                 const TC_TLV_limits* limits, const TC_X509_name_workspace* names,
                                 TC_buffer states, size_t* work, tc_x509_crl_signature_cache* out);
TC_TLV_result tc_x509_crl_signature_cached(const tc_x509_crl_signature_cache* cache, size_t record,
                                           size_t* work);
/* Enumerate compatible deltas for one complete CRL in source order. Start
 * cursor at zero. Policy-rejected rows are skipped but remain in the index.
 * Each candidate needs authentication, freshness and scope checks before use.
 * Stable parsed index and disjoint scratch/output. Cursor/out change only on OK. */
TC_TLV_result tc_x509_crl_delta_next(const TC_X509_crl_index* index, size_t base, size_t* cursor,
                                     const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
                                     const TC_X509_name_workspace* names,
                                     tc_x509_crl_selected* out);

/* Apply the newest authenticated CRLs and retain their selection rank. */
TC_TLV_result tc_x509_crl_scope_evaluate(const tc_x509_crl_scope_context* scope, size_t reference,
                                         const tc_x509_crl_query* query,
                                         TC_X509_crl_evidence* evidence,
                                         tc_x509_crl_selected* preference);

enum { CRL_SIGNATURE_UNCHECKED, CRL_SIGNATURE_VALID, CRL_SIGNATURE_INVALID };

static inline int x509_crl_delta_policy_valid(TC_X509_crl_delta_policy policy)
{
  return policy == TC_X509_CRL_COMPLETE_ONLY || policy == TC_X509_CRL_DELTA_IF_AVAILABLE ||
         policy == TC_X509_CRL_DELTA_REQUIRED;
}

static inline int x509_crl_order_policy_valid(TC_X509_crl_order_policy policy)
{
  return policy == TC_X509_CRL_ORDER_NUMBER || policy == TC_X509_CRL_ORDER_THIS_UPDATE;
}

/* Comparing the first candidate with itself also checks its required number. */
static inline TC_TLV_result x509_crl_order(const tc_x509_crl_selected* left,
                                           const tc_x509_crl_selected* right,
                                           TC_X509_crl_order_policy policy, size_t* work,
                                           int* order)
{
  if (policy == TC_X509_CRL_ORDER_NUMBER) {
    const TC_X509_crl_extensions* a = left->delta ? left->delta_info : left->base_info;
    const TC_X509_crl_extensions* b = right->delta ? right->delta_info : right->base_info;
    if (!(a->present & TC_X509_CRL_EXT_NUMBER) || !(b->present & TC_X509_CRL_EXT_NUMBER))
      return TC_TLV_UNSUPPORTED;
    return tc_x509_crl_number_compare(a->number, b->number, work, order);
  }
  const TC_X509_crl* a = left->delta ? left->delta : left->base;
  const TC_X509_crl* b = right->delta ? right->delta : right->base;
  TC_TLV_result result = tc_pki_work_charge(work, 1);
  return result == TC_TLV_OK ? TC_X509_time_compare(&a->this_update, &b->this_update, order)
                             : result;
}

/* FNV-1a over encoded bytes, used to bucket certificates and CRL scopes. */
static inline uint32_t tc_x509_crl_bytes_hash(TC_bytes bytes)
{
  uint32_t hash = UINT32_C(2166136261);
  for (size_t i = 0; i < bytes.length; ++i)
    hash = (hash ^ bytes.data[i]) * UINT32_C(16777619);
  return hash;
}

/* Preflight storage for a scope operation, then commit the checked budget.
 * writes receives the CRL_SCOPE_* slots used by source guards. */
TC_TLV_result tc_x509_crl_scope_prepare(const tc_x509_crl_operation_source* candidates,
                                        const tc_x509_crl_scope_processing* processing,
                                        const tc_x509_crl_trust* trust, const TC_bytes* points,
                                        const tc_x509_crl_extra_storage* extra,
                                        const tc_x509_crl_held_path* path,
                                        TC_X509_search_report* out,
                                        TC_bytes writes[CRL_SCOPE_WRITES]);

/* Nonzero when a signature cache is bound to an index and signer. */
int tc_x509_crl_signature_cache_valid(const tc_x509_crl_signature_cache* cache);
/* The latest effective CRL for a reference scope. */
TC_TLV_result tc_x509_crl_latest(const tc_x509_crl_scope_context* scope, size_t reference,
                                 tc_x509_crl_selected* out, int* conflict);
/* Select the highest-numbered current delta signed by the validated base's
 * signer. Caller has authenticated the base and established signer trust.
 * Bad signatures are skipped; provider failures and limits stop selection.
 * Conflicting TBS bytes at the highest authenticated number are INVALID.
 * END means no usable delta. A non-NULL cache reuses verified signatures, and
 * a non-NULL conflict reports a conflict in place of returning INVALID. Scope,
 * entries and signer revocation are separate. Stable/disjoint inputs and
 * scratch. out changes only on OK. */
TC_TLV_result tc_x509_crl_delta_select(const tc_x509_crl_signature_cache* signer, size_t base,
                                       const TC_X509_revocation_time* time,
                                       const tc_pki_tree_workspace* tree, int* conflict,
                                       tc_x509_crl_selected* out);
/* Enumerate authenticated effective candidates in one issuer/IDP scope.
 * reference identifies the scope. cursor starts at zero. Signature cache is
 * scoped to the proposed signer, whose trust remains the caller's
 * responsibility. Stale bases need a current delta; future bases are skipped.
 * conflict is optional, as for tc_x509_crl_delta_select. Entry and target
 * coverage checks follow selection. Cursor/out change only on OK. Cache/work
 * and name scratch are provisional. Ranking across bases is a separate step. */
TC_TLV_result tc_x509_crl_effective_next(const tc_x509_crl_scope_context* scope, size_t reference,
                                         size_t* cursor, int* conflict, tc_x509_crl_selected* out);
#endif
