/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_PATH
#include "x509_policy_internal.h"
#include "pki_internal.h"
#include "string_internal.h"

static const uint8_t any_oid[] = {0x55,0x1d,0x20,0};
static int is_any(TC_bytes oid)
{
  return oid.length == sizeof any_oid && memcmp(oid.data, any_oid, sizeof any_oid) == 0;
}
static TC_TLV_result charge(size_t* work, size_t amount)
{
  if (*work < amount) { *work = 0; return TC_TLV_LIMIT; }
  *work -= amount;
  return TC_TLV_OK;
}
static TC_TLV_result equal(TC_bytes a, TC_bytes b, size_t* work, int* same)
{
  TC_TLV_result result = charge(work, 1);
  if (result != TC_TLV_OK) return result;
  *same = 0;
  if (a.length != b.length) return TC_TLV_OK;
  result = charge(work, a.length);
  if (result != TC_TLV_OK) return result;
  *same = tc_pki_equal(a, b);
  return TC_TLV_OK;
}
static TC_TLV_result find(const tc_x509_policy_graph* graph, size_t depth,
    TC_bytes oid, size_t* work, size_t* index)
{
  size_t i;
  for (i = 0; i < graph->node_count; ++i) {
    int same;
    TC_TLV_result result = charge(work, 1);
    if (result != TC_TLV_OK) return result;
    if (!graph->nodes[i].alive || graph->nodes[i].depth != depth) continue;
    result = equal(graph->nodes[i].oid, oid, work, &same);
    if (result != TC_TLV_OK) return result;
    if (same) { *index = i; return TC_TLV_OK; }
  }
  *index = SIZE_MAX;
  return TC_TLV_OK;
}
static TC_TLV_result node(tc_x509_policy_graph* graph, size_t depth,
    TC_bytes oid, size_t* work, size_t* index)
{
  TC_TLV_result result = charge(work, 1);
  tc_x509_policy_node* added;
  if (result != TC_TLV_OK) return result;
  if (graph->node_count == graph->node_capacity) return TC_TLV_LIMIT;
  *index = graph->node_count++;
  added = &graph->nodes[*index];
  added->oid = oid; added->depth = depth; added->alive = 1; added->mapped = 0;
  return TC_TLV_OK;
}
static TC_TLV_result link(tc_x509_policy_graph* graph, size_t parent, size_t child, size_t* work)
{
  TC_TLV_result result = charge(work, 1);
  if (result != TC_TLV_OK) return result;
  if (graph->edge_count == graph->edge_capacity) return TC_TLV_LIMIT;
  graph->edges[graph->edge_count].parent = parent;
  graph->edges[graph->edge_count++].child = child;
  return TC_TLV_OK;
}
static TC_TLV_result expects(const tc_x509_policy_graph* graph, size_t parent,
    TC_bytes oid, size_t* work, int* matched)
{
  size_t i;
  *matched = 0;
  if (!graph->nodes[parent].mapped) return equal(graph->nodes[parent].oid, oid, work, matched);
  for (i = 0; i < graph->expected_count; ++i) {
    TC_TLV_result result = charge(work, 1);
    if (result != TC_TLV_OK) return result;
    if (graph->expected[i].node != parent) continue;
    result = equal(graph->expected[i].oid, oid, work, matched);
    if (result != TC_TLV_OK || *matched) return result;
  }
  return TC_TLV_OK;
}
static TC_TLV_result child(tc_x509_policy_graph* graph, TC_bytes oid, size_t* work)
{
  size_t i, index, previous = graph->node_count, fallback = SIZE_MAX;
  TC_TLV_result result = find(graph, graph->depth, oid, work, &index);
  if (result != TC_TLV_OK || index != SIZE_MAX) return result;
  for (i = 0; i < previous; ++i) {
    int matched;
    result = charge(work, 1);
    if (result != TC_TLV_OK) return result;
    if (!graph->nodes[i].alive || graph->nodes[i].depth != graph->depth - 1) continue;
    if (is_any(graph->nodes[i].oid)) fallback = i;
    result = expects(graph, i, oid, work, &matched);
    if (result != TC_TLV_OK) return result;
    if (!matched) continue;
    if (index == SIZE_MAX) {
      result = node(graph, graph->depth, oid, work, &index);
      if (result != TC_TLV_OK) return result;
    }
    result = link(graph, i, index, work);
    if (result != TC_TLV_OK) return result;
  }
  if (index == SIZE_MAX && fallback != SIZE_MAX) {
    result = node(graph, graph->depth, oid, work, &index);
    if (result != TC_TLV_OK) return result;
    return link(graph, fallback, index, work);
  }
  return TC_TLV_OK;
}
static TC_TLV_result prune(tc_x509_policy_graph* graph, size_t* work)
{
  size_t i = graph->node_count;
  while (i) {
    size_t j;
    int has_child = 0;
    TC_TLV_result result = charge(work, 1);
    if (result != TC_TLV_OK) return result;
    --i;
    if (!graph->nodes[i].alive || graph->nodes[i].depth == graph->depth) continue;
    for (j = 0; j < graph->edge_count; ++j) {
      result = charge(work, 1);
      if (result != TC_TLV_OK) return result;
      if (graph->edges[j].parent == i && graph->nodes[graph->edges[j].child].alive) {
        has_child = 1; break;
      }
    }
    if (!has_child) graph->nodes[i].alive = 0;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_policy_graph_init(tc_x509_policy_graph* graph)
{
  if (!graph) return TC_TLV_ARGUMENT;
  if (!graph->node_capacity) return TC_TLV_LIMIT;
  if (!graph->nodes || (graph->edge_capacity && !graph->edges)
      || (graph->expected_capacity && !graph->expected)) return TC_TLV_ARGUMENT;
  graph->node_count = 1; graph->edge_count = graph->expected_count = graph->depth = 0;
  graph->nodes[0].oid.data = any_oid; graph->nodes[0].oid.length = sizeof any_oid;
  graph->nodes[0].depth = 0; graph->nodes[0].alive = 1; graph->nodes[0].mapped = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_policy_graph_step(tc_x509_policy_graph* graph,
    const TC_bytes* policies, size_t policy_count,
    const TC_X509_policy_mapping* mappings, size_t mapping_count,
    int allow_any, int allow_mapping, size_t* work)
{
  size_t i, j, previous;
  TC_TLV_result result;
  int any = 0;
  if (!graph || !work || (policy_count && !policies) || (mapping_count && !mappings)) return TC_TLV_ARGUMENT;
  if (graph->depth == SIZE_MAX) return TC_TLV_LIMIT;
  previous = graph->node_count; ++graph->depth;
  for (i = 0; i < policy_count; ++i) {
    if (!policies[i].data || !policies[i].length) return TC_TLV_ARGUMENT;
    result = charge(work, 1);
    if (result != TC_TLV_OK) return result;
    for (j = 0; j < i; ++j) {
      int same;
      result = equal(policies[i], policies[j], work, &same);
      if (result != TC_TLV_OK) return result;
      if (same) return TC_TLV_INVALID;
    }
    if (is_any(policies[i])) { any = 1; continue; }
    result = child(graph, policies[i], work);
    if (result != TC_TLV_OK) return result;
  }
  if (any && allow_any) {
    for (i = 0; i < previous; ++i) {
      result = charge(work, 1);
      if (result != TC_TLV_OK) return result;
      if (!graph->nodes[i].alive || graph->nodes[i].depth != graph->depth - 1) continue;
      if (!graph->nodes[i].mapped) {
        result = child(graph, graph->nodes[i].oid, work);
        if (result != TC_TLV_OK) return result;
      } else for (j = 0; j < graph->expected_count; ++j) {
        result = charge(work, 1);
        if (result != TC_TLV_OK) return result;
        if (graph->expected[j].node != i) continue;
        result = child(graph, graph->expected[j].oid, work);
        if (result != TC_TLV_OK) return result;
      }
    }
  }
  result = prune(graph, work);
  if (result != TC_TLV_OK) return result;
  return tc_x509_policy_graph_map(graph, mappings, mapping_count, allow_mapping, work);
}

TC_TLV_result tc_x509_policy_graph_map(tc_x509_policy_graph* graph,
    const TC_X509_policy_mapping* mappings, size_t mapping_count,
    int allow_mapping, size_t* work)
{
  size_t i, j;
  TC_TLV_result result;
  if (!graph || !work || (mapping_count && !mappings)) return TC_TLV_ARGUMENT;
  for (i = 0; i < mapping_count; ++i) {
    size_t index;
    const TC_X509_policy_mapping* mapping = &mappings[i];
    if (!mapping->issuer_policy.data || !mapping->issuer_policy.length
        || !mapping->subject_policy.data || !mapping->subject_policy.length) return TC_TLV_ARGUMENT;
    if (is_any(mapping->issuer_policy) || is_any(mapping->subject_policy)) return TC_TLV_INVALID;
    result = find(graph, graph->depth, mapping->issuer_policy, work, &index);
    if (result != TC_TLV_OK) return result;
    if (!allow_mapping) {
      if (index != SIZE_MAX) graph->nodes[index].alive = 0;
      continue;
    }
    if (index == SIZE_MAX) {
      TC_bytes oid = {any_oid,sizeof any_oid};
      size_t wildcard, parent;
      result = find(graph, graph->depth, oid, work, &wildcard);
      if (result != TC_TLV_OK) return result;
      if (wildcard == SIZE_MAX) continue;
      result = find(graph, graph->depth - 1, oid, work, &parent);
      if (result != TC_TLV_OK) return result;
      if (parent == SIZE_MAX) return TC_TLV_INVALID;
      result = node(graph, graph->depth, mapping->issuer_policy, work, &index);
      if (result != TC_TLV_OK) return result;
      result = link(graph, parent, index, work);
      if (result != TC_TLV_OK) return result;
    }
    graph->nodes[index].mapped = 1;
    for (j = 0; j < graph->expected_count; ++j) {
      int same;
      result = charge(work, 1);
      if (result != TC_TLV_OK) return result;
      if (graph->expected[j].node != index) continue;
      result = equal(graph->expected[j].oid, mapping->subject_policy, work, &same);
      if (result != TC_TLV_OK) return result;
      if (same) break;
    }
    if (j == graph->expected_count) {
      if (j == graph->expected_capacity) return TC_TLV_LIMIT;
      graph->expected[j].node = index; graph->expected[j].oid = mapping->subject_policy;
      ++graph->expected_count;
    }
  }
  return prune(graph, work);
}
static TC_TLV_result append_policy(TC_bytes oid, TC_bytes* output,
    size_t capacity, size_t* count, size_t* work)
{
  size_t i;
  for (i = 0; i < *count; ++i) {
    int same;
    TC_TLV_result result = equal(output[i], oid, work, &same);
    if (result != TC_TLV_OK || same) return result;
  }
  if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (*count == capacity) return TC_TLV_LIMIT;
  output[(*count)++] = oid;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_policy_graph_output(const tc_x509_policy_graph* graph,
    const TC_bytes* initial, size_t initial_count, TC_bytes* output,
    size_t capacity, size_t* work, size_t* count)
{
  size_t i, j, used = 0;
  int unrestricted = 0;
  if (!graph || !work || !count || (initial_count && !initial) || (capacity && !output)) return TC_TLV_ARGUMENT;
  for (i = 0; i < initial_count; ++i) {
    if (!initial[i].data || !initial[i].length) return TC_TLV_ARGUMENT;
    if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (is_any(initial[i])) unrestricted = 1;
  }
  for (i = 0; i < graph->node_count; ++i) {
    const tc_x509_policy_node* node = &graph->nodes[i];
    TC_TLV_result result;
    int authority = 0, wildcard;
    if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!node->alive) continue;
    wildcard = is_any(node->oid);
    if (wildcard) authority = node->depth == graph->depth;
    else for (j = 0; j < graph->edge_count; ++j) {
      if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      if (graph->edges[j].child == i && is_any(graph->nodes[graph->edges[j].parent].oid)) {
        authority = 1; break;
      }
    }
    if (!authority) continue;
    if (unrestricted) {
      result = append_policy(node->oid, output, capacity, &used, work);
      if (result != TC_TLV_OK) return result;
    } else for (j = 0; j < initial_count; ++j) {
      int same = wildcard;
      if (!wildcard) {
        result = equal(node->oid, initial[j], work, &same);
        if (result != TC_TLV_OK) return result;
      }
      if (!same) continue;
      result = append_policy(initial[j], output, capacity, &used, work);
      if (result != TC_TLV_OK) return result;
    }
  }
  *count = used;
  return TC_TLV_OK;
}
static TC_TLV_result qualifier_next(TC_TLV_reader* reader, TC_TLV_limits* budget,
    size_t* work, TC_TLV_element* element)
{
  TC_TLV_result result;
  if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (tc_pki_end(reader)) return TC_TLV_END;
  if (!budget->max_elements) return TC_TLV_LIMIT;
  result = TC_TLV_next(reader, element);
  if (result == TC_TLV_OK) --budget->max_elements;
  return result;
}
static TC_TLV_result qualifier_contents(const TC_TLV_element* element, size_t depth,
    const TC_TLV_limits* budget, TC_TLV_reader* reader)
{
  if (!tc_pki_tag(element, 0x30)) return TC_TLV_INVALID;
  if (depth > budget->max_depth) return TC_TLV_LIMIT;
  return TC_TLV_reader_init(reader, element->value.data, element->value.length, TC_TLV_DER, budget);
}
static TC_TLV_result display_text(const TC_TLV_element* element, size_t* work)
{
  size_t offset = 0;
  uint32_t point;
  unsigned tag;
  if (element->header.tag_length != 1 || !element->value.length) return TC_TLV_INVALID;
  tag = element->header.tag[0];
  if (tag != 0x0c && tag != 0x16 && tag != 0x1a && tag != 0x1e) return TC_TLV_INVALID;
  /* RFC 5280 asks readers to tolerate notices longer than 200 characters. */
  while (offset < element->value.length) {
    TC_TLV_result result;
    if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = tc_asn1_string_next(tag, element->value, &offset, &point);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}
static TC_TLV_result user_notice(TC_bytes encoded, const TC_TLV_limits* limits, size_t* work)
{
  TC_TLV_limits budget = *limits;
  TC_TLV_reader outer, notice, reference, numbers;
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_reader_init(&outer, encoded.data, encoded.length, TC_TLV_DER, limits);
  if (result != TC_TLV_OK) return result;
  result = qualifier_next(&outer, &budget, work, &element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_end(&outer)) return TC_TLV_INVALID;
  result = qualifier_contents(&element, 1, &budget, &notice);
  if (result != TC_TLV_OK) return result;
  result = qualifier_next(&notice, &budget, work, &element);
  if (result == TC_TLV_END) return TC_TLV_OK;
  if (result != TC_TLV_OK) return result;
  if (tc_pki_tag(&element, 0x30)) {
    result = qualifier_contents(&element, 2, &budget, &reference);
    if (result != TC_TLV_OK) return result;
    result = qualifier_next(&reference, &budget, work, &element);
    if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
    result = display_text(&element, work);
    if (result != TC_TLV_OK) return result;
    result = qualifier_next(&reference, &budget, work, &element);
    if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
    result = qualifier_contents(&element, 3, &budget, &numbers);
    if (result != TC_TLV_OK) return result;
    if (!tc_pki_end(&reference)) return TC_TLV_INVALID;
    while ((result = qualifier_next(&numbers, &budget, work, &element)) == TC_TLV_OK) {
      if (!tc_pki_tag(&element, 2)) return TC_TLV_INVALID;
      result = TC_DER_integer_contents(element.value.data, element.value.length);
      if (result != TC_TLV_OK) return result;
    }
    if (result != TC_TLV_END) return result;
    result = qualifier_next(&notice, &budget, work, &element);
    if (result == TC_TLV_END) return TC_TLV_OK;
    if (result != TC_TLV_OK) return result;
  }
  result = display_text(&element, work);
  if (result != TC_TLV_OK) return result;
  return tc_pki_end(&notice) ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_x509_policy_qualifiers_check(const TC_X509_policy* policy,
    int critical, const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t capacity, size_t* work)
{
  static const uint8_t prefix[] = {0x2b,6,1,5,5,7,2};
  TC_TLV_reader reader;
  TC_X509_policy_qualifier qualifier;
  TC_TLV_result result;
  if (!policy || !limits || !work) return TC_TLV_ARGUMENT;
  if (charge(work, policy->qualifiers.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_walk(policy->qualifiers.data, policy->qualifiers.length,
    TC_TLV_DER, limits, frames, capacity, NULL, NULL);
  if (result != TC_TLV_OK) return result;
  result = TC_X509_policy_qualifiers_init(&reader, policy->qualifiers, limits);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_X509_policy_qualifier_next(&reader, &qualifier)) == TC_TLV_OK) {
    unsigned kind = 0;
    if (charge(work, qualifier.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (qualifier.oid.length == sizeof prefix + 1 && !memcmp(qualifier.oid.data, prefix, sizeof prefix))
      kind = qualifier.oid.data[sizeof prefix];
    if (kind == 1) {
      TC_TLV_reader value;
      TC_TLV_element element;
      size_t offset = 0;
      uint32_t point;
      result = TC_TLV_reader_init(&value, qualifier.value.data, qualifier.value.length, TC_TLV_DER, limits);
      if (result != TC_TLV_OK) return result;
      result = tc_pki_next(&value, 0x16, &element);
      if (result != TC_TLV_OK) return result;
      if (!tc_pki_end(&value)) return TC_TLV_INVALID;
      while (offset < element.value.length) {
        if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
        result = tc_asn1_string_next(0x16, element.value, &offset, &point);
        if (result != TC_TLV_OK) return result;
      }
    } else if (kind == 2) {
      result = user_notice(qualifier.value, limits, work);
      if (result != TC_TLV_OK) return result;
    } else if (critical || is_any(policy->oid)) return TC_TLV_UNSUPPORTED;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}
#endif
