/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_PATH_INTERNAL_H_
#define TC_X509_PATH_INTERNAL_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_store.h>
#include "x509_policy_internal.h"
#include "pki_budget_internal.h"
#include "pki_storage_internal.h"
#include "x509_path_status_internal.h"

/* Shared search budget, consumed on failed candidates too. work must be disjoint
 * from all inputs and scratch. Result work_used counts only this candidate. */
TC_X509_path_status tc_x509_path_validate_budget(const TC_bytes* chain, size_t count,
                                                 const TC_X509_trust_anchor* anchor,
                                                 const TC_X509_path_options* options,
                                                 const TC_X509_path_workspace* workspace,
                                                 size_t* work, TC_X509_path_report* out);
/* Additional borrowed constraints are disjoint from scratch and all outputs. */
TC_X509_path_status tc_x509_path_validate_anchor(const TC_bytes* chain, size_t count,
                                                 const TC_X509_store_anchor* anchor,
                                                 const TC_X509_path_options* options,
                                                 const TC_X509_path_workspace* workspace,
                                                 size_t* work, TC_X509_path_report* out);

enum {
  TC_X509_PATH_STORAGE_FRAMES,
  TC_X509_PATH_STORAGE_OIDS,
  TC_X509_PATH_STORAGE_NAME_LEFT,
  TC_X509_PATH_STORAGE_NAME_RIGHT,
  TC_X509_PATH_STORAGE_NAME_MATCHED,
  TC_X509_PATH_STORAGE_NODES,
  TC_X509_PATH_STORAGE_EDGES,
  TC_X509_PATH_STORAGE_EXPECTED,
  TC_X509_PATH_STORAGE_MAPPINGS,
  TC_X509_PATH_STORAGE_POLICIES,
  TC_X509_PATH_STORAGE_CERTIFICATES,
  TC_X509_PATH_STORAGE_SUMMARIES,
  TC_X509_PATH_STORAGE_COUNT
};
/* Record one write per validation scratch array, in TC_X509_PATH_STORAGE_*
 * slot order. The plan must start empty. */
void tc_x509_path_storage_plan(tc_pki_storage_plan* plan, const TC_X509_path_workspace* workspace);
/* Check policy-option bytes: the initial-policy array and entries, purpose and
 * anchor-name constraints. The options object itself needs its own input. */
void tc_x509_policy_plan_inputs(tc_pki_storage_plan* plan, const TC_bytes* initial_policies,
                                size_t initial_policy_count, TC_bytes purpose,
                                const TC_X509_name_constraints* anchor_names);
void tc_x509_path_options_plan_inputs(tc_pki_storage_plan* plan,
                                      const TC_X509_path_options* options);
/* Callbacks consume work without increasing it. Returned spans reference a stable
 * snapshot through validation and result use. Read errors terminate the search. */
TC_X509_path_status tc_x509_path_search_source(TC_bytes target, const TC_X509_store_source* source,
                                               const TC_X509_path_options* options,
                                               const TC_X509_path_workspace* validation,
                                               const TC_X509_search_workspace* search, size_t* work,
                                               TC_X509_search_report* out);

typedef struct {
  /* Anchor-issued certificate first, target last. Optional parsed-view cache. */
  const TC_X509_certificate* certificates;
  size_t count, max_certificates, max_input;
  const TC_X509_trust_anchor* anchor;
  const TC_X509_time* at;
  const TC_X509_signature_provider* signatures;
  const TC_TLV_limits* limits;
  /* Original DER spans and parser for filling cache during the basic pass. */
  const TC_bytes* encoded;
  const TC_X509_workspace* parser;
  /* Filled in order by the basic pass, then exposed through certificates. */
  TC_X509_certificate* cache;
  /* One extension summary per certificate, filled on first use. */
  TC_X509_extension_summary* summaries;
  size_t anchor_path_len;
  int has_anchor_path_len;
  uint32_t clock_skew_seconds;
} tc_x509_path_input;

int tc_x509_path_source_valid(const tc_x509_path_input* input);
/* Parsed view of path entry index, reading into the cache when needed. */
TC_TLV_result tc_x509_path_certificate(const tc_x509_path_input* input, size_t index, size_t* work,
                                       const TC_X509_certificate** certificate);

/* TC_X509_extension_summary slots, one per path-relevant extension. */
enum {
  TC_X509_SUMMARY_KEY_USAGE,
  TC_X509_SUMMARY_SUBJECT_ALT_NAME,
  TC_X509_SUMMARY_BASIC_CONSTRAINTS,
  TC_X509_SUMMARY_NAME_CONSTRAINTS,
  TC_X509_SUMMARY_POLICIES,
  TC_X509_SUMMARY_POLICY_MAPPINGS,
  TC_X509_SUMMARY_POLICY_CONSTRAINTS,
  TC_X509_SUMMARY_EXTENDED_KEY_USAGE,
  TC_X509_SUMMARY_INHIBIT_ANY
};
static inline int tc_x509_summary_has(const TC_X509_extension_summary* summary, unsigned slot)
{
  return (summary->present >> slot) & 1u;
}
static inline int tc_x509_summary_critical(const TC_X509_extension_summary* summary, unsigned slot)
{
  return (summary->critical >> slot) & 1u;
}
/* Record every path-relevant extension in one metered walk. A duplicate
 * extension returns TC_TLV_INVALID. basicConstraints, keyUsage,
 * policyConstraints and inhibitAnyPolicy are decoded into the summary, and
 * the remaining values stay as spans borrowed from the certificate DER.
 * Extension parsing charges work through tc_pki_extension_next. out changes
 * only on TC_TLV_OK. */
TC_TLV_result tc_x509_extensions_summarize(const TC_X509_certificate* certificate,
                                           const TC_TLV_limits* limits, size_t* work,
                                           TC_X509_extension_summary* out);
/* Summary of path entry index, filled on first use together with the
 * self-issued flag of an intermediate. */
TC_TLV_result tc_x509_path_summary(const tc_x509_path_input* input, size_t index,
                                   const TC_X509_name_workspace* names, size_t* work,
                                   const TC_X509_extension_summary** out);
/* Charge and reject unsupported subtree distances in name constraints. */
TC_TLV_result tc_x509_path_constraint_distances(const TC_X509_name_constraints* constraints,
                                                const TC_TLV_limits* limits,
                                                const TC_X509_constraint_workspace* workspace,
                                                size_t* work);

/* Basic certificate pass. Policies, name constraints, application usage,
 * critical extensions and revocation are separate passes. Caller keeps input
 * and provider state disjoint from workspace, work and accepted. */
TC_TLV_result tc_x509_path_basic(const tc_x509_path_input* input,
                                 const TC_X509_name_workspace* workspace, size_t* work,
                                 int* accepted);
/* Apply every issuer's name constraints independently, intersecting permitted
 * sets without allocating a merged subtree list. Run after the basic pass. */
TC_TLV_result tc_x509_path_names(const tc_x509_path_input* input,
                                 const TC_X509_constraint_workspace* workspace, size_t* work,
                                 int* accepted);

typedef struct {
  TC_X509_policy_constraints constraints;
  uint32_t inhibit_any;
  int has_inhibit_any;
} tc_x509_policy_controls;
/* Policy constraints and inhibitAnyPolicy as recorded in a summary. */
static inline void tc_x509_policy_controls_from_summary(const TC_X509_extension_summary* extensions,
                                                        tc_x509_policy_controls* out)
{
  out->constraints = extensions->policy_constraints;
  out->inhibit_any = extensions->inhibit_any;
  out->has_inhibit_any = tc_x509_summary_has(extensions, TC_X509_SUMMARY_INHIBIT_ANY);
}
typedef struct {
  size_t explicit_policy, mapping, any;
} tc_x509_policy_counters;
/* Apply after processing this certificate's policies and mappings. */
void tc_x509_policy_counters_advance(tc_x509_policy_counters* counters,
                                     const tc_x509_policy_controls* controls, int self_issued,
                                     int target);

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
  TC_TLV_frames frames;
} tc_x509_policy_workspace;
/* Run after signature and CA checks. Workspace is scratch and disjoint from
 * all inputs and result pointers. count and accepted change only on OK. */
TC_TLV_result tc_x509_path_policies(const tc_x509_path_input* input,
                                    const tc_x509_policy_options* options,
                                    TC_bytes anchor_policy_set,
                                    const tc_x509_policy_workspace* workspace, size_t* work,
                                    size_t* count, int* accepted);
typedef struct {
  TC_bytes purpose;
  uint16_t key_usage;
  int require_key_usage, require_extended_key_usage;
  int inhibit_any_purpose;
} tc_x509_path_usage;

enum {
  TC_X509_PATH_SUPPORTED_FLAGS =
      TC_X509_PATH_REQUIRE_EXPLICIT_POLICY | TC_X509_PATH_INHIBIT_MAPPING |
      TC_X509_PATH_INHIBIT_ANY_POLICY | TC_X509_PATH_REQUIRE_KEY_USAGE |
      TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE | TC_X509_PATH_INHIBIT_ANY_PURPOSE
};
typedef struct {
  TC_bytes* oids;
  size_t oid_capacity;
  TC_X509_constraint_workspace names;
} tc_x509_extension_workspace;
/* Run only after successful basic, names and policy passes. Those passes
 * account for the corresponding critical extensions. Same scratch rules. */
TC_TLV_result tc_x509_path_extensions(const tc_x509_path_input* input,
                                      const tc_x509_path_usage* usage,
                                      const tc_x509_extension_workspace* workspace, size_t* work,
                                      int* accepted);
/* Checked path discovery using the caller's remaining work instead of
 * options.max_work. Work must be disjoint from inputs, scratch and out.
 * Storage preflight failures leave work unchanged; search failures consume it.
 * Returned work_used covers this call only. */
TC_X509_path_status tc_x509_path_build_work(TC_bytes target, const TC_X509_store_source* source,
                                            const TC_X509_path_options* options,
                                            const TC_X509_path_workspace* validation,
                                            const TC_X509_search_workspace* search, size_t* work,
                                            TC_X509_search_report* out);
#endif
