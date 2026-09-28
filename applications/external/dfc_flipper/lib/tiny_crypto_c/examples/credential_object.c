/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_object.h"
#include <tiny_crypto/piv_cms.h>

TC_result example_validation_options(const TC_CMS_path_options* path,
    const TC_CMS_revocation_policy* revocation, TC_validation_options* out)
{
  int order;
  if (!path || !revocation || !revocation->signer_policy || !out ||
      TC_X509_time_compare(&path->path.at,&revocation->signer_policy->at,&order) != TC_TLV_OK ||
      order || path->path.signatures.verify != revocation->signer_policy->signatures.verify ||
      path->path.signatures.verify_digest !=
          revocation->signer_policy->signatures.verify_digest ||
      path->path.signatures.context != revocation->signer_policy->signatures.context)
    return TC_RESULT_ARGUMENT;
  TC_validation_options converted = {0};
  converted.at = path->path.at;
  converted.signatures = path->path.signatures;
  converted.parsing = path->path.parsing;
  converted.max_certificates = path->path.max_certificates;
  converted.max_input = path->path.max_input;
  converted.max_candidates = path->max_candidates;
  converted.max_candidate_bytes = path->max_candidate_bytes <
      revocation->max_candidate_bytes ? path->max_candidate_bytes :
      revocation->max_candidate_bytes;
  converted.certificate = (TC_validation_certificate_policy){
    path->path.initial_policies,path->path.initial_policy_count,
    path->path.anchor_names,path->path.purpose,path->path.key_usage,path->path.flags};
  converted.crl_signer = (TC_validation_certificate_policy){
    revocation->signer_policy->initial_policies,
    revocation->signer_policy->initial_policy_count,
    revocation->signer_policy->anchor_names,revocation->signer_policy->purpose,
    revocation->signer_policy->key_usage,revocation->signer_policy->flags};
  converted.attributes = path->attributes;
  converted.rsa_parameters = path->rsa_parameters;
  converted.delta_policy = revocation->delta_policy;
  converted.order_policy = revocation->order_policy;
  *out = converted;
  return TC_RESULT_OK;
}

TC_credential_status example_validate_biometric(
    const TC_PIV_biometric_validation_request* request,
    const TC_X509_store_snapshot* snapshot, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation, size_t* work,
    ExampleCMSCredentialWorkspace* storage)
{
  if (!request || !snapshot || !options || !revocation || !work || !storage)
    return TC_CREDENTIAL_ERROR;
  TC_CMS_path_workspace path = example_cms_path_workspace(&storage->cms);
  const TC_CMS_credential_workspace workspace =
      example_cms_credential_workspace(storage,&path);
  TC_validation_options validation;
  TC_validation_context context;
  const TC_validation_trust trust = {&snapshot->source,revocation->index};
  TC_credential_status result = TC_CREDENTIAL_ERROR;
  if (example_validation_options(options,revocation,&validation) == TC_RESULT_OK &&
      TC_validation_context_init(&trust,&validation,&workspace,&context) == TC_RESULT_OK)
    result = TC_PIV_biometric_validate(request,&context,work);
  TC_secure_zero(storage,sizeof *storage);
  return result;
}

TC_credential_status example_validate_security(
    const TC_PIV_security_validation_request* request,
    const TC_X509_store_snapshot* snapshot, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation, size_t* work,
    ExampleSecurityWorkspace* storage)
{
  if (!request || !snapshot || !options || !revocation || !work || !storage)
    return TC_CREDENTIAL_ERROR;
  TC_CMS_path_workspace path = example_cms_path_workspace(&storage->credential.cms);
  const TC_CMS_credential_workspace credential =
      example_cms_credential_workspace(&storage->credential,&path);
  const TC_PIV_security_validation_workspace workspace = {
    storage->content,sizeof storage->content
  };
  TC_validation_options validation;
  TC_validation_context context;
  const TC_validation_trust trust = {&snapshot->source,revocation->index};
  TC_credential_status result = TC_CREDENTIAL_ERROR;
  if (example_validation_options(options,revocation,&validation) == TC_RESULT_OK &&
      TC_validation_context_init(&trust,&validation,&credential,&context) == TC_RESULT_OK)
  {
    TC_PIV_security_result accepted;
    result = TC_PIV_security_validate(request,&context,&workspace,work,&accepted);
  }
  TC_secure_zero(storage,sizeof *storage);
  return result;
}

#if TC_ENABLE_PIV_CVC
TC_credential_status example_validate_cvc(const ExampleCVCRequest* request,
    const TC_X509_store_snapshot* snapshot, const TC_X509_path_options* options,
    const TC_X509_revocation_options* revocation, size_t* work,
    ExampleCVCCredentialWorkspace* workspace, TC_PIV_CVC* out)
{
  if (!request || !request->card.data || !request->card.length ||
      !request->signer_certificate.data || !request->signer_certificate.length ||
      !options || !revocation || !revocation->index || !revocation->signer_policy ||
      !work || !workspace || !out ||
      (request->profile != TC_PIV_CARD && request->profile != TC_TWIC_LEGACY_CARD &&
       request->profile != TC_TWIC_NEXGEN_CARD)) return TC_CREDENTIAL_ERROR;
  if (!snapshot) return TC_CREDENTIAL_UNAVAILABLE;
  if (!snapshot->readers || (snapshot->state != TC_SNAPSHOT_CURRENT &&
      snapshot->state != TC_SNAPSHOT_RETIRED)) return TC_CREDENTIAL_ERROR;
  const TC_PIV_oid_profile oids = request->profile == TC_PIV_CARD ?
      TC_PIV_OIDS_ONLY : TC_PIV_OIDS_TWIC_COMPATIBLE;
  int order;
  if (TC_PIV_oid_identify(options->purpose,oids) != TC_PIV_OID_CONTENT_SIGNING ||
      TC_X509_time_compare(&options->at,&revocation->signer_policy->at,&order) != TC_TLV_OK || order)
    return TC_CREDENTIAL_ERROR;
  TC_CMS_path_options path = {*options,options->max_certificates,
    revocation->max_candidate_bytes,TC_CMS_ATTRIBUTES_DER,
    TC_CMS_RSA_PARAMETERS_NULL};
  const TC_CMS_revocation_policy crls = {revocation->index,
    revocation->signer_policy,revocation->max_candidate_bytes,
    revocation->delta_policy,revocation->order_policy};
  TC_validation_options validation;
  if (example_validation_options(&path,&crls,&validation) != TC_RESULT_OK)
    return TC_CREDENTIAL_ERROR;
  TC_validation_capacity capacity;
  if (TC_validation_capacity_init(TC_VALIDATION_MICRO,&capacity) != TC_RESULT_OK)
    return TC_CREDENTIAL_ERROR;
  capacity.certificates = EXAMPLE_X509_PATH_CAPACITY;
  capacity.crls = EXAMPLE_CMS_CRL_CAPACITY;
  size_t bytes;
  TC_result initialized = TC_validation_workspace_size(&capacity,&bytes);
  if (initialized != TC_RESULT_OK || bytes > sizeof workspace->scratch.arena)
    return initialized == TC_RESULT_LIMIT ? TC_CREDENTIAL_LIMIT :
      TC_CREDENTIAL_ERROR;
  TC_credential_status result = TC_CREDENTIAL_ERROR;
  initialized = TC_validation_workspace_init(&capacity,
      (TC_buffer){(uint8_t*)workspace->scratch.arena,bytes},
      &workspace->validation);
  if (initialized != TC_RESULT_OK) goto cleanup;
  const TC_validation_trust trust = {&snapshot->source,revocation->index};
  TC_validation_context context;
  initialized = TC_validation_context_init(&trust,&validation,
      &workspace->validation.credential,&context);
  if (initialized != TC_RESULT_OK) goto cleanup;
  const TC_PIV_CVC_validation_request selected = {request->card,
    request->intermediate,request->expected_uuid,request->signer_certificate,
    request->curve,request->profile};
  result = TC_PIV_CVC_validate(&selected,&context,&workspace->scratch.point,
      work,out);
cleanup:
  TC_secure_zero(workspace,sizeof *workspace);
  return result;
}
#endif
