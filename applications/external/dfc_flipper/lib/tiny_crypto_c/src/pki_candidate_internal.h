/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_CANDIDATE_INTERNAL_H_
#define TC_PKI_CANDIDATE_INTERNAL_H_
#include "x509_path_internal.h"
#include "pki_tree_internal.h"
#include "pki_source_internal.h"

typedef struct {
  const TC_X509_store_source* source;
  TC_TLV_limits limits;
  size_t index, remaining, bytes_left;
} tc_pki_store_candidates;

/* The source guards returned bytes against the operation's writable storage.
 * Keep its snapshot stable. Advance the cursor only after a successful parse;
 * callback state, parser scratch and work remain provisional on failure. */
static inline TC_TLV_result tc_pki_store_candidate_next(void* context,
    const tc_pki_tree_workspace* tree, TC_X509_workspace* parser, TC_X509_certificate* out)
{
  tc_pki_store_candidates* reader = context;
  if (!reader || !reader->source || !tree || !tree->work || !parser || !out ||
      reader->index > reader->source->candidate_count) return TC_TLV_ARGUMENT;
  if (reader->index == reader->source->candidate_count) return TC_TLV_END;
  if (!reader->remaining || tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
  TC_bytes encoded;
  TC_TLV_result result = tc_pki_source_candidate(reader->source,reader->index,tree->work,&encoded);
  if (result != TC_TLV_OK) return result;
  if (encoded.length > reader->bytes_left || tc_x509_path_charge(tree->work,encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_X509_read(encoded.data,encoded.length,&reader->limits,parser,out);
  if (result != TC_TLV_OK) return result;
  ++reader->index; --reader->remaining; reader->bytes_left -= encoded.length;
  return TC_TLV_OK;
}

typedef TC_TLV_result (*tc_pki_candidate_filter)(const void* context,
    const TC_X509_certificate* candidate, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, int* matched);
typedef TC_TLV_result (*tc_pki_candidate_attempt)(const void* context,
    const TC_X509_certificate* candidate, TC_X509_search_result* out);
typedef TC_TLV_result (*tc_pki_candidate_next)(void* context,
    const tc_pki_tree_workspace* tree, TC_X509_workspace* parser, TC_X509_certificate* out);

/* next supplies one parsed, borrowed certificate. Charge one unit per candidate
 * in addition to callback work. Failed paths may be retried; source errors stop
 * the search. Caller keeps input, cursor, scratch and output storage disjoint. */
static inline TC_TLV_result tc_pki_certificate_search(void* cursor, tc_pki_candidate_next next,
    tc_pki_candidate_filter filter, const void* filter_context,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    const TC_X509_path_workspace* validation, tc_pki_candidate_attempt attempt, const void* context,
    TC_X509_search_result* out, int* source_failed)
{
  if (!next || !filter || !limits || !tree || !tree->work || !validation || !attempt || !out)
    return TC_TLV_ARGUMENT;
  TC_X509_workspace parser = {validation->frames,validation->frame_capacity,
      validation->oids,validation->oid_capacity};
  TC_X509_path_status failure = TC_X509_PATH_INVALID;
  const size_t initial_work = *tree->work;
  for (;;) {
    TC_X509_certificate candidate;
    TC_X509_search_result found;
    size_t before = *tree->work;
    TC_TLV_result result = next(cursor,tree,&parser,&candidate);
    if (*tree->work > before) { *tree->work = 0; result = TC_TLV_ARGUMENT; }
    if (result == TC_TLV_END) return tc_x509_path_result_status(failure);
    if (result != TC_TLV_OK) { if (source_failed) *source_failed = 1; return result; }
    if (tc_x509_path_charge(tree->work,1) != TC_TLV_OK) return TC_TLV_LIMIT;
    int matched = 0;
    before = *tree->work;
    result = filter(filter_context,&candidate,limits,tree,&matched);
    if (*tree->work > before) { *tree->work = 0; return TC_TLV_ARGUMENT; }
    if (result == TC_TLV_END || (result == TC_TLV_OK && matched != 0 && matched != 1))
      return TC_TLV_ARGUMENT;
    if (result == TC_TLV_OK && !matched) continue;
    if (result == TC_TLV_OK) {
      before = *tree->work;
      result = attempt(context,&candidate,&found);
      if (*tree->work > before) { *tree->work = 0; return TC_TLV_ARGUMENT; }
    }
    if (source_failed && *source_failed) return result == TC_TLV_OK ? TC_TLV_ARGUMENT : result;
    /* An attempt may finish without selecting a certificate. */
    if (result == TC_TLV_END) return result;
    if (result == TC_TLV_OK) {
      found.validation.work_used = initial_work - *tree->work;
      *out = found;
      return TC_TLV_OK;
    }
    const TC_X509_path_status status = tc_x509_path_status(result);
    if (status == TC_X509_PATH_ERROR) return result;
    tc_x509_path_remember(status,&failure);
  }
}
#endif
