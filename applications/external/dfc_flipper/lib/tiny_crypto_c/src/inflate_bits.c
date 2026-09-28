/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "inflate_internal.h"
#if TC_ENABLE_GZIP

TC_TLV_result tc_inflate_bits_read(tc_inflate_bits* bits, unsigned count, unsigned* out)
{
  unsigned value = 0, shift = 0;
  if (!bits || !out || !bits->work || count > 16 || bits->bit > 7 ||
      bits->offset > bits->input.length || (!bits->input.data && bits->input.length))
    return TC_TLV_ARGUMENT;
  if (*bits->work < count) return TC_TLV_LIMIT;
  *bits->work -= count;
  while (count) {
    if (bits->offset == bits->input.length) return TC_TLV_INVALID;
    unsigned take = 8 - bits->bit;
    if (take > count) take = count;
    const unsigned mask = (1u << take) - 1;
    value |= ((bits->input.data[bits->offset] >> bits->bit) & mask) << shift;
    bits->bit += take;
    if (bits->bit == 8) { bits->bit = 0; ++bits->offset; }
    shift += take;
    count -= take;
  }
  *out = value;
  return TC_TLV_OK;
}

TC_TLV_result tc_inflate_symbol(tc_inflate_bits* bits, const tc_inflate_tree* tree, unsigned* out)
{
  unsigned code = 0, first = 0, index = 0;
  if (!tree || !tree->symbols || !out) return TC_TLV_ARGUMENT;
  /* Canonical codes of each length form a consecutive interval. */
  for (unsigned length = 1; length <= TC_INFLATE_CODE_BITS; ++length) {
    unsigned bit;
    TC_TLV_result result = tc_inflate_bits_read(bits,1,&bit);
    if (result != TC_TLV_OK) return result;
    code = (code << 1) | bit;
    if (code >= first && code - first < tree->counts[length]) {
      *out = tree->symbols[index + code - first];
      return TC_TLV_OK;
    }
    index += tree->counts[length];
    first = (first + tree->counts[length]) << 1;
  }
  return TC_TLV_INVALID;
}
#endif
