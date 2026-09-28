/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_REVOCATION_INTERNAL_H_
#define TC_X509_REVOCATION_INTERNAL_H_
#include "x509_crl_internal.h"
#include "x509_path_internal.h"
#include "pki_candidate_internal.h"

/* Borrowed path state shared by CRL signer searches and dependency resolution. */
typedef struct {
  const TC_X509_store_source* source;
  size_t anchor_index;
  const TC_X509_path_options* options;
  const tc_pki_tree_workspace* tree;
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
} tc_x509_crl_trust;

static inline int tc_x509_crl_trust_valid(const tc_x509_crl_trust* trust)
{
  return trust && trust->source && trust->options && trust->tree && trust->tree->work &&
      trust->validation && trust->search && trust->source->anchor &&
      trust->anchor_index < trust->source->anchor_count;
}

typedef TC_TLV_result (*tc_x509_crl_attempt)(const void* context,
    const TC_X509_certificate* candidate, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out);

typedef TC_TLV_result (*tc_x509_crl_source_search_fn)(const void* candidates,
    const TC_X509_store_source* external, const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const tc_x509_crl_trust* trust,
    tc_x509_crl_attempt attempt, const void* context, TC_X509_search_result* out, int* source_failed);

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
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed);

TC_TLV_result tc_x509_crl_store_source_search(const void* candidates,
    const TC_X509_store_source* external, const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const tc_x509_crl_trust* trust,
    tc_x509_crl_attempt attempt, const void* context, TC_X509_search_result* out, int* source_failed);

typedef struct {
  const tc_x509_crl* crl;
  const tc_x509_crl_extension_info* extensions;
  const TC_X509_name_workspace* names;
} tc_x509_crl_filter;

TC_TLV_result tc_x509_crl_filter_match(const void* context,
    const TC_X509_certificate* candidate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, int* matched);

/* Search a guarded cursor for a matching CRL signer and attempt validation.
 * Cursor and scratch are provisional; output is published on success. */
TC_TLV_result tc_x509_crl_search_candidates(void* cursor, tc_pki_candidate_next next,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed);

/* candidates points to a guarded store cursor snapshot, reused across searches. */
TC_TLV_result tc_x509_crl_store_search(const void* candidates,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed);

typedef struct {
  const TC_X509_crl_index* index;
  size_t base;
  TC_X509_crl_delta_policy delta_policy;
  const tc_x509_crl_query* query;
  tc_x509_crl_evidence* evidence;
} tc_x509_crl_index_processing;

static inline int tc_x509_crl_index_arguments(const TC_X509_crl_index* index,
    const tc_x509_crl_query* query, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out)
{
  return index && (!index->count || index->records) && out &&
      tc_x509_crl_trust_valid(trust) && query && query->certificate && query->point &&
      (query->certificate_ca == 0 || query->certificate_ca == 1);
}

/* Caller validates the index, base, query and trust before candidate search.
 * Establish the signer path before selecting and applying a delta CRL. */
TC_TLV_result tc_x509_crl_index_attempt(const void* context,
    const TC_X509_certificate* signer, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out);

typedef struct {
  const tc_x509_crl_selected* selected;
  const tc_x509_crl_query* query;
  tc_x509_crl_evidence* evidence;
} tc_x509_crl_processing;

/* Candidate callbacks borrow validated search state for the whole attempt. */
TC_TLV_result tc_x509_crl_check_signer(const void* context,
    const TC_X509_certificate* candidate, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out);
TC_TLV_result tc_x509_crl_process_candidate(const void* context,
    const TC_X509_certificate* candidate, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out);

typedef struct {
  tc_x509_crl_selected selected;
  tc_x509_crl_evidence evidence;
  TC_TLV_result result;
  /* Scope checks passed; only signer dependencies remain unresolved. */
  int pending_only;
} tc_x509_crl_proposal;

/* Merge authenticated scope proposals using the configured freshness order. */
TC_TLV_result tc_x509_crl_proposal_merge(tc_x509_crl_proposal* chosen, const tc_x509_crl_proposal* candidate,
    TC_X509_crl_order_policy policy, const tc_pki_tree_workspace* tree);

/* Called after signature/path validation and scope selection, before publication.
 * selected borrows the effective base/delta pair, or is NULL when no pair could
 * be ranked. A ranked pair can still have failed scope/entry checks; callback
 * success does not override those failures. The views remain live for this call.
 * VALID must include the signer's revocation dependencies. The callback keeps
 * path bytes unchanged and uses separate scratch; work may only decrease. */
typedef struct {
  void* context;
  TC_X509_path_status (*verify)(void* context, const TC_X509_search_result* path,
      const tc_x509_crl_selected* selected, size_t* work);
} tc_x509_crl_path_check;

typedef struct {
  const TC_X509_crl_index* index;
  size_t reference;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
  const tc_x509_crl_query* query;
  uint8_t* states;
  size_t capacity;
  tc_x509_crl_evidence* evidence;
  const tc_x509_crl_path_check* check;
  tc_x509_crl_proposal* proposal;
  tc_x509_crl_proposal* unresolved;
} tc_x509_crl_scope_processing;

/* Validate scope inputs before describing or using writable storage. */
TC_TLV_result tc_x509_crl_scope_arguments(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, int all_scopes, TC_X509_search_result* out);

/* Run a signer attempt after the caller validates state and storage separation. */
TC_TLV_result tc_x509_crl_scope_attempt(const void* context,
    const TC_X509_certificate* signer, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out);

typedef TC_TLV_result (*tc_x509_crl_search)(const void* candidates,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed);

TC_TLV_result tc_x509_crl_same_scope(const tc_x509_crl_scope_processing* processing,
    size_t other, const tc_x509_crl_trust* trust, int* same);
/* Search each reference key before ranking authenticated scope proposals.
 * Inputs and storage are validated by the caller; search preserves work limits. */
TC_TLV_result tc_x509_crl_group(const void* candidates, tc_x509_crl_search search,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    int* source_failed, tc_x509_crl_proposal* out);

typedef struct {
  const TC_TLV_limits* limits;
  const tc_pki_tree_workspace* tree;
  TC_bytes points;
  tc_pki_distribution_point alternative;
  int ca;
} tc_x509_crl_certificate_fields;

enum { CRL_PATH_OPTIONS, CRL_PATH_WORKSPACE, CRL_PATH_METADATA_COUNT };

/* The chain and operation metadata stay live across dependency searches. */
typedef struct {
  const TC_bytes* chain;
  size_t count;
  TC_X509_revocation_result* out;
  size_t dependency_count;
  TC_bytes metadata[CRL_PATH_METADATA_COUNT];
} tc_x509_crl_held_path;

/* Caller guards the path object before this checks its borrowed inputs. */
TC_TLV_result tc_x509_crl_path_storage_inputs(const tc_x509_crl_held_path* path,
    const TC_bytes* writes, size_t write_count, size_t* work);

/* Collect borrowed revocation fields through the checked extension visitor.
 * Zero-initialize fields and supply limits/tree before visiting extensions. */
TC_TLV_result tc_x509_crl_certificate_extension(void* context, const TC_X509_extension* extension);

/* Initialize a borrowed point reader after checking the whole list. A supplied
 * certificate replaces the initial fields. Scratch and work are provisional;
 * fields and reader are published on success. Caller checks storage overlap. */
TC_TLV_result tc_x509_crl_points_init(tc_x509_crl_certificate_fields* fields,
    const TC_X509_certificate* certificate, TC_bytes* oids, size_t oid_capacity,
    TC_TLV_reader* reader);

/* Traverse checked points and issuer fallbacks using guarded candidate sources.
 * Caller validates policies, workspace separation and the complete point list.
 * Evidence and the single-scope path are published on success; scratch is provisional. */
TC_TLV_result tc_x509_crl_scopes(const void* candidates, tc_x509_crl_search search_candidates,
    const tc_x509_crl_scope_processing* input, const tc_x509_crl_trust* trust,
    const tc_x509_crl_certificate_fields* fields, TC_TLV_reader* point_reader,
    int all_scopes, int* source_failed, TC_X509_search_result* out);

/* Caller-owned scratch reused across signer dependency searches. */
typedef struct {
  const tc_pki_tree_workspace* tree;
  const TC_X509_path_workspace* validation;
  const TC_X509_search_workspace* search;
  uint8_t* states;
  size_t state_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
} tc_x509_crl_resolution_workspace;

typedef struct {
  const tc_x509_crl_operation_source* candidates;
  const TC_X509_crl_index* index;
  const TC_X509_store_source* source;
  const TC_X509_path_options* options;
  size_t anchor_index;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} tc_x509_crl_resolution;

typedef struct {
  const TC_X509_path_options* options;
  size_t anchor_index;
  const tc_x509_crl_resolution_workspace* workspace;
  const TC_X509_trust_anchor* anchor;
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
  int* source_failed;
} tc_x509_crl_extra_storage;

/* Caller guards extra and validates the node array/count before inspecting bytes. */
TC_TLV_result tc_x509_crl_extra_storage_inputs(const tc_x509_crl_extra_storage* extra,
    const TC_bytes* writes, size_t write_count, size_t* work);

/* Guard borrowed certificate bytes before adding a dependency. */
TC_TLV_result tc_x509_crl_dependency_add(tc_x509_crl_dependencies* dependencies,
    TC_bytes certificate, size_t* work, size_t* index);

/* Anchor signatures establish a root; other signer paths need resolved dependencies. */
TC_X509_path_status tc_x509_crl_dependencies_check(void* context,
    const TC_X509_search_result* path, const tc_x509_crl_selected* selected, size_t* work);

/* Reuse an encoded certificate's node or append a borrowed, undetermined node.
 * Caller guards certificate bytes against writable storage and keeps existing
 * nodes valid. Failures preserve nodes, count and index; work is provisional. */
TC_TLV_result tc_x509_crl_dependency_find(TC_X509_revocation_node* nodes,
    size_t capacity, size_t* count, TC_bytes certificate, size_t* work, size_t* index);

/* Both effective records must verify with the anchor to bypass signer paths. */
TC_X509_signature_result tc_x509_crl_selected_anchor_check(const tc_x509_crl_selected* selected,
    const TC_X509_trust_anchor* anchor, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work);

/* Extend provisional dependency nodes and check each signer's current status.
 * Caller validates metadata storage; writes covers every writable operation span. */
TC_X509_path_status tc_x509_crl_dependencies_path(const TC_X509_search_result* path,
    const tc_x509_crl_resolution_workspace* workspace, size_t* count,
    const TC_bytes* writes, size_t write_count, size_t* work);

typedef TC_TLV_result (*tc_x509_crl_node_evaluate)(void* context, size_t index,
    tc_x509_crl_evidence* evidence, int* stop);
/* Retry pending nodes while evaluation discovers dependencies or resolves nodes.
 * Evaluators append within capacity and consume work. Nodes remain provisional;
 * OK publishes determined root evidence. stop marks an operation-wide failure. */
TC_TLV_result tc_x509_crl_nodes_resolve(TC_X509_revocation_node* nodes, size_t capacity,
    size_t* count, size_t root, size_t* work, tc_x509_crl_node_evaluate evaluate,
    void* context, tc_x509_crl_evidence* out);

/* Resolve a guarded target, reusing proven dependencies within the held path.
 * Caller prepares the source/anchor and checks operation storage first. */
TC_TLV_result tc_x509_crl_resolve_dependencies(TC_bytes target,
    tc_x509_crl_dependencies* dependencies, tc_x509_crl_node_evaluate evaluate,
    void* context, tc_x509_crl_held_path* path, tc_x509_crl_evidence* out);

typedef TC_TLV_result (*tc_x509_crl_certificate_resolve)(void* context,
    TC_bytes certificate, tc_x509_crl_evidence* evidence);
/* Resolve an anchor-issued-first chain. resolve guards the whole held chain
 * before reading bytes or using scratch. Publish only determined evidence. */
TC_TLV_result tc_x509_crl_path_resolve(const TC_bytes* chain, size_t count,
    tc_x509_crl_certificate_resolve resolve, void* context, TC_X509_revocation_result* out);

enum {
  CRL_SCOPE_PATH = TC_X509_PATH_STORAGE_COUNT, CRL_SCOPE_SEARCH, CRL_SCOPE_TREE,
  CRL_SCOPE_STATES, CRL_SCOPE_EVIDENCE, CRL_SCOPE_RESULT, CRL_SCOPE_WORK,
  CRL_SCOPE_NODES, CRL_SCOPE_OUTPUT, CRL_SCOPE_PATH_OUTPUT, CRL_SCOPE_WRITES
};

/* Run a scope operation after storage preflight. Sources and input bytes stay
 * fixed; source_failed accumulates callback failures across dependency searches. */
TC_TLV_result tc_x509_crl_scope_run(const tc_x509_crl_candidate_source* candidates,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    const TC_bytes* points, int from_certificate, int all_scopes,
    const TC_bytes writes[CRL_SCOPE_WRITES], int* source_failed, TC_X509_search_result* out);

/* Preflight and run one scope through a prepared candidate adapter. */
TC_TLV_result tc_x509_crl_scope_execute(const tc_x509_crl_operation_source* candidates,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    const TC_bytes* points, int from_certificate, int all_scopes,
    const tc_x509_crl_extra_storage* extra, const tc_x509_crl_held_path* path,
    TC_X509_search_result* out);

/* Resolve a parsed target or held path through the shared, non-recursive
 * dependency operation. The path form reuses proven nodes across members. */
TC_TLV_result tc_x509_crl_resolve(const TC_X509_certificate* target,
    const tc_x509_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace,
    tc_x509_crl_held_path* path, tc_x509_crl_evidence* out);
TC_TLV_result tc_x509_crl_path_operation(tc_x509_crl_held_path* held,
    const tc_x509_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace);

/* Append operation outputs to the prepared scope spans and check all overlaps.
 * Writes and work are provisional; caller commits its work budget after input checks. */
TC_TLV_result tc_x509_crl_scope_storage_check(const tc_x509_crl_extra_storage* extra,
    const tc_x509_crl_held_path* path, TC_bytes writes[CRL_SCOPE_WRITES], size_t* work);
/* Describe validation/search/scope storage through CRL_SCOPE_WORK. The caller
 * supplies remaining operation spans and checks all input/output overlaps. */
TC_TLV_result tc_x509_crl_scope_storage_writes(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, TC_X509_search_result* out,
    TC_bytes writes[CRL_SCOPE_NODES]);

/* Check all borrowed record fields against operation writes. The caller
 * validates index/record metadata storage before visiting its byte spans. */
TC_TLV_result tc_x509_crl_index_storage_bytes(const TC_X509_crl_index* index,
    const TC_bytes* writes, size_t write_count, size_t* work);

/* Guard shared scope/query/policy metadata and borrowed bytes against writes.
 * Work is provisional; the operation commits its budget after all guards pass. */
TC_TLV_result tc_x509_crl_scope_storage_inputs(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, const TC_bytes* writes, size_t write_count, size_t* work);

/* Match issuer, authority identifiers and cRLSign usage on parsed inputs.
 * Signature and path checks follow candidate selection. matched changes on OK;
 * name/tree scratch and work are separate from inputs and output. */
TC_TLV_result tc_x509_crl_candidate_matches(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_certificate* candidate,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names,
    const tc_pki_tree_workspace* tree, int* matched);

typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_certificate* signer;
  const TC_X509_signature_provider* provider;
  const TC_TLV_limits* limits;
  const TC_X509_name_workspace* names;
  uint8_t* states;
  size_t capacity;
} tc_x509_crl_signature_cache;
/* One byte per indexed record, scoped to this signer/provider and stable input
 * snapshot. Cache valid/invalid signatures only; limits and provider errors can
 * be retried. This does not cache signer trust, CRL policy or freshness.
 * Reinitialize when the signer, provider policy or indexed records change.
 * Caller keeps metadata/record bytes immutable and disjoint from states/work/out
 * and name scratch. Initialization charges one work unit per record. */
TC_TLV_result tc_x509_crl_signature_cache_init(const TC_X509_crl_index* index,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names,
    uint8_t* states, size_t capacity, size_t* work, tc_x509_crl_signature_cache* out);
TC_TLV_result tc_x509_crl_signature_cached(const tc_x509_crl_signature_cache* cache,
    size_t record, size_t* work);
/* Enumerate compatible deltas for one complete CRL in source order. Start
 * cursor at zero. Policy-rejected rows are skipped but remain in the index.
 * Each candidate needs authentication, freshness and scope checks before use.
 * Stable parsed index; disjoint scratch/output. Cursor/out change only on OK. */
TC_TLV_result tc_x509_crl_delta_next(const TC_X509_crl_index* index, size_t base,
    size_t* cursor, const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, tc_x509_crl_selected* out);
/* Select the highest-numbered current delta signed by the validated base's
 * signer. Caller has authenticated the base and established signer trust.
 * Bad signatures are skipped; provider failures and limits stop selection.
 * Conflicting TBS bytes at the highest authenticated number are INVALID.
 * END means no usable delta. Scope, entries and signer revocation are separate.
 * Stable/disjoint inputs and scratch; out changes only on OK. */
TC_TLV_result tc_x509_crl_delta_select(const TC_X509_crl_index* index, size_t base,
    const TC_X509_certificate* signer, const TC_X509_time* at,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    tc_x509_crl_selected* out);
/* Same selection using the cache's signer/provider and parsing workspace. */
TC_TLV_result tc_x509_crl_delta_select_cached(const tc_x509_crl_signature_cache* cache,
    size_t base, const TC_X509_time* at, const tc_pki_tree_workspace* tree,
    tc_x509_crl_selected* out);
/* Enumerate authenticated effective candidates in one issuer/IDP scope.
 * reference identifies the scope; cursor starts at zero. Signature cache is
 * scoped to the proposed signer, whose trust remains the caller's responsibility.
 * Stale bases need a current delta; future bases are skipped. Entry and target
 * coverage checks follow selection. Cursor/out change only on OK; cache/work
 * and name scratch are provisional. Does not rank candidates across bases. */
TC_TLV_result tc_x509_crl_effective_next(const tc_x509_crl_signature_cache* cache,
    size_t reference, size_t* cursor, TC_X509_crl_delta_policy delta_policy,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, tc_x509_crl_selected* out);
/* Highest authenticated effective number in this scope. A delta supplies its
 * number, not its base's. Unnumbered candidates return UNSUPPORTED; END means
 * no eligible candidate. Output borrows number contents and changes only on OK.
 * Conflicting deltas return INVALID only at the highest effective number.
 * Tied candidates still need entry consistency checks before evidence is applied. */
TC_TLV_result tc_x509_crl_latest_number(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, const TC_X509_time* at,
    const tc_pki_tree_workspace* tree, TC_bytes* out);
/* Apply CRLs from one scope after signer trust has been established.
 * ORDER_NUMBER requires CRLNumber. ORDER_THIS_UPDATE explicitly orders all
 * effective candidates by thisUpdate, including legacy unnumbered complete CRLs.
 * Delta pairing still requires numbers. There is no automatic ordering fallback.
 * First find the latest candidate, then require tied candidates to agree
 * on thisUpdate, reason coverage and the target's revocation information.
 * Evidence changes only on OK; input/cache/scratch storage is disjoint and stable.
 * Signer-path revocation remains separate. Cache/work/scratch are provisional. */
TC_TLV_result tc_x509_crl_scope_apply(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, TC_X509_crl_order_policy order_policy,
    const tc_x509_crl_query* query,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, TC_bytes* oids,
    size_t oid_capacity, tc_x509_crl_evidence* evidence);

/* Apply the newest authenticated CRLs and retain their selection rank. */
TC_TLV_result tc_x509_crl_scope_evaluate(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, TC_X509_crl_order_policy order_policy,
    const tc_x509_crl_query* query,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, TC_bytes* oids,
    size_t oid_capacity, tc_x509_crl_evidence* evidence, tc_x509_crl_selected* preference);

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
    const tc_x509_crl_selected* right, TC_X509_crl_order_policy policy, size_t* work, int* order)
{
  if (policy == TC_X509_CRL_ORDER_NUMBER) {
    const tc_x509_crl_extension_info* a = left->delta ? left->delta_info : left->base_info;
    const tc_x509_crl_extension_info* b = right->delta ? right->delta_info : right->base_info;
    if (!(a->present & TC_CRL_EXT_NUMBER) || !(b->present & TC_CRL_EXT_NUMBER)) return TC_TLV_UNSUPPORTED;
    return tc_x509_crl_number_compare(a->number,b->number,work,order);
  }
  const tc_x509_crl* a = left->delta ? left->delta : left->base;
  const tc_x509_crl* b = right->delta ? right->delta : right->base;
  TC_TLV_result result = tc_x509_path_charge(work,1);
  return result == TC_TLV_OK ? TC_X509_time_compare(&a->this_update,&b->this_update,order) : result;
}

#endif
