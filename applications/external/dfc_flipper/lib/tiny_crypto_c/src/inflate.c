/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "inflate_internal.h"
#if TC_ENABLE_GZIP
#include <string.h>

enum { END_OF_BLOCK = 256, FIRST_LENGTH = 257, LAST_LENGTH = 285, LAST_DISTANCE = 29 };

static TC_GZIP_result reserve(tc_inflate_bits* bits, tc_inflate_output* output, size_t length)
{
  if (length > output->buffer.capacity - output->length || *bits->work < length)
    return TC_GZIP_LIMIT;
  *bits->work -= length;
  return TC_GZIP_OK;
}

static TC_GZIP_result stored(tc_inflate_bits* bits, tc_inflate_output* output)
{
  unsigned length, complement;
  if (bits->bit) {
    ++bits->offset;
    bits->bit = 0;
  }
  TC_GZIP_result result = tc_inflate_bits_read(bits, 16, &length);
  if (result != TC_GZIP_OK)
    return result;
  result = tc_inflate_bits_read(bits, 16, &complement);
  if (result != TC_GZIP_OK)
    return result;
  if ((length ^ complement) != 0xffffu || length > bits->input.length - bits->offset)
    return TC_GZIP_INVALID;
  result = reserve(bits, output, length);
  if (result != TC_GZIP_OK)
    return result;
  if (length)
    memcpy(output->buffer.data + output->length, bits->input.data + bits->offset, length);
  bits->offset += length;
  output->length += length;
  return TC_GZIP_OK;
}

static TC_GZIP_result compressed(tc_inflate_bits* bits, const TC_GZIP_workspace* tables,
                                 tc_inflate_output* output, size_t history_start)
{
  for (;;) {
    unsigned symbol;
    TC_GZIP_result result = tc_inflate_symbol(bits, &tables->literal, &symbol);
    if (result != TC_GZIP_OK)
      return result;
    if (symbol == END_OF_BLOCK)
      return TC_GZIP_OK;
    if (symbol < END_OF_BLOCK) {
      result = reserve(bits, output, 1);
      if (result != TC_GZIP_OK)
        return result;
      output->buffer.data[output->length++] = (uint8_t)symbol;
      continue;
    }
    if (symbol > LAST_LENGTH)
      return TC_GZIP_INVALID;
    unsigned length, extra, distance;
    const unsigned index = symbol - FIRST_LENGTH;
    /* RFC 1951 length groups contain four bases per extra-bit width. */
    if (index < 8)
      length = index + 3;
    else if (symbol == LAST_LENGTH)
      length = 258;
    else {
      const unsigned width = index / 4 - 1;
      result = tc_inflate_bits_read(bits, width, &extra);
      if (result != TC_GZIP_OK)
        return result;
      length = ((4u + index % 4) << width) + 3 + extra;
    }
    result = tc_inflate_symbol(bits, &tables->distance, &symbol);
    if (result != TC_GZIP_OK)
      return result;
    if (symbol > LAST_DISTANCE)
      return TC_GZIP_INVALID;
    if (symbol < 4)
      distance = symbol + 1;
    else {
      const unsigned width = symbol / 2 - 1;
      result = tc_inflate_bits_read(bits, width, &extra);
      if (result != TC_GZIP_OK)
        return result;
      distance = ((2u + symbol % 2) << width) + 1 + extra;
    }
    if (distance > output->length - history_start)
      return TC_GZIP_INVALID;
    result = reserve(bits, output, length);
    if (result != TC_GZIP_OK)
      return result;
    /* Forward copying expands overlapping references such as distance one. */
    for (unsigned i = 0; i < length; ++i) {
      output->buffer.data[output->length] = output->buffer.data[output->length - distance];
      ++output->length;
    }
  }
}

TC_GZIP_result tc_inflate_decode(tc_inflate_bits* bits, TC_GZIP_workspace* tables,
                                 tc_inflate_output* output)
{
  const size_t history_start = output->length;
  unsigned final;
  do {
    unsigned type;
    TC_GZIP_result result = tc_inflate_bits_read(bits, 1, &final);
    if (result != TC_GZIP_OK)
      return result;
    result = tc_inflate_bits_read(bits, 2, &type);
    if (result != TC_GZIP_OK)
      return result;
    if (!type)
      result = stored(bits, output);
    else if (type == 3)
      return TC_GZIP_INVALID;
    else {
      result = tc_inflate_tables_read(bits, type, tables);
      if (result == TC_GZIP_OK)
        result = compressed(bits, tables, output, history_start);
    }
    if (result != TC_GZIP_OK)
      return result;
  } while (!final);
  if (bits->bit) {
    ++bits->offset;
    bits->bit = 0;
  }
  return TC_GZIP_OK;
}
#endif
