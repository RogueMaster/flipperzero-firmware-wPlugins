/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Entry checks and container handling shared by the card object readers. */
#ifndef TC_PIV_CARD_OBJECTS_INTERNAL_H_
#define TC_PIV_CARD_OBJECTS_INTERNAL_H_
#include <tiny_crypto/piv_card_objects.h>

/* Fixed framing limits: one level and at most 16 elements per object. */
extern const TC_TLV_limits tc_piv_object_limits;

/* Check the shared reader arguments (piv_card_objects.h) and return the
 * contents of encoded: the 53 value for CONTAINER, encoded itself for
 * CONTENTS. out and out_length describe the caller's output, which must be
 * disjoint from encoded. Returns ARGUMENT, the container read result, or OK
 * with contents written. */
TC_TLV_result tc_piv_object_contents(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                     const void* out, size_t out_length, TC_bytes* contents);

/* The public reader result. The objects take no caller limits, so the TLV
 * reader's LIMIT for a tag above three bytes or a length field above five
 * bytes is a malformed object. ISO/IEC 7816-4 section 6.3 marks both RFU. */
static inline TC_TLV_result tc_piv_object_result(TC_TLV_result result)
{
  return result == TC_TLV_LIMIT ? TC_TLV_INVALID : result;
}
#endif
