/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/gzip.h>
#if TC_ENABLE_GZIP
#include "inflate_internal.h"
#include "internal.h"

TC_GZIP_result TC_GZIP_decode(const uint8_t* input, size_t input_length,
    uint8_t* output, size_t capacity, TC_GZIP_workspace* workspace,
    size_t* work, size_t* output_length)
{
  if ((!input && input_length) || (!output && capacity) || !workspace || !work || !output_length)
    return TC_GZIP_ARGUMENT;
  const struct { const void* data; size_t length; } storage[] = {
    {input,input_length},{output,capacity},{workspace,sizeof *workspace},
    {work,sizeof *work},{output_length,sizeof *output_length}
  };
  for (size_t i = 0; i < sizeof storage / sizeof *storage; ++i)
    for (size_t j = i + 1; j < sizeof storage / sizeof *storage; ++j)
      if (!tc_internal_ranges_disjoint(storage[i].data,storage[i].length,
          storage[j].data,storage[j].length)) return TC_GZIP_ARGUMENT;
  const TC_bytes encoded = {input,input_length};
  tc_inflate_output decoded = {output,capacity,0};
  TC_TLV_result result = tc_gzip_decode(encoded,workspace,&decoded,work);
  TC_secure_zero(workspace,sizeof *workspace);
  if (result == TC_TLV_OK) { *output_length = decoded.length; return TC_GZIP_OK; }
  if (capacity) TC_secure_zero(output,capacity);
  if (result == TC_TLV_LIMIT) return TC_GZIP_LIMIT;
  if (result == TC_TLV_UNSUPPORTED) return TC_GZIP_UNSUPPORTED;
  return TC_GZIP_INVALID;
}
#endif
