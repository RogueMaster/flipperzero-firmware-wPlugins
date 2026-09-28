/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "inflate_internal.h"
#if TC_ENABLE_GZIP
#include <string.h>

TC_TLV_result tc_inflate_tree_build(const uint8_t* lengths, size_t count,
    tc_inflate_tree_kind kind, tc_inflate_tree* tree, size_t* work)
{
  uint16_t offsets[TC_INFLATE_CODE_BITS + 1] = {0};
  unsigned remaining = 1, symbols = 0;
  if (!lengths || !tree || !tree->symbols || !work || !count ||
      count > TC_INFLATE_LITERAL_CODES ||
      (kind != TC_INFLATE_COMPLETE_TREE && kind != TC_INFLATE_LITERAL_TREE &&
       kind != TC_INFLATE_DISTANCE_TREE)) return TC_TLV_ARGUMENT;
  const size_t cost = 2 * count + TC_INFLATE_CODE_BITS;
  if (*work < cost) return TC_TLV_LIMIT;
  *work -= cost;
  memset(tree->counts,0,sizeof tree->counts);
  for (size_t i = 0; i < count; ++i) {
    if (lengths[i] > TC_INFLATE_CODE_BITS) return TC_TLV_INVALID;
    if (lengths[i]) { ++tree->counts[lengths[i]]; ++symbols; }
  }
  /* Each level doubles available prefixes before assigning codes at that level. */
  for (unsigned bits = 1; bits <= TC_INFLATE_CODE_BITS; ++bits) {
    remaining <<= 1;
    if (tree->counts[bits] > remaining) return TC_TLV_INVALID;
    remaining -= tree->counts[bits];
    if (bits < TC_INFLATE_CODE_BITS)
      offsets[bits + 1] = (uint16_t)(offsets[bits] + tree->counts[bits]);
  }
  /* A literal-only block can omit its distance alphabet. A lone code uses one bit. */
  if (remaining && !(kind == TC_INFLATE_DISTANCE_TREE && !symbols) &&
      !(kind != TC_INFLATE_COMPLETE_TREE && symbols == 1 && tree->counts[1] == 1))
    return TC_TLV_INVALID;
  for (size_t i = 0; i < count; ++i)
    if (lengths[i]) tree->symbols[offsets[lengths[i]]++] = (uint16_t)i;
  return TC_TLV_OK;
}
#endif
