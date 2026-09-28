/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/twic_ccl.h>
#if TC_ENABLE_TWIC_CCL
#include "pki_internal.h"
#include "pki_storage_internal.h"
#include "snapshot_internal.h"
#include <string.h>

enum { CCL_HEX_BYTES = 2 * TC_TWIC_CCL_FASCN_BYTES, CCL_DATE_OFFSET = CCL_HEX_BYTES + 1 };

static int hex_digit(uint8_t c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

TC_TWIC_CCL_result TC_TWIC_CCL_read(TC_bytes line, TC_TWIC_CCL_record* out)
{
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  TC_TWIC_CCL_record record = {{0},0,0,0};
  unsigned year = 0, day;
  if (!out || (!line.data && line.length)) return TC_TWIC_CCL_ARGUMENT;
  if (line.length != TC_TWIC_CCL_RECORD_BYTES) return TC_TWIC_CCL_INVALID;
  if (line.data[CCL_HEX_BYTES] != ',') return TC_TWIC_CCL_INVALID;
  for (size_t i = 0; i < TC_TWIC_CCL_FASCN_BYTES; ++i) {
    int high = hex_digit(line.data[2 * i]), low = hex_digit(line.data[2 * i + 1]);
    if (high < 0 || low < 0) return TC_TWIC_CCL_INVALID;
    record.fascn[i] = (uint8_t)(high * 16 + low);
  }
  const uint8_t* date = line.data + CCL_DATE_OFFSET;
  if (date[0] < '0' || date[0] > '9' || date[1] < '0' || date[1] > '9')
    return TC_TWIC_CCL_INVALID;
  day = (unsigned)(date[0] - '0') * 10 + (unsigned)(date[1] - '0');
  for (unsigned i = 5; i < 9; ++i) {
    if (date[i] < '0' || date[i] > '9') return TC_TWIC_CCL_INVALID;
    year = year * 10 + (unsigned)(date[i] - '0');
  }
  for (unsigned month = 1; month <= 12; ++month)
    if (!memcmp(date + 2, months + 3 * (month - 1), 3)) record.month = (uint8_t)month;
  if (!tc_pki_date(year, record.month, day)) return TC_TWIC_CCL_INVALID;
  record.year = (uint16_t)year;
  record.day = (uint8_t)day;
  *out = record;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_init(TC_TWIC_CCL_stream* stream,
    size_t max_bytes, size_t max_records, TC_TWIC_CCL_visit visit, void* context)
{
  if (!stream || !visit) return TC_TWIC_CCL_ARGUMENT;
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
  if (!stream->records_left) return TC_TWIC_CCL_LIMIT;
  if (line.length && line.data[line.length - 1] == '\r') --line.length;
  result = TC_TWIC_CCL_read(line, &record);
  if (result != TC_TWIC_CCL_OK) return result;
  if (stream->visit(stream->context, &record) != TC_OK) return TC_TWIC_CCL_SINK_ERROR;
  --stream->records_left;
  ++stream->records;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_update(TC_TWIC_CCL_stream* stream, TC_bytes chunk)
{
  if (!stream) return TC_TWIC_CCL_ARGUMENT;
  if (stream->error != TC_TWIC_CCL_OK) return stream->error;
  if ((!chunk.data && chunk.length) || !stream->visit || stream->finished)
    return stream->error = TC_TWIC_CCL_ARGUMENT;
  if (chunk.length > stream->bytes_left) return stream->error = TC_TWIC_CCL_LIMIT;
  stream->bytes_left -= chunk.length;
  while (chunk.length) {
    /* Bound even the newline search when a hostile input contains a long row. */
    size_t available = sizeof stream->pending - stream->used;
    size_t scan = chunk.length < available + 1 ? chunk.length : available + 1;
    const uint8_t* newline = memchr(chunk.data, '\n', scan);
    size_t length = newline ? (size_t)(newline - chunk.data) : scan;
    TC_bytes line = {chunk.data, length};
    if (length > available) return stream->error = TC_TWIC_CCL_INVALID;
    if (stream->used || !newline) {
      memcpy(stream->pending + stream->used, chunk.data, length);
      stream->used += length;
      line = (TC_bytes){stream->pending, stream->used};
    }
    if (!newline) break;
    stream->error = emit_record(stream, line);
    if (stream->error != TC_TWIC_CCL_OK) return stream->error;
    stream->used = 0;
    chunk.data += length + 1;
    chunk.length -= length + 1;
  }
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_stream_finish(TC_TWIC_CCL_stream* stream)
{
  if (!stream) return TC_TWIC_CCL_ARGUMENT;
  if (stream->error != TC_TWIC_CCL_OK) return stream->error;
  if (!stream->visit) return stream->error = TC_TWIC_CCL_ARGUMENT;
  if (stream->used || !stream->records) return stream->error = TC_TWIC_CCL_INVALID;
  stream->finished = 1;
  return TC_TWIC_CCL_OK;
}

typedef struct { TC_bytes fascn; int listed; } ccl_lookup;

static TC_status lookup_record(void* context, const TC_TWIC_CCL_record* record)
{
  ccl_lookup* lookup = context;
  if (!memcmp(record->fascn, lookup->fascn.data, TC_TWIC_CCL_FASCN_BYTES)) lookup->listed = 1;
  return TC_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_contains(TC_bytes csv, TC_bytes fascn,
    size_t max_records, int* listed)
{
  TC_TWIC_CCL_stream stream;
  ccl_lookup lookup = {fascn,0};
  TC_TWIC_CCL_result result;
  if (!listed || !fascn.data || fascn.length != TC_TWIC_CCL_FASCN_BYTES ||
      (!csv.data && csv.length)) return TC_TWIC_CCL_ARGUMENT;
  result = TC_TWIC_CCL_stream_init(&stream,csv.length,max_records,lookup_record,&lookup);
  if (result == TC_TWIC_CCL_OK) result = TC_TWIC_CCL_stream_update(&stream,csv);
  if (result == TC_TWIC_CCL_OK) result = TC_TWIC_CCL_stream_finish(&stream);
  if (result == TC_TWIC_CCL_OK) *listed = lookup.listed;
  return result;
}

static TC_TWIC_CCL_result read_key(const TC_TWIC_CCL_source* source,
    size_t position, TC_bytes* out)
{
  TC_bytes key = {NULL,0};
  if (source->read(source->context,position,&key) != TC_OK) return TC_TWIC_CCL_SOURCE_ERROR;
  if (!key.data || key.length != TC_TWIC_CCL_FASCN_BYTES) return TC_TWIC_CCL_INVALID;
  *out = key;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_prepare(const TC_TWIC_CCL_source* source,
    size_t max_records, TC_TWIC_CCL_index* out)
{
  uint8_t previous[TC_TWIC_CCL_FASCN_BYTES];
  if (!source || !out || !source->read) return TC_TWIC_CCL_ARGUMENT;
  if (!source->count) return TC_TWIC_CCL_INVALID;
  if (source->count > max_records) return TC_TWIC_CCL_LIMIT;
  for (size_t position = 0; position < source->count; ++position) {
    TC_bytes key;
    TC_TWIC_CCL_result result = read_key(source,position,&key);
    if (result != TC_TWIC_CCL_OK) return result;
    if (position && memcmp(previous,key.data,sizeof previous) > 0) return TC_TWIC_CCL_INVALID;
    /* Preserve the preceding key across reuse of the source's read buffer. */
    memcpy(previous,key.data,sizeof previous);
  }
  out->source = *source;
  return TC_TWIC_CCL_OK;
}

static TC_status memory_key(void* context, size_t position, TC_bytes* out)
{
  const TC_bytes* image = context;
  if (!image || !image->data || !out ||
      position >= image->length / TC_TWIC_CCL_FASCN_BYTES) return TC_ERROR;
  *out = (TC_bytes){image->data + position * TC_TWIC_CCL_FASCN_BYTES,TC_TWIC_CCL_FASCN_BYTES};
  return TC_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_from_memory(const TC_bytes* image,
    size_t max_records, TC_TWIC_CCL_index* out)
{
  if (!image || !out || (!image->data && image->length) ||
      !tc_pki_storage_separate(image,sizeof *image,out,sizeof *out) ||
      (image->length && !tc_pki_storage_separate(image->data,image->length,out,sizeof *out)))
    return TC_TWIC_CCL_ARGUMENT;
  if (image->length % TC_TWIC_CCL_FASCN_BYTES) return TC_TWIC_CCL_INVALID;
  const TC_TWIC_CCL_source source = {
    (void*)image,image->length / TC_TWIC_CCL_FASCN_BYTES,memory_key
  };
  return TC_TWIC_CCL_index_prepare(&source,max_records,out);
}

TC_TWIC_CCL_result TC_TWIC_CCL_index_contains(const TC_TWIC_CCL_index* index,
    TC_bytes fascn, size_t max_reads, int* listed)
{
  uint8_t query[TC_TWIC_CCL_FASCN_BYTES];
  size_t begin = 0, end;
  if (!index || !index->source.read || !index->source.count || !listed ||
      !fascn.data || fascn.length != sizeof query) return TC_TWIC_CCL_ARGUMENT;
  memcpy(query,fascn.data,sizeof query);
  end = index->source.count;
  while (begin < end) {
    size_t middle = begin + (end - begin) / 2;
    TC_bytes key;
    TC_TWIC_CCL_result result;
    int order;
    if (!max_reads) return TC_TWIC_CCL_LIMIT;
    --max_reads;
    result = read_key(&index->source,middle,&key);
    if (result != TC_TWIC_CCL_OK) return result;
    order = memcmp(query,key.data,sizeof query);
    if (!order) { *listed = 1; return TC_TWIC_CCL_OK; }
    if (order < 0) end = middle;
    else begin = middle + 1;
  }
  *listed = 0;
  return TC_TWIC_CCL_OK;
}
TC_TWIC_CCL_result TC_TWIC_CCL_check_freshness(const TC_TWIC_CCL_metadata* metadata,
    const TC_TWIC_CCL_freshness_policy* policy)
{
  if (!metadata || !policy) return TC_TWIC_CCL_ARGUMENT;
  if (metadata->published_at > metadata->received_at || metadata->received_at > policy->now)
    return TC_TWIC_CCL_INVALID;
  if (metadata->published_at < policy->minimum_publication ||
      policy->now - metadata->published_at > policy->max_age) return TC_TWIC_CCL_STALE;
  return TC_TWIC_CCL_OK;
}

static TC_TWIC_CCL_result snapshot_result(TC_TLV_result result)
{
  switch (result) {
    case TC_TLV_OK: return TC_TWIC_CCL_OK;
    case TC_TLV_LIMIT: return TC_TWIC_CCL_LIMIT;
    case TC_TLV_INVALID: return TC_TWIC_CCL_INVALID;
    default: return TC_TWIC_CCL_ARGUMENT;
  }
}

static void recycle_snapshot(TC_TWIC_CCL_snapshot* slot)
{
  memset(&slot->index,0,sizeof slot->index);
  memset(&slot->metadata,0,sizeof slot->metadata);
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_prepare(TC_TWIC_CCL_snapshot* slot,
    const TC_TWIC_CCL_index* index, const TC_TWIC_CCL_metadata* metadata)
{
  if (!slot || !index || !metadata || !index->source.read || !index->source.count ||
      !tc_pki_storage_separate(slot,sizeof *slot,index,sizeof *index) ||
      !tc_pki_storage_separate(slot,sizeof *slot,metadata,sizeof *metadata)) return TC_TWIC_CCL_ARGUMENT;
  if (metadata->published_at > metadata->received_at) return TC_TWIC_CCL_INVALID;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_prepare(&slot->state,slot->readers));
  if (result != TC_TWIC_CCL_OK) return result;
  slot->index = *index;
  slot->metadata = *metadata;
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_discard(TC_TWIC_CCL_snapshot* slot)
{
  if (!slot) return TC_TWIC_CCL_ARGUMENT;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_discard(&slot->state,slot->readers));
  if (result == TC_TWIC_CCL_OK) recycle_snapshot(slot);
  return result;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_publish(TC_TWIC_CCL_store* store, size_t revision,
    TC_TWIC_CCL_snapshot* slot)
{
  TC_TWIC_CCL_snapshot* previous;
  if (!store || !slot ||
      !tc_pki_storage_separate(store,sizeof *store,slot,sizeof *slot) ||
      slot->state != TC_SNAPSHOT_PREPARED || slot->readers) return TC_TWIC_CCL_ARGUMENT;
  previous = store->current;
  if (previous && (previous->state != TC_SNAPSHOT_CURRENT ||
      !tc_pki_storage_separate(previous,sizeof *previous,store,sizeof *store) ||
      !tc_pki_storage_separate(previous,sizeof *previous,slot,sizeof *slot))) return TC_TWIC_CCL_ARGUMENT;
  if (previous && slot->metadata.published_at < previous->metadata.published_at) return TC_TWIC_CCL_STALE;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_publish(&slot->state,slot->readers,
      previous ? &previous->state : NULL,previous ? previous->readers : 0,revision,&store->revision));
  if (result != TC_TWIC_CCL_OK) return result;
  store->current = slot;
  if (previous && previous->state == TC_SNAPSHOT_FREE) recycle_snapshot(previous);
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_acquire(TC_TWIC_CCL_store* store,
    TC_TWIC_CCL_snapshot** out)
{
  if (!store || !out ||
      !tc_pki_storage_separate(store,sizeof *store,out,sizeof *out)) return TC_TWIC_CCL_ARGUMENT;
  TC_TWIC_CCL_snapshot* slot = store->current;
  if (!slot) return TC_TWIC_CCL_UNAVAILABLE;
  if (!tc_pki_storage_separate(slot,sizeof *slot,out,sizeof *out) ||
      !tc_pki_storage_separate(slot,sizeof *slot,store,sizeof *store)) return TC_TWIC_CCL_ARGUMENT;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_acquire(slot->state,&slot->readers));
  if (result == TC_TWIC_CCL_OK) *out = slot;
  return result;
}

TC_TWIC_CCL_result TC_TWIC_CCL_store_release(TC_TWIC_CCL_snapshot* slot)
{
  if (!slot) return TC_TWIC_CCL_ARGUMENT;
  TC_TWIC_CCL_result result = snapshot_result(tc_snapshot_release(&slot->state,&slot->readers));
  if (result == TC_TWIC_CCL_OK && slot->state == TC_SNAPSHOT_FREE) recycle_snapshot(slot);
  return result;
}

TC_TWIC_CCL_result TC_TWIC_CCL_snapshot_contains(const TC_TWIC_CCL_snapshot* slot,
    TC_bytes fascn, size_t max_reads, int* listed)
{
  if (!slot || !listed || !slot->readers ||
      (slot->state != TC_SNAPSHOT_CURRENT && slot->state != TC_SNAPSHOT_RETIRED) ||
      !tc_pki_storage_separate(slot,sizeof *slot,listed,sizeof *listed)) return TC_TWIC_CCL_ARGUMENT;
  if (slot->state == TC_SNAPSHOT_RETIRED) return TC_TWIC_CCL_STALE;
  return TC_TWIC_CCL_index_contains(&slot->index,fascn,max_reads,listed);
}
#endif
