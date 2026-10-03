/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>

#if TC_ENABLE_PIV_OBJECTS || TC_ENABLE_PIV_CHUID || TC_ENABLE_TWIC_CCL || TC_ENABLE_TWIC_TPK ||    \
    TC_ENABLE_AAMVA
#include "credential_text_internal.h"
#include "internal.h"
#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define TC_TEXT_STORAGE PROGMEM
#define TC_TEXT_READ(value) pgm_read_byte(value)
#else
#define TC_TEXT_STORAGE
#define TC_TEXT_READ(value) (*(value))
#endif

int tc_credential_hex_digit(uint8_t value)
{
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  return -1;
}

int tc_credential_digits(const uint8_t* value, size_t length)
{
  if (!value || !length)
    return 0;
  for (size_t i = 0; i < length; ++i)
    if (value[i] < '0' || value[i] > '9')
      return 0;
  return 1;
}

int tc_credential_decimal(const uint8_t* value, size_t length, size_t maximum, size_t* out)
{
  size_t result = 0;
  if (!out || !tc_credential_digits(value, length))
    return 0;
  for (size_t i = 0; i < length; ++i) {
    const size_t digit = (size_t)(value[i] - '0');
    if (digit > maximum || result > (maximum - digit) / 10)
      return 0;
    result = 10 * result + digit;
  }
  *out = result;
  return 1;
}

unsigned tc_credential_month3(const uint8_t value[3], int title_case)
{
  static const uint8_t names[] TC_TEXT_STORAGE = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
  for (unsigned month = 0; month < 12; ++month) {
    unsigned character;
    for (character = 0; character < 3; ++character) {
      uint8_t expected = TC_TEXT_READ(names + 3 * month + character);
      if (title_case && character != 0)
        expected += 'a' - 'A';
      if (value[character] != expected)
        break;
    }
    if (character == 3)
      return month + 1;
  }
  return 0;
}

int tc_credential_day_month_year(const uint8_t value[9], int title_case, TC_X509_time* out)
{
  size_t day, year;
  unsigned month;
  if (!value || !out || !tc_credential_decimal(value, 2, 31, &day) ||
      !tc_credential_decimal(value + 5, 4, 9999, &year))
    return 0;
  month = tc_credential_month3(value + 2, title_case);
  if (!tc_internal_calendar_date((unsigned)year, month, (unsigned)day))
    return 0;
  *out = (TC_X509_time){(unsigned)year, (uint8_t)month, (uint8_t)day, 0, 0, 0};
  return 1;
}

int tc_credential_yyyymmdd(const uint8_t* value, size_t length, unsigned* year, unsigned* month,
                           unsigned* day)
{
  size_t y, m, d;
  if (length != 8 || !tc_credential_decimal(value, 4, 9999, &y) ||
      !tc_credential_decimal(value + 4, 2, 12, &m) || !tc_credential_decimal(value + 6, 2, 31, &d))
    return 0;
  if (!tc_internal_calendar_date((unsigned)y, (unsigned)m, (unsigned)d))
    return 0;
  if (year)
    *year = (unsigned)y;
  if (month)
    *month = (unsigned)m;
  if (day)
    *day = (unsigned)d;
  return 1;
}
#endif
