/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "source_internal.h"
#include "internal.h"

TC_status tc_source_memory_read(void* context, uint64_t offset,
    uint8_t* destination, size_t length)
{
  const TC_bytes* bytes = (const TC_bytes*)context;
  if (!bytes || (!bytes->data && bytes->length) || (!destination && length) ||
      offset > bytes->length || length > bytes->length - offset) return TC_ERROR;
  if (length) memcpy(destination,bytes->data + (size_t)offset,length);
  return TC_OK;
}

TC_result tc_source_reader_init(tc_source_reader* reader, const TC_source* source,
    TC_buffer window, uint64_t max_bytes, uint64_t max_reads)
{
  if (!reader || !source || !source->read || !window.data || !window.capacity ||
      !tc_internal_ranges_disjoint(reader,sizeof *reader,source,sizeof *source) ||
      !tc_internal_ranges_disjoint(reader,sizeof *reader,window.data,window.capacity) ||
      !tc_internal_ranges_disjoint(source,sizeof *source,window.data,window.capacity))
    return TC_RESULT_ARGUMENT;
  tc_source_reader parsed = {*source,window,0,max_bytes,max_reads,0};
  *reader = parsed;
  return TC_RESULT_OK;
}

TC_result tc_source_reader_view(tc_source_reader* reader, uint64_t offset,
    size_t length, TC_bytes* out)
{
  if (!reader || !out || !reader->source.read || !reader->window.data ||
      !reader->window.capacity || reader->available > reader->window.capacity ||
      !tc_internal_ranges_disjoint(reader,sizeof *reader,out,sizeof *out) ||
      !tc_internal_ranges_disjoint(reader->window.data,reader->window.capacity,out,sizeof *out))
    return TC_RESULT_ARGUMENT;
  if (offset > reader->source.length || length > reader->source.length - offset)
    return TC_RESULT_ARGUMENT;
  if (!length) {
    const TC_bytes empty = {NULL,0};
    *out = empty;
    return TC_RESULT_OK;
  }
  if (offset >= reader->offset && offset - reader->offset < reader->available) {
    const size_t skip = (size_t)(offset - reader->offset);
    const size_t available = reader->available - skip;
    const TC_bytes cached = {reader->window.data + skip,
      length < available ? length : available};
    *out = cached;
    return TC_RESULT_OK;
  }
  if (!reader->reads_remaining || !reader->bytes_remaining) return TC_RESULT_LIMIT;

  uint64_t remaining = reader->source.length - offset;
  if (remaining > reader->bytes_remaining) remaining = reader->bytes_remaining;
  const size_t transfer = remaining < reader->window.capacity ?
    (size_t)remaining : reader->window.capacity;
  reader->reads_remaining--;
  reader->bytes_remaining -= transfer;
  reader->available = 0;
  if (reader->source.read(reader->source.context,offset,reader->window.data,transfer) != TC_OK) {
    memset(reader->window.data,0,reader->window.capacity);
    return TC_RESULT_ERROR;
  }
  reader->offset = offset;
  reader->available = transfer;
  const TC_bytes loaded = {reader->window.data,length < transfer ? length : transfer};
  *out = loaded;
  return TC_RESULT_OK;
}
