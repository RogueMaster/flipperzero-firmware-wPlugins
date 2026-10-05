/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The machine readable zone.
 *
 * The specimens are the published ones: the TD3 zone of ICAO Doc 9303 part 4
 * and the TD1 and TD2 zones of part 5, all for ANNA MARIA ERIKSSON of Utopia.
 * They matter because every check digit in them, the composite one included,
 * was computed by the issuing authority and not by this code - which is the
 * only way a test of a check digit means anything.
 *
 * The MRZ information vectors are ICAO 9303-11 appendix D.2.
 */

#include "emrtd_test.h"

#include "../../protocol/emrtd_mrz.h"

/* ICAO 9303-4, the TD3 specimen. */
static const char* const td3_line1 = "P<UTOERIKSSON<<ANNA<MARIA<<<<<<<<<<<<<<<<<<<";
static const char* const td3_line2 = "L898902C36UTO7408122F1204159ZE184226B<<<<<10";

/* ICAO 9303-5, the TD1 specimen. */
static const char* const td1 = "I<UTOD231458907<<<<<<<<<<<<<<<"
                               "7408122F1204159UTO<<<<<<<<<<<6"
                               "ERIKSSON<<ANNA<MARIA<<<<<<<<<<";

/* ICAO 9303-5, the TD2 specimen. */
static const char* const td2 = "I<UTOERIKSSON<<ANNA<MARIA<<<<<<<<<<<"
                               "D231458907UTO7408122F1204159<<<<<<<6";

/* A TD1 whose document number does not fit its field, 9303-5 section 4.2.4. */
static const char* const td1_extended = "I<UTOD23145890<7349<<<<<<<<<<<"
                                        "7408122F1204159UTO<<<<<<<<<<<6"
                                        "ERIKSSON<<ANNA<MARIA<<<<<<<<<<";

/*
 * Copy a value into one of the fixed credential fields, always terminated.
 *
 * strncpy() with a bound of exactly the field size minus one is correct here
 * only because the structure was cleared first, and GCC rightly refuses to
 * take that on trust when the value is as long as the bound - a date is
 * exactly six characters in a seven byte field. Saying what is meant is
 * shorter than explaining it.
 */
static void credentials_field_set(char* field, size_t size, const char* value) {
    const size_t len = strlen(value);
    const size_t copied = len < size ? len : size - 1;
    memcpy(field, value, copied);
    field[copied] = '\0';
}

static void credentials_set(
    EmrtdCredentials* credentials,
    const char* number,
    const char* birth,
    const char* expiry) {
    memset(credentials, 0, sizeof(*credentials));
    credentials_field_set(
        credentials->document_number, sizeof(credentials->document_number), number);
    credentials_field_set(credentials->date_of_birth, sizeof(credentials->date_of_birth), birth);
    credentials_field_set(
        credentials->date_of_expiry, sizeof(credentials->date_of_expiry), expiry);
}

static void test_char_values(void) {
    emrtd_test_begin("the character values of 9303-3 section 4.9");

    TEST_EQ_INT(emrtd_mrz_char_value('<'), 0);
    TEST_EQ_INT(emrtd_mrz_char_value('0'), 0);
    TEST_EQ_INT(emrtd_mrz_char_value('9'), 9);
    TEST_EQ_INT(emrtd_mrz_char_value('A'), 10);
    TEST_EQ_INT(emrtd_mrz_char_value('Z'), 35);

    emrtd_test_begin("anything else has no value at all");
    TEST_EQ_INT(emrtd_mrz_char_value('a'), -1);
    TEST_EQ_INT(emrtd_mrz_char_value('-'), -1);
    TEST_EQ_INT(emrtd_mrz_char_value(' '), -1);
    TEST_EQ_INT(emrtd_mrz_char_value('\0'), -1);
    TEST_EQ_INT(emrtd_mrz_char_value((char)0xC3), -1);
}

static void test_check_digits(void) {
    emrtd_test_begin("the check digits of ICAO 9303-11 appendix D.2");

    TEST_EQ_INT(emrtd_mrz_check_digit("L898902C<", 9), '3');
    TEST_EQ_INT(emrtd_mrz_check_digit("690806", 6), '1');
    TEST_EQ_INT(emrtd_mrz_check_digit("940623", 6), '6');

    emrtd_test_begin("the check digits printed on the TD3 specimen");
    TEST_EQ_INT(emrtd_mrz_check_digit(td3_line2, 9), td3_line2[9]);
    TEST_EQ_INT(emrtd_mrz_check_digit(td3_line2 + 13, 6), td3_line2[19]);
    TEST_EQ_INT(emrtd_mrz_check_digit(td3_line2 + 21, 6), td3_line2[27]);
    TEST_EQ_INT(emrtd_mrz_check_digit(td3_line2 + 28, 14), td3_line2[42]);

    emrtd_test_begin("an invalid character has no check digit");
    TEST_EQ_INT(emrtd_mrz_check_digit("ABC-123", 7), 0);
    TEST_EQ_INT(emrtd_mrz_check_digit(NULL, 4), 0);
}

static void test_mrz_information(void) {
    emrtd_test_begin("the MRZ information of ICAO 9303-11 appendix D.2");

    EmrtdCredentials credentials;
    char out[EMRTD_MRZ_INFO_MAX];

    credentials_set(&credentials, "L898902C<", "690806", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorNone);
    TEST_EQ_STR(out, "L898902C<369080619406236");

    emrtd_test_begin("a number entered without its filler gives the same key");
    credentials_set(&credentials, "L898902C", "690806", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorNone);
    TEST_EQ_STR(out, "L898902C<369080619406236");

    emrtd_test_begin("a number entered in lower case gives the same key");
    credentials_set(&credentials, "l898902c", "690806", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorNone);
    TEST_EQ_STR(out, "L898902C<369080619406236");

    emrtd_test_begin("a number longer than its field is weighed whole");
    /* The extended case of appendix D.2, where no padding takes place. */
    credentials_set(&credentials, "D23145890734", "340712", "950712");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorNone);
    TEST_EQ_STR(out, "D23145890734934071279507122");

    emrtd_test_begin("a number of exactly nine characters is not padded");
    credentials_set(&credentials, "D23145890", "740812", "120415");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorNone);
    TEST_EQ_STR(out, "D23145890774081221204159");
}

static void test_mrz_information_refusals(void) {
    emrtd_test_begin("incomplete credentials are refused");

    EmrtdCredentials credentials;
    char out[EMRTD_MRZ_INFO_MAX];

    credentials_set(&credentials, "", "690806", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);
    TEST_EQ_STR(out, "");

    credentials_set(&credentials, "L898902C", "69080", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);

    credentials_set(&credentials, "L898902C", "6908O6", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);

    credentials_set(&credentials, "L898902C", "690806", "94062");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);

    emrtd_test_begin("a character that cannot appear in an MRZ is refused");
    credentials_set(&credentials, "L8989-2C", "690806", "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);

    emrtd_test_begin("a buffer too small is refused, not overrun");
    credentials_set(&credentials, "L898902C", "690806", "940623");
    char small[24];
    TEST_EQ_INT(
        emrtd_mrz_information(&credentials, small, sizeof(small)), EmrtdErrorBufferTooSmall);
    TEST_EQ_STR(small, "");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, small, 0), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_mrz_information(NULL, out, sizeof(out)), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_mrz_information(&credentials, NULL, sizeof(out)), EmrtdErrorInvalidInput);

    emrtd_test_begin("a document number that fills its field entirely is refused");
    /* The field is not terminated, so it cannot be read as a string. */
    memset(&credentials, 0, sizeof(credentials));
    memset(credentials.document_number, 'A', sizeof(credentials.document_number));
    credentials_field_set(credentials.date_of_birth, sizeof(credentials.date_of_birth), "690806");
    credentials_field_set(
        credentials.date_of_expiry, sizeof(credentials.date_of_expiry), "940623");
    TEST_EQ_INT(emrtd_mrz_information(&credentials, out, sizeof(out)), EmrtdErrorInvalidInput);
}

static void test_parse_td3(void) {
    emrtd_test_begin("the TD3 specimen of ICAO 9303-4");

    char raw[89];
    memcpy(raw, td3_line1, 44);
    memcpy(raw + 44, td3_line2, 44);
    raw[88] = '\0';

    EmrtdMrz mrz;
    TEST_EQ_INT(emrtd_mrz_parse(raw, 88, &mrz), EmrtdErrorNone);
    TEST_EQ_INT(mrz.format, EmrtdMrzFormatTd3);
    TEST_EQ_STR(mrz.document_type, "P");
    TEST_EQ_STR(mrz.issuing_state, "UTO");
    TEST_EQ_STR(mrz.document_number, "L898902C3");
    TEST_EQ_STR(mrz.nationality, "UTO");
    TEST_EQ_STR(mrz.date_of_birth, "740812");
    TEST_EQ_STR(mrz.sex, "F");
    TEST_EQ_STR(mrz.date_of_expiry, "120415");
    TEST_EQ_STR(mrz.surname, "ERIKSSON");
    TEST_EQ_STR(mrz.given_names, "ANNA MARIA");
    TEST_EQ_STR(mrz.optional_data, "ZE184226B");
    TEST_CHECK(mrz.check_digits_valid);

    emrtd_test_begin("the full name is surname first");
    char name[64];
    emrtd_mrz_full_name(&mrz, name, sizeof(name));
    TEST_EQ_STR(name, "ERIKSSON, ANNA MARIA");

    emrtd_test_begin("a name longer than the buffer is cut, not overflowed");
    char tiny[6];
    emrtd_mrz_full_name(&mrz, tiny, sizeof(tiny));
    TEST_EQ_STR(tiny, "ERIKS");
    emrtd_mrz_full_name(&mrz, tiny, 1);
    TEST_EQ_STR(tiny, "");

    emrtd_test_begin("line breaks and spaces in the input are ignored");
    char spaced[96];
    size_t pos = 0;
    memcpy(spaced + pos, td3_line1, 44);
    pos += 44;
    spaced[pos++] = '\n';
    memcpy(spaced + pos, td3_line2, 44);
    pos += 44;
    spaced[pos++] = '\r';
    spaced[pos++] = '\n';
    EmrtdMrz again;
    TEST_EQ_INT(emrtd_mrz_parse(spaced, pos, &again), EmrtdErrorNone);
    TEST_EQ_STR(again.document_number, "L898902C3");
    TEST_CHECK(again.check_digits_valid);

    emrtd_test_begin("a single wrong character is caught by a check digit");
    raw[5] = 'X'; /* Inside the name, which the composite digit does not cover. */
    TEST_EQ_INT(emrtd_mrz_parse(raw, 88, &mrz), EmrtdErrorNone);
    TEST_CHECK(mrz.check_digits_valid);
    raw[44] = 'M'; /* The first character of the document number. */
    TEST_EQ_INT(emrtd_mrz_parse(raw, 88, &mrz), EmrtdErrorNone);
    TEST_CHECK(!mrz.check_digits_valid);

    emrtd_test_begin("a composite digit that does not match is caught");
    memcpy(raw + 44, td3_line2, 44);
    raw[87] = (char)(raw[87] == '0' ? '1' : '0');
    TEST_EQ_INT(emrtd_mrz_parse(raw, 88, &mrz), EmrtdErrorNone);
    TEST_CHECK(!mrz.check_digits_valid);
}

static void test_parse_td1_and_td2(void) {
    emrtd_test_begin("the TD1 specimen of ICAO 9303-5");

    EmrtdMrz mrz;
    TEST_EQ_INT(emrtd_mrz_parse(td1, 90, &mrz), EmrtdErrorNone);
    TEST_EQ_INT(mrz.format, EmrtdMrzFormatTd1);
    TEST_EQ_STR(mrz.document_type, "I");
    TEST_EQ_STR(mrz.issuing_state, "UTO");
    TEST_EQ_STR(mrz.document_number, "D23145890");
    TEST_EQ_STR(mrz.date_of_birth, "740812");
    TEST_EQ_STR(mrz.sex, "F");
    TEST_EQ_STR(mrz.date_of_expiry, "120415");
    TEST_EQ_STR(mrz.nationality, "UTO");
    TEST_EQ_STR(mrz.surname, "ERIKSSON");
    TEST_EQ_STR(mrz.given_names, "ANNA MARIA");
    TEST_CHECK(mrz.check_digits_valid);

    emrtd_test_begin("the TD2 specimen of ICAO 9303-5");
    TEST_EQ_INT(emrtd_mrz_parse(td2, 72, &mrz), EmrtdErrorNone);
    TEST_EQ_INT(mrz.format, EmrtdMrzFormatTd2);
    TEST_EQ_STR(mrz.document_type, "I");
    TEST_EQ_STR(mrz.issuing_state, "UTO");
    TEST_EQ_STR(mrz.document_number, "D23145890");
    TEST_EQ_STR(mrz.nationality, "UTO");
    TEST_EQ_STR(mrz.date_of_birth, "740812");
    TEST_EQ_STR(mrz.date_of_expiry, "120415");
    TEST_EQ_STR(mrz.surname, "ERIKSSON");
    TEST_EQ_STR(mrz.given_names, "ANNA MARIA");
    TEST_CHECK(mrz.check_digits_valid);

    emrtd_test_begin("a TD1 number that runs into the optional data");
    TEST_EQ_INT(emrtd_mrz_parse(td1_extended, 90, &mrz), EmrtdErrorNone);
    TEST_EQ_STR(mrz.document_number, "D23145890734");
    TEST_CHECK(mrz.check_digits_valid);

    emrtd_test_begin("the extended number derives the key the chip expects");
    EmrtdCredentials credentials;
    char info[EMRTD_MRZ_INFO_MAX];
    memset(&credentials, 0, sizeof(credentials));
    credentials_field_set(
        credentials.document_number, sizeof(credentials.document_number), mrz.document_number);
    credentials_field_set(
        credentials.date_of_birth, sizeof(credentials.date_of_birth), mrz.date_of_birth);
    credentials_field_set(
        credentials.date_of_expiry, sizeof(credentials.date_of_expiry), mrz.date_of_expiry);
    TEST_EQ_INT(emrtd_mrz_information(&credentials, info, sizeof(info)), EmrtdErrorNone);
    TEST_EQ_STR(info, "D23145890734974081221204159");
}

static void test_parse_refusals(void) {
    emrtd_test_begin("a zone of no recognised length is refused");

    EmrtdMrz mrz;
    TEST_EQ_INT(emrtd_mrz_parse("MRZ TOO SHORT", 13, &mrz), EmrtdErrorParse);
    TEST_EQ_INT(mrz.format, EmrtdMrzFormatUnknown);
    TEST_EQ_INT(emrtd_mrz_parse(td3_line1, 44, &mrz), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_mrz_parse(td1, 89, &mrz), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_mrz_parse("", 0, &mrz), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_mrz_parse(NULL, 88, &mrz), EmrtdErrorInvalidInput);

    emrtd_test_begin("a zone longer than any format is refused before it is copied");
    char oversized[256];
    memset(oversized, '<', sizeof(oversized));
    TEST_EQ_INT(emrtd_mrz_parse(oversized, sizeof(oversized), &mrz), EmrtdErrorParse);

    emrtd_test_begin("a zone of the right length but full of rubbish parses without digits");
    char rubbish[88];
    memset(rubbish, '*', sizeof(rubbish));
    TEST_EQ_INT(emrtd_mrz_parse(rubbish, sizeof(rubbish), &mrz), EmrtdErrorNone);
    TEST_CHECK(!mrz.check_digits_valid);
}

static void test_format_date(void) {
    emrtd_test_begin("dates are shown day first with a guessed century");

    char out[16];
    emrtd_mrz_format_date("740812", false, out, sizeof(out));
    TEST_EQ_STR(out, "12.08.1974");
    emrtd_mrz_format_date("050101", false, out, sizeof(out));
    TEST_EQ_STR(out, "01.01.2005");
    emrtd_mrz_format_date("301231", false, out, sizeof(out));
    TEST_EQ_STR(out, "31.12.2030");
    emrtd_mrz_format_date("310101", false, out, sizeof(out));
    TEST_EQ_STR(out, "01.01.1931");

    emrtd_test_begin("an expiry is read as this century much further out");
    emrtd_mrz_format_date("120415", true, out, sizeof(out));
    TEST_EQ_STR(out, "15.04.2012");
    emrtd_mrz_format_date("350101", true, out, sizeof(out));
    TEST_EQ_STR(out, "01.01.2035");
    emrtd_mrz_format_date("990101", true, out, sizeof(out));
    TEST_EQ_STR(out, "01.01.1999");

    emrtd_test_begin("a date that is not six digits is shown as it stands");
    emrtd_mrz_format_date("74O812", false, out, sizeof(out));
    TEST_EQ_STR(out, "74O812");
    emrtd_mrz_format_date("", false, out, sizeof(out));
    TEST_EQ_STR(out, "");

    emrtd_test_begin("a buffer too small takes what fits");
    char small[4];
    emrtd_mrz_format_date("740812", false, small, sizeof(small));
    TEST_EQ_STR(small, "740");
    emrtd_mrz_format_date("740812", false, small, 0);
    emrtd_mrz_format_date(NULL, false, small, sizeof(small));
    TEST_EQ_STR(small, "");
}

void test_suite_mrz(void) {
    test_char_values();
    test_check_digits();
    test_mrz_information();
    test_mrz_information_refusals();
    test_parse_td3();
    test_parse_td1_and_td2();
    test_parse_refusals();
    test_format_date();
}
