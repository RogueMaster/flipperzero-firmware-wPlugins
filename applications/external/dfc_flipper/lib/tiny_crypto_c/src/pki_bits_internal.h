/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_BITS_INTERNAL_H_
#define TC_PKI_BITS_INTERNAL_H_
#include <tiny_crypto/der.h>

/* DER named bits, excluding the unused-bit count octet. Bit zero maps to 1.
 * Empty sets are valid here; extension-specific requirements belong to callers. */
static inline TC_TLV_result tc_pki_named_bits(TC_bytes bits, unsigned unused,
    unsigned max_bits, uint16_t* out)
{
  enum { OCTET_BITS = 8, OUTPUT_BITS = 16, HIGH_BIT = 128 };
  uint16_t flags = 0;
  unsigned count;
  if (!out || (!bits.data && bits.length) || !max_bits || max_bits > OUTPUT_BITS)
    return TC_TLV_ARGUMENT;
  if (unused >= OCTET_BITS || bits.length > OUTPUT_BITS / OCTET_BITS)
    return TC_TLV_INVALID;
  if (!bits.length) {
    if (unused) return TC_TLV_INVALID;
    *out = 0;
    return TC_TLV_OK;
  }
  count = (unsigned)bits.length * OCTET_BITS - unused;
  /* DER removes trailing zero bits and clears unused padding. */
  if (count > max_bits || !(bits.data[bits.length - 1] & (1u << unused)) ||
      (bits.data[bits.length - 1] & ((1u << unused) - 1))) return TC_TLV_INVALID;
  for (unsigned i = 0; i < count; ++i)
    if (bits.data[i / OCTET_BITS] & (HIGH_BIT >> (i % OCTET_BITS)))
      flags |= (uint16_t)(1u << i);
  *out = flags;
  return TC_TLV_OK;
}
/* IMPLICIT ReasonFlags contents include the unused-bit count octet. */
static inline TC_TLV_result tc_pki_reason_flags(TC_bytes contents, uint16_t* out)
{
  enum { REASON_BITS = 9 };
  if (!out || (!contents.data && contents.length)) return TC_TLV_ARGUMENT;
  if (!contents.length) return TC_TLV_INVALID;
  return tc_pki_named_bits((TC_bytes){contents.data + 1,contents.length - 1},
      contents.data[0],REASON_BITS,out);
}
#endif
