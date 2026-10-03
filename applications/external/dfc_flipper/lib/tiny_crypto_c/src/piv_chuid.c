/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_CHUID
#include <tiny_crypto/piv_chuid.h>
#include "internal.h"
#include "pki_internal.h"
#include "piv_container_internal.h"
#include "credential_text_internal.h"
#include <string.h>

/* One bit per TC_PIV_CHUID_profile value. */
#define PROFILE_BIT(profile) (1u << (profile))
#define PIV_PROFILES                                                                               \
  (PROFILE_BIT(TC_CHUID_PROFILE_PIV) | PROFILE_BIT(TC_CHUID_PROFILE_LEGACY_KEY_MAP))
#define SIGNED_PROFILES (PIV_PROFILES | PROFILE_BIT(TC_CHUID_PROFILE_TWIC_SIGNED))
#define ALL_PROFILES (SIGNED_PROFILES | PROFILE_BIT(TC_CHUID_PROFILE_TWIC_UNSIGNED))

enum {
  BUFFER_LENGTH_TAG = 0xee,
  FASCN_TAG = 0x30,
  ORGANIZATION_TAG = 0x32,
  DUNS_TAG = 0x33,
  CARD_UUID_TAG = 0x34,
  EXPIRATION_TAG = 0x35,
  CARDHOLDER_UUID_TAG = 0x36,
  KEY_MAP_TAG = 0x3d,
  SIGNATURE_TAG = 0x3e,
  ERROR_DETECTION_TAG = 0xfe,
  KEY_MAP_MAX_BYTES = 512
};

/* CHUID fields in their required order. allowed lists the profiles in which a
 * field may appear. required lists the profiles in which it must appear. PIV
 * follows SP 800-73-4 Part 1 Table 9, TWIC follows TWIC Part 2 sections 4.6.1
 * and 4.6.3, and the key map follows SP 800-73-2 Part 1 Table 8. */
static const struct {
  uint8_t tag;
  uint8_t allowed, required;
} fields[] = {
    {BUFFER_LENGTH_TAG, PIV_PROFILES, 0},
    {FASCN_TAG, ALL_PROFILES, ALL_PROFILES},
    {ORGANIZATION_TAG, PIV_PROFILES, 0},
    {DUNS_TAG, PIV_PROFILES, 0},
    {CARD_UUID_TAG, ALL_PROFILES, ALL_PROFILES},
    {EXPIRATION_TAG, ALL_PROFILES, ALL_PROFILES},
    {CARDHOLDER_UUID_TAG, PIV_PROFILES, 0},
    {KEY_MAP_TAG, PROFILE_BIT(TC_CHUID_PROFILE_LEGACY_KEY_MAP), 0},
    {SIGNATURE_TAG, SIGNED_PROFILES, SIGNED_PROFILES},
    {ERROR_DETECTION_TAG, ALL_PROFILES, ALL_PROFILES},
};
enum { FIELD_COUNT = sizeof fields / sizeof *fields };

/* Advance *position to the field matching tag, skipping fields that are
 * optional or absent in this profile. Returns 0 when tag is out of order,
 * repeated, outside the profile, or when a required field is missing. */
static int field_find(unsigned tag, unsigned profile_bit, size_t* position)
{
  for (; *position < FIELD_COUNT; ++*position) {
    if (fields[*position].tag == tag && (fields[*position].allowed & profile_bit))
      return 1;
    if (fields[*position].required & profile_bit)
      return 0;
  }
  return 0;
}

static int fixed_length(const TC_TLV_element* element, size_t length)
{
  return element->value.length == length;
}

static TC_TLV_result field_store(const TC_TLV_element* element, TC_PIV_CHUID* chuid)
{
  switch (element->header.tag[0]) {
  /* SP 800-73-4 Part 1 Table 9 gives fixed sizes for the deprecated fields. */
  case BUFFER_LENGTH_TAG:
    return fixed_length(element, 2) ? TC_TLV_OK : TC_TLV_INVALID;
  case ORGANIZATION_TAG:
    return fixed_length(element, 4) ? TC_TLV_OK : TC_TLV_INVALID;
  case DUNS_TAG:
    return fixed_length(element, 9) ? TC_TLV_OK : TC_TLV_INVALID;
  case FASCN_TAG:
    if (!fixed_length(element, 25))
      return TC_TLV_INVALID;
    chuid->fascn = element->value;
    return TC_TLV_OK;
  case CARD_UUID_TAG:
    if (!fixed_length(element, 16))
      return TC_TLV_INVALID;
    chuid->card_uuid = element->value;
    return TC_TLV_OK;
  case EXPIRATION_TAG:
    if (!tc_credential_yyyymmdd(element->value.data, element->value.length, NULL, NULL, NULL))
      return TC_TLV_INVALID;
    chuid->expiration = element->value;
    return TC_TLV_OK;
  case CARDHOLDER_UUID_TAG:
    if (!fixed_length(element, 16))
      return TC_TLV_INVALID;
    chuid->cardholder_uuid = element->value;
    return TC_TLV_OK;
  case KEY_MAP_TAG:
    if (element->value.length > KEY_MAP_MAX_BYTES)
      return TC_TLV_INVALID;
    chuid->authentication_key_map = element->value;
    return TC_TLV_OK;
  case SIGNATURE_TAG:
    if (!element->value.length)
      return TC_TLV_INVALID;
    chuid->signature = element->value;
    return TC_TLV_OK;
  default:
    /* The Error Detection Code is empty (SP 800-73-4 Part 1 section 3.1.2). */
    return element->value.length ? TC_TLV_INVALID : TC_TLV_OK;
  }
}

static TC_TLV_result read_fields(TC_bytes contents, TC_PIV_CHUID_profile profile,
                                 TC_PIV_CHUID* chuid)
{
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, FIELD_COUNT, 1};
  const unsigned profile_bit = PROFILE_BIT(profile);
  TC_TLV_reader reader;
  TC_TLV_element element;
  size_t position = 0;
  /* Signed content starts after the Buffer Length element, which the
   * signature excludes (SP 800-73-4 Part 1 section 3.1.2). */
  const uint8_t* signed_start = contents.data;
  TC_TLV_result result = TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK)
    return result;
  /* CHUID tags identify opaque fields, even when their constructed bit is set. */
  while ((result = TC_TLV_next(&reader, &element)) == TC_TLV_OK) {
    const unsigned tag = element.header.tag[0];
    if (element.header.tag_length != 1 || !field_find(tag, profile_bit, &position))
      return TC_TLV_INVALID;
    ++position;
    result = field_store(&element, chuid);
    if (result != TC_TLV_OK)
      return result;
    if (tag == BUFFER_LENGTH_TAG)
      signed_start = contents.data + reader.offset;
    else if (tag == SIGNATURE_TAG)
      chuid->signed_content[0] =
          (TC_bytes){signed_start, (size_t)(element.encoded.data - signed_start)};
    else if (tag == ERROR_DETECTION_TAG) {
      if (reader.offset != contents.length)
        return TC_TLV_INVALID;
      /* The signature covers FE's tag and length, which follow the CMS field. */
      if (chuid->signature.data)
        chuid->signed_content[1] = element.encoded;
    }
  }
  if (result != TC_TLV_END)
    return result;
  return position == FIELD_COUNT ? TC_TLV_OK : TC_TLV_INVALID;
}

TC_TLV_result TC_PIV_CHUID_read(TC_bytes encoded, TC_PIV_CHUID_encoding encoding,
                                TC_PIV_CHUID_profile profile, TC_PIV_CHUID* out)
{
  TC_PIV_CHUID chuid;
  TC_bytes contents = encoded;
  TC_TLV_result result;
  if (!out || (!encoded.data && encoded.length) ||
      (profile != TC_CHUID_PROFILE_PIV && profile != TC_CHUID_PROFILE_TWIC_SIGNED &&
       profile != TC_CHUID_PROFILE_TWIC_UNSIGNED && profile != TC_CHUID_PROFILE_LEGACY_KEY_MAP) ||
      (encoding != TC_PIV_CHUID_CONTENTS && encoding != TC_PIV_CHUID_CONTAINER) ||
      !tc_internal_ranges_disjoint(encoded.data, encoded.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (encoding == TC_PIV_CHUID_CONTAINER) {
    const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 1, 1};
    result = tc_piv_container_contents(encoded, &limits, &contents);
    if (result != TC_TLV_OK)
      return result;
  }
  memset(&chuid, 0, sizeof chuid);
  result = read_fields(contents, profile, &chuid);
  if (result != TC_TLV_OK)
    return result == TC_TLV_MORE && encoding == TC_PIV_CHUID_CONTAINER ? TC_TLV_INVALID : result;
  *out = chuid;
  return TC_TLV_OK;
}
#endif
