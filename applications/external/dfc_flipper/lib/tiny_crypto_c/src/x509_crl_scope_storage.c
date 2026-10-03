/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Storage preflight and argument checks for CRL scope operations. */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_revocation_internal.h"
#include "pki_storage_internal.h"
#include "pki_spans_internal.h"
#include "pki_status_internal.h"
#include "pki_identifier_internal.h"
#include "pki_extensions_internal.h"

TC_TLV_result tc_x509_crl_scope_arguments(const tc_x509_crl_scope_processing* processing,
                                          const tc_x509_crl_trust* trust, int all_scopes,
                                          TC_X509_search_report* out)
{
  TC_bytes storage;
  TC_X509_revocation_status status;
  if (!processing ||
      !tc_x509_crl_index_arguments(processing->index, processing->query, trust, out) ||
      (!all_scopes && processing->reference >= processing->index->count) ||
      (processing->check && !processing->check->verify) ||
      !x509_crl_delta_policy_valid(processing->delta_policy) ||
      !x509_crl_order_policy_valid(processing->order_policy) ||
      tc_pki_storage_span(processing->states, processing->capacity, sizeof *processing->states,
                          &storage) != TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  if (processing->capacity < processing->index->count)
    return TC_TLV_LIMIT;
  if (!all_scopes && processing->index->records[processing->reference].policy != TC_TLV_OK)
    return processing->index->records[processing->reference].policy;
  TC_TLV_result result = tc_x509_crl_evidence_status(processing->evidence, &status);
  if (result != TC_TLV_OK)
    return result;
  return status == TC_X509_REVOCATION_UNDETERMINED ? TC_TLV_OK : TC_TLV_END;
}

void tc_x509_crl_scope_plan_outputs(tc_pki_storage_plan* plan,
                                    const tc_x509_crl_extra_storage* extra,
                                    const tc_x509_crl_held_path* path)
{
  tc_pki_storage_plan_write_span(plan, extra ? extra->nodes_storage : (TC_bytes){NULL, 0});
  tc_pki_storage_plan_write_span(plan, extra ? extra->output_storage : (TC_bytes){NULL, 0});
  TC_PKI_PLAN_WRITE(plan, path ? path->out : NULL, path ? 1 : 0);
}

void tc_x509_crl_extra_plan_inputs(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_extra_storage* extra)
{
  if (extra->count && !extra->nodes) {
    tc_pki_storage_plan_fail(plan, TC_TLV_ARGUMENT);
    return;
  }
  tc_pki_storage_plan_input_spans(plan, extra->inputs, CRL_EXTRA_INPUT_COUNT);
  for (size_t i = 0; plan->status == TC_TLV_OK && i < extra->count; ++i)
    tc_pki_storage_plan_input_span(plan, extra->nodes[i].certificate);
}

void tc_x509_crl_path_plan_inputs(tc_pki_storage_plan* plan, const tc_x509_crl_held_path* path)
{
  TC_PKI_PLAN_INPUT(plan, path->chain, path->count);
  if (plan->status != TC_TLV_OK)
    return;
  for (size_t i = 0; i < CRL_PATH_METADATA_COUNT; ++i)
    if (path->metadata[i].length)
      tc_pki_storage_plan_input_span(plan, path->metadata[i]);
  for (size_t i = 0; plan->status == TC_TLV_OK && i < path->count; ++i) {
    if (!path->chain[i].data || !path->chain[i].length) {
      tc_pki_storage_plan_fail(plan, TC_TLV_ARGUMENT);
      return;
    }
    tc_pki_storage_plan_input_span(plan, path->chain[i]);
  }
  if (path->ocsp_count) {
    TC_PKI_PLAN_INPUT(plan, path->ocsp, path->ocsp_count);
    tc_pki_storage_plan_input_spans(plan, path->ocsp, path->ocsp_count);
  }
}

TC_TLV_result tc_x509_crl_points_init(tc_x509_crl_certificate_fields* fields,
                                      const TC_X509_certificate* certificate, TC_bytes* oids,
                                      size_t oid_capacity, TC_TLV_reader* reader)
{
  if (!fields || !fields->limits || !fields->tree || !fields->tree->work || !reader ||
      (oid_capacity && !oids))
    return TC_TLV_ARGUMENT;
  tc_x509_crl_certificate_fields pending = *fields;
  TC_TLV_reader next = {0};
  TC_TLV_result result;
  if (certificate) {
    pending = (tc_x509_crl_certificate_fields){0};
    pending.limits = fields->limits;
    pending.tree = fields->tree;
    result = tc_pki_extensions_visit(certificate->extensions, pending.limits, pending.tree, oids,
                                     oid_capacity, tc_x509_crl_certificate_extension, &pending);
    if (result != TC_TLV_OK)
      return result;
  }
  if (pending.points.length) {
    result = tc_pki_distribution_points_init(pending.points, pending.limits, pending.tree, &next);
    if (result != TC_TLV_OK)
      return result;
    /* Reject malformed later points before revocation can stop the search. */
    TC_TLV_reader check = next;
    tc_pki_distribution_point point;
    while ((result = tc_pki_distribution_point_next(&check, pending.tree, &point)) == TC_TLV_OK) {
    }
    if (result != TC_TLV_END)
      return result;
  }
  *fields = pending;
  *reader = next;
  return TC_TLV_OK;
}

void tc_x509_crl_scope_plan_inputs(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_scope_processing* processing,
                                   const tc_x509_crl_trust* trust)
{
  if (!processing || !processing->index || !processing->query || !processing->query->certificate ||
      !processing->query->point || !tc_x509_crl_trust_valid(trust)) {
    tc_pki_storage_plan_fail(plan, TC_TLV_ARGUMENT);
    return;
  }
  const TC_X509_certificate* certificate = processing->query->certificate;
  const tc_pki_distribution_point* point = processing->query->point;
  const TC_bytes fields[] = {certificate->encoded, certificate->extensions, certificate->issuer,
                             certificate->serial,  point->name.encoded,     point->name.contents,
                             point->issuer};
  TC_PKI_PLAN_INPUT(plan, processing->index, 1);
  TC_PKI_PLAN_INPUT(plan, processing->index->records, processing->index->count);
  TC_PKI_PLAN_INPUT(plan, processing->query, 1);
  TC_PKI_PLAN_INPUT(plan, certificate, 1);
  TC_PKI_PLAN_INPUT(plan, point, 1);
  if (processing->check)
    TC_PKI_PLAN_INPUT(plan, processing->check, 1);
  TC_PKI_PLAN_INPUT(plan, trust->source, 1);
  TC_PKI_PLAN_INPUT(plan, trust->options, 1);
  TC_PKI_PLAN_INPUT(plan, trust->time, 1);
  TC_PKI_PLAN_INPUT(plan, trust->validation, 1);
  TC_PKI_PLAN_INPUT(plan, trust->search, 1);
  TC_PKI_PLAN_INPUT(plan, trust->tree, 1);
  tc_pki_storage_plan_input_spans(plan, fields, sizeof fields / sizeof *fields);
  tc_x509_path_options_plan_inputs(plan, trust->options);
  tc_x509_crl_index_plan_inputs(plan, processing->index);
}

void tc_x509_crl_index_plan_inputs(tc_pki_storage_plan* plan, const TC_X509_crl_index* index)
{
  if (!index || (index->count && !index->records)) {
    tc_pki_storage_plan_fail(plan, TC_TLV_ARGUMENT);
    return;
  }
  for (size_t i = 0; plan->status == TC_TLV_OK && i < index->count; ++i) {
    const TC_X509_crl_record* record = &index->records[i];
    tc_pki_storage_plan_input_span(plan, record->crl.encoded);
    tc_pki_storage_plan_input_span(plan, record->crl.tbs);
    tc_pki_storage_plan_input_span(plan, record->crl.issuer);
    tc_pki_storage_plan_input_span(plan, record->crl.signature);
    tc_pki_storage_plan_input_span(plan, record->crl.revoked);
    tc_pki_storage_plan_input_span(plan, record->crl.extensions);
    tc_pki_storage_plan_input_span(plan, record->crl.signature_algorithm.oid);
    tc_pki_storage_plan_input_span(plan, record->crl.signature_algorithm.parameters);
    if (record->crl.prepared) {
      const TC_X509_crl_prepared* prepared = record->crl.prepared;
      TC_PKI_PLAN_INPUT(plan, prepared, 1);
      TC_PKI_PLAN_INPUT(plan, prepared->targets, prepared->count);
      TC_PKI_PLAN_INPUT(plan, prepared->matches, prepared->count);
      tc_pki_storage_plan_input_span(plan, prepared->digest);
      for (size_t target = 0; target < prepared->count; ++target) {
        tc_pki_storage_plan_input_span(plan, prepared->targets[target].serial);
        tc_pki_storage_plan_input_span(plan, prepared->targets[target].issuer);
      }
    }
    tc_pki_storage_plan_input_span(plan, record->extensions.number);
    tc_pki_storage_plan_input_span(plan, record->extensions.base_number);
    tc_pki_storage_plan_input_span(plan, record->extensions.distribution_encoded);
    tc_pki_storage_plan_input_span(plan, record->extensions.freshest);
    tc_pki_storage_plan_input_span(plan, record->extensions.issuer_alt);
    tc_pki_storage_plan_input_span(plan, record->extensions.unknown_critical_oid);
    tc_pki_storage_plan_input_span(plan, record->extensions.authority.key_identifier);
    tc_pki_storage_plan_input_span(plan, record->extensions.authority.issuer);
    tc_pki_storage_plan_input_span(plan, record->extensions.authority.serial);
    tc_pki_storage_plan_input_span(plan, record->extensions.distribution.name.encoded);
    tc_pki_storage_plan_input_span(plan, record->extensions.distribution.name.contents);
  }
}

void tc_x509_crl_scope_plan_writes(tc_pki_storage_plan* plan,
                                   const tc_x509_crl_scope_processing* processing,
                                   const tc_x509_crl_trust* trust, TC_X509_search_report* out)
{
  if (!processing || !tc_x509_crl_trust_valid(trust) || !out) {
    tc_pki_storage_plan_fail(plan, TC_TLV_ARGUMENT);
    return;
  }
  const tc_x509_crl_signer_cache* signer_cache = processing->signer_cache;
  tc_x509_path_storage_plan(plan, trust->validation);
  TC_PKI_PLAN_WRITE(plan, trust->search->path, trust->search->capacity);
  TC_PKI_PLAN_WRITE(plan, trust->search->frames, trust->search->capacity);
  TC_PKI_PLAN_WRITE(plan, trust->tree->frames, trust->tree->capacity);
  /* Tree traversal and path validation can reuse the same frame array. */
  if (plan->status == TC_TLV_OK && trust->tree->frames == trust->validation->frames.data) {
    if (plan->writes[CRL_SCOPE_TREE].length > plan->writes[TC_X509_PATH_STORAGE_FRAMES].length)
      plan->writes[TC_X509_PATH_STORAGE_FRAMES] = plan->writes[CRL_SCOPE_TREE];
    plan->writes[CRL_SCOPE_TREE] = (TC_bytes){NULL, 0};
  }
  TC_PKI_PLAN_WRITE(plan, processing->states, processing->capacity);
  TC_PKI_PLAN_WRITE(plan, processing->evidence, 1);
  TC_PKI_PLAN_WRITE(plan, out, 1);
  TC_PKI_PLAN_WRITE(plan, trust->tree->work, 1);
  TC_PKI_PLAN_WRITE(plan, processing->scopes, processing->scopes ? processing->index->count : 0);
  TC_PKI_PLAN_WRITE(plan, signer_cache, signer_cache ? 1 : 0);
  TC_PKI_PLAN_WRITE(plan, signer_cache ? signer_cache->path : NULL,
                    signer_cache ? signer_cache->path_capacity : 0);
  TC_PKI_PLAN_WRITE(plan, signer_cache ? signer_cache->policies : NULL,
                    signer_cache ? signer_cache->policy_capacity : 0);
}
#endif
