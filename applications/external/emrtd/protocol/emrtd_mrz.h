/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The machine readable zone, ICAO Doc 9303 part 3.
 *
 * Two jobs: the check digit arithmetic, and assembling the "MRZ information"
 * that both BAC and PACE derive their keys from,
 *
 *     K = DocumentNumber || cd || DateOfBirth || cd || DateOfExpiry || cd
 *
 * and parsing a full MRZ once DG1 has been read.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../access/emrtd_access.h"
#include "../emrtd_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest MRZ information string: 12 + 1 + 6 + 1 + 6 + 1 plus room. */
#define EMRTD_MRZ_INFO_MAX 32

/** The three MRZ layouts. */
typedef enum {
    EmrtdMrzFormatUnknown,
    EmrtdMrzFormatTd1, /**< 3 lines of 30, identity cards. */
    EmrtdMrzFormatTd2, /**< 2 lines of 36. */
    EmrtdMrzFormatTd3, /**< 2 lines of 44, passports. */
} EmrtdMrzFormat;

/** A fully parsed MRZ. */
typedef struct {
    EmrtdMrzFormat format;
    char document_type[3];
    char issuing_state[4];
    char document_number[EMRTD_DOC_NUMBER_MAX + 1];
    char date_of_birth[EMRTD_DATE_LEN + 1];
    char date_of_expiry[EMRTD_DATE_LEN + 1];
    char sex[2];
    char nationality[4];
    char surname[40];
    char given_names[40];
    char optional_data[16];
    bool check_digits_valid;
} EmrtdMrz;

/** Value of one MRZ character in the check digit sum. Returns -1 if invalid. */
int emrtd_mrz_char_value(char c);

/** The ICAO 9303-3 check digit over @p len characters. Returns '0'..'9', or 0 on error. */
char emrtd_mrz_check_digit(const char* data, size_t len);

/**
 * Build the MRZ information string the key derivation hashes.
 *
 * Document numbers longer than nine characters take their check digit over
 * the whole number, which is the extended TD1 and TD2 case of appendix D.2.
 *
 * @param[out] out  at least EMRTD_MRZ_INFO_MAX bytes
 */
EmrtdError emrtd_mrz_information(const EmrtdCredentials* credentials, char* out, size_t out_size);

/** Parse an MRZ given as one run of characters, 90, 72 or 88 long. */
EmrtdError emrtd_mrz_parse(const char* raw, size_t len, EmrtdMrz* out);

/** "Surname, Given Names" into @p out. */
void emrtd_mrz_full_name(const EmrtdMrz* mrz, char* out, size_t out_size);

/** Render a YYMMDD field as "DD.MM.YYYY", guessing the century. */
void emrtd_mrz_format_date(const char* yymmdd, bool is_expiry, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif
