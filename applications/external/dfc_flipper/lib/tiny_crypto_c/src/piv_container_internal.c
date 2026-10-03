/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_TLV && (TC_ENABLE_PIV_CHUID || TC_ENABLE_PIV_OBJECTS || TC_ENABLE_PIV_COMMAND)
#include "piv_container_internal.h"

TC_TLV_result tc_piv_container_contents(TC_bytes encoded, const TC_TLV_limits* limits,
                                        TC_bytes* contents)
{
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_ISO7816, limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.header.tag_length != 1 || element.header.tag[0] != 0x53 ||
      element.encoded.length != encoded.length)
    return TC_TLV_INVALID;
  *contents = element.value;
  return TC_TLV_OK;
}
#endif
