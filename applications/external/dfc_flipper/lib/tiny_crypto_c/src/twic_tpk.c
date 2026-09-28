/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/twic_tpk.h>
#include <tiny_crypto/common.h>
#if TC_ENABLE_TWIC_TPK
#include "internal.h"
#include <string.h>

enum { CONTAINER_BYTES = 44, FIELD_COUNT = 3, AES128_ECB = 8 };

static TC_TLV_result fields_read(TC_bytes input, TC_TWIC_tpk* out)
{
  const TC_TLV_limits limits = {CONTAINER_BYTES,CONTAINER_BYTES,FIELD_COUNT,1};
  TC_TLV_element fields[FIELD_COUNT];
  TC_TLV_reader reader;
  TC_TLV_result result = TC_TLV_reader_init(&reader,input.data,input.length,TC_TLV_ISO7816,&limits);
  if (result != TC_TLV_OK) return result;
  for (size_t i = 0; i < FIELD_COUNT; ++i) {
    result = TC_TLV_next(&reader,&fields[i]);
    if (result != TC_TLV_OK) return result == TC_TLV_END || result == TC_TLV_MORE ? TC_TLV_INVALID : result;
    if (fields[i].encoded.data[0] != 0xc0 + i) return TC_TLV_INVALID;
  }
  if (reader.offset != reader.input.length) return TC_TLV_INVALID;
  if (fields[1].value.length != 1 || fields[2].value.length != 1 || fields[2].value.data[0])
    return TC_TLV_INVALID;
  if (fields[1].value.data[0] != AES128_ECB || fields[0].value.length != TC_TWIC_TPK_BYTES)
    return TC_TLV_UNSUPPORTED;
  memcpy(out->key,fields[0].value.data,TC_TWIC_TPK_BYTES);
  return TC_TLV_OK;
}

static TC_TLV_result container_read(TC_bytes input, TC_TWIC_tpk* out)
{
  static const uint8_t tag[] = {0xdf,0xc1,1};
  const TC_TLV_limits limits = {CONTAINER_BYTES,CONTAINER_BYTES,FIELD_COUNT,1};
  TC_TLV_element container;
  if (input.length < sizeof tag || memcmp(input.data,tag,sizeof tag)) return TC_TLV_INVALID;
  TC_TLV_result result = TC_TLV_read(input.data,input.length,TC_TLV_ISO7816,&limits,&container);
  if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  if (container.encoded.length != input.length) return TC_TLV_INVALID;
  return fields_read(container.value,out);
}

static int nibble(uint8_t byte)
{
  if (byte >= '0' && byte <= '9') return byte - '0';
  if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
  if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
  return -1;
}

TC_TLV_result TC_TWIC_tpk_read(TC_bytes input, TC_TWIC_tpk_encoding encoding, TC_TWIC_tpk* out)
{
  if (!out || (input.length && !input.data) ||
      !tc_internal_ranges_disjoint(input.data,input.length,out,sizeof *out) ||
      (encoding != TC_TWIC_TPK_CARD && encoding != TC_TWIC_TPK_BARCODE_HEX &&
       encoding != TC_TWIC_TPK_CONTENTS)) return TC_TLV_ARGUMENT;
  if (encoding == TC_TWIC_TPK_CONTENTS) return fields_read(input,out);
  if (encoding == TC_TWIC_TPK_CARD) return container_read(input,out);
  if (input.length % 2 || input.length > CONTAINER_BYTES * 2) return TC_TLV_INVALID;
  uint8_t decoded[CONTAINER_BYTES] = {0};
  TC_TLV_result result = TC_TLV_INVALID;
  const size_t length = input.length / 2;
  for (size_t i = 0; i < length; ++i) {
    const int high = nibble(input.data[2 * i]), low = nibble(input.data[2 * i + 1]);
    if (high < 0 || low < 0) goto cleanup;
    decoded[i] = (uint8_t)(high * 16 + low);
  }
  /* The printed barcode example transposes the card-container prefix. */
  if (length >= 3 && decoded[0] == 0xdc && decoded[1] == 0xf1 && decoded[2] == 1) {
    decoded[0] = 0xdf; decoded[1] = 0xc1;
  }
  result = container_read((TC_bytes){decoded,length},out);
cleanup:
  TC_secure_zero(decoded,sizeof decoded);
  return result;
}
#endif
