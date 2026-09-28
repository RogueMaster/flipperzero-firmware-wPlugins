/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_X509_REVOCATION
#include "x509_crl_source_internal.h"
#include "internal.h"

static int source_tag(const tc_source_der_element* field, uint8_t tag)
{
  return field->header.tag_length == 1 && field->header.tag[0] == tag;
}

static tc_source_span source_span(const tc_source_der_element* field)
{
  const tc_source_span span = {field->offset,field->end - field->offset};
  return span;
}

static TC_TLV_result source_field(tc_source_reader* reader, uint64_t* cursor,
    uint64_t end, tc_source_der_element* field)
{
  TC_TLV_result result = tc_source_der_read(reader,*cursor,end,UINT64_MAX,field);
  if (result == TC_TLV_OK) *cursor = field->end;
  return result;
}

static TC_TLV_result source_required(tc_source_reader* reader, uint64_t* cursor,
    uint64_t end, uint8_t tag, tc_source_der_element* field)
{
  TC_TLV_result result = source_field(reader,cursor,end,field);
  if (result == TC_TLV_END) return TC_TLV_INVALID;
  if (result != TC_TLV_OK) return result;
  return source_tag(field,tag) ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_x509_crl_source_layout(tc_source_reader* reader,
    tc_x509_crl_layout* out)
{
  tc_source_der_element outer, tbs, field;
  tc_x509_crl_layout parsed = {0};
  uint64_t cursor = 0;
  if (!reader || !out ||
      !tc_internal_ranges_disjoint(out,sizeof *out,reader,sizeof *reader) ||
      !tc_internal_ranges_disjoint(out,sizeof *out,reader->window.data,reader->window.capacity))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = source_required(reader,&cursor,reader->source.length,0x30,&outer);
  if (result != TC_TLV_OK) return result;
  if (cursor != reader->source.length) return TC_TLV_INVALID;
  cursor = outer.value_offset;
  result = source_required(reader,&cursor,outer.end,0x30,&tbs);
  if (result != TC_TLV_OK) return result;
  parsed.tbs = source_span(&tbs);
  result = source_required(reader,&cursor,outer.end,0x30,&field);
  if (result != TC_TLV_OK) return result;
  parsed.algorithm = source_span(&field);
  result = source_required(reader,&cursor,outer.end,3,&field);
  if (result != TC_TLV_OK) return result;
  parsed.signature = source_span(&field);
  if (cursor != outer.end) return TC_TLV_INVALID;

  cursor = tbs.value_offset;
  result = source_field(reader,&cursor,tbs.end,&field);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (source_tag(&field,2)) {
    parsed.version = source_span(&field);
    result = source_required(reader,&cursor,tbs.end,0x30,&field);
    if (result != TC_TLV_OK) return result;
  }
  if (!source_tag(&field,0x30)) return TC_TLV_INVALID;
  parsed.inner_algorithm = source_span(&field);
  result = source_required(reader,&cursor,tbs.end,0x30,&field);
  if (result != TC_TLV_OK) return result;
  parsed.issuer = source_span(&field);
  result = source_field(reader,&cursor,tbs.end,&field);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!source_tag(&field,0x17) && !source_tag(&field,0x18)) return TC_TLV_INVALID;
  parsed.this_update = source_span(&field);
  result = source_field(reader,&cursor,tbs.end,&field);
  if (result == TC_TLV_OK && (source_tag(&field,0x17) || source_tag(&field,0x18))) {
    parsed.next_update = source_span(&field);
    result = source_field(reader,&cursor,tbs.end,&field);
  }
  if (result == TC_TLV_OK && source_tag(&field,0x30)) {
    parsed.revoked = source_span(&field);
    result = source_field(reader,&cursor,tbs.end,&field);
  }
  if (result == TC_TLV_OK) {
    if (!source_tag(&field,0xa0) || !parsed.version.length || cursor != tbs.end)
      return TC_TLV_INVALID;
    uint64_t explicit_cursor = field.value_offset;
    const uint64_t explicit_end = field.end;
    result = source_required(reader,&explicit_cursor,explicit_end,0x30,&field);
    if (result != TC_TLV_OK) return result;
    if (explicit_cursor != explicit_end || !field.header.length) return TC_TLV_INVALID;
    parsed.extensions = source_span(&field);
  } else if (result != TC_TLV_END) return result;
  *out = parsed;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_crl_source_entries_init(tc_source_reader* reader,
    tc_source_span encoded, unsigned version, uint64_t max_entries,
    tc_x509_crl_source_entries* out)
{
  if (!reader || !out || (version != 1 && version != 2) ||
      encoded.offset > reader->source.length || encoded.length > reader->source.length - encoded.offset)
    return TC_TLV_ARGUMENT;
  tc_x509_crl_source_entries parsed = {0,0,max_entries,version};
  if (encoded.length) {
    tc_source_der_element element;
    TC_TLV_result result = tc_source_der_read(reader,encoded.offset,
      encoded.offset + encoded.length,UINT64_MAX,&element);
    if (result != TC_TLV_OK) return result;
    if (!source_tag(&element,0x30) || element.end != encoded.offset + encoded.length)
      return TC_TLV_INVALID;
    parsed.cursor = element.value_offset;
    parsed.end = element.end;
  }
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_entry_next(tc_source_reader* reader,
    tc_x509_crl_source_entries* entries, TC_buffer scratch,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    tc_x509_crl_entry* out)
{
  if (!reader || !entries || !limits || !tree || !tree->work || !out ||
      entries->cursor > entries->end || entries->end > reader->source.length ||
      (!scratch.data && scratch.capacity)) return TC_TLV_ARGUMENT;
  if (entries->cursor == entries->end) return TC_TLV_END;
  if (!entries->remaining) return TC_TLV_LIMIT;
  tc_source_der_element element;
  TC_TLV_result result = tc_source_der_read(reader,entries->cursor,entries->end,
    limits->max_value,&element);
  if (result != TC_TLV_OK) return result;
  if (!source_tag(&element,0x30)) return TC_TLV_INVALID;
  const uint64_t size = element.end - element.offset;
  if (size > limits->max_input || size > SIZE_MAX) return TC_TLV_LIMIT;
  TC_bytes encoded;
  TC_result read = tc_source_reader_view(reader,element.offset,(size_t)size,&encoded);
  if (read != TC_RESULT_OK)
    return read == TC_RESULT_LIMIT ? TC_TLV_LIMIT : read == TC_RESULT_ARGUMENT ? TC_TLV_ARGUMENT : TC_TLV_IO;
  if (encoded.length != size) {
    if (size > scratch.capacity) return TC_TLV_LIMIT;
    size_t copied = 0;
    for (;;) {
      memcpy(scratch.data + copied,encoded.data,encoded.length);
      copied += encoded.length;
      if (copied == size) break;
      read = tc_source_reader_view(reader,element.offset + copied,(size_t)size - copied,&encoded);
      if (read != TC_RESULT_OK)
        return read == TC_RESULT_LIMIT ? TC_TLV_LIMIT : read == TC_RESULT_ARGUMENT ? TC_TLV_ARGUMENT : TC_TLV_IO;
    }
    encoded = (TC_bytes){scratch.data,(size_t)size};
  }
  TC_TLV_reader bounded;
  result = TC_TLV_reader_init(&bounded,encoded.data,encoded.length,TC_TLV_DER,limits);
  if (result != TC_TLV_OK) return result;
  tc_x509_crl_entry parsed;
  result = tc_x509_crl_entry_next(&bounded,entries->version,tree,&parsed);
  if (result != TC_TLV_OK) return result;
  entries->cursor = element.end;
  entries->remaining--;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_revoked_init(tc_source_reader* reader,
    tc_source_span encoded, const tc_x509_crl* metadata,
    const tc_x509_crl_extension_info* extensions, uint64_t max_entries,
    TC_buffer issuer_storage, tc_x509_crl_source_revoked* out)
{
  if (!metadata || !metadata->issuer.data || !metadata->issuer.length || !extensions || !out ||
      (!issuer_storage.data && issuer_storage.capacity)) return TC_TLV_ARGUMENT;
  tc_x509_crl_source_revoked parsed = {0};
  TC_TLV_result result = tc_x509_crl_extension_policy(extensions);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_source_entries_init(reader,encoded,metadata->version,max_entries,&parsed.entries);
  if (result != TC_TLV_OK) return result;
  parsed.extensions = extensions;
  parsed.issuer.name = metadata->issuer;
  parsed.issuer_storage = issuer_storage;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_revoked_next(tc_source_reader* reader,
    tc_x509_crl_source_revoked* revoked, TC_buffer scratch,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree,
    TC_bytes* oids, size_t capacity, tc_x509_crl_revoked_entry* out)
{
  if (!revoked || !out) return TC_TLV_ARGUMENT;
  tc_x509_crl_source_revoked next = *revoked;
  tc_x509_crl_entry entry;
  tc_x509_crl_revoked_entry parsed;
  TC_TLV_result result = tc_x509_crl_source_entry_next(reader,&next.entries,scratch,limits,tree,&entry);
  if (result != TC_TLV_OK) return result;
  result = tc_x509_crl_entry_resolve(&entry,next.extensions,&next.issuer,
    limits,tree,oids,capacity,&parsed);
  if (result != TC_TLV_OK) return result;
  if (parsed.extensions.present & TC_CRL_ENTRY_ISSUER) {
    const TC_bytes names = parsed.issuer.names;
    if (names.length > next.issuer_storage.capacity) return TC_TLV_LIMIT;
    if (!next.issuer_storage.data) return TC_TLV_ARGUMENT;
    memcpy(next.issuer_storage.data,names.data,names.length);
    parsed.issuer.names = (TC_bytes){next.issuer_storage.data,names.length};
  }
  next.issuer = parsed.issuer;
  *revoked = next;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_scan_init(tc_source_reader* reader,
    const tc_x509_crl_source_revoked* revoked, const tc_x509_crl_serial_query* queries,
    size_t count, tc_x509_crl_match* matches, size_t capacity, tc_x509_crl_source_scan* out)
{
  if (!reader || !revoked || !out || (count && (!queries || !matches)) ||
      count > SIZE_MAX / sizeof *matches || count > SIZE_MAX / sizeof *queries)
    return TC_TLV_ARGUMENT;
  if (count > capacity) return TC_TLV_LIMIT;
  for (size_t i = 0; i < count; ++i)
    if (!queries[i].serial.data || !queries[i].serial.length ||
        !queries[i].issuer.data || !queries[i].issuer.length) return TC_TLV_ARGUMENT;
  const tc_x509_crl_source_scan parsed = {reader,*revoked,queries,matches,count,TC_CRL_SCAN_ACTIVE};
  if (count) memset(matches,0,count * sizeof *matches);
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_scan_step(tc_x509_crl_source_scan* scan,
    size_t max_entries, TC_buffer scratch, const TC_TLV_limits* limits,
    const tc_pki_tree_workspace* tree, const TC_X509_name_workspace* names,
    TC_bytes* oids, size_t capacity, int* complete)
{
  if (!scan || !complete || !max_entries || scan->phase != TC_CRL_SCAN_ACTIVE)
    return TC_TLV_ARGUMENT;
  for (size_t step = 0; step < max_entries; ++step) {
    tc_x509_crl_revoked_entry entry;
    TC_TLV_result result = tc_x509_crl_source_revoked_next(scan->reader,&scan->revoked,
      scratch,limits,tree,oids,capacity,&entry);
    if (result == TC_TLV_END) {
      scan->phase = TC_CRL_SCAN_COMPLETE;
      *complete = 1;
      return TC_TLV_OK;
    }
    if (result == TC_TLV_OK) {
      for (size_t i = 0; i < scan->count; ++i) {
        result = tc_x509_crl_match_update(&entry,&scan->queries[i],limits,tree,names,&scan->matches[i]);
        if (result != TC_TLV_OK) break;
      }
    }
    if (result != TC_TLV_OK) {
      scan->phase = TC_CRL_SCAN_FAILED;
      return result;
    }
  }
  *complete = 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_scan_finish(const tc_x509_crl_source_scan* scan,
    tc_x509_crl_match* out, size_t capacity)
{
  if (!scan || scan->phase != TC_CRL_SCAN_COMPLETE || (scan->count && !out)) return TC_TLV_ARGUMENT;
  if (scan->count > capacity) return TC_TLV_LIMIT;
  if (scan->count) memcpy(out,scan->matches,scan->count * sizeof *out);
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_crl_source_metadata(tc_source_reader* reader,
    const tc_x509_crl_layout* layout, TC_buffer storage,
    const TC_TLV_limits* limits, const tc_pki_tree_workspace* tree, tc_x509_crl* out)
{
  if (!reader || !layout || !storage.data || !out || !limits || !tree || !tree->work)
    return TC_TLV_ARGUMENT;
  const tc_source_span spans[] = {layout->algorithm,layout->signature,layout->version,
    layout->inner_algorithm,layout->issuer,layout->this_update,layout->next_update,layout->extensions};
  tc_x509_crl_fields fields = {0};
  TC_bytes* views[] = {&fields.algorithm,&fields.signature,&fields.version,
    &fields.inner_algorithm,&fields.issuer,&fields.this_update,&fields.next_update,&fields.extensions};
  size_t total = 0;
  for (size_t i = 0; i < sizeof spans / sizeof spans[0]; ++i) {
    if (spans[i].offset > reader->source.length ||
        spans[i].length > reader->source.length - spans[i].offset) return TC_TLV_ARGUMENT;
    if (spans[i].length > storage.capacity - total || spans[i].length > limits->max_input)
      return TC_TLV_LIMIT;
    total += (size_t)spans[i].length;
  }
  size_t used = 0;
  for (size_t i = 0; i < sizeof spans / sizeof spans[0]; ++i) {
    const size_t length = (size_t)spans[i].length;
    if (!length) continue;
    *views[i] = (TC_bytes){storage.data + used,length};
    size_t copied = 0;
    while (copied < length) {
      TC_bytes chunk;
      TC_result result = tc_source_reader_view(reader,spans[i].offset + copied,length - copied,&chunk);
      if (result != TC_RESULT_OK) {
        if (result == TC_RESULT_LIMIT) return TC_TLV_LIMIT;
        return result == TC_RESULT_ARGUMENT ? TC_TLV_ARGUMENT : TC_TLV_IO;
      }
      memcpy(storage.data + used + copied,chunk.data,chunk.length);
      copied += chunk.length;
    }
    used += length;
  }
  return tc_x509_crl_metadata_read(&fields,limits,tree,out);
}
#endif
