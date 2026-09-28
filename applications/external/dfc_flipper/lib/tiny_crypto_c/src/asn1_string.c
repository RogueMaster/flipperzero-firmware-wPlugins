/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/config.h>
#include "string_internal.h"
#if TC_ENABLE_X509
#include "internal.h"

static int string_tag(unsigned tag)
{
  return tag == 0x0c || tag == 0x13 || tag == 0x16 || tag == 0x1a || tag == 0x1c || tag == 0x1e;
}

static TC_TLV_result string_next(unsigned tag, TC_bytes input,
    size_t* offset, uint32_t* point)
{
  size_t i;
  uint32_t code;
  if (!offset || !point || (!input.data && input.length)
      || !tc_internal_ranges_disjoint(input.data, input.length, offset, sizeof(*offset))
      || !tc_internal_ranges_disjoint(input.data, input.length, point, sizeof(*point))
      || !tc_internal_ranges_disjoint(offset, sizeof(*offset), point, sizeof(*point)))
    return TC_TLV_ARGUMENT;
  if (*offset > input.length) return TC_TLV_ARGUMENT;
  if (!string_tag(tag)) return TC_TLV_INVALID;
  i = *offset;
  if (i == input.length) return TC_TLV_END;
  if (tag == 0x0c) {
    unsigned extra = 0, j;
    uint32_t minimum = 0;
    code = input.data[i++];
    if (code < 128) {}
    else if (code >= 0xc2 && code <= 0xdf) { extra = 1; minimum = 128; code &= 31; }
    else if (code >= 0xe0 && code <= 0xef) { extra = 2; minimum = 2048; code &= 15; }
    else if (code >= 0xf0 && code <= 0xf4) { extra = 3; minimum = 65536; code &= 7; }
    else return TC_TLV_INVALID;
    if (extra > input.length - i) return TC_TLV_MORE;
    for (j = 0; j < extra; ++j) {
      if ((input.data[i] & 0xc0) != 0x80) return TC_TLV_INVALID;
      code = code * 64 + (input.data[i++] & 63);
    }
    if (code < minimum) return TC_TLV_INVALID;
  } else if (tag == 0x1e || tag == 0x1c) {
    unsigned width = tag == 0x1e ? 2 : 4, j;
    if (width > input.length - i) return TC_TLV_MORE;
    code = 0;
    for (j = 0; j < width; ++j) code = code * 256 + input.data[i++];
  } else {
    code = input.data[i++];
    if (tag == 0x13) {
      if (!((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z')
          || (code >= '0' && code <= '9') || code == ' ' || code == '\''
          || code == '(' || code == ')' || code == '+' || code == ','
          || code == '-' || code == '.' || code == '/' || code == ':'
          || code == '=' || code == '?')) return TC_TLV_INVALID;
    } else if (tag == 0x1a) {
      if (code < 0x20 || code > 0x7e) return TC_TLV_INVALID;
    } else if (code > 127) return TC_TLV_INVALID;
  }
  if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return TC_TLV_INVALID;
  *offset = i;
  *point = code;
  return TC_TLV_OK;
}

TC_TLV_result tc_asn1_string_next(unsigned tag, TC_bytes input,
    size_t* offset, uint32_t* point)
{
  TC_TLV_result result = string_next(tag,input,offset,point);
  return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
}

TC_TLV_result tc_asn1_string_feed(tc_asn1_string_state* state, TC_bytes bytes,
    tc_asn1_string_consume consume, void* context)
{
  size_t offset = 0;
  if (!state || state->used >= sizeof state->pending || (!bytes.data && bytes.length))
    return TC_TLV_ARGUMENT;
  if (!string_tag(state->tag)) return TC_TLV_INVALID;
  while (offset < bytes.length) {
    uint32_t point;
    TC_TLV_result result;
    if (state->used) {
      size_t decoded = 0;
      state->pending[state->used++] = bytes.data[offset++];
      result = string_next(state->tag,(TC_bytes){state->pending,state->used},&decoded,&point);
      if (result == TC_TLV_MORE) {
        if (state->used == sizeof state->pending) return TC_TLV_INVALID;
        continue;
      }
      if (result != TC_TLV_OK) return result;
      state->used = 0;
    } else {
      result = string_next(state->tag,bytes,&offset,&point);
      if (result == TC_TLV_MORE) {
        state->used = bytes.length - offset;
        if (state->used >= sizeof state->pending) return TC_TLV_INVALID;
        memcpy(state->pending,bytes.data + offset,state->used);
        return TC_TLV_OK;
      }
      if (result != TC_TLV_OK) return result;
    }
    if (consume) {
      result = consume(context,point);
      if (result != TC_TLV_OK) return result;
    }
  }
  return TC_TLV_OK;
}
#endif
