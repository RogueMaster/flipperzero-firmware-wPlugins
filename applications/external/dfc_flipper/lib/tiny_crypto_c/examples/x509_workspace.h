/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_X509_WORKSPACE_H_
#define EXAMPLE_X509_WORKSPACE_H_
#include <tiny_crypto/x509_path.h>

enum { EXAMPLE_X509_PATH_CAPACITY = 4 };
/* Keep this outside a small task stack. One validation at a time per workspace. */
typedef struct {
  TC_TLV_frame frames[16];
  TC_bytes oids[16];
  uint32_t left[64], right[64];
  uint8_t matched[8];
  TC_X509_policy_node nodes[32];
  TC_X509_policy_edge edges[64];
  TC_X509_policy_expected expected[64];
  TC_X509_policy_mapping mappings[16];
  TC_bytes policies[16];
} ExampleX509Workspace;

typedef struct {
  ExampleX509Workspace validation;
  TC_bytes path[EXAMPLE_X509_PATH_CAPACITY];
  TC_X509_search_frame frames[EXAMPLE_X509_PATH_CAPACITY];
} ExampleX509SearchWorkspace;

static inline TC_X509_path_workspace example_x509_workspace(ExampleX509Workspace* storage)
{
  TC_X509_path_workspace workspace = TC_X509_PATH_WORKSPACE_INIT(
      storage->frames,storage->oids,storage->left,storage->right,storage->matched,
      storage->nodes,storage->edges,storage->expected,storage->mappings,storage->policies);
  return workspace;
}

static inline TC_X509_search_workspace example_x509_search_workspace(ExampleX509SearchWorkspace* storage)
{
  TC_X509_search_workspace workspace = {storage->path,storage->frames,
    sizeof storage->path / sizeof storage->path[0]};
  return workspace;
}
#endif
