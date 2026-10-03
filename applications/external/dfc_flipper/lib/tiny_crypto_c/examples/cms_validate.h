/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CMS_VALIDATE_H_
#define EXAMPLE_CMS_VALIDATE_H_
#include <tiny_crypto/cms_validation.h>
#include "cms_reader.h"
#include "x509_workspace.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_CMS_CERTIFICATE_CAPACITY = 8 };
typedef struct {
  ExampleX509SearchWorkspace path;
  TC_bytes certificates[EXAMPLE_CMS_CERTIFICATE_CAPACITY];
  uint8_t signature[EXAMPLE_CMS_SIGNATURE_CAPACITY];
  uint8_t signed_digest[TC_CMS_SIGNED_DIGEST_BYTES];
} ExampleCMSPathWorkspace;

enum { EXAMPLE_CMS_CRL_CAPACITY = 4, EXAMPLE_CMS_REVOCATION_NODES = 8 };
typedef struct {
  ExampleCMSPathWorkspace cms;
  TC_bytes held_path[EXAMPLE_X509_PATH_CAPACITY];
  uint8_t crl_states[EXAMPLE_CMS_CRL_CAPACITY];
  TC_X509_revocation_node nodes[EXAMPLE_CMS_REVOCATION_NODES];
  TC_X509_revocation_scope scopes[EXAMPLE_CMS_CRL_CAPACITY];
  TC_bytes signer_path[EXAMPLE_X509_PATH_CAPACITY];
  TC_bytes signer_policies[EXAMPLE_X509_POLICY_CAPACITY];
} ExampleCMSCredentialWorkspace;

TC_CMS_path_workspace example_cms_path_workspace(ExampleCMSPathWorkspace* storage);
TC_CMS_credential_workspace example_cms_credential_workspace(ExampleCMSCredentialWorkspace* storage,
                                                             const TC_CMS_path_workspace* path);

/* Hold the snapshot and CRL index unchanged through the acceptance decision.
 * Request spans stay borrowed through signature, path and revocation checks. */
TC_credential_status example_validate_cms_credential(const TC_CMS_validation_request* request,
                                                     const TC_X509_store_snapshot* snapshot,
                                                     const TC_CMS_path_options* options,
                                                     const TC_CMS_revocation_policy* revocation,
                                                     size_t* work,
                                                     ExampleCMSCredentialWorkspace* storage);

/* Caller serializes the store through this call and its acceptance decision. */
TC_credential_status example_validate_cms_from_store(const TC_CMS_validation_request* request,
                                                     TC_X509_store* store,
                                                     const TC_CMS_path_options* options,
                                                     const TC_CMS_revocation_policy* revocation,
                                                     size_t* work,
                                                     ExampleCMSCredentialWorkspace* storage);

/* Build a trusted path for an already parsed signer and computed digest. */
TC_X509_path_status example_find_cms_signer_path(const TC_CMS_signer_path_request* request,
                                                 const TC_X509_store_source* source,
                                                 const TC_CMS_path_options* options,
                                                 size_t work_limit,
                                                 ExampleCMSPathWorkspace* storage,
                                                 TC_X509_search_report* out);

/* Parse SignedData, bind its content and build the selected signer's path. */
TC_X509_path_status example_check_cms_signed_data(const TC_CMS_validation_request* request,
                                                  const TC_X509_store_source* source,
                                                  const TC_CMS_path_options* options,
                                                  size_t work_limit,
                                                  ExampleCMSPathWorkspace* storage,
                                                  TC_X509_search_report* out);

#ifdef __cplusplus
}
#endif
#endif
