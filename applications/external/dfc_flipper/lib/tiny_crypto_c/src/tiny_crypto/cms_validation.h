/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_CMS_VALIDATION_H_
#define TINY_CRYPTO_CMS_VALIDATION_H_
#include <tiny_crypto/cms.h>
#include <tiny_crypto/x509_path.h>
#include <tiny_crypto/x509_revocation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_X509_path_options path;
  size_t max_candidates, max_candidate_bytes;
  TC_CMS_attribute_encoding attributes;
  TC_CMS_rsa_parameters rsa_parameters;
} TC_CMS_path_options;

typedef struct {
  TC_X509_path_workspace validation;
  TC_X509_search_workspace search;
  TC_bytes* certificates;
  size_t certificate_capacity;
  uint8_t* signature;
  size_t signature_capacity;
} TC_CMS_path_workspace;

/* Find a signer certificate, verify its signature, and build a trusted path.
 * Embedded certificates precede external candidates; only source anchors
 * establish trust. The result borrows all certificate and source bytes. */
TC_X509_path_status TC_CMS_signer_path_build(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_bytes embedded,
    const TC_X509_store_source* source, const TC_CMS_path_options* options,
    const TC_CMS_path_workspace* workspace, size_t* work, TC_X509_search_result* out);

/* Parse SignedData, bind its content, verify the selected signer and build its
 * path. Attached content requires an empty detached input. */
TC_X509_path_status TC_CMS_signed_data_path_build(TC_bytes encoded, size_t signer_index,
    TC_bytes expected_type, TC_bytes detached_content,
    const TC_X509_store_source* source, const TC_CMS_path_options* options,
    const TC_CMS_path_workspace* workspace, size_t* work, TC_X509_search_result* out);

/* Detached content spans are hashed in array order. Empty parts are allowed. */
TC_X509_path_status TC_CMS_signed_data_path_build_parts(TC_bytes encoded, size_t signer_index,
    TC_bytes expected_type, const TC_bytes* detached_content, size_t detached_count,
    const TC_X509_store_source* source, const TC_CMS_path_options* options,
    const TC_CMS_path_workspace* workspace, size_t* work, TC_X509_search_result* out);

typedef struct {
  const TC_X509_crl_index* index;
  const TC_X509_path_options* signer_policy;
  size_t max_candidate_bytes;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} TC_CMS_revocation_policy;

typedef struct {
  const TC_CMS_path_workspace* path;
  TC_bytes* held_path;
  size_t path_capacity;
  uint8_t* crl_states;
  size_t crl_capacity;
  TC_X509_revocation_node* nodes;
  size_t node_capacity;
} TC_CMS_credential_workspace;

typedef struct {
  TC_bytes encoded;
  size_t signer_index;
  TC_bytes expected_type;
  const TC_bytes* detached_content;
  size_t detached_count;
  TC_bytes signer_certificate;
} TC_CMS_validation_request;

/* Verify one CMS signer, build its path, and check every path member against
 * the fixed CRL index. The selected path source and anchor are applied to CRL
 * signer validation internally, so every supplied policy field affects the
 * result. VALID requires an unrevoked path. */
TC_credential_status TC_CMS_credential_validate(const TC_CMS_validation_request* request,
    const TC_X509_store_source* source, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation,
    const TC_CMS_credential_workspace* workspace, size_t* work);

#ifdef __cplusplus
}
#endif
#endif
