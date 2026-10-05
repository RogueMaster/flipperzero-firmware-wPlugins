/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Command and response APDUs, ISO/IEC 7816-4.
 *
 * The four cases of the standard plus the extended forms, and then the
 * commands ICAO Doc 9303 builds out of them. The vectors are the ones the
 * Python reference implementation is tested against, so both readers encode
 * the same bytes for the same request.
 *
 * The status word tables live in the same layer, so what a chip's refusal
 * turns into - both the error code and the words the user reads - is checked
 * here too.
 */

#include "emrtd_test.h"
#include "emrtd_test_font.h"

#include "../../emrtd_error.h"
#include "../../protocol/emrtd_apdu.h"
#include "../../protocol/emrtd_files.h"

/** Encode a command and compare it with a hex literal. */
static void check_encoding(const EmrtdCommandApdu* command, const char* expected) {
    uint8_t out[512];
    size_t len = 0;

    const EmrtdError error = emrtd_apdu_encode(command, out, sizeof(out), &len);
    TEST_EQ_INT(error, EmrtdErrorNone);
    if(error == EmrtdErrorNone) {
        TEST_EQ_HEX(out, len, expected);
    }
}

static void test_cases(void) {
    emrtd_test_begin("case 1, a header and nothing else");
    EmrtdCommandApdu command = {0x00, 0xA4, 0x04, 0x0C, NULL, 0, EMRTD_LE_NONE};
    check_encoding(&command, "00A4040C");

    emrtd_test_begin("case 2, Le alone");
    command = (EmrtdCommandApdu){0x00, 0x84, 0x00, 0x00, NULL, 0, 8};
    check_encoding(&command, "0084000008");

    emrtd_test_begin("Le of 256 goes on the wire as a zero byte");
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, EMRTD_LE_MAX};
    check_encoding(&command, "00B0000000");

    emrtd_test_begin("case 3, a data field alone");
    const uint8_t fid[2] = {0x01, 0x1E};
    command = (EmrtdCommandApdu){0x00, 0xA4, 0x02, 0x0C, fid, sizeof(fid), EMRTD_LE_NONE};
    check_encoding(&command, "00A4020C02011E");

    emrtd_test_begin("case 4, a data field and Le");
    command = (EmrtdCommandApdu){0x00, 0x82, 0x00, 0x00, fid, sizeof(fid), 2};
    check_encoding(&command, "00820000 02 011E 02");

    emrtd_test_begin("a data field of 255 bytes is still the short form");
    uint8_t payload[400];
    memset(payload, 0xAA, sizeof(payload));
    command = (EmrtdCommandApdu){0x00, 0x86, 0x00, 0x00, payload, 255, EMRTD_LE_NONE};
    uint8_t out[512];
    size_t len = 0;
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorNone);
    TEST_EQ_INT(len, 4 + 1 + 255);
    TEST_EQ_INT(out[4], 0xFF);
}

static void test_extended(void) {
    emrtd_test_begin("a data field over 255 bytes switches to extended length");

    uint8_t payload[400];
    memset(payload, 0xAA, sizeof(payload));
    EmrtdCommandApdu command = {0x00, 0x86, 0x00, 0x00, payload, 300, EMRTD_LE_MAX};

    uint8_t out[512];
    size_t len = 0;
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorNone);
    TEST_EQ_INT(len, 4 + 3 + 300 + 2);
    TEST_EQ_INT(out[4], 0x00);
    TEST_EQ_INT((out[5] << 8) | out[6], 300);
    TEST_EQ_INT((out[len - 2] << 8) | out[len - 1], 256);

    emrtd_test_begin("an Le over 256 switches to extended length on its own");
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, 1024};
    check_encoding(&command, "00B00000 00 0400");

    emrtd_test_begin("the largest Le is two zero bytes");
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, 65536};
    check_encoding(&command, "00B00000 00 0000");
}

static void test_encode_refusals(void) {
    emrtd_test_begin("a buffer too small is refused, not overrun");

    const uint8_t fid[2] = {0x01, 0x01};
    EmrtdCommandApdu command = {0x00, 0xA4, 0x02, 0x0C, fid, sizeof(fid), EMRTD_LE_NONE};

    uint8_t out[7];
    size_t len = 0;
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorNone);
    TEST_EQ_INT(len, 7);
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, 6, &len), EmrtdErrorBufferTooSmall);
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, 0, &len), EmrtdErrorBufferTooSmall);

    emrtd_test_begin("an Le outside the standard's range is refused");
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, 0};
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorInvalidInput);
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, 65537};
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorInvalidInput);
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 0, -2};
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorInvalidInput);

    emrtd_test_begin("a data length with no data is refused");
    command = (EmrtdCommandApdu){0x00, 0xB0, 0x00, 0x00, NULL, 4, EMRTD_LE_NONE};
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), &len), EmrtdErrorInvalidInput);

    emrtd_test_begin("null arguments are refused");
    TEST_EQ_INT(emrtd_apdu_encode(NULL, out, sizeof(out), &len), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_apdu_encode(&command, NULL, sizeof(out), &len), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_apdu_encode(&command, out, sizeof(out), NULL), EmrtdErrorInvalidInput);
}

static void test_decode(void) {
    emrtd_test_begin("a response splits into data and status word");

    uint8_t raw[16];
    size_t len = emrtd_test_hex("6F0A9000", raw, sizeof(raw));

    EmrtdResponseApdu response;
    TEST_EQ_INT(emrtd_apdu_decode(raw, len, &response), EmrtdErrorNone);
    TEST_EQ_INT(response.sw, 0x9000);
    TEST_EQ_INT(response.data_len, 2);
    TEST_EQ_HEX(response.data, response.data_len, "6F0A");
    TEST_CHECK(emrtd_apdu_is_success(&response));

    emrtd_test_begin("a status word with no data is still a response");
    len = emrtd_test_hex("6982", raw, sizeof(raw));
    TEST_EQ_INT(emrtd_apdu_decode(raw, len, &response), EmrtdErrorNone);
    TEST_EQ_INT(response.sw, 0x6982);
    TEST_EQ_INT(response.data_len, 0);
    TEST_CHECK(!emrtd_apdu_is_success(&response));

    emrtd_test_begin("anything shorter than a status word is a protocol error");
    TEST_EQ_INT(emrtd_apdu_decode(raw, 1, &response), EmrtdErrorProtocol);
    TEST_EQ_INT(emrtd_apdu_decode(raw, 0, &response), EmrtdErrorProtocol);
    TEST_EQ_INT(emrtd_apdu_decode(NULL, 4, &response), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_apdu_decode(raw, 4, NULL), EmrtdErrorInvalidInput);
}

static void test_commands(void) {
    emrtd_test_begin("SELECT by application identifier");

    TEST_EQ_HEX(EMRTD_AID, sizeof(EMRTD_AID), "A0000002471001");

    EmrtdCommandApdu command;
    emrtd_apdu_select_application(&command, EMRTD_AID, sizeof(EMRTD_AID));
    check_encoding(&command, "00A4040C07A0000002471001");

    emrtd_test_begin("SELECT of an elementary file by identifier");
    const EmrtdFileInfo* dg1 = emrtd_file_info(EmrtdFileDg1);
    TEST_CHECK(dg1 != NULL);
    uint8_t fid[2];
    emrtd_file_fid_bytes(dg1, fid);
    emrtd_apdu_select_file(&command, fid);
    check_encoding(&command, "00A4020C020101");

    emrtd_test_begin("GET CHALLENGE asks for the eight bytes BAC needs");
    emrtd_apdu_get_challenge(&command);
    check_encoding(&command, "0084000008");

    emrtd_test_begin("EXTERNAL AUTHENTICATE expects as much as it sends");
    uint8_t payload[40];
    memset(payload, 0x5A, sizeof(payload));
    emrtd_apdu_external_authenticate(&command, payload, sizeof(payload));
    TEST_EQ_INT(command.ins, 0x82);
    TEST_EQ_INT(command.le, 40);
    TEST_EQ_INT(command.data_len, 40);

    emrtd_test_begin("READ BINARY puts the offset in P1-P2");
    emrtd_apdu_read_binary(&command, 0x1234, 0x10);
    TEST_EQ_INT(command.ins, 0xB0);
    TEST_EQ_INT(command.p1, 0x12);
    TEST_EQ_INT(command.p2, 0x34);
    TEST_EQ_INT(command.le, 0x10);
    check_encoding(&command, "00B0123410");

    emrtd_test_begin("READ BINARY by short identifier sets the high bit of P1");
    emrtd_apdu_read_binary_sfi(&command, dg1->sfi, 0, 4);
    TEST_EQ_INT(command.p1, 0x81);
    TEST_EQ_INT(command.p2, 0x00);
    check_encoding(&command, "00B0810004");

    emrtd_test_begin("a request longer than a frame is cut to one");
    emrtd_apdu_read_binary(&command, 0, 4096);
    TEST_EQ_INT(command.le, EMRTD_LE_MAX);
    emrtd_apdu_read_binary(&command, 0, 0);
    TEST_EQ_INT(command.le, EMRTD_LE_MAX);

    emrtd_test_begin("MSE:Set AT selects the authentication template");
    const uint8_t at[3] = {0x83, 0x01, 0x01};
    emrtd_apdu_mse_set_at(&command, at, sizeof(at));
    check_encoding(&command, "0022C1A403830101");

    emrtd_test_begin("GENERAL AUTHENTICATE marks the chain in the class byte");
    const uint8_t empty[2] = {0x7C, 0x00};
    emrtd_apdu_general_authenticate(&command, empty, sizeof(empty), true);
    TEST_EQ_INT(command.cla, 0x10);
    check_encoding(&command, "10860000027C0000");
    emrtd_apdu_general_authenticate(&command, empty, sizeof(empty), false);
    TEST_EQ_INT(command.cla, 0x00);
    check_encoding(&command, "00860000027C0000");
}

static void test_read_binary_odd(void) {
    emrtd_test_begin("the even instruction only reaches 32767 bytes");

    TEST_CHECK(emrtd_apdu_offset_is_short(0));
    TEST_CHECK(emrtd_apdu_offset_is_short(0x7FFF));
    TEST_CHECK(!emrtd_apdu_offset_is_short(0x8000));

    emrtd_test_begin("the odd instruction carries the offset in DO '54'");
    EmrtdCommandApdu command;
    uint8_t offset_do[EMRTD_OFFSET_DO_MAX];
    TEST_EQ_INT(
        emrtd_apdu_read_binary_odd(&command, 0x8000, 0xE0, offset_do, sizeof(offset_do)),
        EmrtdErrorNone);
    TEST_EQ_INT(command.ins, 0xB1);
    TEST_EQ_INT(command.p1, 0x00);
    TEST_EQ_INT(command.p2, 0x00);
    TEST_EQ_INT(command.le, 0xE0);
    check_encoding(&command, "00B10000 04 54028000 E0");

    emrtd_test_begin("an offset past 64 KiB takes a third byte");
    TEST_EQ_INT(
        emrtd_apdu_read_binary_odd(&command, 0x012345, 16, offset_do, sizeof(offset_do)),
        EmrtdErrorNone);
    check_encoding(&command, "00B10000 05 5403012345 10");

    emrtd_test_begin("a scratch buffer too small is refused");
    uint8_t small[3];
    TEST_EQ_INT(
        emrtd_apdu_read_binary_odd(&command, 0, 16, small, sizeof(small)),
        EmrtdErrorBufferTooSmall);
    TEST_EQ_INT(emrtd_apdu_read_binary_odd(&command, 0, 16, NULL, 8), EmrtdErrorInvalidInput);
}

static void test_status_words(void) {
    emrtd_test_begin("a status word becomes the error it really means");

    TEST_EQ_INT(emrtd_error_from_sw(0x9000), EmrtdErrorNone);
    TEST_EQ_INT(emrtd_error_from_sw(0x6A82), EmrtdErrorFileNotFound);
    TEST_EQ_INT(emrtd_error_from_sw(0x6A83), EmrtdErrorFileNotFound);
    TEST_EQ_INT(emrtd_error_from_sw(0x6982), EmrtdErrorAccessDenied);
    TEST_EQ_INT(emrtd_error_from_sw(0x6983), EmrtdErrorAccessDenied);
    TEST_EQ_INT(emrtd_error_from_sw(0x6987), EmrtdErrorSecureMessaging);
    TEST_EQ_INT(emrtd_error_from_sw(0x6988), EmrtdErrorSecureMessaging);

    emrtd_test_begin("a failed verification points at the credentials");
    /* What a chip answers to EXTERNAL AUTHENTICATE when BAC did not verify. */
    TEST_EQ_INT(emrtd_error_from_sw(0x6300), EmrtdErrorWrongKey);
    TEST_EQ_INT(emrtd_error_from_sw(0x63C2), EmrtdErrorWrongKey);
    TEST_EQ_INT(emrtd_error_from_sw(0x63C0), EmrtdErrorWrongKey);

    emrtd_test_begin("anything else is reported as the chip's own refusal");
    TEST_EQ_INT(emrtd_error_from_sw(0x6D00), EmrtdErrorApdu);
    TEST_EQ_INT(emrtd_error_from_sw(0x6F42), EmrtdErrorApdu);
    TEST_EQ_INT(emrtd_error_from_sw(0x6110), EmrtdErrorApdu);
    TEST_EQ_INT(emrtd_error_from_sw(0x0000), EmrtdErrorApdu);

    emrtd_test_begin("every status word has words of its own");
    TEST_EQ_STR(emrtd_sw_text(0x9000), "OK");
    TEST_CHECK(strstr(emrtd_sw_text(0x6982), "security status") != NULL);
    TEST_CHECK(strstr(emrtd_sw_text(0x6A82), "not found") != NULL);
    TEST_CHECK(strstr(emrtd_sw_text(0x6110), "GET RESPONSE") != NULL);
    TEST_CHECK(strstr(emrtd_sw_text(0x6C10), "Le") != NULL);
    TEST_CHECK(strstr(emrtd_sw_text(0x63C3), "attempts left") != NULL);
    TEST_EQ_STR(emrtd_sw_text(0x1234), "unknown status word");

    emrtd_test_begin("the status word text is a constant, not a shared buffer");
    /*
     * emrtd_sw_text() is called from the NFC thread while the user interface
     * thread may still be showing the last one, so two results must not be
     * the same storage.
     */
    const char* first = emrtd_sw_text(0x6982);
    const char* second = emrtd_sw_text(0x6A82);
    TEST_CHECK(first != second);
    TEST_EQ_STR(first, emrtd_sw_text(0x6982));
}

/*
 * The error screen is a FontPrimary heading, which does not wrap, over a text
 * scroll element that breaks a line at whichever glyph crosses its width, over
 * two buttons. The same hint is shown again by the text box of a saved report,
 * which is 4 px narrower. See emrtd_error.c for where the numbers come from.
 */
#define EMRTD_TEST_HEADING_WIDTH 124u
#define EMRTD_TEST_HINT_WIDTH    120u
#define EMRTD_TEST_HINT_LINES    3u

/** Hold one heading and its hint to the screen, and say which one when not. */
static void check_error_fits(int code, const char* text, const char* hint) {
    emrtd_test_checks++;
    if(text == NULL || text[0] == '\0' || strchr(text, '\n') != NULL) {
        emrtd_test_fail(__FILE__, __LINE__, "error %d: the heading is not one line", code);
        return;
    }
    const unsigned text_width = emrtd_test_font_advance(EmrtdTestFontPrimary, text, strlen(text));
    if(text_width > EMRTD_TEST_HEADING_WIDTH) {
        emrtd_test_fail(
            __FILE__,
            __LINE__,
            "error %d: \"%s\" is %u px wide, the heading has %u",
            code,
            text,
            text_width,
            EMRTD_TEST_HEADING_WIDTH);
    }

    emrtd_test_checks++;
    if(hint == NULL || hint[0] == '\0') {
        emrtd_test_fail(__FILE__, __LINE__, "error %d (%s): the hint is empty", code, text);
        return;
    }
    unsigned lines = 0;
    for(const char* line = hint; line != NULL;) {
        const char* end = strchr(line, '\n');
        const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        lines++;

        if(len == 0 || line[0] == ' ' || line[len - 1] == ' ') {
            emrtd_test_fail(
                __FILE__,
                __LINE__,
                "error %d (%s): hint line %u is empty or padded with a space",
                code,
                text,
                lines);
        }
        const unsigned width = emrtd_test_font_advance(EmrtdTestFontSecondary, line, len);
        if(width > EMRTD_TEST_HINT_WIDTH) {
            emrtd_test_fail(
                __FILE__,
                __LINE__,
                "error %d (%s): hint line %u \"%.*s\" is %u px wide, a line has %u",
                code,
                text,
                lines,
                (int)len,
                line,
                width,
                EMRTD_TEST_HINT_WIDTH);
        }
        line = end != NULL ? end + 1 : NULL;
    }
    if(lines > EMRTD_TEST_HINT_LINES) {
        emrtd_test_fail(
            __FILE__,
            __LINE__,
            "error %d (%s): the hint has %u lines, the screen shows %u",
            code,
            text,
            lines,
            EMRTD_TEST_HINT_LINES);
    }
}

static void test_error_text(void) {
    emrtd_test_begin("every error fits the screen, heading and hint");

    for(int code = 0; code < EmrtdErrorCount; code++) {
        const char* text = emrtd_error_text((EmrtdError)code);
        const char* hint = emrtd_error_hint((EmrtdError)code);

        check_error_fits(code, text, hint);
        /* A hint that says no more than the heading is not a hint. */
        TEST_CHECK(hint != NULL && text != NULL && strlen(hint) > strlen(text));
    }
    /* The words for a code outside the table are shown on the same screen. */
    check_error_fits(
        EmrtdErrorCount, emrtd_error_text(EmrtdErrorCount), emrtd_error_hint(EmrtdErrorCount));

    emrtd_test_begin("the budget itself catches a line that is too wide");
    /* 21 capital W in FontSecondary is 168 px; a check that passed this would
     * be comparing nothing. */
    TEST_CHECK(
        emrtd_test_font_advance(EmrtdTestFontSecondary, "WWWWWWWWWWWWWWWWWWWWW", 21) >
        EMRTD_TEST_HINT_WIDTH);
    TEST_EQ_INT(emrtd_test_font_advance(EmrtdTestFontPrimary, "No error", 8), 41);

    emrtd_test_begin("the hint for a wrong key tells the user what to do");
    const char* hint = emrtd_error_hint(EmrtdErrorWrongKey);
    /* The number goes in without its check digit, and the values it points at
     * are the ones the error screen reads back under it. Where they come from,
     * the zone or a card access number, is said there, where there is room. */
    TEST_CHECK(strstr(hint, "check digit") != NULL);
    TEST_CHECK(strstr(hint, "number") != NULL);
    TEST_CHECK(strstr(hint, "dates") != NULL);

    emrtd_test_begin("a platform limit is named as one, not blamed on the document");
    TEST_CHECK(strstr(emrtd_error_hint(EmrtdErrorPaceUnsupportedDh), "mbed TLS") != NULL);
    TEST_CHECK(strstr(emrtd_error_hint(EmrtdErrorPaceUnsupportedCurve), "256 bit") != NULL);
    TEST_CHECK(strstr(emrtd_error_hint(EmrtdErrorPaceUnsupportedMapping), "BAC") != NULL);

    emrtd_test_begin("a code from outside the enumeration still reads as something");
    TEST_EQ_STR(emrtd_error_text(EmrtdErrorCount), "Unknown error");
    TEST_CHECK(emrtd_error_text((EmrtdError)1000) != NULL);
    TEST_CHECK(emrtd_error_hint((EmrtdError)1000) != NULL);
    TEST_CHECK(emrtd_error_hint((EmrtdError)-1) != NULL);
}

void test_suite_apdu(void) {
    test_cases();
    test_extended();
    test_encode_refusals();
    test_decode();
    test_commands();
    test_read_binary_odd();
    test_status_words();
    test_error_text();
}
