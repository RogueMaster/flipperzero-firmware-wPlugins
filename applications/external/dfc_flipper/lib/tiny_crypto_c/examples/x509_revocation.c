/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "x509_revocation.h"

TC_TLV_result example_check_path_revocation(const TC_bytes* chain, size_t count,
                                            const TC_X509_revocation_options* options, size_t* work,
                                            ExampleX509RevocationWorkspace* storage,
                                            TC_X509_revocation_report* result)
{
  if (!storage)
    return TC_TLV_ARGUMENT;
  const TC_X509_path_workspace validation = example_x509_workspace(&storage->search.validation);
  const TC_X509_search_workspace search = example_x509_search_workspace(&storage->search);
  const TC_X509_revocation_workspace workspace = {
      &validation,
      &search,
      storage->states,
      sizeof storage->states,
      storage->nodes,
      sizeof storage->nodes / sizeof storage->nodes[0],
      storage->scopes,
      sizeof storage->scopes / sizeof storage->scopes[0],
      storage->signer_path,
      sizeof storage->signer_path / sizeof storage->signer_path[0],
      storage->signer_policies,
      sizeof storage->signer_policies / sizeof storage->signer_policies[0]};
  return TC_X509_path_check_revocation(chain, count, options, &workspace, work, result);
}
