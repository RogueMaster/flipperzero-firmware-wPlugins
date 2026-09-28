/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV
#include "tlv_internal.h"
#include <string.h>

static TC_TLV_result fail(TC_TLV_stream* s, TC_TLV_result error)
{
  s->error = error;
  return error;
}

static void emit(TC_TLV_visit visit, void* user, TC_TLV_event_kind kind,
                 size_t offset, size_t depth, const TC_TLV_header* h,
                 const uint8_t* data, size_t length)
{
  TC_TLV_event event;
  if (!visit) return;
  memset(&event, 0, sizeof event);
  event.kind = kind; event.offset = offset; event.depth = depth;
  event.bytes.data = data; event.bytes.length = length;
  if (h) event.header = *h;
  visit(user, &event);
}

static TC_TLV_result initialize(TC_TLV_stream* s, TC_TLV_profile profile,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t capacity)
{
  TC_TLV_result result = tc_tlv_config(profile, limits);
  if (result != TC_TLV_OK) return result;
  if (!s || (!frames && capacity)) return TC_TLV_ARGUMENT;
  memset(s, 0, sizeof *s);
  s->profile = profile; s->limits = *limits;
  s->frames = frames; s->capacity = capacity;
  return TC_TLV_OK;
}

static size_t bound(const TC_TLV_stream* s)
{
  /* Each frame inherits its parent's bound. Checking the top frame is enough
   * to prevent a child, including an indefinite child, escaping any ancestor. */
  return s->depth ? s->frames[s->depth - 1].bound : s->limits.max_input;
}

static TC_TLV_result boundary_error(const TC_TLV_stream* s)
{
  return !s->depth || s->frames[s->depth - 1].resource_bound ? TC_TLV_LIMIT : TC_TLV_INVALID;
}

static void close_definite(TC_TLV_stream* s, TC_TLV_visit visit, void* user)
{
  while (s->depth && !s->frames[s->depth - 1].indefinite &&
         s->offset == s->frames[s->depth - 1].end) {
    --s->depth;
    emit(visit, user, TC_TLV_CLOSE, s->offset, s->depth, NULL, NULL, 0);
  }
}

static TC_TLV_result feed(TC_TLV_stream* s, const uint8_t* data, size_t length,
                         TC_TLV_visit visit, void* user)
{
  size_t p = 0;
  if (!s || (!data && length)) return TC_TLV_ARGUMENT;
  if (s->error != TC_TLV_OK) return s->error;
  if (s->finished) return TC_TLV_ARGUMENT;
  if (s->offset > s->limits.max_input || length > s->limits.max_input - s->offset)
    return fail(s, TC_TLV_LIMIT);
  while (p < length) {
    TC_TLV_header h;
    TC_TLV_result result;
    size_t ceiling = bound(s), start;
    if (s->offset >= ceiling) return fail(s, boundary_error(s));
    if (s->primitive) {
      /* Primitive contents are opaque. In particular, 00 00 here is data,
       * not the end of an enclosing indefinite-length container. */
      size_t n = length - p;
      if (n > s->remaining) n = s->remaining;
      if (n > ceiling - s->offset) return fail(s, boundary_error(s));
      emit(visit, user, TC_TLV_VALUE, s->offset, s->depth, NULL, data + p, n);
      p += n; s->offset += n; s->remaining -= n;
      if (!s->remaining) {
        s->primitive = 0;
        emit(visit, user, TC_TLV_CLOSE, s->offset, s->depth, NULL, NULL, 0);
        close_definite(s, visit, user);
      }
      continue;
    }
    if (!s->depth && !s->used && tc_tlv_padding(s->profile, data[p])) {
      ++p; ++s->offset;
      continue;
    }
    if (s->used == sizeof s->header) return fail(s, TC_TLV_LIMIT);
    s->header[s->used++] = data[p++];
    ++s->offset;
#if TC_TLV_ENABLE_BER
    /* EOC is framing, not an ordinary tag, and is legal only at the top
     * of an indefinite frame. Primitive contents never reach this branch. */
    if (s->header[0] == 0) {
      if (s->profile != TC_TLV_BER || !s->depth ||
          !s->frames[s->depth - 1].indefinite) return fail(s, TC_TLV_INVALID);
      if (s->used == 1) continue;
      if (s->header[1] != 0) return fail(s, TC_TLV_INVALID);
      if (s->offset - 2 - s->frames[s->depth - 1].start > s->limits.max_value)
        return fail(s, TC_TLV_LIMIT);
      --s->depth; s->used = 0;
      emit(visit, user, TC_TLV_CLOSE, s->offset - 2, s->depth, NULL, s->header, 2);
      close_definite(s, visit, user);
      continue;
    }
#endif
    /* Accumulate only the header; values are delivered from the input chunk. */
    result = TC_TLV_header_read(s->header, s->used, s->profile, &s->limits, &h);
    if (result == TC_TLV_MORE) continue;
    if (result != TC_TLV_OK) return fail(s, result);
    if (s->elements >= s->limits.max_elements) return fail(s, TC_TLV_LIMIT);
    if (h.length > ceiling - s->offset) return fail(s, boundary_error(s));
    if (h.constructed && (s->depth >= s->limits.max_depth || s->depth >= s->capacity))
      return fail(s, TC_TLV_LIMIT);
    ++s->elements;
    start = s->offset - s->used;
    emit(visit, user, TC_TLV_BEGIN, start, s->depth, &h, s->header, s->used);
    s->used = 0;
    if (h.constructed) {
#if TC_TLV_ENABLE_BER
      uint8_t resource_bound = (uint8_t)(boundary_error(s) == TC_TLV_LIMIT);
#endif
      TC_TLV_frame* frame = &s->frames[s->depth++];
      frame->start = s->offset;
      frame->indefinite = h.indefinite;
#if TC_TLV_ENABLE_BER
      if (h.indefinite) {
        /* Include room for EOC while bounding the actual content separately. */
        size_t available = ceiling - s->offset;
        frame->end = 0;
        frame->bound = ceiling;
        frame->resource_bound = resource_bound;
        if (available > 2 && s->limits.max_value < available - 2) {
          frame->bound = s->offset + s->limits.max_value + 2;
          frame->resource_bound = 1;
        }
      } else
#endif
      {
        frame->end = s->offset + h.length;
        frame->bound = frame->end;
        frame->resource_bound = 0;
      }
      close_definite(s, visit, user);
    } else if (h.length) {
      s->remaining = h.length; s->primitive = 1;
    } else {
      emit(visit, user, TC_TLV_CLOSE, s->offset, s->depth, NULL, NULL, 0);
      close_definite(s, visit, user);
    }
  }
  return s->used || s->primitive || s->depth ? TC_TLV_MORE : TC_TLV_OK;
}

static TC_TLV_result finish(TC_TLV_stream* s)
{
  if (!s) return TC_TLV_ARGUMENT;
  if (s->error != TC_TLV_OK) return s->error;
  if (s->used || s->primitive || s->depth) return fail(s, TC_TLV_INVALID);
  s->finished = 1;
  return TC_TLV_OK;
}

#if TC_TLV_ENABLE_STREAM
TC_TLV_result TC_TLV_stream_init(TC_TLV_stream* s, TC_TLV_profile profile,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t capacity)
{ return initialize(s, profile, limits, frames, capacity); }
TC_TLV_result TC_TLV_stream_feed(TC_TLV_stream* s, const uint8_t* data, size_t length,
                               TC_TLV_visit visit, void* user)
{ return feed(s, data, length, visit, user); }
TC_TLV_result TC_TLV_stream_finish(TC_TLV_stream* s)
{ return finish(s); }
#endif

TC_TLV_result TC_TLV_walk(const uint8_t* data, size_t length,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t capacity, TC_TLV_visit visit, void* user)
{
  TC_TLV_stream s;
  TC_TLV_result result;
  if (!data && length) return TC_TLV_ARGUMENT;
  result = initialize(&s, profile, limits, frames, capacity);
  if (result != TC_TLV_OK) return result;
  result = feed(&s, data, length, visit, user);
  if (result != TC_TLV_OK && result != TC_TLV_MORE) return result;
  return finish(&s);
}

TC_TLV_result TC_TLV_read_tree(const uint8_t* data, size_t length,
    TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t capacity, TC_TLV_element* out)
{
  enum { EOC_BYTES = 2 };
  TC_TLV_stream s;
  TC_TLV_element element;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_TLV_header_read(data,length,profile,limits,&element.header);
  if (result != TC_TLV_OK) return result;
  result = initialize(&s,profile,limits,frames,capacity);
  if (result != TC_TLV_OK) return result;
  result = feed(&s,data,element.header.header_length,NULL,NULL);
  while (result == TC_TLV_MORE) {
    size_t chunk = 1;
    /* Feed complete primitive values and definite subtrees. Read indefinite
     * headers incrementally so a root EOC never consumes its next sibling. */
    if (s.primitive) chunk = s.remaining;
    else if (s.depth && !s.frames[s.depth - 1].indefinite)
      chunk = s.frames[s.depth - 1].end - s.offset;
    /* A partial header cannot extend beyond its definite parent. */
    if (!chunk) return TC_TLV_INVALID;
    if (s.offset == length) return TC_TLV_MORE;
    if (chunk > length - s.offset) chunk = length - s.offset;
    result = feed(&s,data + s.offset,chunk,NULL,NULL);
  }
  if (result != TC_TLV_OK) return result;
  element.encoded = (TC_bytes){data,s.offset};
  element.value.data = data + element.header.header_length;
  element.value.length = s.offset - element.header.header_length -
    (element.header.indefinite ? EOC_BYTES : 0);
  *out = element;
  return TC_TLV_OK;
}
#endif
