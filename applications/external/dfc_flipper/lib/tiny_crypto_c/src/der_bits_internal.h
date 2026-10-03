/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_DER_BITS_INTERNAL_H_
#define TC_DER_BITS_INTERNAL_H_

#include <tiny_crypto/der.h>

/* Validate the contents octets of a DER BIT STRING, including unused bits. */
static inline TC_TLV_result tc_der_bit_string_contents(TC_bytes contents, TC_bytes* bits,
                                                       unsigned* unused)
{
  unsigned count;
  if (!bits || !unused || (!contents.data && contents.length))
    return TC_TLV_ARGUMENT;
  if (!contents.length)
    return TC_TLV_INVALID;
  count = contents.data[0];
  if (count > 7 || (contents.length == 1 && count) ||
      (count && (contents.data[contents.length - 1] & ((1u << count) - 1u))))
    return TC_TLV_INVALID;
  bits->data = contents.data + 1;
  bits->length = contents.length - 1;
  *unused = count;
  return TC_TLV_OK;
}

#endif
