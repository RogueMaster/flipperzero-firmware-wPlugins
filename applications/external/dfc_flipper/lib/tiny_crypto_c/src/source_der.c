/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV
#include "source_der_internal.h"
#include "internal.h"
#include <string.h>

TC_TLV_result tc_source_status(TC_result result)
{
  switch (result) {
  case TC_RESULT_OK:
    return TC_TLV_OK;
  case TC_RESULT_LIMIT:
    return TC_TLV_LIMIT;
  case TC_RESULT_ARGUMENT:
    return TC_TLV_ARGUMENT;
  case TC_RESULT_UNSUPPORTED:
    return TC_TLV_UNSUPPORTED;
  default:
    return TC_TLV_IO;
  }
}

TC_TLV_result tc_source_reader_copy(tc_source_reader* reader, uint64_t offset, size_t length,
                                    uint8_t* out)
{
  size_t copied = 0;
  while (copied < length) {
    TC_bytes chunk;
    const TC_result read = tc_source_reader_view(reader, offset + copied, length - copied, &chunk);
    if (read != TC_RESULT_OK)
      return tc_source_status(read);
    memcpy(out + copied, chunk.data, chunk.length);
    copied += chunk.length;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_source_der_read(tc_source_reader* reader, uint64_t offset, uint64_t end,
                                 uint64_t max_value, tc_source_der_element* out)
{
  enum { HEADER_CAPACITY = TC_TLV_TAG_BYTES + 1 + sizeof(uint64_t) };
  uint8_t encoded[HEADER_CAPACITY];
  tc_source_der_element parsed = {0};
  if (!reader || !out || offset > end || end > reader->source.length ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader, sizeof *reader) ||
      !tc_internal_ranges_disjoint(out, sizeof *out, reader->window.data, reader->window.capacity))
    return TC_TLV_ARGUMENT;
  if (offset == end)
    return TC_TLV_END;
  /* Parse after each window chunk and stop once the header is complete, so
   * physical reads cover only header bytes the window did not already hold.
   * A header longer than the parent or than HEADER_CAPACITY stays MORE. */
  const size_t available = end - offset < sizeof encoded ? (size_t)(end - offset) : sizeof encoded;
  size_t used = 0;
  TC_TLV_result result = TC_TLV_MORE;
  while (result == TC_TLV_MORE && used < available) {
    TC_bytes chunk;
    const TC_result read = tc_source_reader_view(reader, offset + used, available - used, &chunk);
    if (read != TC_RESULT_OK)
      return tc_source_status(read);
    memcpy(encoded + used, chunk.data, chunk.length);
    used += chunk.length;
    result =
        tc_tlv_header_read(encoded, used, TC_TLV_DER, sizeof(uint64_t), max_value, &parsed.header);
  }
  if (result == TC_TLV_MORE)
    return available < sizeof encoded ? TC_TLV_INVALID : TC_TLV_LIMIT;
  if (result != TC_TLV_OK)
    return result;
  /* The header fits inside the parent, so adding its size cannot wrap. */
  parsed.offset = offset;
  parsed.value_offset = offset + parsed.header.header_length;
  if (parsed.header.length > end - parsed.value_offset)
    return TC_TLV_INVALID;
  parsed.end = parsed.value_offset + parsed.header.length;
  *out = parsed;
  return TC_TLV_OK;
}
#endif
