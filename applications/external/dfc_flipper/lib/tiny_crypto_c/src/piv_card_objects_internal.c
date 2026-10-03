/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Entry checks and container handling shared by the card object readers. */
#include <tiny_crypto/piv_card_objects.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_card_objects_internal.h"
#include "piv_container_internal.h"

const TC_TLV_limits tc_piv_object_limits = {SIZE_MAX, SIZE_MAX, 16, 1};

TC_TLV_result tc_piv_object_contents(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                     const void* out, size_t out_length, TC_bytes* contents)
{
  if (!out || !tc_internal_span_valid(encoded.data, encoded.length) ||
      (encoding != TC_PIV_CONTENTS && encoding != TC_PIV_CONTAINER) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.length, out, out_length))
    return TC_TLV_ARGUMENT;
  if (encoding == TC_PIV_CONTENTS) {
    *contents = encoded;
    return TC_TLV_OK;
  }
  /* A missing or truncated 53 header is INVALID or MORE, and bytes after the
   * container are INVALID. */
  if (!encoded.length)
    return TC_TLV_INVALID;
  return tc_piv_container_contents(encoded, &tc_piv_object_limits, contents);
}
#endif
