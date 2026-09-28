/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "inflate_internal.h"
#if TC_ENABLE_GZIP

enum { GZIP_HEADER_BYTES = 10, GZIP_HEADER_CRC = 2, GZIP_EXTRA = 4,
  GZIP_NAME = 8, GZIP_COMMENT = 16, GZIP_RESERVED = 0xe0, GZIP_DEFLATE = 8 };

static TC_TLV_result crc32(TC_bytes input, size_t* work, uint32_t* out)
{
  const uint32_t polynomial = UINT32_C(0xedb88320);
  uint32_t crc = UINT32_MAX;
  if (*work < input.length) return TC_TLV_LIMIT;
  *work -= input.length;
  for (size_t i = 0; i < input.length; ++i) {
    crc ^= input.data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (polynomial & (UINT32_C(0) - (crc & 1)));
  }
  *out = ~crc;
  return TC_TLV_OK;
}

static TC_TLV_result skip(tc_inflate_bits* bits, size_t length)
{
  if (length > bits->input.length - bits->offset) return TC_TLV_INVALID;
  if (*bits->work < length) return TC_TLV_LIMIT;
  *bits->work -= length; bits->offset += length;
  return TC_TLV_OK;
}

static TC_TLV_result header(tc_inflate_bits* bits)
{
  const size_t start = bits->offset;
  TC_TLV_result result = skip(bits,GZIP_HEADER_BYTES);
  if (result != TC_TLV_OK) return result;
  const uint8_t* bytes = bits->input.data + start;
  const unsigned flags = bytes[3];
  if (bytes[0] != 0x1f || bytes[1] != 0x8b || (flags & GZIP_RESERVED)) return TC_TLV_INVALID;
  if (bytes[2] != GZIP_DEFLATE) return TC_TLV_UNSUPPORTED;
  if (flags & GZIP_EXTRA) {
    unsigned length;
    result = tc_inflate_bits_read(bits,16,&length);
    if (result != TC_TLV_OK) return result;
    result = skip(bits,length);
    if (result != TC_TLV_OK) return result;
  }
  const unsigned strings[] = {GZIP_NAME,GZIP_COMMENT};
  for (size_t i = 0; i < sizeof strings / sizeof *strings; ++i) {
    if (flags & strings[i]) {
      unsigned byte;
      do {
        result = tc_inflate_bits_read(bits,8,&byte);
        if (result != TC_TLV_OK) return result;
      } while (byte);
    }
  }
  if (flags & GZIP_HEADER_CRC) {
    uint32_t actual;
    const TC_bytes covered = {bytes,bits->offset - start};
    result = crc32(covered,bits->work,&actual);
    if (result != TC_TLV_OK) return result;
    unsigned expected;
    result = tc_inflate_bits_read(bits,16,&expected);
    if (result != TC_TLV_OK) return result;
    if ((actual & UINT32_C(0xffff)) != expected) return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

static TC_TLV_result little32(tc_inflate_bits* bits, uint32_t* out)
{
  unsigned low, high;
  TC_TLV_result result = tc_inflate_bits_read(bits,16,&low);
  if (result != TC_TLV_OK) return result;
  result = tc_inflate_bits_read(bits,16,&high);
  if (result == TC_TLV_OK) *out = (uint32_t)low | ((uint32_t)high << 16);
  return result;
}

TC_TLV_result tc_gzip_decode(TC_bytes input, tc_inflate_tables* tables,
    tc_inflate_output* output, size_t* work)
{
  if (!tables || !output || !work || (!input.data && input.length) ||
      output->length > output->capacity || (!output->data && output->capacity)) return TC_TLV_ARGUMENT;
  if (!input.length) return TC_TLV_INVALID;
  tc_inflate_bits bits = {input,0,0,work};
  while (bits.offset < input.length) {
    const size_t start = output->length;
    TC_TLV_result result = header(&bits);
    if (result != TC_TLV_OK) return result;
    result = tc_inflate_decode(&bits,tables,output);
    if (result != TC_TLV_OK) return result;
    uint32_t expected_crc, expected_size, actual_crc;
    result = little32(&bits,&expected_crc);
    if (result != TC_TLV_OK) return result;
    result = little32(&bits,&expected_size);
    if (result != TC_TLV_OK) return result;
    const TC_bytes covered = {output->data ? output->data + start : NULL,output->length - start};
    if ((uint32_t)covered.length != expected_size) return TC_TLV_INVALID;
    result = crc32(covered,work,&actual_crc);
    if (result != TC_TLV_OK) return result;
    if (actual_crc != expected_crc) return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}
#endif
