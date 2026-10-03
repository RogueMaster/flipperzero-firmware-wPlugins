/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Response APDU reading and status classes (ISO/IEC 7816-4:2020 5.6). */
#include <tiny_crypto/apdu.h>
#if TC_ENABLE_APDU
#include "internal.h"

TC_APDU_status_class TC_APDU_status_classify(uint16_t sw)
{
  const unsigned sw1 = (unsigned)sw >> 8;
  if (sw == 0x9000)
    return TC_APDU_SW_SUCCESS;
  if (sw1 == 0x61)
    return TC_APDU_SW_MORE_DATA;
  if (sw1 == 0x62 || sw1 == 0x63)
    return TC_APDU_SW_WARNING;
  if (sw1 >= 0x64 && sw1 <= 0x66)
    return TC_APDU_SW_EXECUTION;
  if (sw1 == 0x6c)
    return TC_APDU_SW_WRONG_LE;
  if (sw1 >= 0x67 && sw1 <= 0x6f)
    return TC_APDU_SW_CHECKING;
  if ((sw1 & 0xf0) == 0x90)
    return TC_APDU_SW_PROPRIETARY;
  /* 5.6: values outside 6XXX and 9XXX, and every 60XX, are invalid. */
  return TC_APDU_SW_INVALID;
}

TC_APDU_result TC_APDU_response_read(TC_bytes encoded, TC_APDU_response* out)
{
  if (!out || !tc_internal_span_valid(encoded.data, encoded.length) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.length, out, sizeof *out))
    return TC_APDU_ARGUMENT;
  if (encoded.length < TC_APDU_STATUS_BYTES)
    return TC_APDU_INVALID;
  const size_t nr = encoded.length - TC_APDU_STATUS_BYTES;
  const uint8_t sw1 = encoded.data[nr];
  const uint16_t sw = (uint16_t)((unsigned)sw1 << 8 | encoded.data[nr + 1]);
  if (TC_APDU_status_classify(sw) == TC_APDU_SW_INVALID)
    return TC_APDU_INVALID;
  /* 5.6: an execution or checking error (SW1 64 to 6F) carries no data. */
  if (nr && sw1 >= 0x64 && sw1 <= 0x6f)
    return TC_APDU_INVALID;
  out->data.data = encoded.data;
  out->data.length = nr;
  out->sw = sw;
  return TC_APDU_OK;
}
#endif
