/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV
#include "tlv_internal.h"
#include <string.h>

TC_TLV_result tc_tlv_config(TC_TLV_profile profile, const TC_TLV_limits* limits)
{
  if (!limits || profile < TC_TLV_DER || profile > TC_TLV_ISO7816_PAD_ZERO_FF)
    return TC_TLV_ARGUMENT;
#if !TC_TLV_ENABLE_BER
  if (profile == TC_TLV_BER) return TC_TLV_UNSUPPORTED;
#endif
  return TC_TLV_OK;
}

int tc_tlv_padding(TC_TLV_profile profile, uint8_t byte)
{
  return ((profile == TC_TLV_ISO7816_PAD_ZERO ||
           profile == TC_TLV_ISO7816_PAD_ZERO_FF) && byte == 0) ||
         (profile == TC_TLV_ISO7816_PAD_ZERO_FF && byte == 0xff);
}

TC_TLV_result tc_tlv_header_read(const uint8_t* data, size_t length,
    TC_TLV_profile profile, size_t length_octets, uint64_t max_value,
    tc_tlv_wide_header* out)
{
  tc_tlv_wide_header h;
  size_t p = 0, n, i;
  uint8_t b;
  int iso = profile != TC_TLV_DER && profile != TC_TLV_BER;
  if (!out || (!data && length)) return TC_TLV_ARGUMENT;
  if (!length_octets || length_octets > sizeof(uint64_t)) return TC_TLV_ARGUMENT;
  if (!length) return TC_TLV_MORE;
  memset(&h, 0, sizeof h);
  b = data[p++];
  if (!b || (iso && b == 0xff)) return TC_TLV_INVALID;
  h.tag_class = (uint8_t)(b >> 6);
  h.constructed = (uint8_t)((b >> 5) & 1);
  h.number = b & 31;
  if (h.number == 31) {
    h.number = 0;
    do {
      if (p == length) return TC_TLV_MORE;
      if (p == TC_TLV_TAG_BYTES || (iso && p == 3)) return TC_TLV_LIMIT;
      b = data[p++];
      if (p == 2 && !(b & 127)) return TC_TLV_INVALID;
      if (h.number > (UINT32_MAX - (uint32_t)(b & 127)) / 128)
        return TC_TLV_LIMIT;
      h.number = h.number * 128 + (uint32_t)(b & 127);
    } while (b & 128);
    if (h.number < 31) return TC_TLV_INVALID;
  }
  /* Universal tag zero is EOC, handled only by the BER traversal engine. */
  if (!h.tag_class && !h.number) return TC_TLV_INVALID;
  h.tag_length = (uint8_t)p;
  memcpy(h.tag, data, p);
  if (p == length) return TC_TLV_MORE;
  b = data[p++];
  if (b == 0xff) return TC_TLV_INVALID;
  if (b == 0x80) {
#if TC_TLV_ENABLE_BER
    if (profile != TC_TLV_BER || !h.constructed) return TC_TLV_INVALID;
    h.indefinite = 1;
#else
    return TC_TLV_INVALID;
#endif
  } else if (b & 128) {
    n = b & 127;
    /* Limit work before reading length octets. BER/ISO allow non-shortest
     * lengths, but this implementation still caps the encoded width. */
    if (n > length_octets || (iso && n > 4)) return TC_TLV_LIMIT;
    if (n > length - p) return TC_TLV_MORE;
    if (profile == TC_TLV_DER && data[p] == 0) return TC_TLV_INVALID;
    for (i = 0; i < n; ++i) {
      if (h.length > (UINT64_MAX - data[p]) / 256) return TC_TLV_LIMIT;
      h.length = h.length * 256 + data[p++];
    }
    if (profile == TC_TLV_DER && h.length < 128) return TC_TLV_INVALID;
  } else h.length = b;
  if (h.length > max_value) return TC_TLV_LIMIT;
  h.header_length = (uint8_t)p;
  *out = h;
  return TC_TLV_OK;
}

TC_TLV_result TC_TLV_header_read(const uint8_t* data, size_t length,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_header* out)
{
  tc_tlv_wide_header wide;
  TC_TLV_result result = tc_tlv_config(profile,limits);
  if (result != TC_TLV_OK) return result;
  if (!out || (!data && length)) return TC_TLV_ARGUMENT;
  if (length > limits->max_input) return TC_TLV_LIMIT;
  result = tc_tlv_header_read(data,length,profile,sizeof(size_t),limits->max_value,&wide);
  if (result != TC_TLV_OK) return result;
  TC_TLV_header parsed;
  memset(&parsed,0,sizeof parsed);
  parsed.length = (size_t)wide.length;
  parsed.number = wide.number;
  memcpy(parsed.tag,wide.tag,sizeof parsed.tag);
  parsed.tag_length = wide.tag_length;
  parsed.header_length = wide.header_length;
  parsed.tag_class = wide.tag_class;
  parsed.constructed = wide.constructed;
  parsed.indefinite = wide.indefinite;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_TLV_read(const uint8_t* data, size_t length,
    TC_TLV_profile profile, const TC_TLV_limits* limits, TC_TLV_element* out)
{
  TC_TLV_element e;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_TLV_header_read(data, length, profile, limits, &e.header);
  if (result != TC_TLV_OK) return result;
  if (e.header.indefinite) return TC_TLV_UNSUPPORTED;
  /* Subtract only after header_read proved that the header fits. Computing
   * header + value first could wrap before the bounds check on small MCUs. */
  if (e.header.length > length - e.header.header_length) return TC_TLV_MORE;
  e.encoded.data = data;
  e.encoded.length = e.header.header_length + e.header.length;
  e.value.data = data + e.header.header_length;
  e.value.length = e.header.length;
  *out = e;
  return TC_TLV_OK;
}

TC_TLV_result TC_TLV_reader_init(TC_TLV_reader* reader,
    const uint8_t* data, size_t length, TC_TLV_profile profile,
    const TC_TLV_limits* limits)
{
  TC_TLV_reader r;
  TC_TLV_result result = tc_tlv_config(profile, limits);
  if (result != TC_TLV_OK) return result;
  if (!reader || (!data && length)) return TC_TLV_ARGUMENT;
  if (length > limits->max_input) return TC_TLV_LIMIT;
  memset(&r, 0, sizeof r);
  r.input.data = data; r.input.length = length;
  r.profile = profile; r.limits = *limits;
  *reader = r;
  return TC_TLV_OK;
}

TC_TLV_result TC_TLV_next(TC_TLV_reader* reader, TC_TLV_element* out)
{
  TC_TLV_element e;
  TC_TLV_result result;
  size_t p;
  if (!reader || !out || reader->offset > reader->input.length ||
      (!reader->input.data && reader->input.length)) return TC_TLV_ARGUMENT;
  p = reader->offset;
  while (p < reader->input.length && tc_tlv_padding(reader->profile, reader->input.data[p])) ++p;
  if (p == reader->input.length) { reader->offset = p; return TC_TLV_END; }
  if (reader->elements >= reader->limits.max_elements) return TC_TLV_LIMIT;
  result = TC_TLV_read(reader->input.data + p, reader->input.length - p,
                       reader->profile, &reader->limits, &e);
  if (result != TC_TLV_OK) return result;
  /* Commit together so callers can retry a short read without losing position. */
  reader->offset = p + e.encoded.length;
  ++reader->elements;
  *out = e;
  return TC_TLV_OK;
}
#endif
