/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_STORAGE_INTERNAL_H_
#define TC_PKI_STORAGE_INTERNAL_H_
#include <tiny_crypto/tlv.h>
#include "internal.h"

/* Describe an array without wrapping its byte count or address range. */
static inline TC_TLV_result tc_pki_storage_span(const void* data, size_t count,
    size_t width, TC_bytes* span)
{
  size_t length;
  if (!width || count > SIZE_MAX / width) return TC_TLV_ARGUMENT;
  length = count * width;
  if ((length && !data) || length > UINTPTR_MAX - (uintptr_t)data) return TC_TLV_ARGUMENT;
  span->data = (const uint8_t*)data; span->length = length;
  return TC_TLV_OK;
}

static inline int tc_pki_storage_separate(const void* left, size_t left_size,
    const void* right, size_t right_size)
{
  TC_bytes a, b;
  return tc_pki_storage_span(left,1,left_size,&a) == TC_TLV_OK &&
      tc_pki_storage_span(right,1,right_size,&b) == TC_TLV_OK &&
      tc_internal_ranges_disjoint(a.data,a.length,b.data,b.length);
}

/* work is private bookkeeping, disjoint from the ranges being inspected. */
static inline TC_TLV_result tc_pki_storage_input(const TC_bytes* writes, size_t count,
    TC_bytes input, size_t* work)
{
  size_t i;
  if ((input.length && !input.data) || input.length > UINTPTR_MAX - (uintptr_t)input.data)
    return TC_TLV_ARGUMENT;
  for (i = 0; i < count; ++i) {
    if (!*work) return TC_TLV_LIMIT;
    --*work;
    if (!tc_internal_ranges_disjoint(writes[i].data,writes[i].length,input.data,input.length))
      return TC_TLV_ARGUMENT;
  }
  return TC_TLV_OK;
}
#endif
