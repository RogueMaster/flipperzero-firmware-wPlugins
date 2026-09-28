/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "pki_internal.h"
#include <string.h>
#include <tiny_crypto/piv_printed.h>

enum {
  PRINTED_NAME_TAG = 0x01,
  PRINTED_EMPLOYEE_TAG = 0x02,
  PRINTED_EXPIRATION_TAG = 0x04,
  PRINTED_SERIAL_TAG = 0x05,
  PRINTED_ISSUER_TAG = 0x06,
  PRINTED_ORGANIZATION_1_TAG = 0x07,
  PRINTED_ORGANIZATION_2_TAG = 0x08,
  PRINTED_ERROR_TAG = 0xfe,
  PRINTED_CONTAINER_TAG = 0x53,
  PRINTED_NAME_MAX = 125,
  PRINTED_AFFILIATION_MAX = 20,
  PRINTED_DATE_LENGTH = 9,
  PRINTED_SERIAL_MAX = 20,
  PRINTED_ISSUER_LENGTH = 15,
  TWIC_SERIAL_LENGTH = 8,
  TWIC_ISSUER_LENGTH = 8,
  TWIC_CONTENTS_MAX = 200
};

static int printable(TC_bytes value, size_t maximum, int empty) {
  if (value.length > maximum || (!empty && !value.length))
    return 0;
  for (size_t i = 0; i < value.length; ++i)
    if (value.data[i] < 0x20 || value.data[i] > 0x7e)
      return 0;
  return 1;
}

static int decimal(TC_bytes value) {
  for (size_t i = 0; i < value.length; ++i)
    if (value.data[i] < '0' || value.data[i] > '9')
      return 0;
  return 1;
}

static unsigned number(const uint8_t *value, size_t length) {
  unsigned result = 0;
  for (size_t i = 0; i < length; ++i)
    result = result * 10 + value[i] - '0';
  return result;
}

static unsigned month(const uint8_t *value) {
  static const char names[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
  for (unsigned i = 0; i < 12; ++i)
    if (!memcmp(value, names + i * 3, 3))
      return i + 1;
  return 0;
}

static int twic_issuer(TC_bytes value) {
  static const uint8_t prefix[] = {'7', '0', '9', '9'};
  return value.length == TWIC_ISSUER_LENGTH && decimal(value) &&
         !memcmp(value.data, prefix, sizeof prefix);
}

static int date(TC_bytes value, TC_PIV_printed_profile profile,
                TC_X509_time *out) {
  if (value.length != PRINTED_DATE_LENGTH)
    return 0;
  unsigned year, day, parsed_month;
  if (profile == TC_PIV_PRINTED_PROFILE_PIV) {
    if (!decimal((TC_bytes){value.data, 4}) ||
        !decimal((TC_bytes){value.data + 7, 2}))
      return 0;
    year = number(value.data, 4);
    parsed_month = month(value.data + 4);
    day = number(value.data + 7, 2);
  } else {
    if (!decimal((TC_bytes){value.data, 2}) ||
        !decimal((TC_bytes){value.data + 5, 4}))
      return 0;
    day = number(value.data, 2);
    parsed_month = month(value.data + 2);
    year = number(value.data + 5, 4);
  }
  if (!parsed_month || !tc_pki_date(year, parsed_month, day))
    return 0;
  *out = (TC_X509_time){year, (uint8_t)parsed_month, (uint8_t)day, 0, 0, 0};
  return 1;
}

static TC_TLV_result field(TC_TLV_reader *reader, unsigned tag,
                           TC_TLV_element *out) {
  TC_TLV_result result = TC_TLV_next(reader, out);
  if (result == TC_TLV_END)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  return out->header.tag_length == 1 && out->header.tag[0] == tag
             ? TC_TLV_OK
             : TC_TLV_INVALID;
}

static TC_TLV_result text_field(TC_TLV_reader *reader, unsigned tag,
                                size_t maximum, int empty, TC_bytes *out) {
  TC_TLV_element element;
  TC_TLV_result result = field(reader, tag, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!printable(element.value, maximum, empty))
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_printed_read(TC_bytes input,
                                  TC_PIV_printed_encoding encoding,
                                  TC_PIV_printed_profile profile,
                                  TC_PIV_printed *out) {
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 8, 1};
  TC_PIV_printed parsed = {0};
  TC_TLV_element element;
  TC_TLV_reader reader;
  TC_TLV_result result;
  if (!out || (!input.data && input.length) ||
      (encoding != TC_PIV_PRINTED_CONTENTS &&
       encoding != TC_PIV_PRINTED_CONTAINER) ||
      (profile != TC_PIV_PRINTED_PROFILE_PIV &&
       profile != TC_PIV_PRINTED_PROFILE_TWIC))
    return TC_TLV_ARGUMENT;
  if (!tc_internal_ranges_disjoint(input.data, input.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (encoding == TC_PIV_PRINTED_CONTAINER) {
    result = TC_TLV_read(input.data, input.length, TC_TLV_ISO7816, &limits,
                         &element);
    if (result != TC_TLV_OK)
      return result;
    if (element.header.tag_length != 1 ||
        element.header.tag[0] != PRINTED_CONTAINER_TAG ||
        element.encoded.length != input.length)
      return TC_TLV_INVALID;
    input = element.value;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC &&
      input.length > TWIC_CONTENTS_MAX)
    return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader, input.data, input.length, TC_TLV_ISO7816,
                              &limits);
  if (result != TC_TLV_OK)
    return result;

  result =
      text_field(&reader, PRINTED_NAME_TAG, PRINTED_NAME_MAX, 0, &parsed.name);
  if (result != TC_TLV_OK)
    return result;
  result = text_field(&reader, PRINTED_EMPLOYEE_TAG, PRINTED_AFFILIATION_MAX, 1,
                      &parsed.employee_affiliation);
  if (result != TC_TLV_OK)
    return result;
  result = field(&reader, PRINTED_EXPIRATION_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!date(element.value, profile, &parsed.expiration))
    return TC_TLV_INVALID;
  parsed.expiration_text = element.value;
  result = field(&reader, PRINTED_SERIAL_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC) {
    if (element.value.length != TWIC_SERIAL_LENGTH || !decimal(element.value))
      return TC_TLV_INVALID;
  } else if (!printable(element.value, PRINTED_SERIAL_MAX, 0))
    return TC_TLV_INVALID;
  parsed.card_serial_number = element.value;
  result = field(&reader, PRINTED_ISSUER_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC) {
    if (!twic_issuer(element.value))
      return TC_TLV_INVALID;
  } else if (!printable(element.value, PRINTED_ISSUER_LENGTH, 0) ||
             element.value.length != PRINTED_ISSUER_LENGTH)
    return TC_TLV_INVALID;
  parsed.issuer_identification = element.value;
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC ||
      (reader.offset < input.length &&
       input.data[reader.offset] == PRINTED_ORGANIZATION_1_TAG)) {
    result = text_field(&reader, PRINTED_ORGANIZATION_1_TAG,
                        PRINTED_AFFILIATION_MAX, 1, &parsed.organization_1);
    if (result != TC_TLV_OK)
      return result;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC ||
      (reader.offset < input.length &&
       input.data[reader.offset] == PRINTED_ORGANIZATION_2_TAG)) {
    result = text_field(&reader, PRINTED_ORGANIZATION_2_TAG,
                        PRINTED_AFFILIATION_MAX, 1, &parsed.organization_2);
    if (result != TC_TLV_OK)
      return result;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_PIV) {
    result = field(&reader, PRINTED_ERROR_TAG, &element);
    if (result != TC_TLV_OK)
      return result;
    if (element.value.length)
      return TC_TLV_INVALID;
  }
  if (reader.offset != input.length)
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_printed_expiration_check(const TC_PIV_printed *printed,
                                              TC_bytes chuid_expiration,
                                              const TC_X509_time *at,
                                              int *valid) {
  if (!printed || !at || !valid || !chuid_expiration.data ||
      chuid_expiration.length != 8)
    return TC_TLV_ARGUMENT;
  if (!decimal(chuid_expiration))
    return TC_TLV_INVALID;
  const unsigned year = number(chuid_expiration.data, 4);
  const unsigned parsed_month = number(chuid_expiration.data + 4, 2);
  const unsigned day = number(chuid_expiration.data + 6, 2);
  if (!tc_pki_date(year, parsed_month, day))
    return TC_TLV_INVALID;
  TC_X509_time expires = printed->expiration;
  expires.hour = 23;
  expires.minute = 59;
  expires.second = 59;
  int order;
  TC_TLV_result result = TC_X509_time_compare(at, &expires, &order);
  if (result != TC_TLV_OK)
    return result;
  *valid = printed->expiration.year == year &&
           printed->expiration.month == parsed_month &&
           printed->expiration.day == day && order <= 0;
  return TC_TLV_OK;
}
#endif
