/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Discovery Object reader. */
#include <tiny_crypto/piv_discovery.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_aid_internal.h"

enum {
  DISCOVERY_TAG = 0x7e,
  AID_TAG = 0x4f,
  POLICY_TAG_0 = 0x5f,
  POLICY_TAG_1 = 0x2f,
  /* 7E 12 {4F 0B AID} {5F2F 02 xx yy} (Part 1 section 3.3.2). */
  DISCOVERY_VALUE_BYTES = 0x12,
  DISCOVERY_BYTES = 2 + DISCOVERY_VALUE_BYTES,
  AID_OFFSET = 4,
  POLICY_OFFSET = AID_OFFSET + TC_PIV_AID_BYTES + 3,
  POLICY_BYTES = 2
};

/* SP 800-73-5 Part 1 section 3.3.2 and Table 1. */
static int piv_policy(uint8_t policy, uint8_t preference)
{
  /* Bits 8, 2 and 1 are zero and the PIV PIN bit is set in every Table 1
   * value. Pairing-free VCI (bit 3) needs the VCI (bit 4). */
  if ((policy & 0x83) || !(policy & TC_PIV_POLICY_PIV_PIN) ||
      ((policy & TC_PIV_POLICY_VCI_WITHOUT_PAIRING) && !(policy & TC_PIV_POLICY_VCI)))
    return 0;
  /* The second byte is RFU (00) unless the Global PIN is enabled. */
  if (policy & TC_PIV_POLICY_GLOBAL_PIN)
    return preference == TC_PIV_PREFERENCE_PIV_PIN || preference == TC_PIV_PREFERENCE_GLOBAL_PIN;
  return preference == 0;
}

/* TWIC Part 2 v5 section 4.2 (40 00) and section 4.7.5 (04 00 in the PIV
 * application, 00 00 in the TWIC application). */
static int twic_policy(uint8_t policy, uint8_t preference)
{
  return preference == 0 && (policy == TC_PIV_POLICY_PIV_PIN ||
                             policy == TC_PIV_POLICY_VCI_WITHOUT_PAIRING || policy == 0);
}

static int piv_aid(TC_bytes aid)
{
  return aid.length == TC_PIV_AID_BYTES && !memcmp(aid.data, tc_piv_aid, TC_PIV_AID_BYTES);
}

TC_TLV_result TC_PIV_discovery_read(TC_bytes encoded, TC_PIV_discovery_profile profile,
                                    TC_PIV_discovery* out)
{
  static const TC_TLV_limits limits = {DISCOVERY_BYTES, DISCOVERY_VALUE_BYTES, 1, 1};
  if (!out || !tc_internal_span_valid(encoded.data, encoded.length) ||
      (profile != TC_PIV_DISCOVERY_PIV && profile != TC_PIV_DISCOVERY_TWIC) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (!encoded.length)
    return TC_TLV_INVALID;
  /* The outer read reports truncation as MORE. A longer object is INVALID. */
  TC_TLV_element element;
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_ISO7816, &limits, &element);
  if (result == TC_TLV_LIMIT)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  /* Table 19 fixes the layout, so the bytes are compared in place. */
  const uint8_t* bytes = encoded.data;
  if (encoded.length != DISCOVERY_BYTES || bytes[0] != DISCOVERY_TAG ||
      bytes[1] != DISCOVERY_VALUE_BYTES || bytes[2] != AID_TAG || bytes[3] != TC_PIV_AID_BYTES ||
      bytes[POLICY_OFFSET - 3] != POLICY_TAG_0 || bytes[POLICY_OFFSET - 2] != POLICY_TAG_1 ||
      bytes[POLICY_OFFSET - 1] != POLICY_BYTES)
    return TC_TLV_INVALID;
  const TC_bytes aid = {bytes + AID_OFFSET, TC_PIV_AID_BYTES};
  const uint8_t policy = bytes[POLICY_OFFSET], preference = bytes[POLICY_OFFSET + 1];
  const int accepted =
      profile == TC_PIV_DISCOVERY_PIV
          ? piv_aid(aid) && piv_policy(policy, preference)
          : (piv_aid(aid) || tc_twic_aid_known(aid)) && twic_policy(policy, preference);
  if (!accepted)
    return TC_TLV_INVALID;
  out->aid = aid;
  out->policy = policy;
  out->preference = preference;
  out->profile = (uint8_t)profile;
  out->secured = 0;
  return TC_TLV_OK;
}

uint8_t TC_PIV_discovery_pin_reference(const TC_PIV_discovery* discovery)
{
  /* Part 1 section 3.3.2: the second byte selects the primary PIN when both
   * PINs satisfy the ACRs. Part 1 Table 4: 00 Global PIN, 80 PIV PIN. */
  if (discovery && (discovery->policy & TC_PIV_POLICY_GLOBAL_PIN) &&
      discovery->preference == TC_PIV_PREFERENCE_GLOBAL_PIN)
    return 0x00;
  return 0x80;
}
#endif
