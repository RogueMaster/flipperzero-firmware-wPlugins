/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV
#include "source_der_internal.h"
#include "internal.h"

TC_TLV_result tc_source_der_read(tc_source_reader* reader, uint64_t offset,
    uint64_t end, uint64_t max_value, tc_source_der_element* out)
{
  enum { HEADER_CAPACITY = TC_TLV_TAG_BYTES + 1 + sizeof(uint64_t) };
  uint8_t encoded[HEADER_CAPACITY];
  tc_source_der_element parsed;
  if (!reader || !out || offset > end || end > reader->source.length ||
      !tc_internal_ranges_disjoint(out,sizeof *out,reader,sizeof *reader) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,reader->window.data,reader->window.capacity))
    return TC_TLV_ARGUMENT;
  if (offset == end) return TC_TLV_END;
  for (size_t used = 0; used < sizeof encoded; ++used) {
    if (used >= end - offset) return TC_TLV_INVALID;
    TC_bytes byte;
    TC_result read = tc_source_reader_view(reader,offset + used,1,&byte);
    if (read != TC_RESULT_OK) {
      if (read == TC_RESULT_LIMIT) return TC_TLV_LIMIT;
      return read == TC_RESULT_ARGUMENT ? TC_TLV_ARGUMENT : TC_TLV_IO;
    }
    encoded[used] = byte.data[0];
    TC_TLV_result result = tc_tlv_header_read(encoded,used + 1,TC_TLV_DER,
      sizeof(uint64_t),max_value,&parsed.header);
    if (result == TC_TLV_MORE) continue;
    if (result != TC_TLV_OK) return result;
    /* The header fits inside the parent, so adding its size cannot wrap. */
    parsed.offset = offset;
    parsed.value_offset = offset + used + 1;
    if (parsed.header.length > end - parsed.value_offset) return TC_TLV_INVALID;
    parsed.end = parsed.value_offset + parsed.header.length;
    *out = parsed;
    return TC_TLV_OK;
  }
  return TC_TLV_LIMIT;
}
#endif
