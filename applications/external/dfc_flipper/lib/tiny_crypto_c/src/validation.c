/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/validation.h>
#include "cms_internal.h"
#include "internal.h"
#include "validation_internal.h"
#include "pki_storage_internal.h"
#include "pki_source_internal.h"
#include "x509_revocation_internal.h"

#if TC_ENABLE_CMS_VALIDATION

typedef struct { char byte; TC_validation_storage storage; } validation_alignment;

size_t TC_validation_workspace_alignment(void)
{
  return offsetof(validation_alignment,storage);
}

TC_result TC_validation_capacity_init(TC_validation_profile profile,
    TC_validation_capacity* out)
{
  static const TC_validation_capacity presets[] = {
    {.frames = 16, .oids = 16, .name_scalars = 64, .name_attributes = 8,
     .policy_nodes = 32, .policy_edges = 64, .policy_expected = 64,
     .policy_mappings = 16, .policies = 16, .path = 4, .certificates = 8,
     .signature_bytes = 384, .crls = 4, .revocation_nodes = 8},
    {.frames = 24, .oids = 32, .name_scalars = 128, .name_attributes = 16,
     .policy_nodes = 64, .policy_edges = 128, .policy_expected = 128,
     .policy_mappings = 32, .policies = 32, .path = 8, .certificates = 16,
     .signature_bytes = 384, .crls = 16, .revocation_nodes = 32},
    {.frames = 32, .oids = 64, .name_scalars = 512, .name_attributes = 64,
     .policy_nodes = 256, .policy_edges = 512, .policy_expected = 512,
     .policy_mappings = 128, .policies = 128, .path = 16, .certificates = 128,
     .signature_bytes = 512, .crls = 128, .revocation_nodes = 256}
  };
  if (!out || profile < TC_VALIDATION_MICRO || profile > TC_VALIDATION_DESKTOP)
    return TC_RESULT_ARGUMENT;
  *out = presets[profile];
  return TC_RESULT_OK;
}

/* Reserve an aligned array in the arena; reject size arithmetic overflow. */
static int reserve(size_t* offset, size_t count, size_t width, size_t* start)
{
  const size_t alignment = TC_validation_workspace_alignment();
  const size_t remainder = *offset % alignment;
  const size_t padding = remainder ? alignment - remainder : 0;
  if (padding > SIZE_MAX - *offset) return 0;
  *start = *offset + padding;
  if (count > (SIZE_MAX - *start) / width) return 0;
  *offset = *start + count * width;
  return 1;
}

/* Calculate the arena layout and array capacities. A NULL arena sizes the
 * workspace without assigning storage. */
static TC_result layout(const TC_validation_capacity* c, uint8_t* arena,
    TC_validation_workspace* w, size_t* bytes)
{
  size_t offset = 0, start;
  if (!c || !c->frames || !c->oids || !c->name_scalars ||
      !c->name_attributes || !c->path || !c->certificates)
    return TC_RESULT_ARGUMENT;
#define ARRAY(field, type, count) \
  do { \
    if (!reserve(&offset,(count),sizeof(type),&start)) return TC_RESULT_LIMIT; \
    w->field = arena && (count) ? (type*)(arena + start) : NULL; \
  } while (0)
  ARRAY(path.validation.frames,TC_TLV_frame,c->frames);
  ARRAY(path.validation.oids,TC_bytes,c->oids);
  ARRAY(path.validation.names.left,uint32_t,c->name_scalars);
  ARRAY(path.validation.names.right,uint32_t,c->name_scalars);
  ARRAY(path.validation.names.matched,uint8_t,c->name_attributes);
  ARRAY(path.validation.nodes,TC_X509_policy_node,c->policy_nodes);
  ARRAY(path.validation.edges,TC_X509_policy_edge,c->policy_edges);
  ARRAY(path.validation.expected,TC_X509_policy_expected,c->policy_expected);
  ARRAY(path.validation.mappings,TC_X509_policy_mapping,c->policy_mappings);
  ARRAY(path.validation.policies,TC_bytes,c->policies);
  ARRAY(path.search.path,TC_bytes,c->path);
  ARRAY(path.search.frames,TC_X509_search_frame,c->path);
  ARRAY(path.certificates,TC_bytes,c->certificates);
  ARRAY(path.signature,uint8_t,c->signature_bytes);
  ARRAY(credential.held_path,TC_bytes,c->path);
  ARRAY(credential.crl_states,uint8_t,c->crls);
  ARRAY(credential.nodes,TC_X509_revocation_node,c->revocation_nodes);
#undef ARRAY
  w->path.validation.frame_capacity = c->frames;
  w->path.validation.oid_capacity = c->oids;
  w->path.validation.names.scalar_capacity = c->name_scalars;
  w->path.validation.names.attribute_capacity = c->name_attributes;
  w->path.validation.node_capacity = c->policy_nodes;
  w->path.validation.edge_capacity = c->policy_edges;
  w->path.validation.expected_capacity = c->policy_expected;
  w->path.validation.mapping_capacity = c->policy_mappings;
  w->path.validation.policy_capacity = c->policies;
  w->path.search.capacity = c->path;
  w->path.certificate_capacity = c->certificates;
  w->path.signature_capacity = c->signature_bytes;
  w->credential.path_capacity = c->path;
  w->credential.crl_capacity = c->crls;
  w->credential.node_capacity = c->revocation_nodes;
  *bytes = offset;
  return TC_RESULT_OK;
}

TC_result TC_validation_workspace_size(const TC_validation_capacity* capacity,
    size_t* bytes)
{
  TC_validation_workspace workspace = {0};
  size_t size;
  if (!bytes) return TC_RESULT_ARGUMENT;
  TC_result result = layout(capacity,NULL,&workspace,&size);
  if (result == TC_RESULT_OK) *bytes = size;
  return result;
}

TC_result TC_validation_workspace_init(const TC_validation_capacity* capacity,
    TC_buffer arena, TC_validation_workspace* out)
{
  TC_validation_workspace workspace = {0};
  size_t size;
  if (!out || !capacity || !arena.data ||
      (uintptr_t)arena.data % TC_validation_workspace_alignment() ||
      arena.capacity > UINTPTR_MAX - (uintptr_t)arena.data ||
      !tc_internal_ranges_disjoint(arena.data,arena.capacity,out,sizeof *out) ||
      !tc_internal_ranges_disjoint(arena.data,arena.capacity,capacity,sizeof *capacity) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,capacity,sizeof *capacity))
    return TC_RESULT_ARGUMENT;
  TC_result result = layout(capacity,NULL,&workspace,&size);
  if (result != TC_RESULT_OK) return result;
  if (arena.capacity < size) return TC_RESULT_LIMIT;
  result = layout(capacity,arena.data,&workspace,&size);
  if (result != TC_RESULT_OK) return result;
  workspace.credential.path = &out->path;
  *out = workspace;
  return TC_RESULT_OK;
}

TC_result TC_validation_context_init(const TC_validation_trust* trust,
    const TC_validation_options* options,
    const TC_CMS_credential_workspace* workspace, TC_validation_context* out)
{
  int order;
  if (!trust || !trust->certificates || !trust->crls || !options || !workspace ||
      !workspace->path || !out || !options->max_certificates ||
      !options->max_input || !options->max_candidates || !options->max_candidate_bytes ||
      TC_X509_time_compare(&options->at,&options->at,&order) != TC_TLV_OK ||
      (options->attributes != TC_CMS_ATTRIBUTES_DER &&
       options->attributes != TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER) ||
      (options->rsa_parameters != TC_CMS_RSA_PARAMETERS_NULL &&
       options->rsa_parameters != TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT) ||
      options->delta_policy < TC_X509_CRL_COMPLETE_ONLY ||
      options->delta_policy > TC_X509_CRL_DELTA_REQUIRED ||
      (options->order_policy != TC_X509_CRL_ORDER_NUMBER &&
       options->order_policy != TC_X509_CRL_ORDER_THIS_UPDATE) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,trust,sizeof *trust) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,
          trust->certificates,sizeof *trust->certificates) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,
          trust->crls,sizeof *trust->crls) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,options,sizeof *options) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,workspace,sizeof *workspace) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,
          workspace->path,sizeof *workspace->path))
    return TC_RESULT_ARGUMENT;
  const TC_validation_context context = {*trust,options,workspace};
  *out = context;
  return TC_RESULT_OK;
}

static TC_X509_path_options path_policy(const TC_validation_options* options,
    const TC_validation_certificate_policy* policy)
{
  TC_X509_path_options path = {0};
  path.at = options->at;
  path.signatures = options->signatures;
  path.parsing = options->parsing;
  path.max_certificates = options->max_certificates;
  path.max_input = options->max_input;
  path.initial_policies = policy->initial_policies;
  path.initial_policy_count = policy->initial_policy_count;
  path.anchor_names = policy->anchor_names;
  path.purpose = policy->purpose;
  path.key_usage = policy->key_usage;
  path.flags = policy->flags;
  return path;
}

int tc_validation_policies(const TC_validation_context* context,
    TC_CMS_path_options* cms, TC_X509_path_options* crl,
    TC_CMS_revocation_policy* revocation)
{
  if (!context || !context->options || !context->trust.certificates ||
      !context->trust.crls || !context->workspace || !context->workspace->path)
    return 0;
  const TC_validation_options* options = context->options;
  cms->path = path_policy(options,&options->certificate);
  cms->max_candidates = options->max_candidates;
  cms->max_candidate_bytes = options->max_candidate_bytes;
  cms->attributes = options->attributes;
  cms->rsa_parameters = options->rsa_parameters;
  *crl = path_policy(options,&options->crl_signer);
  revocation->index = context->trust.crls;
  revocation->signer_policy = crl;
  revocation->max_candidate_bytes = options->max_candidate_bytes;
  revocation->delta_policy = options->delta_policy;
  revocation->order_policy = options->order_policy;
  return 1;
}

TC_TLV_result tc_validation_storage(const TC_validation_context* context,
    const TC_bytes* inputs, size_t input_count, size_t* work,
    void* out, size_t out_size, TC_bytes writes[TC_VALIDATION_WRITES])
{
  if (!context || !context->options || !context->workspace ||
      !context->workspace->path || !context->trust.certificates ||
      !context->trust.crls || !work || (input_count && !inputs))
    return TC_TLV_ARGUMENT;
  const TC_CMS_credential_workspace* w = context->workspace;
  const TC_CMS_path_workspace* p = w->path;
  size_t budget = *work, next = TC_X509_PATH_STORAGE_COUNT;
  TC_TLV_result status = tc_x509_path_storage_writes(&p->validation,writes);
  if (status != TC_TLV_OK) return status;
#define WRITE(pointer, count) do { \
  status = tc_pki_storage_span((pointer),(count),sizeof *(pointer),&writes[next++]); \
  if (status != TC_TLV_OK) return status; \
} while (0)
  WRITE(p->search.path,p->search.capacity);
  WRITE(p->search.frames,p->search.capacity);
  WRITE(p->certificates,p->certificate_capacity);
  WRITE(p->signature,p->signature_capacity);
  WRITE(w->held_path,w->path_capacity);
  WRITE(w->crl_states,w->crl_capacity);
  WRITE(w->nodes,w->node_capacity);
  WRITE(work,1);
  WRITE((uint8_t*)out,out_size);
#undef WRITE
  for (size_t i = 0; i < next; ++i) {
    status = tc_pki_storage_input(writes,i,writes[i],&budget);
    if (status != TC_TLV_OK) return status;
  }
#define INPUT(pointer, count) do { \
  TC_bytes span; \
  status = tc_pki_storage_span((pointer),(count),sizeof *(pointer),&span); \
  if (status != TC_TLV_OK) return status; \
  status = tc_pki_storage_input(writes,next,span,&budget); \
  if (status != TC_TLV_OK) return status; \
} while (0)
  INPUT(context,1);
  INPUT(context->options,1);
  INPUT(context->trust.certificates,1);
  INPUT(context->trust.crls,1);
  INPUT(w,1);
  INPUT(p,1);
  INPUT(inputs,input_count);
  const TC_validation_certificate_policy* policies[] = {
    &context->options->certificate,&context->options->crl_signer
  };
  for (size_t i = 0; i < sizeof policies / sizeof *policies; ++i) {
    const TC_validation_certificate_policy* policy = policies[i];
    INPUT(policy->initial_policies,policy->initial_policy_count);
    INPUT(policy->purpose.data,policy->purpose.length);
    INPUT(policy->anchor_names.permitted.data,policy->anchor_names.permitted.length);
    INPUT(policy->anchor_names.excluded.data,policy->anchor_names.excluded.length);
    for (size_t j = 0; j < policy->initial_policy_count; ++j)
      INPUT(policy->initial_policies[j].data,policy->initial_policies[j].length);
  }
#undef INPUT
  for (size_t i = 0; i < input_count; ++i) {
    status = tc_pki_storage_input(writes,next,inputs[i],&budget);
    if (status != TC_TLV_OK) return status;
  }
  status = tc_x509_crl_index_storage_bytes(context->trust.crls,writes,next,&budget);
  if (status != TC_TLV_OK) return status;
  if (w->path_capacity < p->search.capacity ||
      w->crl_capacity < context->trust.crls->count) return TC_TLV_LIMIT;
  *work = budget;
  return TC_TLV_OK;
}

TC_credential_status TC_CMS_validate(const TC_CMS_validation_request* request,
    const TC_validation_context* context, size_t* work)
{
  TC_CMS_path_options cms;
  TC_X509_path_options crl;
  TC_CMS_revocation_policy revocation;
  if (!tc_validation_policies(context,&cms,&crl,&revocation))
    return TC_CREDENTIAL_ERROR;
  if (!request || !work) return TC_CREDENTIAL_ERROR;
  const TC_bytes metadata[] = {
    {(const uint8_t*)context,sizeof *context},
    {(const uint8_t*)context->options,sizeof *context->options}
  };
  return tc_cms_credential_validate_with_metadata(request,
      context->trust.certificates,&cms,&revocation,context->workspace,work,
      metadata,sizeof metadata / sizeof *metadata);
}

TC_credential_status tc_validation_status(TC_TLV_result status)
{
  switch (status) {
    case TC_TLV_OK: return TC_CREDENTIAL_VALID;
    case TC_TLV_LIMIT: return TC_CREDENTIAL_LIMIT;
    case TC_TLV_UNSUPPORTED: return TC_CREDENTIAL_UNSUPPORTED;
    case TC_TLV_ARGUMENT: return TC_CREDENTIAL_ERROR;
    default: return TC_CREDENTIAL_INVALID;
  }
}

TC_credential_status TC_X509_validate(TC_bytes encoded,
    const TC_validation_context* context, size_t* work,
    TC_X509_validation_result* out)
{
  TC_CMS_path_options cms;
  TC_X509_path_options crl;
  TC_CMS_revocation_policy revocation;
  if (!encoded.data || !encoded.length || !out || !work ||
      !tc_validation_policies(context,&cms,&crl,&revocation))
    return TC_CREDENTIAL_ERROR;
  if (context->trust.certificates->candidate_count > cms.max_candidates)
    return TC_CREDENTIAL_LIMIT;
  TC_bytes writes[TC_VALIDATION_WRITES];
  TC_TLV_result status = tc_validation_storage(context,&encoded,1,work,
      out,sizeof *out,writes);
  if (status != TC_TLV_OK) return tc_validation_status(status);
  tc_pki_source_guard guard = {context->trust.certificates,writes,TC_VALIDATION_WRITES};
  const TC_X509_store_source source = {&guard,guard.source->candidate_count,
    guard.source->anchor_count,tc_pki_source_guard_candidate,tc_pki_source_guard_anchor};
  const TC_CMS_credential_workspace* workspace = context->workspace;
  TC_X509_search_result path;
  const TC_X509_path_status found = tc_x509_path_build_work(encoded,&source,
      &cms.path,&workspace->path->validation,&workspace->path->search,work,&path);
  if (found != TC_X509_PATH_VALID)
    return tc_validation_status(tc_x509_path_result_status(found));

  /* Preserve the selected chain's descriptors while CRL signer searches reuse
   * the path workspace. Certificate encodings remain in their source buffers. */
  for (size_t i = 0; i < path.count; ++i) workspace->held_path[i] = path.path[i];
  const TC_X509_revocation_options path_revocation = {revocation.index,&source,
    revocation.signer_policy,path.anchor_index,revocation.max_candidate_bytes,
    revocation.delta_policy,revocation.order_policy};
  const TC_X509_revocation_workspace scratch = {
    &workspace->path->validation,&workspace->path->search,
    workspace->crl_states,workspace->crl_capacity,workspace->nodes,workspace->node_capacity
  };
  TC_X509_revocation_result checked;
  status = TC_X509_path_check_revocation(workspace->held_path,path.count,
      &path_revocation,&scratch,work,&checked);
  if (status != TC_TLV_OK) return tc_validation_status(status);
  if (checked.status == TC_X509_CRL_REVOKED) return TC_CREDENTIAL_REVOKED;
  if (checked.status != TC_X509_CRL_UNREVOKED) return TC_CREDENTIAL_UNSUPPORTED;
  if (encoded.length > *work) return TC_CREDENTIAL_LIMIT;
  *work -= encoded.length;
  const TC_X509_path_workspace* storage = &workspace->path->validation;
  TC_X509_workspace parser = {storage->frames,storage->frame_capacity,
    storage->oids,storage->oid_capacity};
  TC_X509_validation_result result;
  status = TC_X509_read(encoded.data,encoded.length,&cms.path.parsing,
      &parser,&result.certificate);
  if (status != TC_TLV_OK) return tc_validation_status(status);
  result.at = context->options->at;
  result.anchor_index = path.anchor_index;
  *out = result;
  return TC_CREDENTIAL_VALID;
}

#endif
