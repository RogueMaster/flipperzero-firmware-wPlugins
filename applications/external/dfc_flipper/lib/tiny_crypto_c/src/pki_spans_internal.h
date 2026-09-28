/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_SPANS_INTERNAL_H_
#define TC_PKI_SPANS_INTERNAL_H_
#include "pki_internal.h"

static inline TC_TLV_result tc_pki_span_compare(TC_bytes a, TC_bytes b,
    size_t* work, int* order)
{
  size_t common = a.length < b.length ? a.length : b.length;
  if (work) {
    if (!*work || common > *work - 1) { *work = 0; return TC_TLV_LIMIT; }
    *work -= common + 1;
  }
  *order = tc_pki_compare(a,b);
  return TC_TLV_OK;
}

static TC_TLV_result tc_pki_span_sift(TC_bytes* values, size_t count,
    size_t root, size_t* work)
{
  while (root < count / 2) {
    size_t child = root * 2 + 1;
    int order;
    TC_bytes saved;
    TC_TLV_result result;
    if (child + 1 < count) {
      result = tc_pki_span_compare(values[child],values[child + 1],work,&order);
      if (result != TC_TLV_OK) return result;
      if (order < 0) ++child;
    }
    result = tc_pki_span_compare(values[root],values[child],work,&order);
    if (result != TC_TLV_OK) return result;
    if (order >= 0) break;
    saved = values[root]; values[root] = values[child]; values[child] = saved;
    root = child;
  }
  return TC_TLV_OK;
}

/* Heapsort caller-owned spans, then reject duplicates. No encoded bytes move.
 * Scratch may be partially sorted on error. NULL work selects an unmetered
 * scan for callers that already impose their own resource bounds. */
static inline TC_TLV_result tc_pki_spans_unique(TC_bytes* values, size_t count, size_t* work)
{
  TC_TLV_result result;
  size_t i;
  if (!values && count) return TC_TLV_ARGUMENT;
  for (i = count / 2; i; --i) {
    result = tc_pki_span_sift(values,count,i - 1,work);
    if (result != TC_TLV_OK) return result;
  }
  for (i = count; i > 1; --i) {
    TC_bytes saved = values[0]; values[0] = values[i - 1]; values[i - 1] = saved;
    result = tc_pki_span_sift(values,i - 1,0,work);
    if (result != TC_TLV_OK) return result;
  }
  for (i = 1; i < count; ++i) {
    int order;
    result = tc_pki_span_compare(values[i - 1],values[i],work,&order);
    if (result != TC_TLV_OK) return result;
    if (!order) return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}
#endif
