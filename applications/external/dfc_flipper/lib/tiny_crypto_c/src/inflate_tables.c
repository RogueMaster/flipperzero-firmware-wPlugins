/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "inflate_internal.h"
#if TC_ENABLE_GZIP
#include <string.h>

static TC_TLV_result dynamic_lengths(tc_inflate_bits* bits, tc_inflate_tables* tables,
    unsigned* literal_count, unsigned* distance_count)
{
  static const uint8_t order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
  unsigned code_count;
  TC_TLV_result result = tc_inflate_bits_read(bits,5,literal_count);
  if (result != TC_TLV_OK) return result;
  result = tc_inflate_bits_read(bits,5,distance_count);
  if (result != TC_TLV_OK) return result;
  result = tc_inflate_bits_read(bits,4,&code_count);
  if (result != TC_TLV_OK) return result;
  *literal_count += 257; ++*distance_count; code_count += 4;
  if (*literal_count > 286) return TC_TLV_INVALID;
  if (*bits->work < sizeof order) return TC_TLV_LIMIT;
  *bits->work -= sizeof order;
  memset(tables->lengths,0,sizeof order);
  for (unsigned i = 0; i < code_count; ++i) {
    unsigned length;
    result = tc_inflate_bits_read(bits,3,&length);
    if (result != TC_TLV_OK) return result;
    tables->lengths[order[i]] = (uint8_t)length;
  }
  /* Reuse the literal table while decoding the two final length sequences. */
  result = tc_inflate_tree_build(tables->lengths,sizeof order,TC_INFLATE_COMPLETE_TREE,
      &tables->literal,bits->work);
  if (result != TC_TLV_OK) return result;
  const unsigned total = *literal_count + *distance_count;
  unsigned used = 0;
  while (used < total) {
    unsigned symbol, repeat = 1, value;
    result = tc_inflate_symbol(bits,&tables->literal,&symbol);
    if (result != TC_TLV_OK) return result;
    if (symbol <= 15) value = symbol;
    else {
      unsigned extra;
      if (symbol == 16 && !used) return TC_TLV_INVALID;
      if (symbol > 18) return TC_TLV_INVALID;
      result = tc_inflate_bits_read(bits,symbol == 16 ? 2 : symbol == 17 ? 3 : 7,&extra);
      if (result != TC_TLV_OK) return result;
      repeat = extra + (symbol == 18 ? 11 : 3);
      value = symbol == 16 ? tables->lengths[used - 1] : 0;
    }
    if (repeat > total - used) return TC_TLV_INVALID;
    if (*bits->work < repeat) return TC_TLV_LIMIT;
    *bits->work -= repeat;
    memset(tables->lengths + used,(int)value,repeat);
    used += repeat;
  }
  return tables->lengths[256] ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result tc_inflate_tables_read(tc_inflate_bits* bits, unsigned type, tc_inflate_tables* tables)
{
  unsigned literals = TC_INFLATE_LITERAL_CODES, distances = TC_INFLATE_DISTANCE_CODES;
  if (!bits || !bits->work || !tables || (type != 1 && type != 2)) return TC_TLV_ARGUMENT;
  tables->literal.symbols = tables->literal_symbols;
  tables->distance.symbols = tables->distance_symbols;
  TC_TLV_result result;
  if (type == 1) {
    if (*bits->work < sizeof tables->lengths) return TC_TLV_LIMIT;
    *bits->work -= sizeof tables->lengths;
    for (unsigned i = 0; i < literals; ++i)
      tables->lengths[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
    memset(tables->lengths + literals,5,distances);
  } else {
    result = dynamic_lengths(bits,tables,&literals,&distances);
    if (result != TC_TLV_OK) return result;
  }
  result = tc_inflate_tree_build(tables->lengths,literals,TC_INFLATE_LITERAL_TREE,
      &tables->literal,bits->work);
  if (result != TC_TLV_OK) return result;
  return tc_inflate_tree_build(tables->lengths + literals,distances,TC_INFLATE_DISTANCE_TREE,
      &tables->distance,bits->work);
}
#endif
