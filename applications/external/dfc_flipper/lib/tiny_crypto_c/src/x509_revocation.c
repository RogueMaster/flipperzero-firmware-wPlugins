/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_revocation_internal.h"
#include "pki_storage_internal.h"
#include "pki_spans_internal.h"
#include "pki_status_internal.h"
#include "pki_identifier_internal.h"
#include "pki_extensions_internal.h"

TC_TLV_result tc_x509_crl_scope_run(const tc_x509_crl_candidate_source* candidates,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    const TC_bytes* points, int from_certificate, int all_scopes,
    const TC_bytes writes[CRL_SCOPE_WRITES], int* source_failed, TC_X509_search_result* out)
{
  if (!candidates || !candidates->search || !writes || !source_failed) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_scope_arguments(processing,trust,all_scopes,out);
  if (result != TC_TLV_OK) return result;
  const TC_X509_path_options* options = trust->options;
  const tc_pki_tree_workspace* tree = trust->tree;
  const TC_X509_path_workspace* validation = trust->validation;
  const TC_X509_store_source* path_source = trust->source;
  const size_t initial_work = *tree->work;
  TC_TLV_reader point_reader;
  tc_x509_crl_certificate_fields fields = {0};
  fields.limits = &options->parsing; fields.tree = tree;
  fields.ca = processing->query->certificate_ca;
  if (points) fields.points = *points;
  result = tc_x509_crl_points_init(&fields,from_certificate ? processing->query->certificate : NULL,
      validation->oids,validation->oid_capacity,&point_reader);
  if (result != TC_TLV_OK) return result;
  tc_pki_source_status_guard path_guard = {{path_source,writes,CRL_SCOPE_WRITES},source_failed};
  tc_pki_source_status_guard candidate_guard = {{candidates->external,writes,CRL_SCOPE_WRITES},source_failed};
  const TC_X509_store_source guarded_path = {&path_guard,path_source->candidate_count,
    path_source->anchor_count,tc_pki_source_status_candidate,tc_pki_source_status_anchor};
  TC_X509_store_source guarded_candidates;
  tc_x509_crl_candidate_source source = *candidates;
  if (candidates->external) {
    guarded_candidates = (TC_X509_store_source){&candidate_guard,candidates->external->candidate_count,
      candidates->external->anchor_count,tc_pki_source_status_candidate,tc_pki_source_status_anchor};
    source.external = &guarded_candidates;
  }
  tc_x509_crl_trust guarded_trust = *trust;
  guarded_trust.source = &guarded_path;
  result = tc_x509_crl_scopes(&source,tc_x509_crl_source_search,processing,&guarded_trust,
      &fields,&point_reader,all_scopes,source_failed,out);
  if (result == TC_TLV_OK && !all_scopes) out->validation.work_used = initial_work - *tree->work;
  return result;
}

static TC_TLV_result x509_crl_scope_prepare(const tc_x509_crl_operation_source* candidates,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    const TC_bytes* points, const tc_x509_crl_extra_storage* extra,
    const tc_x509_crl_held_path* path, TC_X509_search_result* out,
    TC_bytes writes[CRL_SCOPE_WRITES])
{
  if (!candidates || !candidates->candidates.search ||
      (candidates->metadata_count && !candidates->metadata)) return TC_TLV_ARGUMENT;
  size_t budget = *trust->tree->work;
  TC_TLV_result result = tc_x509_crl_scope_storage_writes(processing,trust,out,writes);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_scope_storage_check(extra,path,writes,&budget);
  if (result != TC_TLV_OK) return result;
  if (points) {
    TC_bytes metadata;
    result = tc_pki_storage_span(points,1,sizeof *points,&metadata);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_storage_input(writes,CRL_SCOPE_WRITES,metadata,&budget);
    if (result == TC_TLV_OK)
      result = tc_pki_storage_input(writes,CRL_SCOPE_WRITES,*points,&budget);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < candidates->metadata_count; ++i) {
    result = tc_pki_storage_input(writes,CRL_SCOPE_WRITES,candidates->metadata[i],&budget);
    if (result != TC_TLV_OK) return result;
  }
  if (extra) {
    result = tc_x509_crl_extra_storage_inputs(extra,writes,CRL_SCOPE_WRITES,&budget);
    if (result != TC_TLV_OK) return result;
  }
  if (path) {
    result = tc_x509_crl_path_storage_inputs(path,writes,CRL_SCOPE_WRITES,&budget);
    if (result != TC_TLV_OK) return result;
  }
  result = tc_x509_crl_scope_storage_inputs(processing,trust,writes,CRL_SCOPE_WRITES,&budget);
  if (result != TC_TLV_OK) return result;
  *trust->tree->work = budget;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_execute(const tc_x509_crl_operation_source* candidates,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    const TC_bytes* points, int from_certificate, int all_scopes,
    const tc_x509_crl_extra_storage* extra, const tc_x509_crl_held_path* path,
    TC_X509_search_result* out)
{
  TC_TLV_result result = tc_x509_crl_scope_arguments(processing,trust,all_scopes,out);
  if (result != TC_TLV_OK) return result;
  TC_bytes writes[CRL_SCOPE_WRITES];
  const size_t initial_work = *trust->tree->work;
  result = x509_crl_scope_prepare(candidates,processing,trust,points,extra,path,out,writes);
  if (result != TC_TLV_OK) return result;
  int source_failed = 0;
  result = tc_x509_crl_scope_run(&candidates->candidates,processing,trust,points,
      from_certificate,all_scopes,writes,&source_failed,out);
  if (source_failed && extra && extra->source_failed) *extra->source_failed = 1;
  if (result == TC_TLV_OK && !all_scopes)
    out->validation.work_used = initial_work - *trust->tree->work;
  return result;
}

TC_TLV_result tc_x509_crl_source_search(const void* candidates,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed)
{
  const tc_x509_crl_candidate_source* source = candidates;
  if (!source || !source->search) return TC_TLV_ARGUMENT;
  return source->search(source->context,source->external,crl,extensions,trust,attempt,context,out,source_failed);
}

TC_TLV_result tc_x509_crl_store_source_search(const void* candidates,
    const TC_X509_store_source* external, const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const tc_x509_crl_trust* trust,
    tc_x509_crl_attempt attempt, const void* context, TC_X509_search_result* out, int* source_failed)
{
  if (!candidates) return TC_TLV_ARGUMENT;
  tc_pki_store_candidates cursor = *(const tc_pki_store_candidates*)candidates;
  cursor.source = external;
  return tc_x509_crl_search_candidates(&cursor,tc_pki_store_candidate_next,crl,extensions,
      trust,attempt,context,out,source_failed);
}

TC_TLV_result tc_x509_crl_store_search(const void* candidates,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed)
{
  if (!candidates) return TC_TLV_ARGUMENT;
  const tc_pki_store_candidates* cursor = candidates;
  return tc_x509_crl_store_source_search(cursor,cursor->source,crl,extensions,
      trust,attempt,context,out,source_failed);
}

TC_TLV_result tc_x509_crl_scope_storage_check(const tc_x509_crl_extra_storage* extra,
    const tc_x509_crl_held_path* path, TC_bytes writes[CRL_SCOPE_WRITES], size_t* work)
{
  if (!writes || !work) return TC_TLV_ARGUMENT;
  writes[CRL_SCOPE_NODES] = extra ? extra->nodes_storage : (TC_bytes){NULL,0};
  writes[CRL_SCOPE_OUTPUT] = extra ? extra->output_storage : (TC_bytes){NULL,0};
  writes[CRL_SCOPE_PATH_OUTPUT] = (TC_bytes){NULL,0};
  TC_TLV_result result;
  if (path) {
    result = tc_pki_storage_span(path->out,1,sizeof *path->out,&writes[CRL_SCOPE_PATH_OUTPUT]);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < CRL_SCOPE_WRITES; ++i) {
    result = tc_pki_storage_input(writes,i,writes[i],work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_arguments(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, int all_scopes, TC_X509_search_result* out)
{
  TC_bytes storage;
  tc_x509_crl_status status;
  if (!processing || !tc_x509_crl_index_arguments(processing->index,processing->query,trust,out) ||
      (!all_scopes && processing->reference >= processing->index->count) ||
      (processing->check && !processing->check->verify) ||
      !x509_crl_delta_policy_valid(processing->delta_policy) ||
      !x509_crl_order_policy_valid(processing->order_policy) ||
      tc_pki_storage_span(processing->states,processing->capacity,sizeof *processing->states,&storage) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  if (processing->capacity < processing->index->count) return TC_TLV_LIMIT;
  if (!all_scopes && processing->index->records[processing->reference].policy != TC_TLV_OK)
    return processing->index->records[processing->reference].policy;
  TC_TLV_result result = tc_x509_crl_evidence_status(processing->evidence,&status);
  if (result != TC_TLV_OK) return result;
  return status == TC_X509_CRL_UNDETERMINED ? TC_TLV_OK : TC_TLV_END;
}

TC_TLV_result tc_x509_crl_resolve_dependencies(TC_bytes target,
    tc_x509_crl_dependencies* dependencies, tc_x509_crl_node_evaluate evaluate,
    void* context, tc_x509_crl_held_path* path, tc_x509_crl_evidence* out)
{
  if (!dependencies || !dependencies->workspace || !dependencies->workspace->tree ||
      !dependencies->workspace->tree->work || !evaluate || !out) return TC_TLV_ARGUMENT;
  const tc_x509_crl_resolution_workspace* workspace = dependencies->workspace;
  size_t* work = workspace->tree->work;
  size_t root;
  TC_TLV_result result = tc_x509_crl_dependency_add(dependencies,target,work,&root);
  if (result != TC_TLV_OK) return result;
  if (workspace->nodes[root].status == TC_X509_CRL_UNREVOKED) {
    /* A proven node already covers every revocation reason. */
    tc_x509_crl_evidence evidence = {0};
    evidence.reasons = TC_X509_CRL_ALL_REASONS;
    *out = evidence;
    return TC_TLV_OK;
  }
  result = tc_x509_crl_nodes_resolve(workspace->nodes,workspace->node_capacity,
      &dependencies->count,root,work,evaluate,context,out);
  if (result == TC_TLV_OK && path) path->dependency_count = dependencies->count;
  return result;
}

TC_TLV_result tc_x509_crl_dependency_read(const tc_x509_crl_dependencies* dependencies,
    size_t index, TC_X509_certificate* out)
{
  if (!dependencies || !dependencies->options || !dependencies->workspace || !out)
    return TC_TLV_ARGUMENT;
  const tc_x509_crl_resolution_workspace* workspace = dependencies->workspace;
  if (!workspace->tree || !workspace->tree->work || !workspace->validation || !workspace->nodes ||
      dependencies->count > workspace->node_capacity || index >= dependencies->count)
    return TC_TLV_ARGUMENT;
  const TC_bytes encoded = workspace->nodes[index].certificate;
  TC_X509_workspace parser = {workspace->validation->frames,workspace->validation->frame_capacity,
    workspace->validation->oids,workspace->validation->oid_capacity};
  TC_TLV_result result = tc_x509_path_charge(workspace->tree->work,encoded.length);
  if (result != TC_TLV_OK) return result;
  return TC_X509_read(encoded.data,encoded.length,&dependencies->options->parsing,&parser,out);
}

TC_TLV_result tc_x509_crl_extra_storage_inputs(const tc_x509_crl_extra_storage* extra,
    const TC_bytes* writes, size_t write_count, size_t* work)
{
  if (!extra || !work || (write_count && !writes) || (extra->count && !extra->nodes))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result;
  for (size_t i = 0; i < CRL_EXTRA_INPUT_COUNT; ++i) {
    result = tc_pki_storage_input(writes,write_count,extra->inputs[i],work);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < extra->count; ++i) {
    result = tc_pki_storage_input(writes,write_count,extra->nodes[i].certificate,work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_dependency_add(tc_x509_crl_dependencies* dependencies,
    TC_bytes certificate, size_t* work, size_t* index)
{
  if (!dependencies || !dependencies->workspace || !work || !index ||
      (dependencies->write_count && !dependencies->writes)) return TC_TLV_ARGUMENT;
  const tc_x509_crl_resolution_workspace* workspace = dependencies->workspace;
  TC_TLV_result result = tc_pki_storage_input(dependencies->writes,dependencies->write_count,certificate,work);
  if (result != TC_TLV_OK) return result;
  return tc_x509_crl_dependency_find(workspace->nodes,workspace->node_capacity,
      &dependencies->count,certificate,work,index);
}

TC_X509_path_status tc_x509_crl_dependencies_check(void* context, const TC_X509_search_result* path,
    const tc_x509_crl_selected* selected, size_t* work)
{
  tc_x509_crl_dependencies* dependencies = context;
  if (!dependencies || !path || !work || !dependencies->options || !dependencies->workspace ||
      !dependencies->workspace->validation || !dependencies->anchor ||
      (dependencies->write_count && !dependencies->writes)) return TC_X509_PATH_ERROR;
  const TC_X509_path_options* options = dependencies->options;
  if (path->anchor_index != dependencies->anchor_index) return TC_X509_PATH_ERROR;
  if (!selected) return TC_X509_PATH_UNSUPPORTED;
  TC_X509_signature_result signature = tc_x509_crl_selected_anchor_check(selected,dependencies->anchor,
      &options->signatures,&options->parsing,&dependencies->workspace->validation->names,work);
  if (signature == TC_X509_SIGNATURE_LIMIT) return TC_X509_PATH_LIMIT;
  if (signature == TC_X509_SIGNATURE_ERROR) return TC_X509_PATH_ERROR;
  if (signature == TC_X509_SIGNATURE_VALID) return TC_X509_PATH_VALID;
  return tc_x509_crl_dependencies_path(path,dependencies->workspace,&dependencies->count,
      dependencies->writes,dependencies->write_count,work);
}

TC_TLV_result tc_x509_crl_path_storage_inputs(const tc_x509_crl_held_path* path,
    const TC_bytes* writes, size_t write_count, size_t* work)
{
  if (!path || !work || (write_count && !writes)) return TC_TLV_ARGUMENT;
  TC_bytes input;
  TC_TLV_result result = tc_pki_storage_span(path->chain,path->count,sizeof *path->chain,&input);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_input(writes,write_count,input,work);
  if (result != TC_TLV_OK) return result;
  for (size_t i = 0; i < CRL_PATH_METADATA_COUNT; ++i) {
    if (!path->metadata[i].length) continue;
    result = tc_pki_storage_input(writes,write_count,path->metadata[i],work);
    if (result != TC_TLV_OK) return result;
  }
  for (size_t i = 0; i < path->count; ++i) {
    if (!path->chain[i].data || !path->chain[i].length) return TC_TLV_ARGUMENT;
    result = tc_pki_storage_input(writes,write_count,path->chain[i],work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_filter_match(const void* context,
    const TC_X509_certificate* candidate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, int* matched)
{
  const tc_x509_crl_filter* filter = context;
  if (!filter) return TC_TLV_ARGUMENT;
  return tc_x509_crl_candidate_matches(filter->crl,filter->extensions,candidate,
      limits,filter->names,tree,matched);
}

typedef struct {
  const tc_x509_crl_trust* trust;
  tc_x509_crl_attempt attempt;
  const void* context;
} x509_crl_search_context;

static TC_TLV_result x509_crl_attempt_candidate(const void* context,
    const TC_X509_certificate* candidate, TC_X509_search_result* out)
{
  const x509_crl_search_context* search = context;
  return search->attempt(search->context,candidate,search->trust,out);
}

TC_TLV_result tc_x509_crl_search_candidates(void* cursor, tc_pki_candidate_next next,
    const tc_x509_crl* crl, const tc_x509_crl_extension_info* extensions,
    const tc_x509_crl_trust* trust, tc_x509_crl_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed)
{
  if (!crl || !extensions || !attempt || !tc_x509_crl_trust_valid(trust)) return TC_TLV_ARGUMENT;
  const tc_x509_crl_filter filter = {crl,extensions,&trust->validation->names};
  const x509_crl_search_context search = {trust,attempt,context};
  return tc_pki_certificate_search(cursor,next,tc_x509_crl_filter_match,&filter,&trust->options->parsing,
      trust->tree,trust->validation,x509_crl_attempt_candidate,&search,out,source_failed);
}

TC_TLV_result tc_x509_crl_scopes(const void* candidates, tc_x509_crl_search search_candidates,
    const tc_x509_crl_scope_processing* input, const tc_x509_crl_trust* trust,
    const tc_x509_crl_certificate_fields* fields, TC_TLV_reader* point_reader,
    int all_scopes, int* source_failed, TC_X509_search_result* out)
{
  if (!search_candidates || !input || !fields || !point_reader || !source_failed ||
      !tc_x509_crl_index_arguments(input->index,input->query,trust,out)) return TC_TLV_ARGUMENT;
  const TC_X509_crl_index* index = input->index;
  const size_t reference = input->reference;
  if (reference > index->count || (!all_scopes && reference == index->count)) return TC_TLV_ARGUMENT;
  tc_x509_crl_status status;
  tc_x509_crl_evidence* evidence = input->evidence;
  TC_TLV_result result = tc_x509_crl_evidence_status(evidence,&status);
  if (result != TC_TLV_OK) return result;
  if (status != TC_X509_CRL_UNDETERMINED) return TC_TLV_END;
  tc_x509_crl_scope_processing processing = *input;
  const tc_x509_crl_query* query = input->query;
  tc_x509_crl_query current_query = *query;
  current_query.certificate_ca = fields->ca;
  const TC_X509_path_options* options = trust->options;
  const tc_pki_tree_workspace* tree = trust->tree;
  const TC_X509_path_workspace* validation = trust->validation;
  const size_t initial_work = *tree->work;
  tc_pki_distribution_point point;
  tc_x509_crl_evidence pending = *evidence;
  TC_X509_search_result found;
  TC_TLV_result failure = TC_TLV_END;
  int contributed = 0;
  processing.evidence = &pending;
  processing.query = &current_query;
  const size_t end = all_scopes ? index->count : reference + 1;
  int fallback = !fields->points.length;
  int alternative = 0;
  for (;;) {
    current_query.point = alternative ? &fields->alternative : query->point;
    if (!fallback) {
      result = tc_pki_distribution_point_next(point_reader,tree,&point);
      if (result == TC_TLV_END) fallback = 1;
      else if (result != TC_TLV_OK) return result;
      else current_query.point = &point;
    }
    for (size_t i = reference; i < end; ++i) {
      const TC_X509_crl_record* record = &index->records[i];
      uint16_t reasons;
      processing.reference = i;
      result = tc_x509_path_charge(tree->work,1);
      if (result != TC_TLV_OK) return result;
      result = record->policy;
      if (result == TC_TLV_OK) {
        const tc_x509_crl_distribution* distribution = record->extensions.present & TC_CRL_EXT_DISTRIBUTION ?
            &record->extensions.distribution : NULL;
        /* Defer freshness checks until a delta has been selected. */
        result = tc_x509_crl_scope_reasons(&record->crl,distribution,current_query.point,
            query->certificate->issuer,current_query.certificate_ca,&options->parsing,tree,&validation->names,&reasons);
        if (result == TC_TLV_OK && !(reasons & ~pending.reasons)) result = TC_TLV_END;
        if (result == TC_TLV_OK && all_scopes) {
          for (size_t previous = 0; previous < i; ++previous) {
            if (index->records[previous].policy != TC_TLV_OK) continue;
            int same;
            result = tc_x509_crl_same_scope(&processing,previous,trust,&same);
            if (result != TC_TLV_OK || same) {
              if (result == TC_TLV_OK) result = TC_TLV_END;
              break;
            }
          }
          if (result == TC_TLV_OK) {
            tc_x509_crl_proposal chosen;
            result = tc_x509_crl_group(candidates,search_candidates,&processing,trust,source_failed,&chosen);
            if (result == TC_TLV_OK) result = chosen.result;
            if (result == TC_TLV_OK)
              result = tc_x509_crl_evidence_add(&pending,chosen.evidence.reasons,&chosen.evidence.revocation);
          }
        } else if (result == TC_TLV_OK)
            result = search_candidates(candidates,&record->crl,&record->extensions,trust,
                tc_x509_crl_scope_attempt,&processing,&found,source_failed);
      }
      if (result == TC_TLV_OK) {
        contributed = 1;
        result = tc_x509_crl_evidence_status(&pending,&status);
        if (result != TC_TLV_OK) return result;
        if (status != TC_X509_CRL_UNDETERMINED) break;
      } else {
        if (!all_scopes || *source_failed || result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT) return result;
        if (result == TC_TLV_UNSUPPORTED || (result != TC_TLV_END && failure == TC_TLV_END)) failure = result;
      }
    }
    if (status != TC_X509_CRL_UNDETERMINED) break;
    if (fallback) {
      if (alternative || !fields->alternative.name.encoded.length) break;
      alternative = 1;
    }
  }
  if (status == TC_X509_CRL_UNDETERMINED && failure != TC_TLV_END) return failure;
  if (!contributed) return TC_TLV_END;
  *evidence = pending;
  if (!all_scopes) {
    found.validation.work_used = initial_work - *tree->work;
    *out = found;
  }
  return TC_TLV_OK;
}


TC_TLV_result tc_x509_crl_points_init(tc_x509_crl_certificate_fields* fields,
    const TC_X509_certificate* certificate, TC_bytes* oids, size_t oid_capacity,
    TC_TLV_reader* reader)
{
  if (!fields || !fields->limits || !fields->tree || !fields->tree->work || !reader ||
      (oid_capacity && !oids)) return TC_TLV_ARGUMENT;
  tc_x509_crl_certificate_fields pending = *fields;
  TC_TLV_reader next = {0};
  TC_TLV_result result;
  if (certificate) {
    pending = (tc_x509_crl_certificate_fields){0};
    pending.limits = fields->limits;
    pending.tree = fields->tree;
    result = tc_pki_extensions_visit(certificate->extensions,pending.limits,pending.tree,
        oids,oid_capacity,tc_x509_crl_certificate_extension,&pending);
    if (result != TC_TLV_OK) return result;
  }
  if (pending.points.length) {
    result = tc_pki_distribution_points_init(pending.points,pending.limits,pending.tree,&next);
    if (result != TC_TLV_OK) return result;
    /* Reject malformed later points before revocation can stop the search. */
    TC_TLV_reader check = next;
    tc_pki_distribution_point point;
    while ((result = tc_pki_distribution_point_next(&check,pending.tree,&point)) == TC_TLV_OK) {}
    if (result != TC_TLV_END) return result;
  }
  *fields = pending;
  *reader = next;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_storage_inputs(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, const TC_bytes* writes, size_t write_count, size_t* work)
{
  if (!processing || !processing->index || !processing->query || !processing->query->certificate ||
      !processing->query->point || !tc_x509_crl_trust_valid(trust) || !work || (write_count && !writes))
    return TC_TLV_ARGUMENT;
  TC_bytes input;
  TC_TLV_result result;
#define CRL_BYTES(span) do { \
  result = tc_pki_storage_input(writes,write_count,(span),work); \
  if (result != TC_TLV_OK) return result; \
} while (0)
#define CRL_INPUT(pointer, count) do { \
  result = tc_pki_storage_span((pointer),(count),sizeof *(pointer),&input); \
  if (result != TC_TLV_OK) return result; \
  CRL_BYTES(input); \
} while (0)
  CRL_INPUT(processing->index,1);
  CRL_INPUT(processing->index->records,processing->index->count);
  CRL_INPUT(processing->query,1);
  CRL_INPUT(processing->query->certificate,1);
  CRL_INPUT(processing->query->point,1);
  if (processing->check) { CRL_INPUT(processing->check,1); }
  CRL_INPUT(trust->source,1);
  CRL_INPUT(trust->options,1);
  CRL_INPUT(trust->validation,1);
  CRL_INPUT(trust->search,1);
  CRL_INPUT(trust->tree,1);
  CRL_INPUT(trust->options->initial_policies,trust->options->initial_policy_count);
  CRL_BYTES(processing->query->certificate->encoded);
  CRL_BYTES(processing->query->certificate->extensions);
  CRL_BYTES(processing->query->certificate->issuer);
  CRL_BYTES(processing->query->certificate->serial);
  CRL_BYTES(processing->query->point->name.encoded);
  CRL_BYTES(processing->query->point->name.contents);
  CRL_BYTES(processing->query->point->issuer);
  CRL_BYTES(trust->options->purpose);
  CRL_BYTES(trust->options->anchor_names.permitted);
  CRL_BYTES(trust->options->anchor_names.excluded);
  for (size_t i = 0; i < trust->options->initial_policy_count; ++i) {
    CRL_BYTES(trust->options->initial_policies[i]);
  }
  result = tc_x509_crl_index_storage_bytes(processing->index,writes,write_count,work);
  if (result != TC_TLV_OK) return result;
#undef CRL_INPUT
#undef CRL_BYTES
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_index_storage_bytes(const TC_X509_crl_index* index,
    const TC_bytes* writes, size_t write_count, size_t* work)
{
  if (!index || !work || (index->count && !index->records) || (write_count && !writes))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result;
#define CRL_BYTES(span) do { \
  result = tc_pki_storage_input(writes,write_count,(span),work); \
  if (result != TC_TLV_OK) return result; \
} while (0)
  for (size_t i = 0; i < index->count; ++i) {
    const TC_X509_crl_record* record = &index->records[i];
    CRL_BYTES(record->crl.encoded);
    CRL_BYTES(record->crl.tbs);
    CRL_BYTES(record->crl.issuer);
    CRL_BYTES(record->crl.signature);
    CRL_BYTES(record->crl.revoked);
    CRL_BYTES(record->crl.extensions);
    CRL_BYTES(record->crl.signature_algorithm.oid);
    CRL_BYTES(record->crl.signature_algorithm.parameters);
    if (record->crl.prepared) {
      const TC_X509_crl_prepared* prepared = record->crl.prepared;
      TC_bytes span;
      result = tc_pki_storage_span(prepared,1,sizeof *prepared,&span);
      if (result != TC_TLV_OK) return result;
      CRL_BYTES(span);
      result = tc_pki_storage_span(prepared->targets,prepared->count,sizeof *prepared->targets,&span);
      if (result != TC_TLV_OK) return result;
      CRL_BYTES(span);
      result = tc_pki_storage_span(prepared->matches,prepared->count,sizeof *prepared->matches,&span);
      if (result != TC_TLV_OK) return result;
      CRL_BYTES(span);
      CRL_BYTES(prepared->digest);
      for (size_t target = 0; target < prepared->count; ++target) {
        CRL_BYTES(prepared->targets[target].serial);
        CRL_BYTES(prepared->targets[target].issuer);
      }
    }
    CRL_BYTES(record->extensions.number);
    CRL_BYTES(record->extensions.base_number);
    CRL_BYTES(record->extensions.distribution_encoded);
    CRL_BYTES(record->extensions.freshest);
    CRL_BYTES(record->extensions.issuer_alt);
    CRL_BYTES(record->extensions.unknown_critical_oid);
    CRL_BYTES(record->extensions.authority.key_identifier);
    CRL_BYTES(record->extensions.authority.issuer);
    CRL_BYTES(record->extensions.authority.serial);
    CRL_BYTES(record->extensions.distribution.name.encoded);
    CRL_BYTES(record->extensions.distribution.name.contents);
  }
#undef CRL_BYTES
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_storage_writes(const tc_x509_crl_scope_processing* processing,
    const tc_x509_crl_trust* trust, TC_X509_search_result* out,
    TC_bytes writes[CRL_SCOPE_NODES])
{
  if (!processing || !tc_x509_crl_trust_valid(trust) || !out || !writes) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_path_storage_writes(trust->validation,writes);
  if (result != TC_TLV_OK) return result;
#define CRL_ARRAY(pointer, count, slot) do { \
  result = tc_pki_storage_span((pointer),(count),sizeof *(pointer),&writes[slot]); \
  if (result != TC_TLV_OK) return result; \
} while (0)
  CRL_ARRAY(trust->search->path,trust->search->capacity,CRL_SCOPE_PATH);
  CRL_ARRAY(trust->search->frames,trust->search->capacity,CRL_SCOPE_SEARCH);
  CRL_ARRAY(trust->tree->frames,trust->tree->capacity,CRL_SCOPE_TREE);
  /* Tree traversal and path validation can reuse the same frame array. */
  if (trust->tree->frames == trust->validation->frames) {
    if (writes[CRL_SCOPE_TREE].length > writes[TC_X509_PATH_STORAGE_FRAMES].length)
      writes[TC_X509_PATH_STORAGE_FRAMES] = writes[CRL_SCOPE_TREE];
    writes[CRL_SCOPE_TREE] = (TC_bytes){NULL,0};
  }
  CRL_ARRAY(processing->states,processing->capacity,CRL_SCOPE_STATES);
  CRL_ARRAY(processing->evidence,1,CRL_SCOPE_EVIDENCE);
  CRL_ARRAY(out,1,CRL_SCOPE_RESULT);
  CRL_ARRAY(trust->tree->work,1,CRL_SCOPE_WORK);
#undef CRL_ARRAY
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_path_resolve(const TC_bytes* chain, size_t count,
    tc_x509_crl_certificate_resolve resolve, void* context, TC_X509_revocation_result* out)
{
  TC_bytes storage;
  if (!chain || !count || !resolve || !out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_storage_span(chain,count,sizeof *chain,&storage);
  if (result != TC_TLV_OK) return result;
  TC_X509_revocation_result proposed = {TC_X509_CRL_UNREVOKED,SIZE_MAX,{0}};
  for (size_t i = 0; i < count; ++i) {
    tc_x509_crl_evidence evidence = {0};
    result = resolve(context,chain[i],&evidence);
    if (result != TC_TLV_OK) return result;
    tc_x509_crl_status status;
    result = tc_x509_crl_evidence_status(&evidence,&status);
    if (result != TC_TLV_OK) return result;
    if (status == TC_X509_CRL_UNDETERMINED) return TC_TLV_UNSUPPORTED;
    if (status == TC_X509_CRL_REVOKED) {
      proposed.status = status; proposed.certificate_index = i; proposed.evidence = evidence;
      break;
    }
  }
  *out = proposed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_nodes_resolve(TC_X509_revocation_node* nodes, size_t capacity,
    size_t* count, size_t root, size_t* work, tc_x509_crl_node_evaluate evaluate,
    void* context, tc_x509_crl_evidence* out)
{
  if (!nodes || !count || !work || !evaluate || !out || *count > capacity || root >= *count)
    return TC_TLV_ARGUMENT;
  TC_TLV_result root_result = TC_TLV_UNSUPPORTED;
  for (;;) {
    const size_t previous_count = *count;
    size_t resolved = 0;
    for (size_t i = 0; i < *count; ++i) {
      /* Re-evaluate the root to recover its revocation reason and date. */
      if (i != root && nodes[i].status != TC_X509_CRL_UNDETERMINED) continue;
      tc_x509_crl_evidence evidence = {0};
      int stop = 0;
      const size_t before_work = *work, before_count = *count;
      TC_TLV_result result = evaluate(context,i,&evidence,&stop);
      if (*work > before_work) { *work = 0; return TC_TLV_ARGUMENT; }
      if (*count < before_count || *count > capacity) return TC_TLV_ARGUMENT;
      if (stop) return result == TC_TLV_OK ? TC_TLV_ARGUMENT : result;
      if (result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT) return result;
      if (i == root) root_result = result;
      if (result != TC_TLV_OK) continue;
      tc_x509_crl_status status;
      result = tc_x509_crl_evidence_status(&evidence,&status);
      if (result != TC_TLV_OK) return result;
      if (status == TC_X509_CRL_UNDETERMINED) continue;
      nodes[i].status = status;
      ++resolved;
      if (i == root) { *out = evidence; return TC_TLV_OK; }
    }
    if (!resolved && *count == previous_count)
      return root_result == TC_TLV_INVALID ? TC_TLV_INVALID : TC_TLV_UNSUPPORTED;
  }
}

typedef struct {
  tc_x509_crl_dependencies* dependencies;
  const tc_x509_crl_resolution* resolution;
  tc_x509_crl_extra_storage* extra;
  tc_x509_crl_held_path* path;
  const tc_x509_crl_path_check* check;
} x509_crl_node_context;

static TC_TLV_result x509_crl_node_evaluate(void* context, size_t index,
    tc_x509_crl_evidence* evidence, int* stop)
{
  const x509_crl_node_context* node = context;
  const tc_x509_crl_resolution* resolution = node->resolution;
  const tc_x509_crl_resolution_workspace* workspace = node->dependencies->workspace;
  TC_X509_certificate certificate;
  TC_TLV_result result = tc_x509_crl_dependency_read(node->dependencies,index,&certificate);
  if (result != TC_TLV_OK) { *stop = 1; return result; }
  const tc_pki_distribution_point fallback = {0};
  const tc_x509_crl_query query = {&certificate,&fallback,0};
  tc_x509_crl_evidence pending = {0};
  TC_X509_search_result scratch;
  tc_x509_crl_scope_processing processing = {resolution->index,0,resolution->delta_policy,
    resolution->order_policy,&query,workspace->states,workspace->state_capacity,
    &pending,node->check,NULL,NULL};
  node->extra->count = node->dependencies->count;
  result = tc_x509_crl_scope_execute(resolution->candidates,&processing,
      &(tc_x509_crl_trust){resolution->source,resolution->anchor_index,resolution->options,
        workspace->tree,workspace->validation,workspace->search},
      NULL,1,1,node->extra,node->path,&scratch);
  *stop = node->extra->source_failed && *node->extra->source_failed;
  if (result == TC_TLV_OK) *evidence = pending;
  return result;
}

TC_TLV_result tc_x509_crl_resolve(const TC_X509_certificate* target,
    const tc_x509_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace,
    tc_x509_crl_held_path* path, tc_x509_crl_evidence* out)
{
  if (!target || !resolution || !workspace || !out || !target->encoded.data ||
      !target->encoded.length || !resolution->candidates) return TC_TLV_ARGUMENT;
  const tc_x509_crl_trust trust = {resolution->source,resolution->anchor_index,resolution->options,
    workspace->tree,workspace->validation,workspace->search};
  const tc_pki_distribution_point fallback = {0};
  const tc_x509_crl_query query = {target,&fallback,0};
  TC_X509_search_result scratch;
  if (!tc_x509_crl_index_arguments(resolution->index,&query,&trust,&scratch) ||
      !x509_crl_delta_policy_valid(resolution->delta_policy) ||
      !x509_crl_order_policy_valid(resolution->order_policy)) return TC_TLV_ARGUMENT;
  if (!workspace->node_capacity || workspace->state_capacity < resolution->index->count)
    return TC_TLV_LIMIT;
  int source_failed = 0;
  tc_x509_crl_extra_storage extra = {0};
  extra.count = path ? path->dependency_count : 0;
  if (extra.count > workspace->node_capacity) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_storage_span(workspace->nodes,workspace->node_capacity,
      sizeof *workspace->nodes,&extra.nodes_storage);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_span(out,1,sizeof *out,&extra.output_storage);
  if (result != TC_TLV_OK) return result;
  extra.inputs[CRL_EXTRA_RESOLUTION] = (TC_bytes){(const uint8_t*)resolution,sizeof *resolution};
  extra.inputs[CRL_EXTRA_WORKSPACE] = (TC_bytes){(const uint8_t*)workspace,sizeof *workspace};
  extra.nodes = workspace->nodes;
  extra.source_failed = &source_failed;
  TC_bytes writes[CRL_SCOPE_WRITES];
  TC_X509_store_anchor anchor;
  tc_x509_crl_evidence pending = {0};
  tc_x509_crl_dependencies dependencies = {resolution->options,resolution->anchor_index,
    workspace,&anchor.trust,writes,CRL_SCOPE_WRITES,extra.count};
  const tc_x509_crl_path_check check = {&dependencies,tc_x509_crl_dependencies_check};
  const tc_x509_crl_scope_processing processing = {resolution->index,0,
    resolution->delta_policy,resolution->order_policy,&query,workspace->states,
    workspace->state_capacity,&pending,&check,NULL,NULL};
  result = x509_crl_scope_prepare(resolution->candidates,&processing,&trust,NULL,
      &extra,path,&scratch,writes);
  if (result != TC_TLV_OK) return result;
  const tc_pki_source_guard source_guard = {resolution->source,writes,CRL_SCOPE_WRITES};
  result = tc_pki_source_guard_anchor((void*)&source_guard,resolution->anchor_index,
      workspace->tree->work,&anchor);
  if (result != TC_TLV_OK) return result;
  x509_crl_node_context node = {&dependencies,resolution,&extra,path,&check};
  return tc_x509_crl_resolve_dependencies(target->encoded,&dependencies,
      x509_crl_node_evaluate,&node,path,out);
}

typedef struct {
  tc_x509_crl_held_path* held;
  const tc_x509_crl_resolution* resolution;
  const tc_x509_crl_resolution_workspace* workspace;
} x509_crl_path_context;

static TC_TLV_result x509_crl_path_certificate(void* context, TC_bytes encoded,
    tc_x509_crl_evidence* evidence)
{
  const x509_crl_path_context* path = context;
  TC_X509_certificate certificate = {0};
  certificate.encoded = encoded;
  return tc_x509_crl_resolve(&certificate,path->resolution,path->workspace,path->held,evidence);
}

TC_TLV_result tc_x509_crl_path_operation(tc_x509_crl_held_path* held,
    const tc_x509_crl_resolution* resolution, const tc_x509_crl_resolution_workspace* workspace)
{
  if (!held) return TC_TLV_ARGUMENT;
  x509_crl_path_context context = {held,resolution,workspace};
  return tc_x509_crl_path_resolve(held->chain,held->count,x509_crl_path_certificate,
      &context,held->out);
}

TC_TLV_result TC_X509_path_check_revocation(const TC_bytes* chain, size_t count,
    const TC_X509_revocation_options* options, const TC_X509_revocation_workspace* workspace,
    size_t* work, TC_X509_revocation_result* out)
{
  if (!options || !options->source || !options->signer_policy || !workspace ||
      !workspace->validation || !work) return TC_TLV_ARGUMENT;
  const tc_pki_tree_workspace tree = {workspace->validation->frames,
    workspace->validation->frame_capacity,work};
  tc_pki_store_candidates cursor = {options->source,options->signer_policy->parsing,
    0,options->source->candidate_count,options->max_candidate_bytes};
  const TC_bytes candidate_metadata[] = {
    {(const uint8_t*)&cursor,sizeof cursor}
  };
  const tc_x509_crl_operation_source candidates = {
    {&cursor,options->source,tc_x509_crl_store_source_search},
    candidate_metadata,sizeof candidate_metadata / sizeof *candidate_metadata
  };
  const tc_x509_crl_resolution resolution = {&candidates,options->index,options->source,
    options->signer_policy,options->anchor_index,options->delta_policy,options->order_policy};
  const tc_x509_crl_resolution_workspace scratch = {&tree,workspace->validation,
    workspace->search,workspace->states,workspace->state_capacity,
    workspace->nodes,workspace->node_capacity};
  tc_x509_crl_held_path held = {0};
  held.chain = chain;
  held.count = count;
  held.out = out;
  held.metadata[CRL_PATH_OPTIONS] = (TC_bytes){(const uint8_t*)options,sizeof *options};
  held.metadata[CRL_PATH_WORKSPACE] = (TC_bytes){(const uint8_t*)workspace,sizeof *workspace};
  return tc_x509_crl_path_operation(&held,&resolution,&scratch);
}

TC_X509_path_status tc_x509_crl_dependencies_path(const TC_X509_search_result* path,
    const tc_x509_crl_resolution_workspace* workspace, size_t* count,
    const TC_bytes* writes, size_t write_count, size_t* work)
{
  if (!path || !workspace || !count || !work || (path->count && !path->path) ||
      (write_count && !writes) || *count > workspace->node_capacity ||
      (workspace->node_capacity && !workspace->nodes)) return TC_X509_PATH_ERROR;
  int unresolved = 0;
  for (size_t i = 0; i < path->count; ++i) {
    size_t index;
    TC_TLV_result result = tc_pki_storage_input(writes,write_count,path->path[i],work);
    if (result != TC_TLV_OK) return tc_x509_path_status(result);
    result = tc_x509_crl_dependency_find(workspace->nodes,workspace->node_capacity,
        count,path->path[i],work,&index);
    if (result != TC_TLV_OK) return tc_x509_path_status(result);
    switch (workspace->nodes[index].status) {
      case TC_X509_CRL_REVOKED: return TC_X509_PATH_INVALID;
      case TC_X509_CRL_UNDETERMINED: unresolved = 1; break;
      case TC_X509_CRL_UNREVOKED: break;
      default: return TC_X509_PATH_ERROR;
    }
  }
  return unresolved ? TC_X509_PATH_UNSUPPORTED : TC_X509_PATH_VALID;
}

TC_X509_signature_result tc_x509_crl_selected_anchor_check(const tc_x509_crl_selected* selected,
    const TC_X509_trust_anchor* anchor, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names, size_t* work)
{
  if (!selected || !selected->base || !anchor || !provider || !limits || !names || !work)
    return TC_X509_SIGNATURE_ERROR;
  const tc_x509_crl* records[] = {selected->base,selected->delta};
  for (size_t i = 0; i < sizeof records / sizeof *records; ++i) {
    if (!records[i]) continue;
    TC_X509_signature_result result = tc_x509_crl_anchor_check(records[i],anchor,provider,limits,names,work);
    if (result != TC_X509_SIGNATURE_VALID) return result;
  }
  return TC_X509_SIGNATURE_VALID;
}

TC_TLV_result tc_x509_crl_dependency_find(TC_X509_revocation_node* nodes,
    size_t capacity, size_t* count, TC_bytes certificate, size_t* work, size_t* index)
{
  if (!count || !work || !index || *count > capacity || (capacity && !nodes) ||
      !certificate.data || !certificate.length) return TC_TLV_ARGUMENT;
  for (size_t i = 0; i < *count; ++i) {
    const TC_bytes previous = nodes[i].certificate;
    TC_TLV_result result = tc_x509_path_charge(work,1);
    if (result != TC_TLV_OK) return result;
    if (previous.length != certificate.length) continue;
    result = tc_x509_path_charge(work,certificate.length);
    if (result != TC_TLV_OK) return result;
    if (tc_pki_equal(previous,certificate)) { *index = i; return TC_TLV_OK; }
  }
  if (*count == capacity) return TC_TLV_LIMIT;
  nodes[*count] = (TC_X509_revocation_node){certificate,TC_X509_CRL_UNDETERMINED};
  *index = (*count)++;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_certificate_extension(void* context, const TC_X509_extension* extension)
{
  enum { ISSUER_ALT_NAME = 18, BASIC_CONSTRAINTS = 19, DISTRIBUTION_POINTS = 31 };
  tc_x509_crl_certificate_fields* fields = context;
  TC_TLV_result result;
  switch (tc_pki_extension_id(extension)) {
    case BASIC_CONSTRAINTS: {
      TC_X509_basic_constraints basic;
      result = TC_X509_basic_constraints_read(extension->value.data,extension->value.length,&basic);
      if (result == TC_TLV_OK) fields->ca = basic.ca;
      return result;
    }
    case DISTRIBUTION_POINTS:
      fields->points = extension->value;
      return TC_TLV_OK;
    case ISSUER_ALT_NAME: {
      TC_bytes contents;
      result = TC_DER_sequence(extension->value.data,extension->value.length,&contents);
      if (result != TC_TLV_OK) return result;
      result = tc_pki_general_names_contents_check(contents,fields->limits,fields->tree);
      if (result != TC_TLV_OK) return result;
      /* fullName and issuerAltName carry the same GeneralNames contents. */
      fields->alternative.name = (tc_pki_distribution_name){extension->value,contents,0};
      return TC_TLV_OK;
    }
    default: return TC_TLV_OK;
  }
}

TC_TLV_result tc_x509_crl_same_scope(const tc_x509_crl_scope_processing* processing,
    size_t other, const tc_x509_crl_trust* trust, int* same)
{
  const TC_X509_crl_record* a = &processing->index->records[processing->reference];
  const TC_X509_crl_record* b = &processing->index->records[other];
  return tc_x509_crl_scope_equal(&a->crl,&a->extensions,&b->crl,&b->extensions,
      &trust->options->parsing,trust->tree,&trust->validation->names,same);
}

/* Authenticate proposals from every reference key before choosing this scope. */
TC_TLV_result tc_x509_crl_group(const void* candidates, tc_x509_crl_search search,
    const tc_x509_crl_scope_processing* processing, const tc_x509_crl_trust* trust,
    int* source_failed, tc_x509_crl_proposal* out)
{
  tc_x509_crl_proposal chosen = {0};
  TC_TLV_result failure = TC_TLV_END;
  int unranked = 0;
  for (size_t i = 0; i < processing->index->count; ++i) {
    TC_TLV_result result = tc_x509_path_charge(trust->tree->work,1);
    if (result != TC_TLV_OK) return result;
    const TC_X509_crl_record* record = &processing->index->records[i];
    if (record->policy != TC_TLV_OK) continue;
    int same;
    result = tc_x509_crl_same_scope(processing,i,trust,&same);
    if (result != TC_TLV_OK) return result;
    if (!same) continue;
    tc_x509_crl_evidence empty = {0};
    tc_x509_crl_proposal candidate = {0}, unresolved = {0};
    TC_X509_search_result path;
    tc_x509_crl_scope_processing attempt = *processing;
    attempt.reference = i; attempt.evidence = &empty; attempt.proposal = &candidate;
    attempt.unresolved = &unresolved;
    result = search(candidates,&record->crl,&record->extensions,trust,
        tc_x509_crl_scope_attempt,&attempt,&path,source_failed);
    if (*source_failed || result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT) return result;
    if (unresolved.selected.base && result != TC_TLV_OK) {
      TC_TLV_result merged = tc_x509_crl_proposal_merge(&chosen,&unresolved,processing->order_policy,trust->tree);
      if (merged != TC_TLV_OK) return merged;
    }
    if (result != TC_TLV_OK) {
      if (result == TC_TLV_UNSUPPORTED && !unresolved.selected.base) unranked = 1;
      if (result != TC_TLV_END && failure == TC_TLV_END) failure = result;
      continue;
    }
    if (unresolved.selected.base) {
      result = tc_x509_crl_proposal_merge(&candidate,&unresolved,processing->order_policy,trust->tree);
      if (result != TC_TLV_OK) return result;
    }
    result = tc_x509_crl_proposal_merge(&chosen,&candidate,processing->order_policy,trust->tree);
    if (result != TC_TLV_OK) return result;
  }
  /* Resolving an unranked candidate is required before choosing a record. */
  if (unranked) return TC_TLV_UNSUPPORTED;
  if (!chosen.selected.base) return failure;
  *out = chosen;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_attempt(const void* context,
    const TC_X509_certificate* signer, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out)
{
  const tc_x509_crl_scope_processing* processing = context;
  tc_x509_crl_signature_cache cache;
  TC_X509_search_result found;
  TC_TLV_result result = tc_x509_crl_signature_cache_init(processing->index,signer,
      &trust->options->signatures,&trust->options->parsing,&trust->validation->names,
      processing->states,processing->capacity,trust->tree->work,&cache);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_path_result_status(tc_x509_crl_signer_validate(
      &processing->index->records[processing->reference].crl,signer,trust->source,
      trust->anchor_index,trust->options,trust->validation,trust->search,trust->tree->work,&found));
  if (result != TC_TLV_OK) return result;
  /* Signer validation already checked this signature with the cache's provider. */
  cache.states[processing->reference] = CRL_SIGNATURE_VALID;
  tc_x509_crl_evidence pending = *processing->evidence;
  tc_x509_crl_selected selected = {0};
  result = tc_x509_crl_scope_evaluate(&cache,processing->reference,processing->delta_policy,
      processing->order_policy,processing->query,&trust->options->at,trust->tree,
      trust->validation->oids,trust->validation->oid_capacity,&pending,
      processing->proposal || processing->check ? &selected : NULL);
  if (result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT) return result;
  if (processing->check) {
    const size_t before = *trust->tree->work;
    TC_X509_path_status checked = processing->check->verify(processing->check->context,&found,
        selected.base ? &selected : NULL,trust->tree->work);
    if (*trust->tree->work > before) { *trust->tree->work = 0; return TC_TLV_ARGUMENT; }
    TC_TLV_result check_result = tc_x509_path_result_status(checked);
    if (check_result == TC_TLV_UNSUPPORTED && processing->unresolved && selected.base && result != TC_TLV_END) {
      const tc_x509_crl_proposal pending_check = {selected,pending,TC_TLV_UNSUPPORTED,result == TC_TLV_OK};
      TC_TLV_result merged = tc_x509_crl_proposal_merge(processing->unresolved,&pending_check,
          processing->order_policy,trust->tree);
      if (merged != TC_TLV_OK) return merged;
    }
    if (check_result != TC_TLV_OK) return check_result;
  }
  if (processing->proposal && selected.base &&
      (result == TC_TLV_OK || result == TC_TLV_INVALID || result == TC_TLV_UNSUPPORTED)) {
    *processing->proposal = (tc_x509_crl_proposal){selected,pending,result,0};
    *out = found;
    return TC_TLV_OK;
  }
  if (result != TC_TLV_OK) return result;
  *processing->evidence = pending;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_proposal_merge(tc_x509_crl_proposal* chosen, const tc_x509_crl_proposal* candidate,
    TC_X509_crl_order_policy policy, const tc_pki_tree_workspace* tree)
{
  int order, equal;
  if (!candidate->selected.base) return TC_TLV_ARGUMENT;
  if (!chosen->selected.base) { *chosen = *candidate; return TC_TLV_OK; }
  TC_TLV_result result = x509_crl_order(&candidate->selected,&chosen->selected,policy,tree->work,&order);
  if (result != TC_TLV_OK || order < 0) return result;
  if (order > 0) { *chosen = *candidate; return TC_TLV_OK; }
  /* Another valid signer certificate can establish the same selected records. */
  if (candidate->selected.base == chosen->selected.base && candidate->selected.delta == chosen->selected.delta) {
    if (candidate->result == TC_TLV_OK && chosen->pending_only) {
      *chosen = *candidate; return TC_TLV_OK;
    }
    if (chosen->result == TC_TLV_OK && candidate->pending_only) return TC_TLV_OK;
  }
  if (candidate->result != TC_TLV_OK || chosen->result != TC_TLV_OK) {
    chosen->pending_only = chosen->pending_only && candidate->pending_only &&
        candidate->selected.base == chosen->selected.base && candidate->selected.delta == chosen->selected.delta;
    chosen->result = candidate->result == TC_TLV_UNSUPPORTED || chosen->result == TC_TLV_UNSUPPORTED ?
        TC_TLV_UNSUPPORTED : TC_TLV_INVALID;
    return TC_TLV_OK;
  }
  const tc_x509_crl* a = candidate->selected.delta ? candidate->selected.delta : candidate->selected.base;
  const tc_x509_crl* b = chosen->selected.delta ? chosen->selected.delta : chosen->selected.base;
  result = tc_x509_path_charge(tree->work,1);
  if (result != TC_TLV_OK) return result;
  result = TC_X509_time_compare(&a->this_update,&b->this_update,&order);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_evidence_equal(&candidate->evidence,&chosen->evidence,&equal);
  if (result != TC_TLV_OK) return result;
  if (order || !equal) chosen->result = TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_check_signer(const void* context,
    const TC_X509_certificate* candidate, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out)
{
  return tc_x509_path_result_status(tc_x509_crl_signer_validate(context,candidate,
      trust->source,trust->anchor_index,trust->options,trust->validation,trust->search,
      trust->tree->work,out));
}

TC_TLV_result tc_x509_crl_process_candidate(const void* context,
    const TC_X509_certificate* candidate, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out)
{
  const tc_x509_crl_processing* processing = context;
  return tc_x509_crl_process(processing->selected,candidate,processing->query,trust->source,
      trust->anchor_index,trust->options,trust->validation,trust->search,trust->tree->work,
      processing->evidence,out);
}

TC_TLV_result tc_x509_crl_index_attempt(const void* context,
    const TC_X509_certificate* signer, const tc_x509_crl_trust* trust,
    TC_X509_search_result* out)
{
  const tc_x509_crl_index_processing* processing = context;
  const TC_X509_crl_record* base = &processing->index->records[processing->base];
  tc_x509_crl_selected selected = {&base->crl,&base->extensions,NULL,NULL};
  TC_X509_search_result found;
  if (processing->delta_policy == TC_X509_CRL_COMPLETE_ONLY)
    return tc_x509_crl_process(&selected,signer,processing->query,trust->source,
        trust->anchor_index,trust->options,trust->validation,trust->search,trust->tree->work,
        processing->evidence,out);
  TC_TLV_result result = tc_x509_crl_selected_validate(&selected,signer,trust->source,
      trust->anchor_index,trust->options,trust->validation,trust->search,trust->tree->work,&found);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_delta_select(processing->index,processing->base,signer,&trust->options->at,
      &trust->options->signatures,&trust->options->parsing,trust->tree,&trust->validation->names,&selected);
  if (result != TC_TLV_OK &&
      !(result == TC_TLV_END && processing->delta_policy == TC_X509_CRL_DELTA_IF_AVAILABLE))
    return result;
  /* Selection preserves the established signer path and verified base. */
  result = tc_x509_crl_apply(&selected,processing->query,&trust->options->at,
      &trust->options->parsing,trust->tree,&trust->validation->names,
      trust->validation->oids,trust->validation->oid_capacity,processing->evidence);
  if (result != TC_TLV_OK) return result;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_candidate_matches(const tc_x509_crl* crl,
    const tc_x509_crl_extension_info* extensions, const TC_X509_certificate* candidate,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names,
    const tc_pki_tree_workspace* tree, int* matched)
{
  if (!crl || !extensions || !candidate || !limits || !names || !tree || !tree->work || !matched)
    return TC_TLV_ARGUMENT;
  int accepted;
  TC_TLV_result result = TC_X509_name_equal(crl->issuer,candidate->subject,limits,names,tree->work,&accepted);
  if (result != TC_TLV_OK) return result;
  if (accepted && (extensions->present & TC_CRL_EXT_AUTHORITY)) {
    result = tc_pki_authority_matches(&extensions->authority,candidate,limits,tree,names,&accepted);
    if (result != TC_TLV_OK) return result;
  }
  if (accepted) {
    result = tc_x509_crl_signer_usage(candidate,limits,tree->work,&accepted);
    if (result != TC_TLV_OK) return result;
  }
  *matched = accepted;
  return TC_TLV_OK;
}


TC_TLV_result tc_x509_crl_delta_next(const TC_X509_crl_index* index, size_t base,
    size_t* cursor, const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_name_workspace* names, tc_x509_crl_selected* out)
{
  if (!index || !index->records || base >= index->count || !cursor ||
      *cursor > index->count || !limits || !tree || !tree->work || !names || !out)
    return TC_TLV_ARGUMENT;
  const TC_X509_crl_record* complete = &index->records[base];
  if (complete->policy != TC_TLV_OK) return complete->policy;
  if (complete->extensions.present & TC_CRL_EXT_DELTA) return TC_TLV_ARGUMENT;
  for (size_t i = *cursor; i < index->count; ++i) {
    TC_TLV_result result = tc_x509_path_charge(tree->work,1);
    if (result != TC_TLV_OK) return result;
    const TC_X509_crl_record* delta = &index->records[i];
    if (i == base || delta->policy != TC_TLV_OK ||
        !(delta->extensions.present & TC_CRL_EXT_DELTA)) continue;
    int compatible;
    result = tc_x509_crl_delta_compatible(&complete->crl,&complete->extensions,
        &delta->crl,&delta->extensions,limits,tree,names,&compatible);
    if (result != TC_TLV_OK) return result;
    if (!compatible) continue;
    *out = (tc_x509_crl_selected){&complete->crl,&complete->extensions,
      &delta->crl,&delta->extensions};
    *cursor = i + 1;
    return TC_TLV_OK;
  }
  return TC_TLV_END;
}

TC_TLV_result tc_x509_crl_signature_cache_init(const TC_X509_crl_index* index,
    const TC_X509_certificate* signer, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* names,
    uint8_t* states, size_t capacity, size_t* work, tc_x509_crl_signature_cache* out)
{
  TC_bytes storage;
  if (!index || (index->count && !index->records) || !signer || !limits || !names || !work || !out ||
      tc_pki_storage_span(states,capacity,sizeof *states,&storage) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  if (capacity < index->count) return TC_TLV_LIMIT;
  TC_TLV_result result = tc_x509_path_charge(work,index->count);
  if (result != TC_TLV_OK) return result;
  if (index->count) memset(states,CRL_SIGNATURE_UNCHECKED,index->count);
  *out = (tc_x509_crl_signature_cache){index,signer,provider,limits,names,states,capacity};
  return TC_TLV_OK;
}

static int x509_crl_signature_cache_valid(const tc_x509_crl_signature_cache* cache)
{
  return cache && cache->index && cache->signer && cache->limits && cache->names &&
      (!cache->index->count || (cache->index->records && cache->states)) &&
      cache->capacity >= cache->index->count;
}

TC_TLV_result tc_x509_crl_signature_cached(const tc_x509_crl_signature_cache* cache,
    size_t record, size_t* work)
{
  if (!x509_crl_signature_cache_valid(cache) || !work || record >= cache->index->count)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_path_charge(work,1);
  if (result != TC_TLV_OK) return result;
  if (cache->states[record] == CRL_SIGNATURE_VALID) return TC_TLV_OK;
  if (cache->states[record] == CRL_SIGNATURE_INVALID) return TC_TLV_INVALID;
  if (cache->states[record] != CRL_SIGNATURE_UNCHECKED) return TC_TLV_ARGUMENT;
  result = tc_pki_signature_status(tc_x509_crl_signer_check(&cache->index->records[record].crl,
      cache->signer,cache->provider,cache->limits,cache->names,work));
  if (result == TC_TLV_OK) cache->states[record] = CRL_SIGNATURE_VALID;
  else if (result == TC_TLV_INVALID) cache->states[record] = CRL_SIGNATURE_INVALID;
  return result;
}

static TC_TLV_result x509_crl_delta_select(const TC_X509_crl_index* index, size_t base,
    const TC_X509_certificate* signer, const TC_X509_time* at,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    const tc_x509_crl_signature_cache* cache, int* conflict, tc_x509_crl_selected* out)
{
  tc_x509_crl_selected candidate, chosen = {0};
  TC_TLV_result result;
  size_t cursor = 0;
  int conflicting = 0;
  if (!signer || !at || !out) return TC_TLV_ARGUMENT;
  while ((result = tc_x509_crl_delta_next(index,base,&cursor,limits,tree,names,&candidate)) == TC_TLV_OK) {
    tc_x509_crl_freshness freshness;
    result = tc_x509_crl_fresh_at(candidate.delta,at,&freshness);
    if (result != TC_TLV_OK) return result;
    if (freshness != TC_X509_CRL_CURRENT) continue;
    result = cache ? tc_x509_crl_signature_cached(cache,cursor - 1,tree->work) :
        tc_pki_signature_status(tc_x509_crl_signer_check(candidate.delta,signer,
            provider,limits,names,tree->work));
    if (result == TC_TLV_INVALID) continue;
    if (result != TC_TLV_OK) return result;
    if (chosen.delta) {
      int order;
      result = tc_x509_crl_number_compare(candidate.delta_info->number,
          chosen.delta_info->number,tree->work,&order);
      if (result != TC_TLV_OK) return result;
      if (order < 0) continue;
      if (!order) {
        int equal;
        result = tc_x509_crl_content_equal(candidate.delta,chosen.delta,tree->work,&equal);
        if (result != TC_TLV_OK) return result;
        /* A higher authenticated number may supersede this conflict. */
        if (!equal) {
          conflicting = 1;
          result = tc_x509_path_charge(tree->work,1);
          if (result != TC_TLV_OK) return result;
          result = TC_X509_time_compare(&candidate.delta->this_update,&chosen.delta->this_update,&order);
          if (result != TC_TLV_OK) return result;
          if (order > 0) chosen = candidate;
        }
        continue;
      }
    }
    chosen = candidate;
    conflicting = 0;
  }
  if (result != TC_TLV_END) return result;
  if (!chosen.delta) return TC_TLV_END;
  /* Scope selection can defer rejection until it knows the highest number. */
  if (conflicting && !conflict) return TC_TLV_INVALID;
  if (conflict) *conflict = conflicting;
  *out = chosen;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_delta_select(const TC_X509_crl_index* index, size_t base,
    const TC_X509_certificate* signer, const TC_X509_time* at,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    tc_x509_crl_selected* out)
{
  return x509_crl_delta_select(index,base,signer,at,provider,limits,tree,names,NULL,NULL,out);
}

TC_TLV_result tc_x509_crl_delta_select_cached(const tc_x509_crl_signature_cache* cache,
    size_t base, const TC_X509_time* at, const tc_pki_tree_workspace* tree,
    tc_x509_crl_selected* out)
{
  if (!x509_crl_signature_cache_valid(cache)) return TC_TLV_ARGUMENT;
  return x509_crl_delta_select(cache->index,base,cache->signer,at,cache->provider,
      cache->limits,tree,cache->names,cache,NULL,out);
}

static TC_TLV_result x509_crl_effective_next(const tc_x509_crl_signature_cache* cache,
    size_t reference, size_t* cursor, TC_X509_crl_delta_policy delta_policy,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, int* conflict,
    tc_x509_crl_selected* out)
{
  if (!x509_crl_signature_cache_valid(cache) || reference >= cache->index->count ||
      !cursor || *cursor > cache->index->count || !at || !tree || !tree->work || !out ||
      !x509_crl_delta_policy_valid(delta_policy)) return TC_TLV_ARGUMENT;
  const TC_X509_crl_record* scope = &cache->index->records[reference];
  for (size_t i = *cursor; i < cache->index->count; ++i) {
    TC_TLV_result result = tc_x509_path_charge(tree->work,1);
    if (result != TC_TLV_OK) return result;
    const TC_X509_crl_record* base = &cache->index->records[i];
    if (base->policy != TC_TLV_OK || (base->extensions.present & TC_CRL_EXT_DELTA)) continue;
    int same_scope;
    result = tc_x509_crl_scope_equal(&scope->crl,&scope->extensions,&base->crl,&base->extensions,
        cache->limits,tree,cache->names,&same_scope);
    if (result != TC_TLV_OK) return result;
    if (!same_scope) continue;
    tc_x509_crl_freshness freshness;
    result = tc_x509_crl_fresh_at(&base->crl,at,&freshness);
    if (result != TC_TLV_OK) return result;
    if (freshness == TC_X509_CRL_FUTURE ||
        (delta_policy == TC_X509_CRL_COMPLETE_ONLY && freshness != TC_X509_CRL_CURRENT)) continue;
    result = tc_x509_crl_signature_cached(cache,i,tree->work);
    if (result == TC_TLV_INVALID) continue;
    if (result != TC_TLV_OK) return result;
    tc_x509_crl_selected selected = {&base->crl,&base->extensions,NULL,NULL};
    int conflicting = 0;
    if (delta_policy != TC_X509_CRL_COMPLETE_ONLY) {
      result = x509_crl_delta_select(cache->index,i,cache->signer,at,cache->provider,
          cache->limits,tree,cache->names,cache,conflict ? &conflicting : NULL,&selected);
      if (result == TC_TLV_END) {
        if (delta_policy == TC_X509_CRL_DELTA_REQUIRED || freshness != TC_X509_CRL_CURRENT) continue;
      } else if (result != TC_TLV_OK) return result;
    }
    if (conflict) *conflict = conflicting;
    *out = selected; *cursor = i + 1;
    return TC_TLV_OK;
  }
  return TC_TLV_END;
}

TC_TLV_result tc_x509_crl_effective_next(const tc_x509_crl_signature_cache* cache,
    size_t reference, size_t* cursor, TC_X509_crl_delta_policy delta_policy,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, tc_x509_crl_selected* out)
{
  return x509_crl_effective_next(cache,reference,cursor,delta_policy,at,tree,NULL,out);
}

static TC_TLV_result x509_crl_latest(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, TC_X509_crl_order_policy order_policy,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, tc_x509_crl_selected* out, int* conflict_out)
{
  tc_x509_crl_selected selected, latest = {0};
  TC_TLV_result result;
  size_t cursor = 0;
  int conflict, latest_conflict = 0;
  if (!out || !x509_crl_order_policy_valid(order_policy)) return TC_TLV_ARGUMENT;
  while ((result = x509_crl_effective_next(cache,reference,&cursor,delta_policy,at,tree,&conflict,&selected)) == TC_TLV_OK) {
    int order;
    result = x509_crl_order(&selected,latest.base ? &latest : &selected,order_policy,tree->work,&order);
    if (result != TC_TLV_OK) return result;
    if (latest.base) {
      if (order < 0) continue;
      if (!order) { latest_conflict |= conflict; continue; }
    }
    latest = selected;
    latest_conflict = conflict;
  }
  if (result != TC_TLV_END) return result;
  if (!latest.base) return TC_TLV_END;
  if (latest_conflict && !conflict_out) return TC_TLV_INVALID;
  if (conflict_out) *conflict_out = latest_conflict;
  *out = latest;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_latest_number(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, const TC_X509_time* at,
    const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  tc_x509_crl_selected latest;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = x509_crl_latest(cache,reference,delta_policy,TC_X509_CRL_ORDER_NUMBER,at,tree,&latest,NULL);
  if (result == TC_TLV_OK) *out = latest.delta ? latest.delta_info->number : latest.base_info->number;
  return result;
}

TC_TLV_result tc_x509_crl_scope_evaluate(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, TC_X509_crl_order_policy order_policy,
    const tc_x509_crl_query* query,
    const TC_X509_time* at, const tc_pki_tree_workspace* tree, TC_bytes* oids,
    size_t oid_capacity, tc_x509_crl_evidence* evidence, tc_x509_crl_selected* preference)
{
  tc_x509_crl_status status;
  tc_x509_crl_selected selected;
  tc_x509_crl_evidence chosen = {0};
  const TC_X509_time* update = NULL;
  tc_x509_crl_selected latest;
  size_t cursor = 0;
  int conflict = 0;
  if (!x509_crl_signature_cache_valid(cache) || reference >= cache->index->count ||
      !x509_crl_delta_policy_valid(delta_policy) || !x509_crl_order_policy_valid(order_policy) ||
      !query || !query->certificate || !query->point ||
      (query->certificate_ca != 0 && query->certificate_ca != 1) || !at || !tree || !tree->work)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_x509_crl_evidence_status(evidence,&status);
  if (result != TC_TLV_OK) return result;
  if (status != TC_X509_CRL_UNDETERMINED) return TC_TLV_END;
  result = x509_crl_latest(cache,reference,delta_policy,order_policy,at,tree,&latest,preference ? &conflict : NULL);
  if (result != TC_TLV_OK) return result;
  /* A proposal retains its rank even when entries or tied records conflict. */
  if (preference) *preference = latest;
  if (conflict) return TC_TLV_INVALID;
  while ((result = x509_crl_effective_next(cache,reference,&cursor,delta_policy,at,tree,&conflict,&selected)) == TC_TLV_OK) {
    const tc_x509_crl* effective = selected.delta ? selected.delta : selected.base;
    int order, equal;
    result = x509_crl_order(&selected,&latest,order_policy,tree->work,&order);
    if (result != TC_TLV_OK) return result;
    if (order) continue;
    if (conflict) return TC_TLV_INVALID;
    tc_x509_crl_evidence candidate = {0};
    result = tc_x509_crl_apply(&selected,query,at,cache->limits,tree,cache->names,oids,oid_capacity,&candidate);
    if (result == TC_TLV_END) continue;
    if (result != TC_TLV_OK) return result;
    if (update) {
      result = TC_X509_time_compare(update,&effective->this_update,&order);
      if (result != TC_TLV_OK) return result;
      if (order) return TC_TLV_INVALID;
      result = tc_x509_crl_evidence_equal(&chosen,&candidate,&equal);
      if (result != TC_TLV_OK) return result;
      if (!equal) return TC_TLV_INVALID;
    } else {
      chosen = candidate;
      update = &effective->this_update;
    }
  }
  if (result != TC_TLV_END) return result;
  if (!update || !(chosen.reasons & ~evidence->reasons)) return TC_TLV_END;
  return tc_x509_crl_evidence_add(evidence,chosen.reasons,&chosen.revocation);
}

TC_TLV_result tc_x509_crl_scope_apply(const tc_x509_crl_signature_cache* cache,
    size_t reference, TC_X509_crl_delta_policy delta_policy, TC_X509_crl_order_policy order_policy,
    const tc_x509_crl_query* query, const TC_X509_time* at, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t oid_capacity, tc_x509_crl_evidence* evidence)
{
  return tc_x509_crl_scope_evaluate(cache,reference,delta_policy,order_policy,query,at,tree,oids,
      oid_capacity,evidence,NULL);
}

#endif
