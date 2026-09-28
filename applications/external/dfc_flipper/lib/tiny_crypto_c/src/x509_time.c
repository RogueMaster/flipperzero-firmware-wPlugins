/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509
#include "x509_time_internal.h"
#include "pki_internal.h"
#include "internal.h"

static int time_valid(const TC_X509_time* value)
{
  return value->year <= 9999 && tc_pki_date(value->year, value->month, value->day)
    && value->hour <= 23 && value->minute <= 59 && value->second <= 59;
}

static int time_compare(const TC_X509_time* left, const TC_X509_time* right)
{
  const unsigned a[] = {left->year,left->month,left->day,left->hour,left->minute,left->second};
  const unsigned b[] = {right->year,right->month,right->day,right->hour,right->minute,right->second};
  size_t i;
  for (i = 0; i < sizeof a / sizeof a[0]; ++i)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}

TC_TLV_result TC_X509_time_to_unix(const TC_X509_time* value, int64_t* seconds)
{
  static const unsigned before_month[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  enum { DAYS_TO_UNIX_EPOCH = 719162, SECONDS_PER_DAY = 86400,
    SECONDS_PER_HOUR = 3600, SECONDS_PER_MINUTE = 60 };
  if (!value || !seconds ||
      !tc_internal_ranges_disjoint(value,sizeof *value,seconds,sizeof *seconds)) return TC_TLV_ARGUMENT;
  if (!time_valid(value)) return TC_TLV_INVALID;
  const unsigned previous_year = value->year - 1;
  /* Count complete Gregorian years, then the elapsed days in this year. */
  int64_t days = (int64_t)previous_year * 365 + previous_year / 4 - previous_year / 100 +
      previous_year / 400 + before_month[value->month - 1] + value->day - 1;
  if (value->month > 2 && tc_pki_date(value->year,2,29)) ++days;
  *seconds = (days - DAYS_TO_UNIX_EPOCH) * SECONDS_PER_DAY +
      (int64_t)value->hour * SECONDS_PER_HOUR + value->minute * SECONDS_PER_MINUTE + value->second;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_time_compare(const TC_X509_time* left, const TC_X509_time* right, int* order)
{
  if (!left || !right || !order
      || !tc_internal_ranges_disjoint(order, sizeof(*order), left, sizeof(*left))
      || !tc_internal_ranges_disjoint(order, sizeof(*order), right, sizeof(*right))) return TC_TLV_ARGUMENT;
  if (!time_valid(left) || !time_valid(right)) return TC_TLV_INVALID;
  *order = time_compare(left, right);
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_valid_at(const TC_X509_certificate* certificate, const TC_X509_time* at, int* valid)
{
  if (!certificate || !at || !valid
      || !tc_internal_ranges_disjoint(valid, sizeof(*valid), certificate, sizeof(*certificate))
      || !tc_internal_ranges_disjoint(valid, sizeof(*valid), at, sizeof(*at))
      || !tc_internal_ranges_disjoint(valid, sizeof(*valid), certificate->encoded.data, certificate->encoded.length))
    return TC_TLV_ARGUMENT;
  if (!time_valid(at) || !time_valid(&certificate->not_before) || !time_valid(&certificate->not_after)
      || time_compare(&certificate->not_before, &certificate->not_after) > 0) return TC_TLV_INVALID;
  *valid = time_compare(&certificate->not_before, at) <= 0 && time_compare(at, &certificate->not_after) <= 0;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_time_value(const TC_TLV_element* element, TC_X509_time* out)
{
  TC_X509_time parsed;
  const uint8_t* p = element->value.data;
  unsigned parts[7] = {0}, i, digits;
  int generalized = tc_pki_tag(element, 0x18);
  if (!generalized && !tc_pki_tag(element, 0x17)) return TC_TLV_INVALID;
  digits = generalized ? 14 : 12;
  if (element->value.length != digits + 1 || p[digits] != 'Z') return TC_TLV_INVALID;
  for (i = 0; i < digits; ++i) {
    if (p[i] < '0' || p[i] > '9') return TC_TLV_INVALID;
    parts[i / 2] = parts[i / 2] * 10 + p[i] - '0';
  }
  if (generalized) parts[1] += parts[0] * 100;
  else parts[0] += parts[0] < 50 ? 2000 : 1900;
  i = generalized ? 1 : 0;
  /* RFC 5280 section 4.1.2.5 requires readers to accept either time type. */
  parsed.year = parts[i]; parsed.month = (uint8_t)parts[i + 1]; parsed.day = (uint8_t)parts[i + 2];
  parsed.hour = (uint8_t)parts[i + 3]; parsed.minute = (uint8_t)parts[i + 4]; parsed.second = (uint8_t)parts[i + 5];
  if (!time_valid(&parsed)) return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

#endif
