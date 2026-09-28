/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_SOURCE_INTERNAL_H_
#define TC_PKI_SOURCE_INTERNAL_H_
#include <tiny_crypto/x509_store.h>
#include "pki_storage_internal.h"

typedef struct {
  void* context;
  size_t count;
  TC_TLV_result (*read)(void* context, size_t index, size_t* work, TC_bytes* out);
} tc_pki_record_source;

/* Callback bytes remain borrowed. The boundary caller checks their overlap
 * with its scratch before parsing. Invalid records leave out unchanged. */
static inline TC_TLV_result tc_pki_record_read(const tc_pki_record_source* source,
    size_t index, size_t* work, TC_bytes* out)
{
  TC_bytes candidate = {NULL,0};
  TC_TLV_result result;
  size_t before;
  if (!source || !source->read || !work || !out || index >= source->count)
    return TC_TLV_ARGUMENT;
  before = *work;
  result = source->read(source->context,index,work,&candidate);
  if (*work > before) { *work = 0; return TC_TLV_ARGUMENT; }
  if (result != TC_TLV_OK)
    return result == TC_TLV_LIMIT || result == TC_TLV_UNSUPPORTED ? result : TC_TLV_ARGUMENT;
  if (!candidate.data || !candidate.length || candidate.length > UINTPTR_MAX - (uintptr_t)candidate.data)
    return TC_TLV_ARGUMENT;
  *out = candidate;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_pki_source_candidate(const TC_X509_store_source* source,
    size_t index, size_t* work, TC_bytes* out)
{
  if (!source) return TC_TLV_ARGUMENT;
  const tc_pki_record_source records = {source->context,source->candidate_count,source->candidate};
  return tc_pki_record_read(&records,index,work,out);
}

/* Borrow source metadata and the caller's writable-range list for one operation. */
typedef struct {
  const tc_pki_record_source* source;
  const TC_bytes* writes;
  size_t write_count;
} tc_pki_record_guard;

/* Check returned bytes before a parser can use overlapping scratch. */
static inline TC_TLV_result tc_pki_record_guard_read(void* context,
    size_t index, size_t* work, TC_bytes* out)
{
  const tc_pki_record_guard* guard = context;
  TC_bytes record;
  TC_TLV_result result;
  if (!guard || !work || !out || (guard->write_count && !guard->writes)) return TC_TLV_ARGUMENT;
  result = tc_pki_record_read(guard->source,index,work,&record);
  if (result == TC_TLV_OK)
    result = tc_pki_storage_input(guard->writes,guard->write_count,record,work);
  if (result == TC_TLV_OK) *out = record;
  return result;
}

typedef struct {
  const TC_X509_store_source* source;
  const TC_bytes* writes;
  size_t write_count;
} tc_pki_source_guard;

static inline TC_TLV_result tc_pki_source_guard_input(const tc_pki_source_guard* guard,
    TC_bytes input, size_t* work)
{
  return tc_pki_storage_input(guard->writes,guard->write_count,input,work);
}

/* Returned records must remain separate from every consumer's scratch/output.
 * Failed checks preserve out; callback state and work are provisional. */
static inline TC_TLV_result tc_pki_source_guard_candidate(void* context,
    size_t index, size_t* work, TC_bytes* out)
{
  const tc_pki_source_guard* guard = context;
  if (!guard || !guard->source) return TC_TLV_ARGUMENT;
  const tc_pki_record_source records = {guard->source->context,
    guard->source->candidate_count,guard->source->candidate};
  tc_pki_record_guard record_guard = {&records,guard->writes,guard->write_count};
  return tc_pki_record_guard_read(&record_guard,index,work,out);
}

static inline TC_TLV_result tc_pki_source_guard_anchor(void* context,
    size_t index, size_t* work, TC_X509_store_anchor* out)
{
  const tc_pki_source_guard* guard = context;
  TC_X509_store_anchor anchor = {0};
  TC_TLV_result result;
  size_t before;
  if (!guard || !guard->source || !guard->source->anchor ||
      index >= guard->source->anchor_count || !work || !out ||
      (guard->write_count && !guard->writes)) return TC_TLV_ARGUMENT;
  before = *work;
  result = guard->source->anchor(guard->source->context,index,work,&anchor);
  if (*work > before) { *work = 0; return TC_TLV_ARGUMENT; }
  if (result != TC_TLV_OK)
    return result == TC_TLV_LIMIT || result == TC_TLV_UNSUPPORTED ? result : TC_TLV_ARGUMENT;
  const TC_bytes spans[] = {
    anchor.trust.name,anchor.trust.public_key.algorithm.oid,anchor.trust.public_key.algorithm.parameters,
    anchor.trust.public_key.key,anchor.trust.public_key.modulus,anchor.trust.public_key.exponent,
    anchor.trust.public_key.curve_oid,anchor.names.permitted,anchor.names.excluded
  };
  for (size_t i = 0; i < sizeof spans / sizeof *spans; ++i) {
    result = tc_pki_source_guard_input(guard,spans[i],work);
    if (result != TC_TLV_OK) return result;
  }
  *out = anchor;
  return TC_TLV_OK;
}

typedef struct {
  const TC_X509_store_source* source;
  size_t anchor_index;
} tc_pki_anchor_source;

static inline TC_TLV_result tc_pki_anchor_candidate(void* context,
    size_t index, size_t* work, TC_bytes* out)
{
  const tc_pki_anchor_source* selected = context;
  if (!selected) return TC_TLV_ARGUMENT;
  return tc_pki_source_candidate(selected->source,index,work,out);
}

static inline TC_TLV_result tc_pki_anchor_record(void* context,
    size_t index, size_t* work, TC_X509_store_anchor* out)
{
  const tc_pki_anchor_source* selected = context;
  if (!selected || !selected->source || !selected->source->anchor || index ||
      selected->anchor_index >= selected->source->anchor_count || !work || !out)
    return TC_TLV_ARGUMENT;
  return selected->source->anchor(selected->source->context,selected->anchor_index,work,out);
}

/* Borrow all candidates but expose only one anchor, at view index zero.
 * Keep the original source snapshot and this context alive and unchanged.
 * Results use the view's anchor index; context retains the original index.
 * Input/context/output storage must be disjoint. Outputs change only on OK.
 * Path-build callback guards still validate the forwarded anchor record. */
static inline TC_TLV_result tc_pki_source_select_anchor(const TC_X509_store_source* source,
    size_t anchor_index, tc_pki_anchor_source* context, TC_X509_store_source* out)
{
  TC_X509_store_source view;
  if (!source || !context || !out || !source->anchor || anchor_index >= source->anchor_count ||
      (source->candidate_count && !source->candidate)) return TC_TLV_ARGUMENT;
  view = (TC_X509_store_source){context,source->candidate_count,1,
      source->candidate ? tc_pki_anchor_candidate : NULL,tc_pki_anchor_record};
  *context = (tc_pki_anchor_source){source,anchor_index};
  *out = view;
  return TC_TLV_OK;
}
/* Keep source failures sticky while a search retries candidate paths. */
typedef struct {
  tc_pki_source_guard guard;
  int* failed;
} tc_pki_source_status_guard;


static inline TC_TLV_result tc_pki_source_status_candidate(void* context, size_t index, size_t* work, TC_bytes* out)
{
  tc_pki_source_status_guard* guard = context;
  if (!guard || !guard->failed) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_source_guard_candidate(&guard->guard,index,work,out);
  if (result != TC_TLV_OK) *guard->failed = 1;
  return result;
}

static inline TC_TLV_result tc_pki_source_status_anchor(void* context, size_t index, size_t* work, TC_X509_store_anchor* out)
{
  tc_pki_source_status_guard* guard = context;
  if (!guard || !guard->failed) return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_source_guard_anchor(&guard->guard,index,work,out);
  if (result != TC_TLV_OK) *guard->failed = 1;
  return result;
}
#endif
