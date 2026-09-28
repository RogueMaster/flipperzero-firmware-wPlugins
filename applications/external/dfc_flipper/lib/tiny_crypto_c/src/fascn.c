/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/fascn.h>
#if TC_ENABLE_FASCN
#include "internal.h"
#include <string.h>

enum { CHARACTER_BITS = 5, CHARACTERS = 40, FIELDS = 9,
  START = 0x0b, SEPARATOR = 0x0d, END = 0x0f, END_POSITION = 38, LRC_POSITION = 39 };
static const struct { uint8_t first, digits; } fields[FIELDS] = {
  {1,4},{6,4},{11,6},{18,1},{20,1},{22,10},{32,1},{33,4},{37,1}
};

static unsigned character(const uint8_t* encoded, size_t index, unsigned* parity)
{
  unsigned value = 0, odd = 0;
  for (unsigned bit = 0; bit < CHARACTER_BITS; ++bit) {
    const size_t position = index * CHARACTER_BITS + bit;
    const unsigned set = (encoded[position / 8] >> (7 - position % 8)) & 1u;
    odd ^= set;
    if (bit < CHARACTER_BITS - 1) value |= set << bit;
  }
  *parity = odd;
  return value;
}

static void put_character(uint8_t* encoded, size_t index, unsigned value)
{
  unsigned parity = 1;
  for (unsigned bit = 0; bit < CHARACTER_BITS; ++bit) {
    const size_t position = index * CHARACTER_BITS + bit;
    const unsigned set = bit < CHARACTER_BITS - 1 ? (value >> bit) & 1u : parity;
    parity ^= set;
    encoded[position / 8] |= (uint8_t)(set << (7 - position % 8));
  }
}

static unsigned delimiter(size_t position)
{
  if (!position) return START;
  if (position == END_POSITION) return END;
  return SEPARATOR;
}

TC_TLV_result TC_FASCN_read(TC_bytes encoded, TC_FASCN* out)
{
  uint64_t values[FIELDS] = {0};
  unsigned checksum = 0;
  size_t field = 0;
  if (!out || (!encoded.data && encoded.length) ||
      !tc_internal_ranges_disjoint(encoded.data,encoded.length,out,sizeof *out)) return TC_TLV_ARGUMENT;
  if (encoded.length != TC_FASCN_BYTES) return TC_TLV_INVALID;
  for (size_t i = 0; i < CHARACTERS; ++i) {
    unsigned parity;
    const unsigned value = character(encoded.data,i,&parity);
    if (!parity) return TC_TLV_INVALID;
    checksum ^= value;
    if (i == LRC_POSITION) break;
    while (field < FIELDS && i >= (size_t)fields[field].first + fields[field].digits) ++field;
    if (field < FIELDS && i >= fields[field].first) {
      if (value > 9) return TC_TLV_INVALID;
      values[field] = values[field] * 10 + value;
    } else if (value != delimiter(i)) return TC_TLV_INVALID;
  }
  if (checksum) return TC_TLV_INVALID;
  const TC_FASCN parsed = {values[5],(uint32_t)values[2],(uint16_t)values[0],
    (uint16_t)values[1],(uint16_t)values[7],(uint8_t)values[3],(uint8_t)values[4],
    (uint8_t)values[6],(uint8_t)values[8]};
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_FASCN_write(const TC_FASCN* value, uint8_t* out, size_t capacity)
{
  if (!value || !out || !tc_internal_ranges_disjoint(value,sizeof *value,out,capacity)) return TC_TLV_ARGUMENT;
  if (capacity < TC_FASCN_BYTES) return TC_TLV_LIMIT;
  uint64_t values[FIELDS] = {value->agency,value->system,value->credential,value->series,
    value->issue,value->person,value->category,value->organization,value->association};
  for (size_t i = 0; i < FIELDS; ++i) {
    uint64_t limit = 1;
    for (unsigned j = 0; j < fields[i].digits; ++j) limit *= 10;
    if (values[i] >= limit) return TC_TLV_INVALID;
  }
  /* All validation precedes output writes. Each character occupies five bits. */
  memset(out,0,TC_FASCN_BYTES);
  unsigned checksum = 0;
  size_t field = 0;
  for (size_t i = 0; i < LRC_POSITION; ++i) {
    while (field < FIELDS && i >= (size_t)fields[field].first + fields[field].digits) ++field;
    if (field < FIELDS && i >= fields[field].first) continue;
    const unsigned control = delimiter(i);
    put_character(out,i,control); checksum ^= control;
  }
  for (size_t i = 0; i < FIELDS; ++i) {
    for (size_t j = fields[i].digits; j; --j) {
      const unsigned digit = (unsigned)(values[i] % 10);
      values[i] /= 10;
      put_character(out,fields[i].first + j - 1,digit); checksum ^= digit;
    }
  }
  put_character(out,LRC_POSITION,checksum);
  return TC_TLV_OK;
}
#endif
