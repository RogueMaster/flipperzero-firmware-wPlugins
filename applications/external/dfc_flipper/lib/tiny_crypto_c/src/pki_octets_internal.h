/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_OCTETS_INTERNAL_H_
#define TC_PKI_OCTETS_INTERNAL_H_
#include "pki_internal.h"
#include "x509_path_internal.h"

typedef TC_TLV_result (*tc_pki_octets_consume)(void* context, TC_bytes bytes);
typedef struct {
  tc_pki_octets_consume consume;
  void* context;
  size_t roots;
  TC_TLV_profile profile;
  TC_TLV_result result;
  unsigned root_tag;
} tc_pki_octets_state;

static void tc_pki_octets_visit(void* user, const TC_TLV_event* event)
{
  tc_pki_octets_state* state = user;
  if (state->result != TC_TLV_OK) return;
  if (event->kind == TC_TLV_BEGIN) {
    const TC_TLV_header* header = &event->header;
    unsigned tag = event->depth ? 4 : state->root_tag;
    if (event->depth == 0 && ++state->roots != 1) state->result = TC_TLV_INVALID;
    if (header->tag_length != 1 || (header->tag[0] != tag && header->tag[0] != (tag | 0x20)) ||
        (state->profile == TC_TLV_DER && header->constructed))
      state->result = TC_TLV_INVALID;
  } else if (event->kind == TC_TLV_VALUE && state->consume)
    state->result = state->consume(state->context,event->bytes);
}

/* Visit only primitive value bytes, in order, including nested BER chunks.
 * Headers/EOC are excluded. Callback state is provisional until OK; discard
 * it on error. Input, frames, work and callback state must be disjoint. */
static inline TC_TLV_result tc_pki_octets_implicit(TC_bytes encoded, unsigned root_tag,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_frame* frames,
    size_t capacity, size_t* work, tc_pki_octets_consume consume, void* context)
{
  tc_pki_octets_state state = {consume,context,0,profile,TC_TLV_OK,root_tag};
  TC_TLV_result result;
  if (!work || root_tag > 255 || (root_tag & 0x20) || (root_tag & 31) == 31)
    return TC_TLV_ARGUMENT;
  if (tc_x509_path_charge(work,encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_walk(encoded.data,encoded.length,profile,limits,frames,capacity,
      tc_pki_octets_visit,&state);
  if (result != TC_TLV_OK) return result;
  if (state.roots != 1) return TC_TLV_INVALID;
  return state.result;
}

static inline TC_TLV_result tc_pki_octets(TC_bytes encoded,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_frame* frames,
    size_t capacity, size_t* work, tc_pki_octets_consume consume, void* context)
{
  return tc_pki_octets_implicit(encoded,4,profile,limits,frames,capacity,work,consume,context);
}
typedef struct {
  TC_bytes expected;
  size_t offset;
  size_t* work;
  int equal;
} tc_pki_octets_comparison;

static TC_TLV_result tc_pki_octets_compare_chunk(void* context, TC_bytes bytes)
{
  tc_pki_octets_comparison* state = context;
  size_t remaining = state->expected.length - state->offset;
  size_t count = bytes.length < remaining ? bytes.length : remaining;
  if (tc_x509_path_charge(state->work,bytes.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (bytes.length > remaining || (count &&
      memcmp(bytes.data,state->expected.data + state->offset,count))) state->equal = 0;
  state->offset += count;
  return TC_TLV_OK;
}

/* Compare chunk contents without flattening them. Complete framing is checked
 * even after a mismatch. Identifiers are public, so comparison is not constant-time.
 * Callers preflight disjoint inputs and writable ranges. */
static inline TC_TLV_result tc_pki_octets_equal(TC_bytes encoded, unsigned root_tag,
    TC_bytes expected, TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t capacity, size_t* work, int* out)
{
  tc_pki_octets_comparison state = {expected,0,work,1};
  TC_TLV_result result;
  if (!out || (!expected.data && expected.length)) return TC_TLV_ARGUMENT;
  result = tc_pki_octets_implicit(encoded,root_tag,profile,limits,frames,capacity,work,
      tc_pki_octets_compare_chunk,&state);
  if (result != TC_TLV_OK) return result;
  *out = state.equal && state.offset == expected.length;
  return TC_TLV_OK;
}
typedef struct {
  TC_bytes first;
  size_t length;
  size_t chunks;
  uint8_t* buffer;
  size_t buffer_size;
  size_t* work;
} tc_pki_octets_storage;

static TC_TLV_result tc_pki_octets_store_chunk(void* context, TC_bytes bytes)
{
  tc_pki_octets_storage* state = context;
  if (bytes.length > SIZE_MAX - state->length) return TC_TLV_LIMIT;
  if (state->buffer) {
    if (bytes.length > state->buffer_size - state->length) return TC_TLV_LIMIT;
    if (tc_x509_path_charge(state->work,bytes.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    memcpy(state->buffer + state->length,bytes.data,bytes.length);
  }
  if (!state->chunks) state->first = bytes;
  /* Only distinguish zero, one, and multiple nonempty chunks. */
  if (bytes.length && state->chunks < 2) ++state->chunks;
  state->length += bytes.length;
  return TC_TLV_OK;
}

/* Borrow a single chunk; join multiple chunks in caller storage. Input must
 * stay unchanged across both passes. All writable ranges must be disjoint.
 * Output changes only on success; scratch contents are provisional on error. */
static inline TC_TLV_result tc_pki_octets_contiguous(TC_bytes encoded,
    unsigned root_tag, TC_TLV_profile profile, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t capacity, size_t* work,
    uint8_t* buffer, size_t buffer_size, TC_bytes* out)
{
  tc_pki_octets_storage state = {{NULL,0},0,0,NULL,0,work};
  TC_TLV_result result;
  size_t length;
  if (!out || (!buffer && buffer_size)) return TC_TLV_ARGUMENT;
  result = tc_pki_octets_implicit(encoded,root_tag,profile,limits,frames,capacity,
      work,tc_pki_octets_store_chunk,&state);
  if (result != TC_TLV_OK) return result;
  if (state.chunks < 2) { *out = state.first; return TC_TLV_OK; }
  length = state.length;
  if (length > buffer_size) return TC_TLV_LIMIT;
  state.buffer = buffer;
  state.buffer_size = length;
  state.length = 0;
  result = tc_pki_octets_implicit(encoded,root_tag,profile,limits,frames,capacity,
      work,tc_pki_octets_store_chunk,&state);
  if (result != TC_TLV_OK) return result;
  *out = (TC_bytes){buffer,length};
  return TC_TLV_OK;
}
#endif
