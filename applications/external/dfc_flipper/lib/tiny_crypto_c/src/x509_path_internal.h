/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_PATH_INTERNAL_H_
#define TC_X509_PATH_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_store.h>
#include "x509_policy_internal.h"

/* An unevaluated branch prevents a definitive no-path result. */
static inline void tc_x509_path_remember(TC_X509_path_status status, TC_X509_path_status* failure)
{
  if (status == TC_X509_PATH_LIMIT ||
      (status == TC_X509_PATH_UNSUPPORTED && *failure != TC_X509_PATH_LIMIT)) *failure = status;
}

static inline TC_TLV_result tc_x509_path_charge(size_t* work, size_t amount)
{
  if (amount > *work) { *work = 0; return TC_TLV_LIMIT; }
  *work -= amount;
  return TC_TLV_OK;
}

static inline TC_X509_path_status tc_x509_path_status(TC_TLV_result result)
{
  switch (result) {
    case TC_TLV_OK: return TC_X509_PATH_VALID;
    case TC_TLV_LIMIT: return TC_X509_PATH_LIMIT;
    case TC_TLV_UNSUPPORTED: return TC_X509_PATH_UNSUPPORTED;
    case TC_TLV_ARGUMENT: return TC_X509_PATH_ERROR;
    default: return TC_X509_PATH_INVALID;
  }
}

static inline TC_TLV_result tc_x509_path_result_status(TC_X509_path_status status)
{
  switch (status) {
    case TC_X509_PATH_VALID: return TC_TLV_OK;
    case TC_X509_PATH_INVALID: return TC_TLV_INVALID;
    case TC_X509_PATH_UNSUPPORTED: return TC_TLV_UNSUPPORTED;
    case TC_X509_PATH_LIMIT: return TC_TLV_LIMIT;
    default: return TC_TLV_ARGUMENT;
  }
}

/* Shared search budget, consumed on failed candidates too. work must be disjoint
 * from all inputs and scratch. Result work_used counts only this candidate. */
TC_X509_path_status tc_x509_path_validate_budget(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, size_t* work, TC_X509_path_result* out);
/* Additional borrowed constraints are disjoint from scratch and all outputs. */
TC_X509_path_status tc_x509_path_validate_anchor(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_name_constraints* anchor_names,
    const TC_X509_path_options* options, const TC_X509_path_workspace* workspace,
    size_t* work, TC_X509_path_result* out);

typedef TC_X509_search_frame tc_x509_search_frame;
typedef TC_X509_search_workspace tc_x509_search_workspace;
typedef TC_X509_search_result tc_x509_search_result;
enum {
  TC_X509_PATH_STORAGE_FRAMES, TC_X509_PATH_STORAGE_OIDS,
  TC_X509_PATH_STORAGE_NAME_LEFT, TC_X509_PATH_STORAGE_NAME_RIGHT,
  TC_X509_PATH_STORAGE_NAME_MATCHED, TC_X509_PATH_STORAGE_NODES,
  TC_X509_PATH_STORAGE_EDGES, TC_X509_PATH_STORAGE_EXPECTED,
  TC_X509_PATH_STORAGE_MAPPINGS, TC_X509_PATH_STORAGE_POLICIES,
  TC_X509_PATH_STORAGE_COUNT
};
/* One checked byte range per validation scratch array. */
TC_TLV_result tc_x509_path_storage_writes(const TC_X509_path_workspace* workspace,
    TC_bytes writes[TC_X509_PATH_STORAGE_COUNT]);
typedef TC_X509_store_anchor tc_x509_search_anchor;
typedef TC_X509_store_source tc_x509_search_source;
/* Callbacks consume work without increasing it. Returned spans reference a stable
 * snapshot through validation and result use. Read errors terminate the search. */
TC_X509_path_status tc_x509_path_search_source(TC_bytes target,
    const tc_x509_search_source* source, const TC_X509_path_options* options,
    const TC_X509_path_workspace* validation, const tc_x509_search_workspace* search,
    size_t* work, tc_x509_search_result* out);
/* Internal engine: caller validates storage ranges and keeps input/store records
 * stable and disjoint from both workspaces, work, and out. Path storage is scratch;
 * successful output borrows its suffix in anchor-issued-first order. */
TC_X509_path_status tc_x509_path_search(TC_bytes target,
    const TC_bytes* candidates, size_t candidate_count,
    const TC_X509_trust_anchor* anchors, size_t anchor_count,
    const TC_X509_path_options* options, const TC_X509_path_workspace* validation,
    const tc_x509_search_workspace* search, size_t* work, tc_x509_search_result* out);

typedef struct {
  /* Anchor-issued certificate first, target last. Optional parsed-view cache. */
  const TC_X509_certificate* certificates;
  size_t count, max_certificates, max_input;
  const TC_X509_trust_anchor* anchor;
  const TC_X509_time* at;
  const TC_X509_signature_provider* signatures;
  const TC_TLV_limits* limits;
  /* Without a cache, reuse parser workspace to read these original DER spans. */
  const TC_bytes* encoded;
  TC_X509_workspace* parser;
} tc_x509_path_input;

/* Basic certificate pass. Does not process policies, name constraints,
 * application usage, critical extensions or revocation. Caller keeps input
 * and provider state disjoint from workspace, work and accepted. */
TC_TLV_result tc_x509_path_basic(const tc_x509_path_input* input,
    const TC_X509_name_workspace* workspace, size_t* work, int* accepted);
/* Apply every issuer's name constraints independently, intersecting permitted
 * sets without allocating a merged subtree list. Run after the basic pass. */
TC_TLV_result tc_x509_path_names(const tc_x509_path_input* input,
    const TC_X509_constraint_workspace* workspace, size_t* work, int* accepted);

typedef struct {
  TC_X509_policy_constraints constraints;
  uint32_t inhibit_any;
  int has_inhibit_any;
} tc_x509_policy_controls;
typedef struct {
  size_t explicit_policy, mapping, any;
} tc_x509_policy_counters;
TC_TLV_result tc_x509_policy_controls_read(const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, size_t* work, tc_x509_policy_controls* out);
/* Apply after processing this certificate's policies and mappings. */
void tc_x509_policy_counters_advance(tc_x509_policy_counters* counters,
    const tc_x509_policy_controls* controls, int self_issued, int target);

typedef struct {
  const TC_bytes* initial;
  size_t initial_count;
  int require_explicit, inhibit_mapping, inhibit_any;
} tc_x509_policy_options;
typedef struct {
  tc_x509_policy_graph* graph;
  TC_bytes* policies;
  size_t policy_capacity;
  TC_X509_policy_mapping* mappings;
  size_t mapping_capacity;
  TC_bytes* output;
  size_t output_capacity;
  const TC_X509_name_workspace* names;
  TC_TLV_frame* frames;
  size_t frame_capacity;
} tc_x509_policy_workspace;
/* Run after signature and CA checks. Workspace is scratch and disjoint from
 * all inputs and result pointers. count and accepted change only on OK. */
TC_TLV_result tc_x509_path_policies(const tc_x509_path_input* input,
    const tc_x509_policy_options* options, const tc_x509_policy_workspace* workspace,
    size_t* work, size_t* count, int* accepted);
typedef struct {
  TC_bytes purpose;
  uint16_t key_usage;
  int require_key_usage, require_extended_key_usage;
  int inhibit_any_purpose;
} tc_x509_path_usage;

enum { TC_X509_PATH_SUPPORTED_FLAGS = TC_X509_PATH_REQUIRE_EXPLICIT_POLICY |
  TC_X509_PATH_INHIBIT_MAPPING | TC_X509_PATH_INHIBIT_ANY_POLICY |
  TC_X509_PATH_REQUIRE_KEY_USAGE | TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
  TC_X509_PATH_INHIBIT_ANY_PURPOSE };
typedef struct {
  TC_bytes* oids;
  size_t oid_capacity;
  TC_X509_constraint_workspace names;
} tc_x509_extension_workspace;
/* Run only after successful basic, names and policy passes. Those passes
 * account for the corresponding critical extensions. Same scratch rules. */
TC_TLV_result tc_x509_path_extensions(const tc_x509_path_input* input,
    const tc_x509_path_usage* usage, const tc_x509_extension_workspace* workspace,
    size_t* work, int* accepted);
/* Checked path discovery using the caller's remaining work instead of
 * options.max_work. Work must be disjoint from inputs, scratch and out.
 * Storage preflight failures leave work unchanged; search failures consume it.
 * Returned work_used covers this call only. */
TC_X509_path_status tc_x509_path_build_work(TC_bytes target,
    const TC_X509_store_source* source, const TC_X509_path_options* options,
    const TC_X509_path_workspace* validation, const TC_X509_search_workspace* search,
    size_t* work, TC_X509_search_result* out);
#endif
