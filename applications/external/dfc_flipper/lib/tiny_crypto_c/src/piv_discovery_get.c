/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Discovery Object read over the card link (SP 800-73-5 Part 1 section
 * 3.3.2, Part 2 section 3.1.2). */
#include <tiny_crypto/piv_vci.h>
#if TC_ENABLE_PIV_VCI
#include "internal.h"
#include "piv_link_internal.h"

TC_PIV_result TC_PIV_discovery_get(TC_PIV_link* link, TC_PIV_discovery_profile profile,
                                   TC_buffer response, TC_PIV_discovery* out)
{
  static const uint8_t tag[] = {0x7e};
  if (!tc_piv_link_ready(link) || !out ||
      (profile != TC_PIV_DISCOVERY_PIV && profile != TC_PIV_DISCOVERY_TWIC) ||
      !tc_piv_response_valid(link, response, out, sizeof *out))
    return TC_PIV_ARGUMENT;
  /* GET DATA on a secured link always travels under secure messaging, so
   * the state before the command tells how the answer arrived. */
  const uint8_t secured = (link->flags & TC_PIV_LINK_SECURED) != 0;
  TC_PIV_data_object object;
  const TC_PIV_result result =
      TC_PIV_get_data(link, (TC_bytes){tag, sizeof tag}, response, &object);
  if (result != TC_PIV_OK)
    return result;
  /* An empty form means the card announces no policy (TWIC Part 2 v5
   * section 3.3.6). */
  if (!object.value.length)
    return tc_piv_link_fail(link, response, link->status, TC_PIV_UNSUPPORTED);
  /* The PIV application answers with 7E itself (Part 2 section 3.1.2). The
   * TWIC application may wrap it in 53. */
  const TC_bytes encoded = object.form == TC_PIV_FORM_CONTAINER ? object.value : object.encoded;
  TC_PIV_discovery discovery;
  if (TC_PIV_discovery_read(encoded, profile, &discovery) != TC_TLV_OK)
    return tc_piv_link_fail(link, response, 0, TC_PIV_INVALID);
  discovery.secured = secured;
  *out = discovery;
  return TC_PIV_OK;
}
#endif
