/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CRL scope processing: candidate search, grouping records that share a
 * scope, signer checks and per-scope attempts. */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_revocation_internal.h"
#include "pki_storage_internal.h"
#include "pki_spans_internal.h"
#include "pki_status_internal.h"
#include "pki_identifier_internal.h"
#include "pki_extensions_internal.h"

TC_TLV_result tc_x509_crl_scope_run(const tc_x509_crl_candidate_source* candidates,
                                    const tc_x509_crl_scope_processing* processing,
                                    const tc_x509_crl_trust* trust,
                                    const tc_x509_crl_scope_selection* selection,
                                    const TC_bytes writes[CRL_SCOPE_WRITES], int* source_failed,
                                    TC_X509_search_report* out)
{
  const TC_bytes* points = selection->points;
  const TC_X509_path_options* options = trust->options;
  const tc_pki_tree_workspace* tree = trust->tree;
  const TC_X509_path_workspace* validation = trust->validation;
  const TC_X509_store_source* path_source = trust->source;
  TC_TLV_reader point_reader;
  tc_x509_crl_certificate_fields fields = {0};
  fields.limits = &options->parsing;
  fields.tree = tree;
  fields.ca = processing->query->certificate_ca;
  if (points)
    fields.points = *points;
  TC_TLV_result result = tc_x509_crl_points_init(
      &fields, selection->from_certificate ? processing->query->certificate : NULL,
      validation->oids, validation->oid_capacity, &point_reader);
  if (result != TC_TLV_OK)
    return result;
  tc_pki_source_status_guard path_guard = {{path_source, writes, CRL_SCOPE_WRITES}, source_failed};
  tc_pki_source_status_guard candidate_guard = {{candidates->external, writes, CRL_SCOPE_WRITES},
                                                source_failed};
  const TC_X509_store_source guarded_path = {
      &path_guard, path_source->candidate_count, path_source->anchor_count,
      tc_pki_source_status_candidate, tc_pki_source_status_anchor};
  TC_X509_store_source guarded_candidates;
  tc_x509_crl_candidate_source source = *candidates;
  if (candidates->external) {
    guarded_candidates = (TC_X509_store_source){
        &candidate_guard, candidates->external->candidate_count, candidates->external->anchor_count,
        tc_pki_source_status_candidate, tc_pki_source_status_anchor};
    source.external = &guarded_candidates;
  }
  tc_x509_crl_trust guarded_trust = *trust;
  guarded_trust.source = &guarded_path;
  const tc_x509_crl_searcher searcher = {&source, tc_x509_crl_source_search};
  return tc_x509_crl_scopes(&searcher, processing, &guarded_trust, &fields, &point_reader,
                            selection->all_scopes, source_failed, out);
}

TC_TLV_result tc_x509_crl_scope_prepare(const tc_x509_crl_operation_source* candidates,
                                        const tc_x509_crl_scope_processing* processing,
                                        const tc_x509_crl_trust* trust, const TC_bytes* points,
                                        const tc_x509_crl_extra_storage* extra,
                                        const tc_x509_crl_held_path* path,
                                        TC_X509_search_report* out,
                                        TC_bytes writes[CRL_SCOPE_WRITES])
{
  tc_pki_storage_plan plan;
  if (!candidates || !candidates->candidates.search ||
      (candidates->metadata_count && !candidates->metadata))
    return TC_TLV_ARGUMENT;
  tc_pki_storage_plan_begin(&plan, writes, CRL_SCOPE_WRITES, *trust->tree->work);
  tc_x509_crl_scope_plan_writes(&plan, processing, trust, out);
  tc_x509_crl_scope_plan_outputs(&plan, extra, path);
  tc_pki_storage_plan_seal(&plan);
  if (points) {
    TC_PKI_PLAN_INPUT(&plan, points, 1);
    tc_pki_storage_plan_input_span(&plan, *points);
  }
  tc_pki_storage_plan_input_spans(&plan, candidates->metadata, candidates->metadata_count);
  if (extra)
    tc_x509_crl_extra_plan_inputs(&plan, extra);
  if (path)
    tc_x509_crl_path_plan_inputs(&plan, path);
  tc_x509_crl_scope_plan_inputs(&plan, processing, trust);
  return tc_pki_storage_plan_finish(&plan, trust->tree->work);
}

TC_TLV_result tc_x509_crl_scope_execute(const tc_x509_crl_operation_source* candidates,
                                        const tc_x509_crl_scope_processing* processing,
                                        const tc_x509_crl_trust* trust,
                                        const tc_x509_crl_scope_selection* selection,
                                        TC_X509_search_report* out)
{
  if (!selection)
    return TC_TLV_ARGUMENT;
  const int all_scopes = selection->all_scopes;
  TC_TLV_result result = tc_x509_crl_scope_arguments(processing, trust, all_scopes, out);
  if (result != TC_TLV_OK)
    return result;
  TC_bytes writes[CRL_SCOPE_WRITES];
  const size_t initial_work = *trust->tree->work;
  result = tc_x509_crl_scope_prepare(candidates, processing, trust, selection->points, NULL, NULL,
                                     out, writes);
  if (result != TC_TLV_OK)
    return result;
  int source_failed = 0;
  result = tc_x509_crl_scope_run(&candidates->candidates, processing, trust, selection, writes,
                                 &source_failed, out);
  /* The only work_used write: it covers the preflight and every attempt. */
  if (result == TC_TLV_OK && !all_scopes)
    out->validation.work_used = initial_work - *trust->tree->work;
  return result;
}

TC_TLV_result tc_x509_crl_source_search(const void* candidates,
                                        const tc_x509_crl_signer_query* query,
                                        const tc_x509_crl_trust* trust, TC_X509_search_report* out,
                                        int* source_failed)
{
  const tc_x509_crl_candidate_source* source = candidates;
  if (!source || !source->search)
    return TC_TLV_ARGUMENT;
  return source->search(source->context, source->external, query, trust, out, source_failed);
}

TC_TLV_result tc_x509_crl_store_source_search(const void* candidates,
                                              const TC_X509_store_source* external,
                                              const tc_x509_crl_signer_query* query,
                                              const tc_x509_crl_trust* trust,
                                              TC_X509_search_report* out, int* source_failed)
{
  if (!candidates)
    return TC_TLV_ARGUMENT;
  tc_pki_store_candidates cursor = *(const tc_pki_store_candidates*)candidates;
  cursor.source = external;
  return tc_x509_crl_search_candidates(&cursor, tc_pki_store_candidate_next, query, trust, out,
                                       source_failed);
}

TC_TLV_result tc_x509_crl_filter_match(const void* context, const TC_X509_certificate* candidate,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, int* matched)
{
  const tc_x509_crl_filter* filter = context;
  if (!filter)
    return TC_TLV_ARGUMENT;
  return tc_x509_crl_candidate_matches(filter->crl, filter->extensions, candidate, limits,
                                       filter->names, tree, matched);
}

typedef struct {
  const tc_x509_crl_trust* trust;
  const tc_x509_crl_signer_query* query;
} x509_crl_search_context;

static TC_TLV_result x509_crl_attempt_candidate(const void* context,
                                                const TC_X509_certificate* candidate,
                                                TC_X509_search_report* out)
{
  const x509_crl_search_context* search = context;
  return search->query->attempt(search->query->context, candidate, search->trust, out);
}

TC_TLV_result tc_x509_crl_search_candidates(void* cursor, tc_pki_candidate_next next,
                                            const tc_x509_crl_signer_query* query,
                                            const tc_x509_crl_trust* trust,
                                            TC_X509_search_report* out, int* source_failed)
{
  if (!query || !query->crl || !query->extensions || !query->attempt ||
      !tc_x509_crl_trust_valid(trust))
    return TC_TLV_ARGUMENT;
  const tc_x509_crl_filter filter = {query->crl, query->extensions, &trust->validation->names};
  const x509_crl_search_context search = {trust, query};
  const tc_pki_candidate_checks checks = {tc_x509_crl_filter_match, &filter,
                                          x509_crl_attempt_candidate, &search};
  return tc_pki_certificate_search(cursor, next, &checks, &trust->options->parsing, trust->tree,
                                   trust->validation, out, source_failed);
}

TC_TLV_result tc_x509_crl_scopes(const tc_x509_crl_searcher* searcher,
                                 const tc_x509_crl_scope_processing* input,
                                 const tc_x509_crl_trust* trust,
                                 const tc_x509_crl_certificate_fields* fields,
                                 TC_TLV_reader* point_reader, int all_scopes, int* source_failed,
                                 TC_X509_search_report* out)
{
  if (!searcher || !searcher->search || !input || !fields || !point_reader || !source_failed ||
      !tc_x509_crl_index_arguments(input->index, input->query, trust, out))
    return TC_TLV_ARGUMENT;
  const TC_X509_crl_index* index = input->index;
  const size_t reference = input->reference;
  if (reference > index->count || (!all_scopes && reference == index->count))
    return TC_TLV_ARGUMENT;
  TC_X509_revocation_status status;
  TC_X509_crl_evidence* evidence = input->evidence;
  TC_TLV_result result = tc_x509_crl_evidence_status(evidence, &status);
  if (result != TC_TLV_OK)
    return result;
  if (status != TC_X509_REVOCATION_UNDETERMINED)
    return TC_TLV_END;
  tc_x509_crl_scope_processing processing = *input;
  const tc_x509_crl_query* query = input->query;
  tc_x509_crl_query current_query = *query;
  current_query.certificate_ca = fields->ca;
  const TC_X509_path_options* options = trust->options;
  const tc_pki_tree_workspace* tree = trust->tree;
  const TC_X509_path_workspace* validation = trust->validation;
  tc_pki_distribution_point point;
  TC_X509_crl_evidence pending = *evidence;
  TC_X509_search_report found = {0};
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
      result = tc_pki_distribution_point_next(point_reader, tree, &point);
      if (result == TC_TLV_END)
        fallback = 1;
      else if (result != TC_TLV_OK)
        return result;
      else
        current_query.point = &point;
    }
    for (size_t i = reference; i < end; ++i) {
      const TC_X509_crl_record* record = &index->records[i];
      uint16_t reasons;
      processing.reference = i;
      result = tc_pki_work_charge(tree->work, 1);
      if (result != TC_TLV_OK)
        return result;
      result = record->policy;
      if (result == TC_TLV_OK) {
        const TC_X509_crl_distribution* distribution =
            record->extensions.present & TC_X509_CRL_EXT_DISTRIBUTION
                ? &record->extensions.distribution
                : NULL;
        /* Defer freshness checks until a delta has been selected. */
        result = tc_x509_crl_scope_reasons(
            &record->crl, distribution, current_query.point, query->certificate->issuer,
            current_query.certificate_ca,
            &(tc_x509_crl_decode){&options->parsing, tree, &validation->names, NULL, 0}, &reasons);
        if (result == TC_TLV_OK && !(reasons & ~pending.reasons))
          result = TC_TLV_END;
        if (result == TC_TLV_OK && all_scopes && processing.scopes &&
            processing.scopes[i].representative != i)
          result = TC_TLV_END;
        if (result == TC_TLV_OK && all_scopes) {
          if (!processing.scopes) {
            for (size_t previous = 0; previous < i; ++previous) {
              if (index->records[previous].policy != TC_TLV_OK)
                continue;
              int same;
              result = tc_x509_crl_same_scope(&processing, previous, trust, &same);
              if (result != TC_TLV_OK || same) {
                if (result == TC_TLV_OK)
                  result = TC_TLV_END;
                break;
              }
            }
          }
          if (result == TC_TLV_OK) {
            tc_x509_crl_proposal chosen;
            result = tc_x509_crl_group(searcher, &processing, trust, source_failed, &chosen);
            if (result == TC_TLV_OK)
              result = chosen.result;
            if (result == TC_TLV_OK)
              result = tc_x509_crl_evidence_add(&pending, chosen.evidence.reasons,
                                                &chosen.evidence.revocation);
          }
        } else if (result == TC_TLV_OK) {
          const tc_x509_crl_signer_query signer_query = {&record->crl, &record->extensions,
                                                         tc_x509_crl_scope_attempt, &processing};
          result =
              searcher->search(searcher->candidates, &signer_query, trust, &found, source_failed);
        }
      }
      if (result == TC_TLV_OK) {
        contributed = 1;
        result = tc_x509_crl_evidence_status(&pending, &status);
        if (result != TC_TLV_OK)
          return result;
        if (status != TC_X509_REVOCATION_UNDETERMINED)
          break;
      } else {
        if (!all_scopes || *source_failed || result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT)
          return result;
        if (result == TC_TLV_UNSUPPORTED || (result != TC_TLV_END && failure == TC_TLV_END))
          failure = result;
      }
    }
    if (status != TC_X509_REVOCATION_UNDETERMINED)
      break;
    if (fallback) {
      if (alternative || !fields->alternative.name.encoded.length)
        break;
      alternative = 1;
    }
  }
  if (status == TC_X509_REVOCATION_UNDETERMINED && failure != TC_TLV_END)
    return failure;
  if (!contributed)
    return TC_TLV_END;
  *evidence = pending;
  if (!all_scopes)
    *out = found;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_certificate_extension(void* context, const TC_X509_extension* extension)
{
  tc_x509_crl_certificate_fields* fields = context;
  TC_TLV_result result;
  switch (tc_pki_extension_id(extension)) {
  case TC_PKI_EXT_BASIC_CONSTRAINTS: {
    TC_X509_basic_constraints basic;
    result = TC_X509_basic_constraints_read(extension->value, fields->limits, &basic);
    if (result == TC_TLV_OK)
      fields->ca = basic.ca;
    return result;
  }
  case TC_PKI_EXT_CRL_DISTRIBUTION_POINTS:
    fields->points = extension->value;
    return TC_TLV_OK;
  case TC_PKI_EXT_ISSUER_ALT_NAME: {
    TC_bytes contents;
    result = TC_DER_sequence(extension->value, &contents);
    if (result != TC_TLV_OK)
      return result;
    result = tc_pki_general_names_contents_check(contents, fields->limits, fields->tree);
    if (result != TC_TLV_OK)
      return result;
    /* fullName and issuerAltName carry the same GeneralNames contents. */
    fields->alternative.name = (TC_X509_distribution_name){extension->value, contents, 0};
    return TC_TLV_OK;
  }
  default:
    return TC_TLV_OK;
  }
}

TC_TLV_result tc_x509_crl_scopes_index(const TC_X509_crl_index* index, const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree,
                                       const TC_X509_name_workspace* names,
                                       TC_X509_revocation_scope* slots, size_t capacity)
{
  if (!index || (index->count && !index->records) || !limits || !tree || !tree->work || !names ||
      (capacity && !slots))
    return TC_TLV_ARGUMENT;
  if (capacity < index->count)
    return TC_TLV_LIMIT;
  if (tc_pki_work_charge(tree->work, index->count) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  for (size_t i = 0; i < index->count; ++i)
    slots[i].head = SIZE_MAX;
  for (size_t i = 0; i < index->count; ++i) {
    const TC_X509_crl_record* record = &index->records[i];
    slots[i].representative = SIZE_MAX;
    slots[i].next = SIZE_MAX;
    if (record->policy != TC_TLV_OK)
      continue;
    const TC_bytes distribution = record->extensions.distribution_encoded;
    if (distribution.length && !distribution.data)
      return TC_TLV_ARGUMENT;
    if (tc_pki_work_charge(tree->work, distribution.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    uint32_t hash = tc_x509_crl_bytes_hash(distribution);
    hash =
        (hash ^ !!(record->extensions.present & TC_X509_CRL_EXT_DISTRIBUTION)) * UINT32_C(16777619);
    const size_t bucket = (size_t)hash % index->count;
    for (size_t cursor = slots[bucket].head; cursor != SIZE_MAX; cursor = slots[cursor].next) {
      const TC_X509_crl_record* previous = &index->records[cursor];
      int same;
      if (tc_pki_work_charge(tree->work, 1) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      TC_TLV_result result =
          tc_x509_crl_scope_equal(&record->crl, &record->extensions, &previous->crl,
                                  &previous->extensions, limits, tree, names, &same);
      if (result != TC_TLV_OK)
        return result;
      if (same) {
        slots[i].representative = cursor;
        break;
      }
    }
    if (slots[i].representative == SIZE_MAX) {
      slots[i].representative = i;
      slots[i].next = slots[bucket].head;
      slots[bucket].head = i;
    }
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_same_scope(const tc_x509_crl_scope_processing* processing, size_t other,
                                     const tc_x509_crl_trust* trust, int* same)
{
  const TC_X509_crl_record* a = &processing->index->records[processing->reference];
  const TC_X509_crl_record* b = &processing->index->records[other];
  return tc_x509_crl_scope_equal(&a->crl, &a->extensions, &b->crl, &b->extensions,
                                 &trust->options->parsing, trust->tree, &trust->validation->names,
                                 same);
}

/* Authenticate proposals from every reference key before choosing this scope. */
TC_TLV_result tc_x509_crl_group(const tc_x509_crl_searcher* searcher,
                                const tc_x509_crl_scope_processing* processing,
                                const tc_x509_crl_trust* trust, int* source_failed,
                                tc_x509_crl_proposal* out)
{
  tc_x509_crl_proposal chosen = {0};
  TC_TLV_result failure = TC_TLV_END;
  int unranked = 0;
  for (size_t i = 0; i < processing->index->count; ++i) {
    TC_TLV_result result = tc_pki_work_charge(trust->tree->work, 1);
    if (result != TC_TLV_OK)
      return result;
    const TC_X509_crl_record* record = &processing->index->records[i];
    if (record->policy != TC_TLV_OK)
      continue;
    int same;
    if (processing->scopes)
      same = processing->scopes[i].representative ==
             processing->scopes[processing->reference].representative;
    else {
      result = tc_x509_crl_same_scope(processing, i, trust, &same);
      if (result != TC_TLV_OK)
        return result;
    }
    if (!same)
      continue;
    TC_X509_crl_evidence empty = {0};
    tc_x509_crl_proposal candidate = {0}, unresolved = {0};
    TC_X509_search_report path;
    tc_x509_crl_scope_processing attempt = *processing;
    attempt.reference = i;
    attempt.evidence = &empty;
    attempt.proposal = &candidate;
    attempt.unresolved = &unresolved;
    const tc_x509_crl_signer_query query = {&record->crl, &record->extensions,
                                            tc_x509_crl_scope_attempt, &attempt};
    result = searcher->search(searcher->candidates, &query, trust, &path, source_failed);
    if (*source_failed || result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT)
      return result;
    if (unresolved.selected.base && result != TC_TLV_OK) {
      TC_TLV_result merged =
          tc_x509_crl_proposal_merge(&chosen, &unresolved, processing->order_policy, trust->tree);
      if (merged != TC_TLV_OK)
        return merged;
    }
    if (result != TC_TLV_OK) {
      if (result == TC_TLV_UNSUPPORTED && !unresolved.selected.base)
        unranked = 1;
      if (result != TC_TLV_END && failure == TC_TLV_END)
        failure = result;
      continue;
    }
    if (unresolved.selected.base) {
      result = tc_x509_crl_proposal_merge(&candidate, &unresolved, processing->order_policy,
                                          trust->tree);
      if (result != TC_TLV_OK)
        return result;
    }
    result = tc_x509_crl_proposal_merge(&chosen, &candidate, processing->order_policy, trust->tree);
    if (result != TC_TLV_OK)
      return result;
  }
  /* Resolving an unranked candidate is required before choosing a record. */
  if (unranked)
    return TC_TLV_UNSUPPORTED;
  if (!chosen.selected.base)
    return failure;
  *out = chosen;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_scope_attempt(const void* context, const TC_X509_certificate* signer,
                                        const tc_x509_crl_trust* trust, TC_X509_search_report* out)
{
  const tc_x509_crl_scope_processing* processing = context;
  tc_x509_crl_signature_cache cache;
  TC_X509_search_report found = {0};
  tc_x509_crl_signer_cache* saved = processing->signer_cache;
  int reuse = 0;
  if (saved && saved->valid && saved->signer.length == signer->encoded.length) {
    if (tc_pki_work_charge(trust->tree->work, signer->encoded.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    reuse = tc_pki_equal(saved->signer, signer->encoded);
  }
  TC_TLV_result result;
  if (reuse) {
    cache = (tc_x509_crl_signature_cache){processing->index,           signer,
                                          &trust->options->signatures, &trust->options->parsing,
                                          &trust->validation->names,   processing->states,
                                          processing->capacity,        processing->scopes};
    result = tc_x509_crl_signature_cached(&cache, processing->reference, trust->tree->work);
    if (result != TC_TLV_OK)
      return result;
    found = saved->result;
    found.validation.work_used = 0;
  } else {
    if (saved)
      saved->valid = 0;
    result = tc_x509_crl_signature_cache_init(
        processing->index, signer, &trust->options->signatures, &trust->options->parsing,
        &trust->validation->names, (TC_buffer){processing->states, processing->capacity},
        trust->tree->work, &cache);
    if (result != TC_TLV_OK)
      return result;
    result = tc_x509_path_result_status(tc_x509_crl_signer_validate(
        &processing->index->records[processing->reference].crl, signer, trust, &found));
    if (result != TC_TLV_OK)
      return result;
    /* Signer validation already checked this signature with the cache's provider. */
    cache.states[processing->reference] = CRL_SIGNATURE_VALID;
    if (saved) {
      if (found.count > saved->path_capacity ||
          found.validation.policy_count > saved->policy_capacity)
        return TC_TLV_LIMIT;
      if (tc_pki_work_charge(trust->tree->work, found.count) != TC_TLV_OK ||
          tc_pki_work_charge(trust->tree->work, found.validation.policy_count) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (found.count)
        memcpy(saved->path, found.path, found.count * sizeof *saved->path);
      if (found.validation.policy_count)
        memcpy(saved->policies, found.validation.policies,
               found.validation.policy_count * sizeof *saved->policies);
      found.path = saved->path;
      found.validation.policies = saved->policies;
      saved->result = found;
      saved->signer = signer->encoded;
      saved->valid = 1;
    }
  }
  cache.scopes = processing->scopes;
  TC_X509_crl_evidence pending = *processing->evidence;
  tc_x509_crl_selected selected = {0};
  const tc_x509_crl_scope_context scope = {
      &cache,      processing->delta_policy, processing->order_policy,       trust->time,
      trust->tree, trust->validation->oids,  trust->validation->oid_capacity};
  result = tc_x509_crl_scope_evaluate(&scope, processing->reference, processing->query, &pending,
                                      processing->proposal || processing->check ? &selected : NULL);
  if (result == TC_TLV_ARGUMENT || result == TC_TLV_LIMIT)
    return result;
  if (processing->check) {
    const size_t before = *trust->tree->work;
    TC_X509_path_status checked = processing->check->verify(
        processing->check->context, &found, selected.base ? &selected : NULL, trust->tree->work);
    if (*trust->tree->work > before) {
      *trust->tree->work = 0;
      return TC_TLV_ARGUMENT;
    }
    TC_TLV_result check_result = tc_x509_path_result_status(checked);
    if (check_result == TC_TLV_UNSUPPORTED && processing->unresolved && selected.base &&
        result != TC_TLV_END) {
      const tc_x509_crl_proposal pending_check = {selected, pending, TC_TLV_UNSUPPORTED,
                                                  result == TC_TLV_OK};
      TC_TLV_result merged = tc_x509_crl_proposal_merge(processing->unresolved, &pending_check,
                                                        processing->order_policy, trust->tree);
      if (merged != TC_TLV_OK)
        return merged;
    }
    if (check_result != TC_TLV_OK)
      return check_result;
  }
  if (processing->proposal && selected.base &&
      (result == TC_TLV_OK || result == TC_TLV_INVALID || result == TC_TLV_UNSUPPORTED)) {
    *processing->proposal = (tc_x509_crl_proposal){selected, pending, result, 0};
    *out = found;
    return TC_TLV_OK;
  }
  if (result != TC_TLV_OK)
    return result;
  *processing->evidence = pending;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_proposal_merge(tc_x509_crl_proposal* chosen,
                                         const tc_x509_crl_proposal* candidate,
                                         TC_X509_crl_order_policy policy,
                                         const tc_pki_tree_workspace* tree)
{
  int order, equal;
  if (!candidate->selected.base)
    return TC_TLV_ARGUMENT;
  if (!chosen->selected.base) {
    *chosen = *candidate;
    return TC_TLV_OK;
  }
  TC_TLV_result result =
      x509_crl_order(&candidate->selected, &chosen->selected, policy, tree->work, &order);
  if (result != TC_TLV_OK || order < 0)
    return result;
  if (order > 0) {
    *chosen = *candidate;
    return TC_TLV_OK;
  }
  /* Another valid signer certificate can establish the same selected records. */
  if (candidate->selected.base == chosen->selected.base &&
      candidate->selected.delta == chosen->selected.delta) {
    if (candidate->result == TC_TLV_OK && chosen->pending_only) {
      *chosen = *candidate;
      return TC_TLV_OK;
    }
    if (chosen->result == TC_TLV_OK && candidate->pending_only)
      return TC_TLV_OK;
  }
  if (candidate->result != TC_TLV_OK || chosen->result != TC_TLV_OK) {
    chosen->pending_only = chosen->pending_only && candidate->pending_only &&
                           candidate->selected.base == chosen->selected.base &&
                           candidate->selected.delta == chosen->selected.delta;
    chosen->result = candidate->result == TC_TLV_UNSUPPORTED || chosen->result == TC_TLV_UNSUPPORTED
                         ? TC_TLV_UNSUPPORTED
                         : TC_TLV_INVALID;
    return TC_TLV_OK;
  }
  const TC_X509_crl* a =
      candidate->selected.delta ? candidate->selected.delta : candidate->selected.base;
  const TC_X509_crl* b = chosen->selected.delta ? chosen->selected.delta : chosen->selected.base;
  result = tc_pki_work_charge(tree->work, 1);
  if (result != TC_TLV_OK)
    return result;
  result = TC_X509_time_compare(&a->this_update, &b->this_update, &order);
  if (result != TC_TLV_OK)
    return result;
  result = tc_x509_crl_evidence_equal(&candidate->evidence, &chosen->evidence, &equal);
  if (result != TC_TLV_OK)
    return result;
  if (order || !equal)
    chosen->result = TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_candidate_matches(const TC_X509_crl* crl,
                                            const TC_X509_crl_extensions* extensions,
                                            const TC_X509_certificate* candidate,
                                            const TC_TLV_limits* limits,
                                            const TC_X509_name_workspace* names,
                                            const tc_pki_tree_workspace* tree, int* matched)
{
  if (!crl || !extensions || !candidate || !limits || !names || !tree || !tree->work || !matched)
    return TC_TLV_ARGUMENT;
  int accepted;
  TC_TLV_result result =
      TC_X509_name_equal(crl->issuer, candidate->subject, limits, names, tree->work, &accepted);
  if (result != TC_TLV_OK)
    return result;
  if (accepted && (extensions->present & TC_X509_CRL_EXT_AUTHORITY)) {
    result =
        tc_pki_authority_matches(&extensions->authority, candidate, limits, tree, names, &accepted);
    if (result != TC_TLV_OK)
      return result;
  }
  if (accepted) {
    result = tc_x509_crl_signer_usage(candidate, limits, tree->work, &accepted);
    if (result != TC_TLV_OK)
      return result;
  }
  *matched = accepted;
  return TC_TLV_OK;
}
#endif
