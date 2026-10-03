/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "x509_client.h"
#include <string.h>

static void client_options(TC_X509_path_options* options, const TC_X509_time* at,
                           const TC_X509_signature_provider* verifier, size_t work_limit)
{
  static const uint8_t client_auth[] = {0x2b, 6, 1, 5, 5, 7, 3, 2};
  static const uint8_t any_policy[] = {0x55, 0x1d, 0x20, 0};
  static const TC_bytes initial_policy = {any_policy, sizeof any_policy};
  memset(options, 0, sizeof *options);
  options->at = *at;
  options->signatures = *verifier;
  options->parsing.max_input = options->parsing.max_value = 4096;
  options->parsing.max_elements = 256;
  options->parsing.max_depth = 16;
  options->max_certificates = EXAMPLE_CLIENT_PATH_CAPACITY;
  options->max_input = 16384;
  options->max_work = work_limit;
  options->initial_policies = &initial_policy;
  options->initial_policy_count = 1;
  options->purpose.data = client_auth;
  options->purpose.length = sizeof client_auth;
  options->key_usage = TC_KEY_USAGE_DIGITAL_SIGNATURE;
}

/* Graph, name and policy capacities for four-certificate client paths. */
static const TC_X509_path_capacity client_capacity = {.frames = 16,
                                                      .oids = 16,
                                                      .name_scalars = 64,
                                                      .name_attributes = 8,
                                                      .policy_nodes = 32,
                                                      .policy_edges = 64,
                                                      .policy_expected = 64,
                                                      .policy_mappings = 16,
                                                      .policies = 16,
                                                      .path = EXAMPLE_CLIENT_PATH_CAPACITY};

TC_result example_client_workspace_size(size_t* bytes)
{
  return TC_X509_path_workspace_size(&client_capacity, bytes);
}

/* Lay out the validation workspace in arena. */
static TC_X509_path_status client_workspace(TC_buffer arena, TC_X509_path_workspace* workspace)
{
  switch (TC_X509_path_workspace_init(&client_capacity, arena, workspace)) {
  case TC_RESULT_OK:
    return TC_X509_PATH_VALID;
  case TC_RESULT_LIMIT:
    return TC_X509_PATH_LIMIT;
  default:
    return TC_X509_PATH_ERROR;
  }
}

TC_X509_path_status example_check_client_certificate(const TC_bytes* chain, size_t count,
                                                     const TC_X509_trust_anchor* anchor,
                                                     const TC_X509_time* at,
                                                     const TC_X509_signature_provider* verifier,
                                                     size_t work_limit, TC_buffer arena,
                                                     TC_X509_path_report* result)
{
  TC_X509_path_options options;
  TC_X509_path_workspace workspace;
  TC_X509_path_status status;
  if (!at || !verifier)
    return TC_X509_PATH_ERROR;
  status = client_workspace(arena, &workspace);
  if (status != TC_X509_PATH_VALID)
    return status;
  client_options(&options, at, verifier, work_limit);
  return TC_X509_path_validate(chain, count, anchor, &options, &workspace, result);
}

TC_X509_path_status example_find_client_path(TC_bytes target, const TC_X509_store_source* source,
                                             const TC_X509_time* at,
                                             const TC_X509_signature_provider* verifier,
                                             size_t work_limit, TC_buffer arena,
                                             const TC_X509_search_workspace* search,
                                             TC_X509_search_report* result)
{
  TC_X509_path_options options;
  TC_X509_path_workspace workspace;
  TC_X509_path_status status;
  if (!at || !verifier)
    return TC_X509_PATH_ERROR;
  status = client_workspace(arena, &workspace);
  if (status != TC_X509_PATH_VALID)
    return status;
  client_options(&options, at, verifier, work_limit);
  return TC_X509_path_build(target, source, &options, &workspace, search, result);
}
