/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/aamva.h>
#if TC_ENABLE_AAMVA
#include "internal.h"
#include <string.h>

enum { HEADER_BYTES = 21, ENTRY_BYTES = 10, TYPE_BYTES = 2, OFFSET_DIGITS = 4 };

static int lookup_storage(TC_bytes input, const char* identifier, size_t length, TC_bytes* out)
{
  if (!out || !identifier || (input.length && !input.data) ||
      !tc_internal_ranges_disjoint(input.data,input.length,out,sizeof *out) ||
      !tc_internal_ranges_disjoint(identifier,length,out,sizeof *out)) return 0;
  for (size_t i = 0; i < length; ++i)
    if (identifier[i] < 'A' || identifier[i] > 'Z') return 0;
  return 1;
}

static int decimal(const uint8_t* bytes, size_t length, size_t* out)
{
  size_t value = 0;
  for (size_t i = 0; i < length; ++i) {
    if (bytes[i] < '0' || bytes[i] > '9') return 0;
    const unsigned digit = bytes[i] - '0';
    if (value > (SIZE_MAX - digit) / 10) return 0;
    value = value * 10 + digit;
  }
  *out = value;
  return 1;
}

TC_TLV_result TC_AAMVA_subfile_find(TC_bytes encoded, const char designator[2], TC_bytes* out)
{
  static const uint8_t prefix[] = {'@',10,30,13,'A','N','S','I',' '};
  size_t version, count, ignored;
  TC_bytes found = {NULL,0};
  if (!lookup_storage(encoded,designator,TYPE_BYTES,out)) return TC_TLV_ARGUMENT;
  if (encoded.length < HEADER_BYTES || memcmp(encoded.data,prefix,sizeof prefix)) return TC_TLV_INVALID;
  /* Validate issuer digits individually so six digits fit on 16-bit targets. */
  for (size_t i = 9; i < 15; ++i)
    if (!decimal(encoded.data + i,1,&ignored)) return TC_TLV_INVALID;
  if (!decimal(encoded.data + 15,2,&version) || !decimal(encoded.data + 17,2,&ignored) ||
      !decimal(encoded.data + 19,2,&count) || !count) return TC_TLV_INVALID;
  if (!version) return TC_TLV_UNSUPPORTED;
  const size_t directory_end = HEADER_BYTES + count * ENTRY_BYTES;
  if (directory_end > encoded.length) return TC_TLV_INVALID;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* entry = encoded.data + HEADER_BYTES + i * ENTRY_BYTES;
    size_t offset, length;
    for (size_t j = 0; j < TYPE_BYTES; ++j)
      if (entry[j] < 'A' || entry[j] > 'Z') return TC_TLV_INVALID;
    if (!decimal(entry + TYPE_BYTES,OFFSET_DIGITS,&offset) ||
        !decimal(entry + TYPE_BYTES + OFFSET_DIGITS,OFFSET_DIGITS,&length) ||
        offset < directory_end || offset > encoded.length || length < TYPE_BYTES + 1 ||
        length > encoded.length - offset || memcmp(entry,encoded.data + offset,TYPE_BYTES) ||
        encoded.data[offset + length - 1] != 13) return TC_TLV_INVALID;
    for (size_t j = 0; j < i; ++j) {
      const uint8_t* prior = encoded.data + HEADER_BYTES + j * ENTRY_BYTES;
      size_t previous_offset, previous_length;
      if (!decimal(prior + TYPE_BYTES,OFFSET_DIGITS,&previous_offset) ||
          !decimal(prior + TYPE_BYTES + OFFSET_DIGITS,OFFSET_DIGITS,&previous_length)) return TC_TLV_INVALID;
      if (!memcmp(entry,prior,TYPE_BYTES) ||
          (offset < previous_offset + previous_length && previous_offset < offset + length)) return TC_TLV_INVALID;
    }
    if (!memcmp(entry,designator,TYPE_BYTES)) found = (TC_bytes){encoded.data + offset,length};
  }
  if (!found.data) return TC_TLV_END;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result TC_AAMVA_field_find(TC_bytes subfile, const char identifier[3], TC_bytes* out)
{
  enum { IDENTIFIER_BYTES = 3, LF = 10, CR = 13 };
  TC_bytes found = {NULL,0};
  if (!lookup_storage(subfile,identifier,IDENTIFIER_BYTES,out)) return TC_TLV_ARGUMENT;
  if (subfile.length < TYPE_BYTES + 1 || subfile.data[subfile.length - 1] != CR) return TC_TLV_INVALID;
  for (size_t i = 0; i < TYPE_BYTES; ++i)
    if (subfile.data[i] < 'A' || subfile.data[i] > 'Z') return TC_TLV_INVALID;
  const size_t end = subfile.length - 1;
  size_t offset = TYPE_BYTES;
  if (offset < end && subfile.data[offset] == LF) ++offset;
  while (offset < end) {
    if (end - offset < IDENTIFIER_BYTES) return TC_TLV_INVALID;
    const uint8_t* field = subfile.data + offset;
    for (size_t i = 0; i < IDENTIFIER_BYTES; ++i)
      if (field[i] < 'A' || field[i] > 'Z') return TC_TLV_INVALID;
    offset += IDENTIFIER_BYTES;
    const size_t start = offset;
    while (offset < end && subfile.data[offset] != LF) {
      if (subfile.data[offset] < 0x20 || subfile.data[offset] > 0x7e) return TC_TLV_INVALID;
      ++offset;
    }
    if (!memcmp(field,identifier,IDENTIFIER_BYTES)) {
      if (found.data) return TC_TLV_INVALID;
      found = (TC_bytes){subfile.data + start,offset - start};
    }
    if (offset < end) ++offset;
  }
  if (!found.data) return TC_TLV_END;
  *out = found;
  return TC_TLV_OK;
}
#endif
