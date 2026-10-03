/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Virtual contact interface establishment (SP 800-73-5 Part 1 section 5.5
 * and Table 2 footnote 9, Part 2 sections 3.2.1.3 and A.6). */
#include <tiny_crypto/piv_vci.h>
#if TC_ENABLE_PIV_VCI
#include "internal.h"
#include "piv_link_internal.h"

enum { REFERENCE_PAIRING_CODE = 0x98 };

/* Part 1 section 5.5: bit 4 announces the VCI, and bit 3 waives the pairing
 * code. */
static int pairing_required(const TC_PIV_discovery* discovery)
{
  return (discovery->policy & TC_PIV_POLICY_VCI) &&
         !(discovery->policy & TC_PIV_POLICY_VCI_WITHOUT_PAIRING);
}

/* An empty code is accepted only where the policy waives pairing. */
static int arguments_valid(const TC_PIV_link* link, const TC_PIV_discovery* discovery,
                           TC_bytes code, const TC_PIV_vci_mode* out)
{
  if (!tc_piv_link_ready(link) || !discovery || !out)
    return 0;
  if (!code.length)
    return !pairing_required(discovery);
  return code.length == TC_PIV_PAIRING_CODE_DIGITS &&
         tc_piv_digits_valid(code, TC_PIV_PAIRING_CODE_DIGITS) &&
         tc_piv_link_disjoint(link, code.data, code.length) &&
         tc_internal_ranges_disjoint(code.data, code.length, out, sizeof *out);
}

/* Preconditions checked before anything is sent. The VCI exists only on the
 * PIV application under secure messaging, for a Discovery Object read under
 * that protection with bit 4 set (footnote 9). */
static TC_PIV_result vci_allowed(const TC_PIV_link* link, const TC_PIV_discovery* discovery)
{
  if (link->application == TC_PIV_APPLICATION_NONE)
    return TC_PIV_REFUSED;
  if (link->application != TC_PIV_APPLICATION_PIV || discovery->profile != TC_PIV_DISCOVERY_PIV ||
      !(discovery->policy & TC_PIV_POLICY_VCI))
    return TC_PIV_UNSUPPORTED;
  if (!(link->flags & TC_PIV_LINK_SECURED) || !discovery->secured)
    return TC_PIV_REFUSED;
  return TC_PIV_OK;
}

TC_PIV_result TC_PIV_vci_establish(TC_PIV_link* link, const TC_PIV_discovery* discovery,
                                   TC_bytes pairing_code, TC_PIV_vci_mode* out)
{
  if (!arguments_valid(link, discovery, pairing_code, out))
    return TC_PIV_ARGUMENT;
  TC_PIV_result result = vci_allowed(link, discovery);
  if (result != TC_PIV_OK)
    return result;
  if (!pairing_required(discovery)) {
    link->flags |= TC_PIV_LINK_VCI;
    *out = TC_PIV_VCI_WITHOUT_PAIRING;
    return TC_PIV_OK;
  }
  /* The secured link sends VERIFY under secure messaging (Part 2 A.6). */
  uint16_t sw = 0;
  result = tc_piv_verify_submit(link, REFERENCE_PAIRING_CODE, pairing_code, &sw);
  if (result != TC_PIV_OK)
    return result;
  /* 6300 sets the pairing status FALSE (Part 2 section 3.2.1.3). Every other
   * refusal also clears the VCI, which keeps the link on the safe side. */
  if (sw != TC_PIV_SW_SUCCESS_VALUE) {
    link->flags &= (uint8_t)~TC_PIV_LINK_VCI;
    return TC_PIV_CARD_STATUS;
  }
  link->flags |= TC_PIV_LINK_VCI;
  *out = TC_PIV_VCI_PAIRED;
  return TC_PIV_OK;
}
#endif
