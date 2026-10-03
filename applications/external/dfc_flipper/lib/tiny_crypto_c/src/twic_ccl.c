/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/twic_ccl.h>
#if TC_ENABLE_TWIC_CCL
#include "credential_text_internal.h"
#include "pki_storage_internal.h"
#include "snapshot_internal.h"
#include <string.h>

enum { CCL_HEX_BYTES = 2 * TC_TWIC_CCL_FASCN_BYTES, CCL_DATE_OFFSET = CCL_HEX_BYTES + 1 };

TC_TWIC_CCL_result TC_TWIC_CCL_read(TC_bytes line, TC_TWIC_CCL_record* out)
{
  TC_TWIC_CCL_record record = {{0}, 0, 0, 0};
  TC_X509_time date;
  if (!out || (!line.data && line.length))
    return TC_TWIC_CCL_ARGUMENT;
  if (line.length != TC_TWIC_CCL_RECORD_BYTES)
    return TC_TWIC_CCL_INVALID;
  if (line.data[CCL_HEX_BYTES] != ',')
    return TC_TWIC_CCL_INVALID;
  for (size_t i = 0; i < TC_TWIC_CCL_FASCN_BYTES; ++i) {
    int high = tc_credential_hex_digit(line.data[2 * i]);
    int low = tc_credential_hex_digit(line.data[2 * i + 1]);
    if (high < 0 || low < 0)
      return TC_TWIC_CCL_INVALID;
    record.fascn[i] = (uint8_t)(high * 16 + low);
  }
  /* CCL rows spell the date as DDMmmYYYY, for example 29Feb2024. */
  if (!tc_credential_day_month_year(line.data + CCL_DATE_OFFSET, 1, &date))
    return TC_TWIC_CCL_INVALID;
  record.year = (uint16_t)date.year;
  record.month = date.month;
  record.day = date.day;
  *out = record;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_init(TC_TWIC_CCL_stream* stream, size_t max_bytes,
                                           size_t max_records, TC_TWIC_CCL_visit visit,
                                           void* context)
{
  if (!stream || !visit)
    return TC_TWIC_CCL_ARGUMENT;
  memset(stream, 0, sizeof *stream);
  stream->bytes_left = max_bytes;
  stream->records_left = max_records;
  stream->visit = visit;
  stream->context = context;
  return TC_TWIC_CCL_OK;
}

static TC_TWIC_CCL_result emit_record(TC_TWIC_CCL_stream* stream, TC_bytes line)
{
  TC_TWIC_CCL_record record;
  TC_TWIC_CCL_result result;
  if (!stream->records_left)
    return TC_TWIC_CCL_LIMIT;
  if (line.length && line.data[line.length - 1] == '\r')
    --line.length;
  result = TC_TWIC_CCL_read(line, &record);
  if (result != TC_TWIC_CCL_OK)
    return result;
  if (stream->visit(stream->context, &record) != TC_OK)
    return TC_TWIC_CCL_SINK_ERROR;
  --stream->records_left;
  ++stream->records;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_update(TC_TWIC_CCL_stream* stream, TC_bytes chunk)
{
  if (!stream)
    return TC_TWIC_CCL_ARGUMENT;
  if (stream->error != TC_TWIC_CCL_OK)
    return stream->error;
  if ((!chunk.data && chunk.length) || !stream->visit || stream->finished)
    return stream->error = TC_TWIC_CCL_ARGUMENT;
  if (chunk.length > stream->bytes_left)
    return stream->error = TC_TWIC_CCL_LIMIT;
  stream->bytes_left -= chunk.length;
  while (chunk.length) {
    /* Bound the newline search so a hostile long row stays within limits. */
    size_t available = sizeof stream->pending - stream->used;
    size_t scan = chunk.length < available + 1 ? chunk.length : available + 1;
    const uint8_t* newline = memchr(chunk.data, '\n', scan);
    size_t length = newline ? (size_t)(newline - chunk.data) : scan;
    TC_bytes line = {chunk.data, length};
    if (length > available)
      return stream->error = TC_TWIC_CCL_INVALID;
    if (stream->used || !newline) {
      memcpy(stream->pending + stream->used, chunk.data, length);
      stream->used += length;
      line = (TC_bytes){stream->pending, stream->used};
    }
    if (!newline)
      break;
    stream->error = emit_record(stream, line);
    if (stream->error != TC_TWIC_CCL_OK)
      return stream->error;
    stream->used = 0;
    chunk.data += length + 1;
    chunk.length -= length + 1;
  }
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_finish(TC_TWIC_CCL_stream* stream)
{
  if (!stream)
    return TC_TWIC_CCL_ARGUMENT;
  if (stream->error != TC_TWIC_CCL_OK)
    return stream->error;
  if (!stream->visit)
    return stream->error = TC_TWIC_CCL_ARGUMENT;
  if (stream->used || !stream->records)
    return stream->error = TC_TWIC_CCL_INVALID;
  stream->finished = 1;
  return TC_TWIC_CCL_OK;
}

typedef struct {
  TC_bytes fascn;
  int listed;
} ccl_lookup;

static TC_status lookup_record(void* context, const TC_TWIC_CCL_record* record)
{
  ccl_lookup* lookup = context;
  if (!memcmp(record->fascn, lookup->fascn.data, TC_TWIC_CCL_FASCN_BYTES))
    lookup->listed = 1;
  return TC_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_contains(TC_bytes csv, TC_bytes fascn, size_t max_records,
                                        int* listed)
{
  TC_TWIC_CCL_stream stream;
  ccl_lookup lookup = {fascn, 0};
  TC_TWIC_CCL_result result;
  if (!listed || !fascn.data || fascn.length != TC_TWIC_CCL_FASCN_BYTES ||
      (!csv.data && csv.length))
    return TC_TWIC_CCL_ARGUMENT;
  result = TC_TWIC_CCL_stream_init(&stream, csv.length, max_records, lookup_record, &lookup);
  if (result == TC_TWIC_CCL_OK)
    result = TC_TWIC_CCL_stream_update(&stream, csv);
  if (result == TC_TWIC_CCL_OK)
    result = TC_TWIC_CCL_stream_finish(&stream);
  if (result == TC_TWIC_CCL_OK)
    *listed = lookup.listed;
  return result;
}

static TC_TWIC_CCL_result read_key(const TC_TWIC_CCL_source* source, size_t position, TC_bytes* out)
{
  TC_bytes key = {NULL, 0};
  if (source->read(source->context, position, &key) != TC_OK)
    return TC_TWIC_CCL_SOURCE_ERROR;
  if (!key.data || key.length != TC_TWIC_CCL_FASCN_BYTES)
    return TC_TWIC_CCL_INVALID;
  *out = key;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_prepare(const TC_TWIC_CCL_source* source, size_t max_records,
                                             TC_TWIC_CCL_index* out)
{
  uint8_t previous[TC_TWIC_CCL_FASCN_BYTES];
  if (!source || !out || !source->read)
    return TC_TWIC_CCL_ARGUMENT;
  if (!source->count)
    return TC_TWIC_CCL_INVALID;
  if (source->count > max_records)
    return TC_TWIC_CCL_LIMIT;
  for (size_t position = 0; position < source->count; ++position) {
    TC_bytes key;
    TC_TWIC_CCL_result result = read_key(source, position, &key);
    if (result != TC_TWIC_CCL_OK)
      return result;
    if (position && memcmp(previous, key.data, sizeof previous) > 0)
      return TC_TWIC_CCL_INVALID;
    /* Preserve the preceding key across reuse of the source's read buffer. */
    memcpy(previous, key.data, sizeof previous);
  }
  out->source = *source;
  return TC_TWIC_CCL_OK;
}

static TC_status memory_key(void* context, size_t position, TC_bytes* out)
{
  const TC_bytes* image = context;
  if (!image || !image->data || !out || position >= image->length / TC_TWIC_CCL_FASCN_BYTES)
    return TC_ERROR;
  *out = (TC_bytes){image->data + position * TC_TWIC_CCL_FASCN_BYTES, TC_TWIC_CCL_FASCN_BYTES};
  return TC_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_from_memory(const TC_bytes* image, size_t max_records,
                                                 TC_TWIC_CCL_index* out)
{
  if (!image || !out || (!image->data && image->length) ||
      !tc_pki_storage_separate(image, sizeof *image, out, sizeof *out) ||
      (image->length && !tc_pki_storage_separate(image->data, image->length, out, sizeof *out)))
    return TC_TWIC_CCL_ARGUMENT;
  if (image->length % TC_TWIC_CCL_FASCN_BYTES)
    return TC_TWIC_CCL_INVALID;
  const TC_TWIC_CCL_source source = {(void*)image, image->length / TC_TWIC_CCL_FASCN_BYTES,
                                     memory_key};
  return TC_TWIC_CCL_index_prepare(&source, max_records, out);
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_contains(const TC_TWIC_CCL_index* index, TC_bytes fascn,
                                              size_t max_reads, int* listed)
{
  uint8_t query[TC_TWIC_CCL_FASCN_BYTES];
  size_t begin = 0, end;
  if (!index || !index->source.read || !index->source.count || !listed || !fascn.data ||
      fascn.length != sizeof query)
    return TC_TWIC_CCL_ARGUMENT;
  memcpy(query, fascn.data, sizeof query);
  end = index->source.count;
  while (begin < end) {
    size_t middle = begin + (end - begin) / 2;
    TC_bytes key;
    TC_TWIC_CCL_result result;
    int order;
    if (!max_reads)
      return TC_TWIC_CCL_LIMIT;
    --max_reads;
    result = read_key(&index->source, middle, &key);
    if (result != TC_TWIC_CCL_OK)
      return result;
    order = memcmp(query, key.data, sizeof query);
    if (!order) {
      *listed = 1;
      return TC_TWIC_CCL_OK;
    }
    if (order < 0)
      end = middle;
    else
      begin = middle + 1;
  }
  *listed = 0;
  return TC_TWIC_CCL_OK;
}
TC_TWIC_CCL_result TC_TWIC_CCL_check_freshness(const TC_TWIC_CCL_metadata* metadata,
                                               const TC_TWIC_CCL_freshness_policy* policy)
{
  if (!metadata || !policy)
    return TC_TWIC_CCL_ARGUMENT;
  if (metadata->published_at > metadata->received_at || metadata->received_at > policy->now)
    return TC_TWIC_CCL_INVALID;
  if (metadata->published_at < policy->minimum_publication ||
      policy->now - metadata->published_at > policy->max_age)
    return TC_TWIC_CCL_STALE;
  return TC_TWIC_CCL_OK;
}

static TC_TWIC_CCL_result snapshot_result(TC_TLV_result result)
{
  switch (result) {
  case TC_TLV_OK:
    return TC_TWIC_CCL_OK;
  case TC_TLV_LIMIT:
    return TC_TWIC_CCL_LIMIT;
  case TC_TLV_INVALID:
    return TC_TWIC_CCL_INVALID;
  default:
    return TC_TWIC_CCL_ARGUMENT;
  }
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_prepare(TC_TWIC_CCL_snapshot* slot,
                                             const TC_TWIC_CCL_index* index,
                                             const TC_TWIC_CCL_metadata* metadata)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  if (!slot || !index || !metadata || !index->source.read || !index->source.count ||
      !tc_pki_storage_separate(slot, sizeof *slot, index, sizeof *index) ||
      !tc_pki_storage_separate(slot, sizeof *slot, metadata, sizeof *metadata))
    return TC_TWIC_CCL_ARGUMENT;
  if (metadata->published_at > metadata->received_at)
    return TC_TWIC_CCL_INVALID;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_prepare(&view));
  if (result == TC_TWIC_CCL_OK) {
    slot->index = *index;
    slot->metadata = *metadata;
  }
  return result;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_discard(TC_TWIC_CCL_snapshot* slot)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  return snapshot_result(tc_snapshot_discard(&view));
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_publish(TC_TWIC_CCL_store* store, size_t revision,
                                             TC_TWIC_CCL_snapshot* slot)
{
  const tc_snapshot_store store_view = TC_SNAPSHOT_STORE(store);
  const tc_snapshot_slot next = TC_SNAPSHOT_SLOT(slot);
  TC_TWIC_CCL_snapshot* current = store ? store->current : NULL;
  const tc_snapshot_slot previous = TC_SNAPSHOT_SLOT(current);
  /* State errors outrank staleness, which outranks a stale revision. */
  TC_TWIC_CCL_result result =
      snapshot_result(tc_snapshot_publish_check(&store_view, &next, &previous));
  if (result == TC_TWIC_CCL_OK && current &&
      slot->metadata.published_at < current->metadata.published_at)
    result = TC_TWIC_CCL_STALE;
  if (result == TC_TWIC_CCL_OK)
    result = snapshot_result(tc_snapshot_publish_commit(&store_view, &next, &previous, revision));
  if (result == TC_TWIC_CCL_OK)
    store->current = slot;
  return result;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_acquire(TC_TWIC_CCL_store* store, TC_TWIC_CCL_snapshot** out)
{
  const tc_snapshot_store store_view = TC_SNAPSHOT_STORE(store);
  const tc_snapshot_slot current = TC_SNAPSHOT_SLOT(store ? store->current : NULL);
  const TC_TLV_result result = tc_snapshot_acquire(&store_view, &current, out, sizeof *out);
  if (result == TC_TLV_END)
    return TC_TWIC_CCL_UNAVAILABLE;
  if (result == TC_TLV_OK)
    *out = store->current;
  return snapshot_result(result);
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_release(TC_TWIC_CCL_snapshot* slot)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  return snapshot_result(tc_snapshot_release(&view));
}

TC_TWIC_CCL_result TC_TWIC_CCL_snapshot_contains(const TC_TWIC_CCL_snapshot* slot, TC_bytes fascn,
                                                 size_t max_reads, int* listed)
{
  if (!slot || !listed || !tc_snapshot_held(slot->state, slot->readers) ||
      !tc_pki_storage_separate(slot, sizeof *slot, listed, sizeof *listed))
    return TC_TWIC_CCL_ARGUMENT;
  if (slot->state == TC_SNAPSHOT_RETIRED)
    return TC_TWIC_CCL_STALE;
  return TC_TWIC_CCL_index_contains(&slot->index, fascn, max_reads, listed);
}
#endif
