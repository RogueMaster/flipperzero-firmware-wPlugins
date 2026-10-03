/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "cms_validate.h"

TC_CMS_path_workspace example_cms_path_workspace(ExampleCMSPathWorkspace* storage)
{
  const TC_CMS_path_workspace workspace = {example_x509_workspace(&storage->path.validation),
                                           example_x509_search_workspace(&storage->path),
                                           storage->certificates,
                                           EXAMPLE_CMS_CERTIFICATE_CAPACITY,
                                           storage->signature,
                                           sizeof storage->signature,
                                           storage->signed_digest,
                                           sizeof storage->signed_digest};
  return workspace;
}

TC_CMS_credential_workspace example_cms_credential_workspace(ExampleCMSCredentialWorkspace* storage,
                                                             const TC_CMS_path_workspace* path)
{
  const TC_CMS_credential_workspace workspace = {path,
                                                 storage->held_path,
                                                 EXAMPLE_X509_PATH_CAPACITY,
                                                 storage->crl_states,
                                                 sizeof storage->crl_states,
                                                 storage->nodes,
                                                 EXAMPLE_CMS_REVOCATION_NODES,
                                                 storage->scopes,
                                                 EXAMPLE_CMS_CRL_CAPACITY,
                                                 storage->signer_path,
                                                 EXAMPLE_X509_PATH_CAPACITY,
                                                 storage->signer_policies,
                                                 EXAMPLE_X509_POLICY_CAPACITY};
  return workspace;
}

TC_credential_status example_validate_cms_credential(const TC_CMS_validation_request* request,
                                                     const TC_X509_store_snapshot* snapshot,
                                                     const TC_CMS_path_options* options,
                                                     const TC_CMS_revocation_policy* revocation,
                                                     size_t* work,
                                                     ExampleCMSCredentialWorkspace* storage)
{
  if (!snapshot || !snapshot->readers ||
      (snapshot->state != TC_SNAPSHOT_CURRENT && snapshot->state != TC_SNAPSHOT_RETIRED) ||
      !storage)
    return TC_CREDENTIAL_ERROR;
  const TC_CMS_path_workspace cms = example_cms_path_workspace(&storage->cms);
  const TC_CMS_credential_workspace workspace = example_cms_credential_workspace(storage, &cms);
  return TC_CMS_credential_validate(request, &snapshot->source, options, revocation, &workspace,
                                    work);
}

TC_credential_status example_validate_cms_from_store(const TC_CMS_validation_request* request,
                                                     TC_X509_store* store,
                                                     const TC_CMS_path_options* options,
                                                     const TC_CMS_revocation_policy* revocation,
                                                     size_t* work,
                                                     ExampleCMSCredentialWorkspace* storage)
{
  TC_X509_store_snapshot* snapshot;
  switch (TC_X509_store_acquire(store, &snapshot)) {
  case TC_TLV_OK:
    break;
  case TC_TLV_END:
    return TC_CREDENTIAL_UNAVAILABLE;
  case TC_TLV_LIMIT:
    return TC_CREDENTIAL_LIMIT;
  default:
    return TC_CREDENTIAL_ERROR;
  }
  const TC_credential_status result =
      example_validate_cms_credential(request, snapshot, options, revocation, work, storage);
  if (TC_X509_store_release(snapshot) != TC_TLV_OK)
    return TC_CREDENTIAL_ERROR;
  return result;
}

TC_X509_path_status example_find_cms_signer_path(const TC_CMS_signer_path_request* request,
                                                 const TC_X509_store_source* source,
                                                 const TC_CMS_path_options* options,
                                                 size_t work_limit,
                                                 ExampleCMSPathWorkspace* storage,
                                                 TC_X509_search_report* out)
{
  if (!storage)
    return TC_X509_PATH_ERROR;
  const TC_CMS_path_workspace workspace = example_cms_path_workspace(storage);
  return TC_CMS_signer_path_build(request, source, options, &workspace, &work_limit, out);
}

TC_X509_path_status example_check_cms_signed_data(const TC_CMS_validation_request* request,
                                                  const TC_X509_store_source* source,
                                                  const TC_CMS_path_options* options,
                                                  size_t work_limit,
                                                  ExampleCMSPathWorkspace* storage,
                                                  TC_X509_search_report* out)
{
  if (!storage)
    return TC_X509_PATH_ERROR;
  const TC_CMS_path_workspace workspace = example_cms_path_workspace(storage);
  return TC_CMS_signed_data_path_build(request, source, options, &workspace, &work_limit, out);
}
