/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The PIV and TWIC AID table. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_COMMAND || TC_ENABLE_PIV_OBJECTS
#include "piv_aid_internal.h"
#include <string.h>

const uint8_t tc_piv_aid_prefixes[2][TC_PIV_AID_PREFIX_BYTES] = {
    {0xa0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00},
    {0xa0, 0x00, 0x00, 0x03, 0x67, 0x20, 0x00, 0x00, 0x01}};
const uint8_t tc_piv_aid[TC_PIV_AID_BYTES] = {0xa0, 0x00, 0x00, 0x03, 0x08, 0x00,
                                              0x00, 0x10, 0x00, 0x01, 0x00};

int tc_twic_aid_known(TC_bytes aid)
{
  if (aid.length != TC_PIV_AID_BYTES ||
      memcmp(aid.data, tc_piv_aid_prefixes[1], TC_PIV_AID_PREFIX_BYTES) != 0)
    return 0;
  const uint8_t* version = aid.data + TC_PIV_AID_PREFIX_BYTES;
  return version[0] == TC_PIV_AID_VERSION && (version[1] == TC_TWIC_AID_SUBVERSION_LEGACY ||
                                              version[1] == TC_TWIC_AID_SUBVERSION_NEXGEN);
}
#endif
