/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_POLICY_INTERNAL_H_
#define TC_X509_POLICY_INTERNAL_H_
#include <tiny_crypto/x509_path.h>

typedef TC_X509_policy_node tc_x509_policy_node;
typedef TC_X509_policy_edge tc_x509_policy_edge;
typedef TC_X509_policy_expected tc_x509_policy_expected;
typedef struct {
  tc_x509_policy_node* nodes;
  size_t node_capacity, node_count;
  tc_x509_policy_edge* edges;
  size_t edge_capacity, edge_count;
  tc_x509_policy_expected* expected;
  size_t expected_capacity, expected_count;
  size_t depth;
} tc_x509_policy_graph;

/* Internal scratch storage. Inputs are decoded OIDs and must not overlap it.
 * On failure discard the graph. OID bytes remain borrowed from certificates. */
TC_TLV_result tc_x509_policy_graph_init(tc_x509_policy_graph* graph);
TC_TLV_result tc_x509_policy_graph_step(tc_x509_policy_graph* graph,
    const TC_bytes* policies, size_t policy_count,
    const TC_X509_policy_mapping* mappings, size_t mapping_count,
    int allow_any, int allow_mapping, size_t* work);
TC_TLV_result tc_x509_policy_graph_map(tc_x509_policy_graph* graph,
    const TC_X509_policy_mapping* mappings, size_t mapping_count,
    int allow_mapping, size_t* work);
/* Output contains OIDs in the caller's initial policy namespace, not mapped
 * leaf OIDs. Output storage is scratch; count changes only on success. */
TC_TLV_result tc_x509_policy_graph_output(const tc_x509_policy_graph* graph,
    const TC_bytes* initial, size_t initial_count, TC_bytes* output,
    size_t capacity, size_t* work, size_t* count);
/* Qualifiers are checked but not included in the policy-set output. */
TC_TLV_result tc_x509_policy_qualifiers_check(const TC_X509_policy* policy,
    int critical, const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t capacity, size_t* work);
#endif
