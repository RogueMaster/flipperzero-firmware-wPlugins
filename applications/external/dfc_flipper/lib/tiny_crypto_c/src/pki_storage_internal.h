/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_STORAGE_INTERNAL_H_
#define TC_PKI_STORAGE_INTERNAL_H_
#include <tiny_crypto/tlv.h>
#include "internal.h"

/* Describe an array without wrapping its byte count or address range. */
static inline TC_TLV_result tc_pki_storage_span(const void* data, size_t count, size_t width,
                                                TC_bytes* span)
{
  size_t length;
  if (!width || count > SIZE_MAX / width)
    return TC_TLV_ARGUMENT;
  length = count * width;
  if ((length && !data) || length > UINTPTR_MAX - (uintptr_t)data)
    return TC_TLV_ARGUMENT;
  span->data = (const uint8_t*)data;
  span->length = length;
  return TC_TLV_OK;
}

static inline int tc_pki_storage_separate(const void* left, size_t left_size, const void* right,
                                          size_t right_size)
{
  TC_bytes a, b;
  return tc_pki_storage_span(left, 1, left_size, &a) == TC_TLV_OK &&
         tc_pki_storage_span(right, 1, right_size, &b) == TC_TLV_OK &&
         tc_internal_ranges_disjoint(a.data, a.length, b.data, b.length);
}

/* work is private bookkeeping, disjoint from the ranges being inspected. */
static inline TC_TLV_result tc_pki_storage_input(const TC_bytes* writes, size_t count,
                                                 TC_bytes input, size_t* work)
{
  size_t i;
  if ((input.length && !input.data) || input.length > UINTPTR_MAX - (uintptr_t)input.data)
    return TC_TLV_ARGUMENT;
  for (i = 0; i < count; ++i) {
    if (!*work)
      return TC_TLV_LIMIT;
    --*work;
    if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, input.data, input.length))
      return TC_TLV_ARGUMENT;
  }
  return TC_TLV_OK;
}

/* Storage preflight for an operation with caller-owned buffers:
 *
 *   1. write: record each range the operation may modify, in slot order.
 *   2. seal:  reject overlapping writes (one work unit per comparison).
 *   3. input: reject reads that overlap any write.
 *   4. finish: return the first failure, or commit the private budget.
 *
 * The first failure is kept and later steps return at once, so callers check
 * the status once at finish. writes is caller storage and stays valid for
 * guards after finish. A caller may adjust a recorded slot between write and
 * seal. budget is a private copy of the caller's work counter, because that
 * counter can itself lie inside a checked range.
 *
 * Costs: seal charges one unit per pair of writes, and each input charges one
 * unit per write it is compared with. Exhausting the budget sets
 * TC_TLV_LIMIT. Zero-length ranges are recorded and always pass. A NULL
 * pointer with a nonzero length, a zero width, or a byte size or address
 * range that wraps sets TC_TLV_ARGUMENT. */
typedef struct {
  TC_bytes* writes;
  size_t count;
  size_t capacity;
  size_t budget;
  size_t initial_budget;
  int sealed;
  TC_TLV_result status;
} tc_pki_storage_plan;

/* Start a plan with room for capacity writes and a copy of the work budget. */
void tc_pki_storage_plan_begin(tc_pki_storage_plan* plan, TC_bytes* writes, size_t capacity,
                               size_t budget);
/* Record count elements of width bytes at data as writable. Recording more
 * than capacity writes, or writing after seal, sets TC_TLV_ARGUMENT. */
void tc_pki_storage_plan_write(tc_pki_storage_plan* plan, const void* data, size_t count,
                               size_t width);
void tc_pki_storage_plan_write_span(tc_pki_storage_plan* plan, TC_bytes write);
/* Record an argument failure found while describing storage. */
void tc_pki_storage_plan_fail(tc_pki_storage_plan* plan, TC_TLV_result status);
/* Check every pair of recorded writes for overlap. Overlap sets
 * TC_TLV_ARGUMENT. Inputs are accepted only after seal. */
void tc_pki_storage_plan_seal(tc_pki_storage_plan* plan);
/* Check a read-only range against every write. Overlap sets TC_TLV_ARGUMENT. */
void tc_pki_storage_plan_input(tc_pki_storage_plan* plan, const void* data, size_t count,
                               size_t width);
void tc_pki_storage_plan_input_span(tc_pki_storage_plan* plan, TC_bytes input);
/* Check each span's bytes. The span array itself needs its own input call. */
void tc_pki_storage_plan_input_spans(tc_pki_storage_plan* plan, const TC_bytes* inputs,
                                     size_t count);
/* Return the first failure. On success, a non-NULL work receives the
 * remaining budget, and a NULL work discards it. */
TC_TLV_result tc_pki_storage_plan_finish(const tc_pki_storage_plan* plan, size_t* work);

/* Work units spent by seal and input steps so far. Fixed-cost preflights
 * begin with SIZE_MAX and charge this amount to the caller afterwards. */
size_t tc_pki_storage_plan_used(const tc_pki_storage_plan* plan);

/* Typed forms: the element width comes from the pointer. */
#define TC_PKI_PLAN_WRITE(plan, pointer, count)                                                    \
  tc_pki_storage_plan_write((plan), (pointer), (count), sizeof *(pointer))
#define TC_PKI_PLAN_INPUT(plan, pointer, count)                                                    \
  tc_pki_storage_plan_input((plan), (pointer), (count), sizeof *(pointer))
#endif
