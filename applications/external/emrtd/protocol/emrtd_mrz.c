/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The machine readable zone, ICAO Doc 9303 parts 3, 4 and 5.
 *
 * Two of the things here are load bearing and easy to get subtly wrong. The
 * first is that a check digit is computed over the field exactly as printed,
 * filler characters included, which is why the nine character document number
 * field is padded before it is weighed. The second is the composite check
 * digit, which spans several disjoint stretches of the last line and keeps one
 * running weight across all of them; the ranges below are the ones in 9303-4
 * section 4.2.4 and 9303-5 section 4.2.4, and the test suite pins them against
 * the published specimens.
 */

#include "emrtd_mrz.h"

#include <string.h>

/** The weights repeat 7, 3, 1 across the whole field (9303-3, section 4.9). */
static const int emrtd_mrz_weights[3] = {7, 3, 1};

/**
 * Where a two digit year of birth stops being this century.
 *
 * The MRZ gives no century and the reader has no trustworthy clock, so the
 * split is a convention rather than a fact: a birth year above the pivot is
 * read as the twentieth century. It only ever affects what is displayed - the
 * key derivation uses the two digits as printed.
 */
#define EMRTD_MRZ_BIRTH_PIVOT 30

/** An expiry this far ahead is read as the twentieth century instead. */
#define EMRTD_MRZ_EXPIRY_PIVOT 80

/** One stretch of characters taking part in a check digit. */
typedef struct {
    const char* data;
    size_t len;
} EmrtdMrzRange;

int emrtd_mrz_char_value(char c) {
    if(c == '<') {
        return 0;
    }
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1;
}

/** Weigh several stretches as one field, which is what a composite digit is. */
static char mrz_check_digit_ranges(const EmrtdMrzRange* ranges, size_t count) {
    int total = 0;
    size_t index = 0;

    for(size_t r = 0; r < count; r++) {
        if(ranges[r].data == NULL) {
            return 0;
        }
        for(size_t i = 0; i < ranges[r].len; i++) {
            const int value = emrtd_mrz_char_value(ranges[r].data[i]);
            if(value < 0) {
                return 0;
            }
            total += value * emrtd_mrz_weights[index % 3];
            index++;
        }
    }
    return (char)('0' + (total % 10));
}

char emrtd_mrz_check_digit(const char* data, size_t len) {
    if(data == NULL) {
        return 0;
    }
    const EmrtdMrzRange range = {data, len};
    return mrz_check_digit_ranges(&range, 1);
}

/** True when a field carries nothing but filler. */
static bool mrz_all_filler(const char* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(data[i] != '<') {
            return false;
        }
    }
    return true;
}

/**
 * Verify one printed check digit.
 *
 * A filler in the check digit position is only correct for a field that is
 * itself all filler; elsewhere it means the document number continues in the
 * optional data, which the caller has already dealt with.
 */
static bool mrz_check_digit_ok(const char* data, size_t len, char printed) {
    if(printed == '<') {
        return mrz_all_filler(data, len);
    }
    const char computed = emrtd_mrz_check_digit(data, len);
    return computed != 0 && computed == printed;
}

/** Copy a fixed width field, dropping the filler characters. */
static void mrz_copy_compact(char* dst, size_t dst_size, const char* src, size_t len) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    size_t pos = 0;
    for(size_t i = 0; i < len && pos + 1 < dst_size; i++) {
        if(src[i] != '<') {
            dst[pos++] = src[i];
        }
    }
    dst[pos] = '\0';
}

/**
 * Copy a name field.
 *
 * The identifiers are separated by a double filler and their parts by a
 * single one (9303-3, section 4.6), so a run of two or more becomes the
 * separator between surname and given names and a single one a space.
 */
static void mrz_copy_name(
    const char* src,
    size_t len,
    char* surname,
    size_t surname_size,
    char* given,
    size_t given_size) {
    if(surname != NULL && surname_size > 0) {
        surname[0] = '\0';
    }
    if(given != NULL && given_size > 0) {
        given[0] = '\0';
    }

    /* The separator is the first run of two fillers, if there is one. */
    size_t split = len;
    for(size_t i = 0; i + 1 < len; i++) {
        if(src[i] == '<' && src[i + 1] == '<') {
            split = i;
            break;
        }
    }

    /* Within an identifier a single filler stands for a space. */
    size_t pos = 0;
    for(size_t i = 0; i < split && surname != NULL && pos + 1 < surname_size; i++) {
        const char c = src[i];
        if(c == '<') {
            if(pos > 0) {
                surname[pos++] = ' ';
            }
        } else {
            surname[pos++] = c;
        }
    }
    while(pos > 0 && surname != NULL && surname[pos - 1] == ' ') {
        pos--;
    }
    if(surname != NULL && surname_size > 0) {
        surname[pos] = '\0';
    }

    if(split >= len) {
        return;
    }

    size_t start = split;
    while(start < len && src[start] == '<') {
        start++;
    }

    pos = 0;
    for(size_t i = start; i < len && given != NULL && pos + 1 < given_size; i++) {
        const char c = src[i];
        if(c == '<') {
            if(pos > 0) {
                given[pos++] = ' ';
            }
        } else {
            given[pos++] = c;
        }
    }
    while(pos > 0 && given != NULL && given[pos - 1] == ' ') {
        pos--;
    }
    if(given != NULL && given_size > 0) {
        given[pos] = '\0';
    }
}

/** True when @p len characters are all digits. */
static bool mrz_all_digits(const char* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(data[i] < '0' || data[i] > '9') {
            return false;
        }
    }
    return true;
}

/*
 * strnlen() is listed in the firmware's API table but disabled there, so a
 * .fap that calls it will not load. The credentials are small fixed arrays,
 * so a bounded length of our own costs nothing.
 */
static size_t mrz_bounded_len(const char* text, size_t max) {
    size_t n = 0;
    while(n < max && text[n] != '\0') {
        n++;
    }
    return n;
}

EmrtdError emrtd_mrz_information(const EmrtdCredentials* credentials, char* out, size_t out_size) {
    if(credentials == NULL || out == NULL || out_size == 0) {
        return EmrtdErrorInvalidInput;
    }

    /*
     * Terminated before anything else, so that a caller who prints the result
     * without looking at the error shows nothing rather than stack contents.
     */
    out[0] = '\0';

    const size_t doc_len =
        mrz_bounded_len(credentials->document_number, sizeof(credentials->document_number));
    if(doc_len == 0 || doc_len >= sizeof(credentials->document_number)) {
        return EmrtdErrorInvalidInput;
    }
    if(mrz_bounded_len(credentials->date_of_birth, sizeof(credentials->date_of_birth)) !=
           EMRTD_DATE_LEN ||
       mrz_bounded_len(credentials->date_of_expiry, sizeof(credentials->date_of_expiry)) !=
           EMRTD_DATE_LEN) {
        return EmrtdErrorInvalidInput;
    }
    if(!mrz_all_digits(credentials->date_of_birth, EMRTD_DATE_LEN) ||
       !mrz_all_digits(credentials->date_of_expiry, EMRTD_DATE_LEN)) {
        return EmrtdErrorInvalidInput;
    }

    /*
     * The number is upper cased into scratch: it is part of the password the
     * session keys come from, so the copy is wiped again on every exit path.
     */
    char doc[EMRTD_DOC_NUMBER_MAX + 1];
    EmrtdError error = EmrtdErrorNone;
    size_t field_len = doc_len;

    memset(doc, 0, sizeof(doc));
    for(size_t i = 0; i < doc_len; i++) {
        char c = credentials->document_number[i];
        if(c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if(emrtd_mrz_char_value(c) < 0) {
            error = EmrtdErrorInvalidInput;
            goto done;
        }
        doc[i] = c;
    }

    /*
     * ICAO 9303-11 appendix D.2: up to nine characters the number is padded
     * with fillers to the width of the field and the check digit is taken
     * over the padded field. A longer number, which only TD1 and TD2 can
     * carry, is weighed whole.
     */
    if(doc_len < 9) {
        for(size_t i = doc_len; i < 9; i++) {
            doc[i] = '<';
        }
        field_len = 9;
    }

    const char doc_cd = emrtd_mrz_check_digit(doc, field_len);
    const char dob_cd = emrtd_mrz_check_digit(credentials->date_of_birth, EMRTD_DATE_LEN);
    const char doe_cd = emrtd_mrz_check_digit(credentials->date_of_expiry, EMRTD_DATE_LEN);
    if(doc_cd == 0 || dob_cd == 0 || doe_cd == 0) {
        error = EmrtdErrorInvalidInput;
        goto done;
    }

    const size_t needed = field_len + 1 + 2 * (EMRTD_DATE_LEN + 1);
    if(needed + 1 > out_size) {
        error = EmrtdErrorBufferTooSmall;
        goto done;
    }

    size_t pos = 0;
    memcpy(out + pos, doc, field_len);
    pos += field_len;
    out[pos++] = doc_cd;
    memcpy(out + pos, credentials->date_of_birth, EMRTD_DATE_LEN);
    pos += EMRTD_DATE_LEN;
    out[pos++] = dob_cd;
    memcpy(out + pos, credentials->date_of_expiry, EMRTD_DATE_LEN);
    pos += EMRTD_DATE_LEN;
    out[pos++] = doe_cd;
    out[pos] = '\0';

done:
    memset(doc, 0, sizeof(doc));
    if(error != EmrtdErrorNone && out_size > 0) {
        out[0] = '\0';
    }
    return error;
}

/**
 * Work out the document number when it does not fit its field.
 *
 * A number longer than nine characters leaves a filler in the check digit
 * position and continues at the start of the optional data, where it is
 * followed by the check digit of the whole number and then by filler
 * (9303-5, section 4.2.4). Anything else is a plain nine character field.
 */
static void mrz_document_number(
    const char* field,
    char printed_cd,
    const char* optional,
    size_t optional_len,
    char* out,
    size_t out_size,
    char* out_cd,
    bool* out_extended) {
    *out_cd = printed_cd;
    *out_extended = false;
    mrz_copy_compact(out, out_size, field, 9);

    if(printed_cd != '<') {
        return;
    }

    /* The continuation runs up to the first filler; its last character is the digit. */
    size_t run = 0;
    while(run < optional_len && optional[run] != '<') {
        run++;
    }
    if(run < 2) {
        /* Nothing usable there: leave the number as the field alone. */
        return;
    }

    *out_extended = true;
    *out_cd = optional[run - 1];

    size_t pos = strlen(out);
    for(size_t i = 0; i + 1 < run && pos + 1 < out_size; i++) {
        out[pos++] = optional[i];
    }
    out[pos] = '\0';
}

/** Verify the document number's own check digit, extended or not. */
static bool
    mrz_document_number_ok(const char* field, const char* number, char printed_cd, bool extended) {
    if(extended) {
        return mrz_check_digit_ok(number, strlen(number), printed_cd);
    }
    return mrz_check_digit_ok(field, 9, printed_cd);
}

static void mrz_parse_td3(const char* raw, EmrtdMrz* out) {
    const char* l1 = raw;
    const char* l2 = raw + 44;
    bool extended = false;
    char doc_cd = 0;

    out->format = EmrtdMrzFormatTd3;
    mrz_copy_compact(out->document_type, sizeof(out->document_type), l1, 2);
    mrz_copy_compact(out->issuing_state, sizeof(out->issuing_state), l1 + 2, 3);
    mrz_copy_name(
        l1 + 5, 39, out->surname, sizeof(out->surname), out->given_names, sizeof(out->given_names));

    mrz_document_number(
        l2, l2[9], NULL, 0, out->document_number, sizeof(out->document_number), &doc_cd, &extended);
    mrz_copy_compact(out->nationality, sizeof(out->nationality), l2 + 10, 3);
    memcpy(out->date_of_birth, l2 + 13, EMRTD_DATE_LEN);
    out->date_of_birth[EMRTD_DATE_LEN] = '\0';
    mrz_copy_compact(out->sex, sizeof(out->sex), l2 + 20, 1);
    memcpy(out->date_of_expiry, l2 + 21, EMRTD_DATE_LEN);
    out->date_of_expiry[EMRTD_DATE_LEN] = '\0';
    mrz_copy_compact(out->optional_data, sizeof(out->optional_data), l2 + 28, 14);

    /* Composite digit over line 2 positions 1-10, 14-20 and 22-43 (9303-4). */
    const EmrtdMrzRange ranges[3] = {{l2, 10}, {l2 + 13, 7}, {l2 + 21, 22}};
    const char composite = mrz_check_digit_ranges(ranges, 3);

    out->check_digits_valid = mrz_document_number_ok(l2, out->document_number, doc_cd, extended) &&
                              mrz_check_digit_ok(l2 + 13, EMRTD_DATE_LEN, l2[19]) &&
                              mrz_check_digit_ok(l2 + 21, EMRTD_DATE_LEN, l2[27]) &&
                              mrz_check_digit_ok(l2 + 28, 14, l2[42]) && composite != 0 &&
                              composite == l2[43];
}

static void mrz_parse_td2(const char* raw, EmrtdMrz* out) {
    const char* l1 = raw;
    const char* l2 = raw + 36;
    bool extended = false;
    char doc_cd = 0;

    out->format = EmrtdMrzFormatTd2;
    mrz_copy_compact(out->document_type, sizeof(out->document_type), l1, 2);
    mrz_copy_compact(out->issuing_state, sizeof(out->issuing_state), l1 + 2, 3);
    mrz_copy_name(
        l1 + 5, 31, out->surname, sizeof(out->surname), out->given_names, sizeof(out->given_names));

    mrz_document_number(
        l2,
        l2[9],
        l2 + 28,
        7,
        out->document_number,
        sizeof(out->document_number),
        &doc_cd,
        &extended);
    mrz_copy_compact(out->nationality, sizeof(out->nationality), l2 + 10, 3);
    memcpy(out->date_of_birth, l2 + 13, EMRTD_DATE_LEN);
    out->date_of_birth[EMRTD_DATE_LEN] = '\0';
    mrz_copy_compact(out->sex, sizeof(out->sex), l2 + 20, 1);
    memcpy(out->date_of_expiry, l2 + 21, EMRTD_DATE_LEN);
    out->date_of_expiry[EMRTD_DATE_LEN] = '\0';
    if(!extended) {
        mrz_copy_compact(out->optional_data, sizeof(out->optional_data), l2 + 28, 7);
    }

    /* Composite digit over line 2 positions 1-10, 14-20 and 22-35 (9303-5). */
    const EmrtdMrzRange ranges[3] = {{l2, 10}, {l2 + 13, 7}, {l2 + 21, 14}};
    const char composite = mrz_check_digit_ranges(ranges, 3);

    out->check_digits_valid = mrz_document_number_ok(l2, out->document_number, doc_cd, extended) &&
                              mrz_check_digit_ok(l2 + 13, EMRTD_DATE_LEN, l2[19]) &&
                              mrz_check_digit_ok(l2 + 21, EMRTD_DATE_LEN, l2[27]) &&
                              composite != 0 && composite == l2[35];
}

static void mrz_parse_td1(const char* raw, EmrtdMrz* out) {
    const char* l1 = raw;
    const char* l2 = raw + 30;
    const char* l3 = raw + 60;
    bool extended = false;
    char doc_cd = 0;

    out->format = EmrtdMrzFormatTd1;
    mrz_copy_compact(out->document_type, sizeof(out->document_type), l1, 2);
    mrz_copy_compact(out->issuing_state, sizeof(out->issuing_state), l1 + 2, 3);
    mrz_copy_name(
        l3, 30, out->surname, sizeof(out->surname), out->given_names, sizeof(out->given_names));

    mrz_document_number(
        l1 + 5,
        l1[14],
        l1 + 15,
        15,
        out->document_number,
        sizeof(out->document_number),
        &doc_cd,
        &extended);
    memcpy(out->date_of_birth, l2, EMRTD_DATE_LEN);
    out->date_of_birth[EMRTD_DATE_LEN] = '\0';
    mrz_copy_compact(out->sex, sizeof(out->sex), l2 + 7, 1);
    memcpy(out->date_of_expiry, l2 + 8, EMRTD_DATE_LEN);
    out->date_of_expiry[EMRTD_DATE_LEN] = '\0';
    mrz_copy_compact(out->nationality, sizeof(out->nationality), l2 + 15, 3);
    mrz_copy_compact(out->optional_data, sizeof(out->optional_data), l2 + 18, 11);

    /*
     * Composite digit over line 1 positions 6-30 and line 2 positions 1-7,
     * 9-15 and 19-29 (9303-5). The two lines are weighed as one field.
     */
    const EmrtdMrzRange ranges[4] = {{l1 + 5, 25}, {l2, 7}, {l2 + 8, 7}, {l2 + 18, 11}};
    const char composite = mrz_check_digit_ranges(ranges, 4);

    out->check_digits_valid =
        mrz_document_number_ok(l1 + 5, out->document_number, doc_cd, extended) &&
        mrz_check_digit_ok(l2, EMRTD_DATE_LEN, l2[6]) &&
        mrz_check_digit_ok(l2 + 8, EMRTD_DATE_LEN, l2[14]) && composite != 0 &&
        composite == l2[29];
}

EmrtdError emrtd_mrz_parse(const char* raw, size_t len, EmrtdMrz* out) {
    if(raw == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }

    memset(out, 0, sizeof(*out));
    out->format = EmrtdMrzFormatUnknown;

    /*
     * DG1 carries the zone as one run of characters, but a person typing it
     * in, or a file that kept its line breaks, will have separators in it.
     * They are dropped before the length decides the format.
     */
    char buf[91];
    size_t n = 0;
    for(size_t i = 0; i < len; i++) {
        const char c = raw[i];
        if(c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        if(n >= sizeof(buf) - 1) {
            memset(buf, 0, sizeof(buf));
            return EmrtdErrorParse;
        }
        buf[n++] = c;
    }
    buf[n] = '\0';

    EmrtdError error = EmrtdErrorNone;
    switch(n) {
    case 90:
        mrz_parse_td1(buf, out);
        break;
    case 72:
        mrz_parse_td2(buf, out);
        break;
    case 88:
        mrz_parse_td3(buf, out);
        break;
    default:
        error = EmrtdErrorParse;
        break;
    }

    /*
     * The copy holds the document number and both dates, which together are
     * the password every access protocol derives its keys from.
     */
    memset(buf, 0, sizeof(buf));
    return error;
}

void emrtd_mrz_full_name(const EmrtdMrz* mrz, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if(mrz == NULL) {
        return;
    }

    size_t pos = 0;
    for(size_t i = 0; mrz->surname[i] != '\0' && pos + 1 < out_size; i++) {
        out[pos++] = mrz->surname[i];
    }
    if(pos > 0 && mrz->given_names[0] != '\0' && pos + 2 < out_size) {
        out[pos++] = ',';
        out[pos++] = ' ';
    }
    for(size_t i = 0; mrz->given_names[i] != '\0' && pos + 1 < out_size; i++) {
        out[pos++] = mrz->given_names[i];
    }
    out[pos] = '\0';
}

void emrtd_mrz_format_date(const char* yymmdd, bool is_expiry, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if(yymmdd == NULL) {
        return;
    }

    /* "DD.MM.YYYY" and the terminator. */
    if(out_size < 11 || !mrz_all_digits(yymmdd, EMRTD_DATE_LEN)) {
        /* Show whatever was printed rather than nothing at all. */
        size_t pos = 0;
        for(size_t i = 0; i < EMRTD_DATE_LEN && yymmdd[i] != '\0' && pos + 1 < out_size; i++) {
            out[pos++] = yymmdd[i];
        }
        out[pos] = '\0';
        return;
    }

    const int yy = (yymmdd[0] - '0') * 10 + (yymmdd[1] - '0');
    const bool this_century = is_expiry ? (yy < EMRTD_MRZ_EXPIRY_PIVOT) :
                                          (yy <= EMRTD_MRZ_BIRTH_PIVOT);

    out[0] = yymmdd[4];
    out[1] = yymmdd[5];
    out[2] = '.';
    out[3] = yymmdd[2];
    out[4] = yymmdd[3];
    out[5] = '.';
    out[6] = this_century ? '2' : '1';
    out[7] = this_century ? '0' : '9';
    out[8] = yymmdd[0];
    out[9] = yymmdd[1];
    out[10] = '\0';
}
