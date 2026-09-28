/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/twic_uuid.h>
#if TC_ENABLE_TWIC_UUID
#include "pki_storage_internal.h"
#include <string.h>

enum { PREFIX_BYTES = 10, NUMBER_BYTES = 6, MAX_AGENCY = 9999,
  MAX_SYSTEM = 9999, MAX_CREDENTIAL = 999999 };
static const uint8_t prefix[PREFIX_BYTES] = {0x91,0xbe,0x20,0x94,0xf6,0xdc,0x53,0x49,0x80,0};
static const uint64_t maximum_number = UINT64_C(99999999999999);

TC_TLV_result TC_TWIC_uuid_read(TC_bytes encoded, uint64_t* number)
{
  TC_bytes input, output;
  if (!number || tc_pki_storage_span(encoded.data,encoded.length,1,&input) != TC_TLV_OK ||
      tc_pki_storage_span(number,1,sizeof *number,&output) != TC_TLV_OK ||
      !tc_internal_ranges_disjoint(input.data,input.length,output.data,output.length))
    return TC_TLV_ARGUMENT;
  if (encoded.length != TC_TWIC_UUID_BYTES || memcmp(encoded.data,prefix,sizeof prefix)) return TC_TLV_INVALID;
  uint64_t parsed = 0;
  for (size_t i = PREFIX_BYTES; i < TC_TWIC_UUID_BYTES; ++i) parsed = (parsed << 8) | encoded.data[i];
  if (parsed > maximum_number) return TC_TLV_INVALID;
  *number = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_TWIC_uuid_write(uint64_t number, uint8_t* out, size_t capacity)
{
  TC_bytes output;
  if (!out || tc_pki_storage_span(out,capacity,1,&output) != TC_TLV_OK) return TC_TLV_ARGUMENT;
  if (capacity < TC_TWIC_UUID_BYTES) return TC_TLV_LIMIT;
  if (number > maximum_number) return TC_TLV_INVALID;
  memcpy(out,prefix,sizeof prefix);
  for (size_t i = 0; i < NUMBER_BYTES; ++i) {
    out[TC_TWIC_UUID_BYTES - 1 - i] = (uint8_t)number;
    number >>= 8;
  }
  return TC_TLV_OK;
}

TC_TLV_result TC_TWIC_uuid_match(TC_bytes encoded, const TC_FASCN* fascn, int* matched)
{
  if (!fascn || !matched ||
      !tc_internal_ranges_disjoint(encoded.data,encoded.length,matched,sizeof *matched) ||
      !tc_internal_ranges_disjoint(fascn,sizeof *fascn,matched,sizeof *matched)) return TC_TLV_ARGUMENT;
  if (fascn->agency > MAX_AGENCY || fascn->system > MAX_SYSTEM || fascn->credential > MAX_CREDENTIAL)
    return TC_TLV_INVALID;
  uint64_t number;
  TC_TLV_result result = TC_TWIC_uuid_read(encoded,&number);
  if (result != TC_TLV_OK) return result;
  const uint64_t expected = ((uint64_t)fascn->agency * (MAX_SYSTEM + 1) + fascn->system) *
      (MAX_CREDENTIAL + 1) + fascn->credential;
  *matched = number == expected;
  return TC_TLV_OK;
}
#endif
