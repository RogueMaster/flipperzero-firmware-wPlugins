/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* BER-TLV header writer shared by encoders (OCSP requests, card commands). */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV
#include "tlv_internal.h"
#include <string.h>

/* The length field holds at most three subsequent octets. */
enum { SHORT_FORM_MAX = 0x7f, MAX_LENGTH_OCTETS = 3 };

/* Octets after the initial length byte for a long-form length. */
static size_t long_form_octets(size_t value_length)
{
  size_t octets = 0;
  for (size_t value = value_length; value; value >>= 8)
    ++octets;
  return octets;
}

size_t tc_tlv_header_size(size_t tag_length, size_t value_length)
{
  if (!tag_length || tag_length > TC_TLV_TAG_BYTES)
    return 0;
  /* X.690 8.1.3.4: the short form covers 0 to 127. DER 10.1 and ISO/IEC
   * 7816-4 6.3 ask for the shortest long form above that. */
  if (value_length <= SHORT_FORM_MAX)
    return tag_length + 1;
  const size_t octets = long_form_octets(value_length);
  return octets <= MAX_LENGTH_OCTETS ? tag_length + 1 + octets : 0;
}

uint8_t* tc_tlv_header_write(uint8_t* out, const uint8_t* tag, size_t tag_length,
                             size_t value_length)
{
  memcpy(out, tag, tag_length);
  out += tag_length;
  if (value_length <= SHORT_FORM_MAX) {
    *out++ = (uint8_t)value_length;
    return out;
  }
  const size_t octets = long_form_octets(value_length);
  *out++ = (uint8_t)(0x80u | octets);
  for (size_t i = octets; i; --i)
    *out++ = (uint8_t)(value_length >> (8 * (i - 1)));
  return out;
}
#endif
