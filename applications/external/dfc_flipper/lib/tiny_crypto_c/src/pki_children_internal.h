/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_CHILDREN_INTERNAL_H_
#define TC_PKI_CHILDREN_INTERNAL_H_
#include "pki_internal.h"
#include "x509_path_internal.h"

typedef struct {
  TC_bytes input;
  TC_TLV_element* fields;
  size_t capacity, count, roots;
  int overflow;
} tc_pki_children_state;

static void tc_pki_children_visit(void* user, const TC_TLV_event* event)
{
  tc_pki_children_state* state = user;
  TC_TLV_element* field;
  if (event->kind == TC_TLV_BEGIN && event->depth == 0) ++state->roots;
  if (event->depth != 1 || state->overflow) return;
  if (event->kind == TC_TLV_BEGIN) {
    if (state->count == state->capacity) { state->overflow = 1; return; }
    field = &state->fields[state->count++];
    field->header = event->header;
    field->encoded = (TC_bytes){state->input.data + event->offset,0};
    field->value = (TC_bytes){field->encoded.data + event->header.header_length,0};
  } else if (event->kind == TC_TLV_CLOSE && state->count) {
    field = &state->fields[state->count - 1];
    field->value.length = (size_t)(state->input.data + event->offset - field->value.data);
    field->encoded.length = (size_t)(state->input.data + event->offset - field->encoded.data) + event->bytes.length;
  }
}

/* Collect immediate fields while validating the whole constructed value.
 * BER EOC belongs to encoded, not value. fields/frames are caller scratch and
 * may change on failure; count changes only on OK. All storage is disjoint. */
static inline TC_TLV_result tc_pki_children(TC_bytes encoded, unsigned tag,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_frame* frames,
    size_t frame_capacity, size_t* work, TC_TLV_element* fields, size_t capacity, size_t* count)
{
  TC_TLV_element root;
  tc_pki_children_state state = {encoded,fields,capacity,0,0,0};
  TC_TLV_result result;
  if (!work || !count || (capacity && !fields)) return TC_TLV_ARGUMENT;
  result = TC_TLV_header_read(encoded.data,encoded.length,profile,limits,&root.header);
  if (result != TC_TLV_OK) return result;
  if (!root.header.constructed || !tc_pki_tag(&root,tag)) return TC_TLV_INVALID;
  if (tc_x509_path_charge(work,encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_walk(encoded.data,encoded.length,profile,limits,frames,frame_capacity,
      tc_pki_children_visit,&state);
  if (result != TC_TLV_OK) return result;
  if (state.roots != 1) return TC_TLV_INVALID;
  if (state.overflow) return TC_TLV_LIMIT;
  *count = state.count;
  return TC_TLV_OK;
}
#endif
