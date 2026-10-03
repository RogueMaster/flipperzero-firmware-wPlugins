/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/gzip.h>
#if TC_ENABLE_GZIP
#include "inflate_internal.h"
#include "internal.h"

TC_GZIP_result TC_GZIP_decode(TC_bytes input, TC_GZIP_workspace* workspace, size_t* work,
                              TC_buffer output, size_t* output_length)
{
  if ((!input.data && input.length) || (!output.data && output.capacity) || !workspace || !work ||
      !output_length)
    return TC_GZIP_ARGUMENT;
  const struct {
    const void* data;
    size_t length;
  } storage[] = {{input.data, input.length},
                 {output.data, output.capacity},
                 {workspace, sizeof *workspace},
                 {work, sizeof *work},
                 {output_length, sizeof *output_length}};
  for (size_t i = 0; i < sizeof storage / sizeof *storage; ++i)
    for (size_t j = i + 1; j < sizeof storage / sizeof *storage; ++j)
      if (!tc_internal_ranges_disjoint(storage[i].data, storage[i].length, storage[j].data,
                                       storage[j].length))
        return TC_GZIP_ARGUMENT;
  tc_inflate_output decoded = {output, 0};
  const TC_GZIP_result result = tc_gzip_decode(input, workspace, &decoded, work);
  TC_secure_zero(workspace, sizeof *workspace);
  if (result == TC_GZIP_OK)
    *output_length = decoded.length;
  else if (output.capacity)
    TC_secure_zero(output.data, output.capacity);
  return result;
}
#endif
