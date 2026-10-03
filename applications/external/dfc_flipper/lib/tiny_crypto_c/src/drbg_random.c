/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * TC_random_fn adapter over an instantiated DRBG, for the library APIs that
 * take a TC_random_source. */
#include <tiny_crypto/drbg.h>

#if TC_ENABLE_DRBG
#include "internal.h"

TC_status TC_DRBG_random(void* user, uint8_t* output, size_t length)
{
  const TC_bytes empty = {NULL, 0};
  TC_DRBG* drbg = (TC_DRBG*)user;
  size_t offset = 0;

  /* A NULL drbg and output inside the DRBG are argument errors. Reject them
   * before the first generate call, so no chunk runs and the output and the
   * generator state stay intact. */
  if (drbg == NULL || !tc_internal_ranges_disjoint(drbg, sizeof *drbg, output, length))
    return TC_ERROR;
  /* A zero-length request still checks that the DRBG is instantiated. */
  do {
#if SIZE_MAX > TC_DRBG_MAX_REQUEST_BYTES
    const size_t chunk =
        length - offset < TC_DRBG_MAX_REQUEST_BYTES ? length - offset : TC_DRBG_MAX_REQUEST_BYTES;
#else
    const size_t chunk = length - offset; /* every size_t length fits one request */
#endif
    const TC_DRBG_result result = TC_DRBG_generate(
        drbg, (TC_buffer){output == NULL ? NULL : output + offset, chunk}, 0, empty);
    /* Every chunk has the same arguments, so an argument error can only come
     * from the first chunk, before any byte is written. Later failures wipe
     * the chunks already generated. */
    if (result != TC_DRBG_OK) {
      if (result != TC_DRBG_ARGUMENT && output != NULL)
        TC_secure_zero(output, length);
      return TC_ERROR;
    }
    offset += chunk;
  } while (offset < length);
  return TC_OK;
}

TC_random_source TC_DRBG_random_source(TC_DRBG* drbg)
{
  TC_random_source source = {TC_DRBG_random, drbg};
  return source;
}

#endif
