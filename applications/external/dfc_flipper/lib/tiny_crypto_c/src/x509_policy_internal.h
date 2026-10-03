/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_POLICY_INTERNAL_H_
#define TC_X509_POLICY_INTERNAL_H_
#include <tiny_crypto/x509_path.h>

typedef struct {
  TC_X509_policy_node* nodes;
  size_t node_capacity, node_count;
  TC_X509_policy_edge* edges;
  size_t edge_capacity, edge_count;
  TC_X509_policy_expected* expected;
  size_t expected_capacity, expected_count;
  size_t depth;
} tc_x509_policy_graph;

/* Internal scratch storage. Inputs are decoded OIDs and must not overlap it.
 * On failure discard the graph. OID bytes remain borrowed from certificates. */
TC_TLV_result tc_x509_policy_graph_init(tc_x509_policy_graph* graph);
/* Process one certificate's policies (RFC 5280 section 6.1.3 (d)-(f)) and
 * prune once. policies must be unique, which TC_X509_policy_next enforces
 * when it decodes them. allow_any enables the anyPolicy expansion. */
TC_TLV_result tc_x509_policy_graph_step(tc_x509_policy_graph* graph, const TC_bytes* policies,
                                        size_t policy_count, int allow_any, size_t* work);
/* Apply a CA certificate's policyMappings to the depth added by the last step
 * (RFC 5280 section 6.1.4 (a)-(b)). With allow_mapping clear, mapped nodes
 * are deleted and the graph is pruned again. No mappings charge no work. */
TC_TLV_result tc_x509_policy_graph_map(tc_x509_policy_graph* graph,
                                       const TC_X509_policy_mapping* mappings, size_t mapping_count,
                                       int allow_mapping, size_t* work);
/* Policies a path may report. initial is the user-initial-policy-set, where
 * an empty set means {anyPolicy}. anchor_set is the trust anchor's policy set
 * (SEQUENCE contents), where NULL data means no anchor restriction. */
typedef struct {
  const TC_bytes* initial;
  size_t initial_count;
  TC_bytes anchor_set;
} tc_x509_policy_filter;
/* Output contains OIDs in the caller's initial policy namespace, before leaf
 * mappings. Output storage is scratch. count changes only on success. */
TC_TLV_result tc_x509_policy_graph_output(const tc_x509_policy_graph* graph,
                                          const tc_x509_policy_filter* filter,
                                          const TC_TLV_limits* limits, TC_bytes* output,
                                          size_t capacity, size_t* work, size_t* count);
/* Qualifiers are checked and omitted from the policy-set output. */
TC_TLV_result tc_x509_policy_qualifiers_check(const TC_X509_policy* policy, int critical,
                                              const TC_TLV_limits* limits, TC_TLV_frames frames,
                                              size_t* work);
#endif
