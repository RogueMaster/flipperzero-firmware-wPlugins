/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Arena layout for the path validation workspace. */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_PATH
#include "x509_path_workspace_internal.h"
#include "internal.h"
#include <stddef.h>

size_t TC_X509_path_workspace_alignment(void)
{
  return offsetof(tc_x509_path_storage_alignment, storage);
}

int tc_x509_path_arena_reserve(size_t* offset, size_t count, size_t width, size_t alignment,
                               size_t* start)
{
  const size_t remainder = *offset % alignment;
  const size_t padding = remainder ? alignment - remainder : 0;
  if (padding > SIZE_MAX - *offset)
    return 0;
  *start = *offset + padding;
  if (count > (SIZE_MAX - *start) / width)
    return 0;
  *offset = *start + count * width;
  return 1;
}

int tc_x509_path_workspace_layout(const TC_X509_path_capacity* c, uint8_t* arena, size_t alignment,
                                  size_t* offset, TC_X509_path_workspace* w)
{
  size_t start;
#define ARRAY(field, type, count)                                                                  \
  do {                                                                                             \
    if (!tc_x509_path_arena_reserve(offset, (count), sizeof(type), alignment, &start))             \
      return 0;                                                                                    \
    w->field = arena && (count) ? (type*)(void*)(arena + start) : NULL;                            \
  } while (0)
  ARRAY(frames.data, TC_TLV_frame, c->frames);
  ARRAY(oids, TC_bytes, c->oids);
  ARRAY(names.left, uint32_t, c->name_scalars);
  ARRAY(names.right, uint32_t, c->name_scalars);
  ARRAY(names.matched, uint8_t, c->name_attributes);
  ARRAY(nodes, TC_X509_policy_node, c->policy_nodes);
  ARRAY(edges, TC_X509_policy_edge, c->policy_edges);
  ARRAY(expected, TC_X509_policy_expected, c->policy_expected);
  ARRAY(mappings, TC_X509_policy_mapping, c->policy_mappings);
  ARRAY(policies, TC_bytes, c->policies);
  ARRAY(certificates, TC_X509_certificate, c->path);
  ARRAY(summaries, TC_X509_extension_summary, c->path);
#undef ARRAY
  w->frames.capacity = c->frames;
  w->oid_capacity = c->oids;
  w->names.scalar_capacity = c->name_scalars;
  w->names.attribute_capacity = c->name_attributes;
  w->node_capacity = c->policy_nodes;
  w->edge_capacity = c->policy_edges;
  w->expected_capacity = c->policy_expected;
  w->mapping_capacity = c->policy_mappings;
  w->policy_capacity = c->policies;
  w->certificate_capacity = c->path;
  w->summary_capacity = c->path;
  return 1;
}

/* Validate the counts and compute the arena size for an aligned base. The
 * sizing pass writes capacities and NULL pointers to scratch. */
static TC_result workspace_size(const TC_X509_path_capacity* c, TC_X509_path_workspace* scratch,
                                size_t* bytes)
{
  size_t offset = 0;
  if (!c->frames || !c->oids || !c->name_scalars || !c->name_attributes || !c->path)
    return TC_RESULT_ARGUMENT;
  if (!tc_x509_path_workspace_layout(c, NULL, TC_X509_path_workspace_alignment(), &offset, scratch))
    return TC_RESULT_LIMIT;
  *bytes = offset;
  return TC_RESULT_OK;
}

TC_result TC_X509_path_workspace_size(const TC_X509_path_capacity* capacity, size_t* bytes)
{
  TC_X509_path_workspace scratch = {0};
  size_t size = 0;
  TC_result result;
  if (!capacity || !bytes)
    return TC_RESULT_ARGUMENT;
  result = workspace_size(capacity, &scratch, &size);
  if (result == TC_RESULT_OK)
    *bytes = size;
  return result;
}

TC_result TC_X509_path_workspace_init(const TC_X509_path_capacity* capacity, TC_buffer arena,
                                      TC_X509_path_workspace* out)
{
  TC_X509_path_workspace workspace = {0};
  size_t size = 0, offset = 0;
  TC_result result;
  if (!out || !capacity || !arena.data ||
      (uintptr_t)arena.data % TC_X509_path_workspace_alignment() ||
      arena.capacity > UINTPTR_MAX - (uintptr_t)arena.data ||
      !tc_internal_ranges_disjoint(arena.data, arena.capacity, out, sizeof *out) ||
      !tc_internal_ranges_disjoint(arena.data, arena.capacity, capacity, sizeof *capacity) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, capacity, sizeof *capacity))
    return TC_RESULT_ARGUMENT;
  result = workspace_size(capacity, &workspace, &size);
  if (result != TC_RESULT_OK)
    return result;
  if (arena.capacity < size)
    return TC_RESULT_LIMIT;
  /* The sizing pass succeeded, so this pass cannot overflow. */
  (void)tc_x509_path_workspace_layout(capacity, arena.data, TC_X509_path_workspace_alignment(),
                                      &offset, &workspace);
  *out = workspace;
  return TC_RESULT_OK;
}
#endif
