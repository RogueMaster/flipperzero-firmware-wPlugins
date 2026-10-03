/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CMS certificate collections: the shared collection reader and candidate
 * search over embedded and external sources. Embedded revocation
 * information is ignored by CMS validation. */
#include <tiny_crypto/cms_validation.h>
#include <tiny_crypto/piv_oid.h>
#if TC_ENABLE_CMS_VALIDATION
#include "cms_internal.h"

TC_TLV_result tc_cms_other_format_read(TC_bytes encoded, tc_cms_other_kind kind,
                                       const TC_TLV_limits* limits,
                                       const tc_pki_tree_workspace* tree, tc_cms_other_format* out)
{
  tc_pki_oid_value parsed;
  TC_TLV_result result;
  unsigned tag;
  if (!out)
    return TC_TLV_ARGUMENT;
  if (kind == TC_CMS_OTHER_CERTIFICATE)
    tag = 0xa3;
  else if (kind == TC_CMS_OTHER_REVOCATION)
    tag = 0xa1;
  else
    return TC_TLV_ARGUMENT;
  result = tc_pki_tree_oid_value(encoded, tag, TC_TLV_BER, limits, tree, &parsed);
  if (result != TC_TLV_OK)
    return result;
  if (!parsed.value.data)
    return TC_TLV_INVALID;
  *out = (tc_cms_other_format){parsed.oid, parsed.value};
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_collection_init(TC_bytes embedded, unsigned tag, size_t max_records,
                                     size_t max_bytes, const TC_TLV_limits* limits,
                                     const tc_pki_tree_workspace* tree, tc_cms_collection* out)
{
  tc_cms_collection parsed = {0};
  TC_TLV_result result;
  if (!out || !tree || !tree->work || (!embedded.data && embedded.length))
    return TC_TLV_ARGUMENT;
  if (embedded.length > max_bytes)
    return TC_TLV_LIMIT;
  if (embedded.length)
    result = tc_pki_tree_open(embedded, tag, TC_TLV_BER, limits, tree, &parsed.embedded);
  else
    result = TC_TLV_reader_init(&parsed.embedded, (TC_bytes){NULL, 0}, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  parsed.remaining = max_records;
  parsed.bytes_left = max_bytes - embedded.length;
  *out = parsed;
  return TC_TLV_OK;
}

/* One bounded traversal for certificates and revocation objects. Typed wrappers
 * classify embedded choices before committing the reader position. */
TC_TLV_result tc_cms_collection_next(tc_cms_collection* reader,
                                     const tc_pki_record_source* external,
                                     const tc_pki_tree_workspace* tree, TC_TLV_element* out,
                                     int* embedded)
{
  tc_cms_collection next;
  TC_TLV_element element = {0};
  TC_TLV_result result;
  if (!reader || !tree || !tree->work || !out)
    return TC_TLV_ARGUMENT;
  next = *reader;
  if (tc_pki_end(&next.embedded) && (!external || next.external_index == external->count))
    return TC_TLV_END;
  if (!next.remaining || tc_pki_work_charge(tree->work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (!tc_pki_end(&next.embedded)) {
    result = tc_pki_tree_next(&next.embedded, tree, &element);
    if (result != TC_TLV_OK)
      return result;
    *embedded = 1;
  } else {
    result = tc_pki_record_read(external, next.external_index, tree->work, &element.encoded);
    if (result != TC_TLV_OK)
      return result;
    if (element.encoded.length > next.bytes_left)
      return TC_TLV_LIMIT;
    next.bytes_left -= element.encoded.length;
    ++next.external_index;
    *embedded = 0;
  }
  --next.remaining;
  *reader = next;
  *out = element;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_candidates_init(TC_bytes embedded, const TC_X509_store_source* external,
                                     size_t max_candidates, size_t max_bytes,
                                     const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
                                     tc_cms_candidates* out)
{
  tc_cms_candidates parsed;
  TC_TLV_result result;
  if (!out || (external && external->candidate_count && !external->candidate))
    return TC_TLV_ARGUMENT;
  result = tc_cms_collection_init(embedded, 0xa0, max_candidates, max_bytes, limits, tree,
                                  &parsed.collection);
  if (result != TC_TLV_OK)
    return result;
  parsed.external = external;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_candidates_next(tc_cms_candidates* reader, const tc_pki_tree_workspace* tree,
                                     tc_cms_certificate_choice* out)
{
  tc_cms_candidates next;
  tc_pki_record_source external = {0};
  tc_cms_certificate_choice choice = {{NULL, 0}, TC_CMS_CERT_X509};
  TC_TLV_element element;
  TC_TLV_result result;
  int embedded;
  if (!reader || !out)
    return TC_TLV_ARGUMENT;
  next = *reader;
  if (next.external)
    external = (tc_pki_record_source){next.external->context, next.external->candidate_count,
                                      next.external->candidate};
  result = tc_cms_collection_next(&next.collection, &external, tree, &element, &embedded);
  if (result != TC_TLV_OK)
    return result;
  if (embedded) {
    result = tc_cms_classify_certificate(&element, &choice.kind);
    if (result != TC_TLV_OK)
      return result;
    if (choice.kind == TC_CMS_CERT_OTHER) {
      tc_cms_other_format other;
      result = tc_cms_other_format_read(element.encoded, TC_CMS_OTHER_CERTIFICATE,
                                        &next.collection.embedded.limits, tree, &other);
      if (result != TC_TLV_OK)
        return result;
    }
  }
  choice.encoded = element.encoded;
  *reader = next;
  *out = choice;
  return TC_TLV_OK;
}

static TC_TLV_result cms_indexed_candidate(void* context, size_t index, size_t* work, TC_bytes* out)
{
  const tc_cms_path_source* source = context;
  if (!source || index >= source->count || !work || !out)
    return TC_TLV_ARGUMENT;
  if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  *out = source->certificates[index];
  return TC_TLV_OK;
}

static TC_TLV_result cms_indexed_anchor(void* context, size_t index, size_t* work,
                                        TC_X509_store_anchor* out)
{
  const tc_cms_path_source* source = context;
  if (!source || !source->external || !source->external->anchor ||
      index >= source->external->anchor_count || !work || !out)
    return TC_TLV_ARGUMENT;
  return source->external->anchor(source->external->context, index, work, out);
}

TC_TLV_result tc_cms_path_source_init(const tc_cms_candidates* candidates,
                                      const tc_pki_tree_workspace* tree, TC_bytes* index,
                                      size_t capacity, tc_cms_path_source* context,
                                      TC_X509_store_source* out)
{
  tc_cms_candidates reader;
  tc_cms_certificate_choice choice;
  TC_TLV_result result;
  size_t count = 0;
  if (!candidates || !tree || !tree->work || (!index && capacity) || !context || !out ||
      (candidates->external && candidates->external->anchor_count && !candidates->external->anchor))
    return TC_TLV_ARGUMENT;
  reader = *candidates;
  while ((result = tc_cms_candidates_next(&reader, tree, &choice)) == TC_TLV_OK) {
    if (choice.kind != TC_CMS_CERT_X509)
      continue;
    if (count == capacity)
      return TC_TLV_LIMIT;
    index[count++] = choice.encoded;
  }
  if (result != TC_TLV_END)
    return result;
  *context = (tc_cms_path_source){index, count, candidates->external};
  *out = (TC_X509_store_source){context, count,
                                candidates->external ? candidates->external->anchor_count : 0,
                                cms_indexed_candidate, cms_indexed_anchor};
  return TC_TLV_OK;
}

TC_TLV_result tc_cms_x509_candidate_next(tc_cms_candidates* reader, tc_pki_candidate_filter filter,
                                         const void* context, const tc_pki_tree_workspace* tree,
                                         const TC_X509_workspace* parser,
                                         TC_X509_certificate* scratch, TC_bytes* out)
{
  tc_cms_candidates next;
  tc_cms_certificate_choice choice;
  TC_TLV_result result;
  if (!reader || !tree || !tree->work || !parser || !scratch || !out)
    return TC_TLV_ARGUMENT;
  next = *reader;
  for (;;) {
    int matched = -1;
    result = tc_cms_candidates_next(&next, tree, &choice);
    if (result == TC_TLV_END) {
      *reader = next;
      return result;
    }
    if (result != TC_TLV_OK)
      return result;
    if (choice.kind != TC_CMS_CERT_X509)
      continue;
    if (tc_pki_work_charge(tree->work, choice.encoded.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = TC_X509_read(choice.encoded, &next.collection.embedded.limits, parser, scratch);
    if (result != TC_TLV_OK)
      return result;
    if (filter) {
      const size_t before = *tree->work;
      result = filter(context, scratch, &next.collection.embedded.limits, tree, &matched);
      if (*tree->work > before) {
        *tree->work = 0;
        return TC_TLV_ARGUMENT;
      }
      if (result == TC_TLV_END)
        return TC_TLV_ARGUMENT;
      if (result != TC_TLV_OK)
        return result;
      if (matched != 0 && matched != 1)
        return TC_TLV_ARGUMENT;
    } else
      matched = 1;
    if (matched) {
      *reader = next;
      *out = choice.encoded;
      return TC_TLV_OK;
    }
  }
}

static TC_TLV_result cms_next_candidate(void* context, const tc_pki_tree_workspace* tree,
                                        const TC_X509_workspace* parser, TC_X509_certificate* out)
{
  TC_bytes encoded;
  return tc_cms_x509_candidate_next(context, NULL, NULL, tree, parser, out, &encoded);
}

tc_pki_store_candidates tc_cms_store_cursor(const tc_cms_candidates* source)
{
  return (tc_pki_store_candidates){source->external, source->collection.embedded.limits,
                                   source->collection.external_index, source->collection.remaining,
                                   source->collection.bytes_left};
}

/* Each search owns its cursor; certificate bytes remain borrowed from the source. */
tc_pki_candidate_next tc_cms_candidate_cursor_init(const tc_cms_candidates* source,
                                                   tc_cms_candidate_cursor* storage, void** cursor)
{
  if (source->external && tc_pki_end(&source->collection.embedded)) {
    storage->store = tc_cms_store_cursor(source);
    *cursor = &storage->store;
    return tc_pki_store_candidate_next;
  }
  storage->collection = *source;
  *cursor = &storage->collection;
  return cms_next_candidate;
}

TC_TLV_result tc_cms_certificate_search(const tc_cms_candidates* candidates,
                                        const tc_pki_candidate_checks* checks,
                                        const TC_TLV_limits* limits,
                                        const tc_pki_tree_workspace* tree,
                                        const TC_X509_path_workspace* validation,
                                        TC_X509_search_report* out, int* source_failed)
{
  if (!candidates)
    return TC_TLV_ARGUMENT;
  tc_cms_candidate_cursor storage;
  void* cursor;
  const tc_pki_candidate_next next = tc_cms_candidate_cursor_init(candidates, &storage, &cursor);
  return tc_pki_certificate_search(cursor, next, checks, limits, tree, validation, out,
                                   source_failed);
}

#endif
