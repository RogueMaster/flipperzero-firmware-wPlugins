/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_VALIDATION_H_
#define TINY_CRYPTO_VALIDATION_H_

#include <tiny_crypto/cms_validation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_VALIDATION_MICRO, TC_VALIDATION_MINI, TC_VALIDATION_DESKTOP
} TC_validation_profile;

/* Element counts, except signature_bytes. Adjust these to the held trust set
 * and expected credentials. Exhausted capacities return a limit result. */
typedef struct {
  size_t frames, oids, name_scalars, name_attributes;
  size_t policy_nodes, policy_edges, policy_expected, policy_mappings, policies;
  size_t path, certificates, signature_bytes, crls, revocation_nodes;
} TC_validation_capacity;

/* Suitable alignment for statically allocated arena storage. */
typedef union {
  TC_TLV_frame frame;
  TC_X509_policy_node policy;
  TC_X509_policy_mapping mapping;
  TC_X509_search_frame search;
  TC_X509_revocation_node revocation;
  uint32_t scalar;
} TC_validation_storage;

typedef struct {
  TC_CMS_path_workspace path;
  TC_CMS_credential_workspace credential;
} TC_validation_workspace;

/* Presets select storage capacities only. Algorithms and trust policy are
 * configured separately. out changes only on OK. */
TC_result TC_validation_capacity_init(TC_validation_profile profile,
    TC_validation_capacity* out);
size_t TC_validation_workspace_alignment(void);
/* Calculate arena bytes, including alignment padding between arrays.
 * bytes changes only on OK. Invalid capacities and size overflow fail. */
TC_result TC_validation_workspace_size(const TC_validation_capacity* capacity,
    size_t* bytes);

/* arena must have the reported size and alignment. Metadata and arena are
 * disjoint. Keep out at the same address until its last use; its credential
 * view refers to out->path. Arena bytes are scratch for one operation at a time.
 * Failure leaves out and arena unchanged. Initialization leaves arena untouched.
 * Typed-array workspace initializers remain available for fixed layouts. */
TC_result TC_validation_workspace_init(const TC_validation_capacity* capacity,
    TC_buffer arena, TC_validation_workspace* out);

typedef struct {
  const TC_bytes* initial_policies;
  size_t initial_policy_count;
  TC_X509_name_constraints anchor_names;
  TC_bytes purpose;
  uint16_t key_usage;
  unsigned flags;
} TC_validation_certificate_policy;

typedef struct {
  TC_X509_time at;
  TC_X509_signature_provider signatures;
  TC_TLV_limits parsing;
  size_t max_certificates, max_input, max_candidates, max_candidate_bytes;
  TC_validation_certificate_policy certificate, crl_signer;
  TC_CMS_attribute_encoding attributes;
  TC_CMS_rsa_parameters rsa_parameters;
  TC_X509_crl_delta_policy delta_policy;
  TC_X509_crl_order_policy order_policy;
} TC_validation_options;

/* Hold both sources stable through validation and the acceptance decision. */
typedef struct {
  const TC_X509_store_source* certificates;
  const TC_X509_crl_index* crls;
} TC_validation_trust;

/* Borrowed configuration for a sequence of validation operations. Use separate
 * contexts when card certificates and content signers have different trust. */
typedef struct {
  TC_validation_trust trust;
  const TC_validation_options* options;
  const TC_CMS_credential_workspace* workspace;
} TC_validation_context;

/* Bind trust sources, options and workspace for subsequent validation calls.
 * Keep the referenced objects alive and unchanged while using the context.
 * out must be separate from those objects and changes only on OK. */
TC_result TC_validation_context_init(const TC_validation_trust* trust,
    const TC_validation_options* options,
    const TC_CMS_credential_workspace* workspace, TC_validation_context* out);

/* Validate CMS content, signer path and revocation under one time and provider.
 * work is the remaining byte/operation budget, consumed across the sequence.
 * Inputs and source bytes remain borrowed; workspace is reusable on return. */
TC_credential_status TC_CMS_validate(const TC_CMS_validation_request* request,
    const TC_validation_context* context, size_t* work);

typedef struct {
  TC_X509_certificate certificate;
  TC_X509_time at;
  size_t anchor_index;
} TC_X509_validation_result;

/* Build a trusted path and require current CRL evidence for every member.
 * out changes only on VALID. Its certificate spans borrow encoded and survive
 * scratch reuse. Keep encoded and the held trust snapshot stable through use. */
TC_credential_status TC_X509_validate(TC_bytes encoded,
    const TC_validation_context* context, size_t* work,
    TC_X509_validation_result* out);

#ifdef __cplusplus
}
#endif
#endif
