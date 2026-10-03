/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_CREDENTIAL_TEXT_INTERNAL_H_
#define TC_CREDENTIAL_TEXT_INTERNAL_H_

#include <stdint.h>
#include <stddef.h>
#include <tiny_crypto/x509.h>

/* Printed and encoded credential text helpers shared by PIV, TWIC and AAMVA
 * readers. Inputs are borrowed and only read. Outputs change on success only. */

int tc_credential_hex_digit(uint8_t value);

/* Return 1 when value holds one or more ASCII digits. */
int tc_credential_digits(const uint8_t* value, size_t length);

/* Parse one or more ASCII digits as an unsigned decimal no greater than
 * maximum. Return 0 on a non-digit, an empty span or a value above maximum.
 * Validate digit strings that may exceed size_t with tc_credential_digits. */
int tc_credential_decimal(const uint8_t* value, size_t length, size_t maximum, size_t* out);

/* title_case selects Jan, otherwise JAN. Both formats require exact case. */
unsigned tc_credential_month3(const uint8_t value[3], int title_case);

/* Parse a full Gregorian DDMMMYYYY date such as 01JAN2026. title_case selects
 * the month case as in tc_credential_month3. out receives the date at
 * 00:00:00. */
int tc_credential_day_month_year(const uint8_t value[9], int title_case, TC_X509_time* out);

/* Parse a full Gregorian YYYYMMDD date. Output pointers may be null. */
int tc_credential_yyyymmdd(const uint8_t* value, size_t length, unsigned* year, unsigned* month,
                           unsigned* day);

#endif
