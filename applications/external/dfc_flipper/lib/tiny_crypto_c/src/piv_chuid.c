/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_CHUID
#include <tiny_crypto/piv_chuid.h>
#include "internal.h"
#include "pki_internal.h"
#include <string.h>

static int expiration(TC_bytes date)
{
  unsigned i, y = 0, m, d;
  if (date.length != 8) return 0;
  for (i = 0; i < 8; ++i) if (date.data[i] < '0' || date.data[i] > '9') return 0;
  for (i = 0; i < 4; ++i) y = y * 10 + date.data[i] - '0';
  m = (date.data[4] - '0') * 10 + date.data[5] - '0';
  d = (date.data[6] - '0') * 10 + date.data[7] - '0';
  return tc_pki_date(y, m, d);
}

TC_TLV_result TC_PIV_CHUID_read(const uint8_t* data, size_t length,
                               TC_PIV_CHUID_encoding encoding, TC_PIV_CHUID* out)
{
  return TC_PIV_CHUID_read_profile(data, length, encoding, TC_CHUID_PROFILE_PIV, out);
}

TC_TLV_result TC_PIV_CHUID_read_profile(const uint8_t* data, size_t length,
    TC_PIV_CHUID_encoding encoding, TC_PIV_CHUID_profile profile, TC_PIV_CHUID* out)
{
  enum { KEY_MAP_TAG = 0x3d, KEY_MAP_MAX_BYTES = 512 };
  static const uint8_t tags[] = {0x30,0x34,0x35,0x36,KEY_MAP_TAG,0x3e,0xfe};
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, sizeof tags, 1};
  TC_PIV_CHUID chuid;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result;
  unsigned position = 0;
  if (!out || (!data && length) ||
      (profile != TC_CHUID_PROFILE_PIV && profile != TC_CHUID_PROFILE_TWIC_SIGNED &&
       profile != TC_CHUID_PROFILE_TWIC_UNSIGNED && profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP) ||
      (encoding != TC_PIV_CHUID_CONTENTS && encoding != TC_PIV_CHUID_CONTAINER)) return TC_TLV_ARGUMENT;
  if (!tc_internal_ranges_disjoint(data,length,out,sizeof *out)) return TC_TLV_ARGUMENT;
  if (encoding == TC_PIV_CHUID_CONTAINER) {
    result = TC_TLV_read(data, length, TC_TLV_ISO7816, &limits, &element);
    if (result != TC_TLV_OK) return result;
    if (element.header.tag_length != 1 || element.header.tag[0] != 0x53 ||
        element.encoded.length != length) return TC_TLV_INVALID;
    data = element.value.data; length = element.value.length;
  }
  memset(&chuid, 0, sizeof chuid);
  result = TC_TLV_reader_init(&reader, data, length, TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK) return result;
  /* CHUID tags identify opaque fields, even when their constructed bit is set. */
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    unsigned tag = element.header.tag[0];
    if (position == 3 && ((profile != TC_CHUID_PROFILE_PIV &&
        profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP) || tag != 0x36)) ++position;
    if (position == 4 && (profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP || tag != KEY_MAP_TAG)) ++position;
    if (position == 5 && profile == TC_CHUID_PROFILE_TWIC_UNSIGNED) ++position;
    if (position >= sizeof tags || element.header.tag_length != 1 || tag != tags[position])
      return TC_TLV_INVALID;
    ++position;
    switch (tag) {
      case 0x30:
        if (element.value.length != 25) return TC_TLV_INVALID;
        chuid.fascn = element.value; break;
      case 0x34:
        if (element.value.length != 16) return TC_TLV_INVALID;
        chuid.card_uuid = element.value; break;
      case 0x35:
        if (!expiration(element.value)) return TC_TLV_INVALID;
        chuid.expiration = element.value; break;
      case 0x36:
        if (element.value.length != 16) return TC_TLV_INVALID;
        chuid.cardholder_uuid = element.value; break;
      case KEY_MAP_TAG:
        if (element.value.length > KEY_MAP_MAX_BYTES) return TC_TLV_INVALID;
        chuid.authentication_key_map = element.value;
        break;
      case 0x3e:
        if (!element.value.length) return TC_TLV_INVALID;
        chuid.signature = element.value;
        chuid.signed_content[0] = (TC_bytes){data,(size_t)(element.encoded.data - data)};
        break;
      case 0xfe:
        if (element.value.length || reader.offset != length) return TC_TLV_INVALID;
        /* The signature covers FE's tag and length, which follow the CMS field. */
        if (chuid.signature.data) chuid.signed_content[1] = element.encoded;
        break;
      default: break;
    }
  }
  if (result != TC_TLV_END)
    return result == TC_TLV_MORE && encoding == TC_PIV_CHUID_CONTAINER ? TC_TLV_INVALID : result;
  if (position != sizeof tags) return TC_TLV_INVALID;
  *out = chuid;
  return TC_TLV_OK;
}
#endif
