/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_OBJECTS
#include <tiny_crypto/piv_security.h>
#include <tiny_crypto/lds.h>
#include "internal.h"

enum { MAPPING_RECORD_BYTES = 3 };

static TC_TLV_result security_mapping(TC_bytes mapping, uint16_t container,
    uint16_t* groups, unsigned* number)
{
  uint16_t present = 0;
  unsigned selected = 0;
  if (!mapping.data || !mapping.length || mapping.length % MAPPING_RECORD_BYTES ||
      mapping.length > TC_LDS_MAX_GROUPS * MAPPING_RECORD_BYTES) return TC_TLV_INVALID;
  for (size_t i = 0; i < mapping.length; i += MAPPING_RECORD_BYTES) {
    const uint8_t* record = mapping.data + i;
    if (!record[0] || record[0] > TC_LDS_MAX_GROUPS) return TC_TLV_INVALID;
    const uint16_t bit = (uint16_t)(1u << (record[0] - 1));
    if (present & bit) return TC_TLV_INVALID;
    present |= bit;
    /* At most 16 records; rescanning avoids a separate container index. */
    for (size_t j = 0; j < i; j += MAPPING_RECORD_BYTES)
      if (record[1] == mapping.data[j + 1] && record[2] == mapping.data[j + 2])
        return TC_TLV_INVALID;
    if (((uint16_t)record[1] << 8 | record[2]) == container) selected = record[0];
  }
  *groups = present;
  if (number) *number = selected;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_security_read(TC_bytes encoded,
    TC_PIV_security_encoding encoding, TC_PIV_security_object* out)
{
  enum { RESPONSE_TAG = 0x53, MAPPING_TAG = 0xba, CMS_TAG = 0xbb,
    CHECK_TAG = 0xfe, FIELD_COUNT = 3 };
  static const uint8_t tags[FIELD_COUNT] = {MAPPING_TAG,CMS_TAG,CHECK_TAG};
  const TC_TLV_limits limits = {SIZE_MAX,SIZE_MAX,FIELD_COUNT,1};
  TC_PIV_security_object parsed = {{NULL,0},{NULL,0},0};
  TC_TLV_reader reader;
  TC_TLV_element field;
  if (!out || (!encoded.data && encoded.length) ||
      (encoding != TC_PIV_SECURITY_CONTENTS && encoding != TC_PIV_SECURITY_CONTAINER) ||
      !tc_internal_ranges_disjoint(encoded.data,encoded.length,out,sizeof *out))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result;
  if (encoding == TC_PIV_SECURITY_CONTAINER) {
    result = TC_TLV_read(encoded.data,encoded.length,TC_TLV_ISO7816,&limits,&field);
    if (result != TC_TLV_OK) return result;
    if (field.header.tag_length != 1 || field.header.tag[0] != RESPONSE_TAG ||
        field.encoded.length != encoded.length) return TC_TLV_INVALID;
    encoded = field.value;
  }
  result = TC_TLV_reader_init(&reader,encoded.data,encoded.length,TC_TLV_ISO7816,&limits);
  if (result != TC_TLV_OK) return result;
  /* These tags carry opaque values despite their ASN.1 constructed bits. */
  for (size_t i = 0; i < FIELD_COUNT; ++i) {
    result = TC_TLV_next(&reader,&field);
    if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
    if (field.header.tag_length != 1 || field.header.tag[0] != tags[i]) return TC_TLV_INVALID;
    if (i == 0) parsed.mapping = field.value;
    else if (i == 1) {
      if (!field.value.length) return TC_TLV_INVALID;
      parsed.cms = field.value;
    } else if (field.value.length) return TC_TLV_INVALID;
  }
  if (reader.offset != encoded.length) return TC_TLV_INVALID;
  result = security_mapping(parsed.mapping,0,&parsed.groups,NULL);
  if (result != TC_TLV_OK) return result;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_security_group_find(const TC_PIV_security_object* object,
    uint16_t container, unsigned* number)
{
  uint16_t groups;
  unsigned selected;
  if (!object || !number ||
      !tc_internal_ranges_disjoint(object,sizeof *object,number,sizeof *number) ||
      !tc_internal_ranges_disjoint(object->mapping.data,object->mapping.length,number,sizeof *number) ||
      !tc_internal_ranges_disjoint(object->cms.data,object->cms.length,number,sizeof *number))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = security_mapping(object->mapping,container,&groups,&selected);
  if (result != TC_TLV_OK) return result;
  if (groups != object->groups) return TC_TLV_INVALID;
  if (!selected) return TC_TLV_END;
  *number = selected;
  return TC_TLV_OK;
}
#endif
