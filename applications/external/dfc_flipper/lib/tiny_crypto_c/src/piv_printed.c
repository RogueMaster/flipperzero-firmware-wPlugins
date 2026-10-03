/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_OBJECTS
#include "credential_text_internal.h"
#include "internal.h"
#include "pki_internal.h"
#include "piv_container_internal.h"
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
  PRINTED_NAME_MAX = 125,
  PRINTED_AFFILIATION_MAX = 20,
  PRINTED_DATE_LENGTH = 9,
  PRINTED_SERIAL_MAX = 20,
  PRINTED_ISSUER_LENGTH = 15,
  TWIC_SERIAL_LENGTH = 8,
  TWIC_ISSUER_LENGTH = 8,
  TWIC_CONTENTS_MAX = 200
};

static int printable(TC_bytes value, size_t maximum, int empty)
{
  if (value.length > maximum || (!empty && !value.length))
    return 0;
  for (size_t i = 0; i < value.length; ++i)
    if (value.data[i] < 0x20 || value.data[i] > 0x7e)
      return 0;
  return 1;
}

static int twic_issuer(TC_bytes value)
{
  static const uint8_t prefix[] = {'7', '0', '9', '9'};
  return value.length == TWIC_ISSUER_LENGTH && tc_credential_digits(value.data, value.length) &&
         !memcmp(value.data, prefix, sizeof prefix);
}

/* SP 800-73-5 Part 1 Table 15 encodes PIV expiration as YYYYMMMDD. TWIC
 * NEXGEN/Legacy Part 2 section 4.7.2 uses DDMMMYYYY. Month names are upper
 * case in both. */
static int printed_date(TC_bytes value, TC_PIV_printed_profile profile, TC_X509_time* out)
{
  size_t year, day;
  unsigned month;
  if (value.length != PRINTED_DATE_LENGTH)
    return 0;
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC)
    return tc_credential_day_month_year(value.data, 0, out);
  if (!tc_credential_decimal(value.data, 4, 9999, &year) ||
      !tc_credential_decimal(value.data + 7, 2, 31, &day))
    return 0;
  month = tc_credential_month3(value.data + 4, 0);
  if (!tc_internal_calendar_date((unsigned)year, month, (unsigned)day))
    return 0;
  *out = (TC_X509_time){(unsigned)year, (uint8_t)month, (uint8_t)day, 0, 0, 0};
  return 1;
}

static TC_TLV_result text_field(TC_TLV_reader* reader, unsigned tag, size_t maximum, int empty,
                                TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result result = tc_pki_field(reader, tag, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!printable(element.value, maximum, empty))
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_printed_read(TC_bytes input, TC_PIV_printed_encoding encoding,
                                  TC_PIV_printed_profile profile, TC_PIV_printed* out)
{
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 8, 1};
  TC_PIV_printed parsed = {0};
  TC_TLV_element element;
  TC_TLV_reader reader;
  TC_TLV_result result;
  if (!out || (!input.data && input.length) ||
      (encoding != TC_PIV_PRINTED_CONTENTS && encoding != TC_PIV_PRINTED_CONTAINER) ||
      (profile != TC_PIV_PRINTED_PROFILE_PIV && profile != TC_PIV_PRINTED_PROFILE_TWIC))
    return TC_TLV_ARGUMENT;
  if (!tc_internal_ranges_disjoint(input.data, input.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (encoding == TC_PIV_PRINTED_CONTAINER) {
    result = tc_piv_container_contents(input, &limits, &input);
    if (result != TC_TLV_OK)
      return result;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC && input.length > TWIC_CONTENTS_MAX)
    return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader, input, TC_TLV_ISO7816, &limits);
  if (result != TC_TLV_OK)
    return result;

  result = text_field(&reader, PRINTED_NAME_TAG, PRINTED_NAME_MAX, 0, &parsed.name);
  if (result != TC_TLV_OK)
    return result;
  result = text_field(&reader, PRINTED_EMPLOYEE_TAG, PRINTED_AFFILIATION_MAX, 1,
                      &parsed.employee_affiliation);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_field(&reader, PRINTED_EXPIRATION_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!printed_date(element.value, profile, &parsed.expiration))
    return TC_TLV_INVALID;
  parsed.expiration_text = element.value;
  result = tc_pki_field(&reader, PRINTED_SERIAL_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC) {
    if (element.value.length != TWIC_SERIAL_LENGTH ||
        !tc_credential_digits(element.value.data, element.value.length))
      return TC_TLV_INVALID;
  } else if (!printable(element.value, PRINTED_SERIAL_MAX, 0))
    return TC_TLV_INVALID;
  parsed.card_serial_number = element.value;
  result = tc_pki_field(&reader, PRINTED_ISSUER_TAG, &element);
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
      (reader.offset < input.length && input.data[reader.offset] == PRINTED_ORGANIZATION_1_TAG)) {
    result = text_field(&reader, PRINTED_ORGANIZATION_1_TAG, PRINTED_AFFILIATION_MAX, 1,
                        &parsed.organization_1);
    if (result != TC_TLV_OK)
      return result;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_TWIC ||
      (reader.offset < input.length && input.data[reader.offset] == PRINTED_ORGANIZATION_2_TAG)) {
    result = text_field(&reader, PRINTED_ORGANIZATION_2_TAG, PRINTED_AFFILIATION_MAX, 1,
                        &parsed.organization_2);
    if (result != TC_TLV_OK)
      return result;
  }
  if (profile == TC_PIV_PRINTED_PROFILE_PIV) {
    result = tc_pki_field(&reader, PRINTED_ERROR_TAG, &element);
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

TC_TLV_result TC_PIV_printed_expiration_check(const TC_PIV_printed* printed,
                                              TC_bytes chuid_expiration, const TC_X509_time* at,
                                              int* valid)
{
  if (!printed || !at || !valid || !chuid_expiration.data || chuid_expiration.length != 8)
    return TC_TLV_ARGUMENT;
  unsigned year, parsed_month, day;
  if (!tc_credential_yyyymmdd(chuid_expiration.data, chuid_expiration.length, &year, &parsed_month,
                              &day))
    return TC_TLV_INVALID;
  TC_X509_time expires = printed->expiration;
  expires.hour = 23;
  expires.minute = 59;
  expires.second = 59;
  int order;
  TC_TLV_result result = TC_X509_time_compare(at, &expires, &order);
  if (result != TC_TLV_OK)
    return result;
  *valid = printed->expiration.year == year && printed->expiration.month == parsed_month &&
           printed->expiration.day == day && order <= 0;
  return TC_TLV_OK;
}
#endif
