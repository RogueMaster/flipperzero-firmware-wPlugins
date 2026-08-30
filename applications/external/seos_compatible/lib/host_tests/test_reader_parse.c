/* Parsing the answers a card gives a reader.
 *
 * These run on a workstation because they take a buffer and a length rather
 * than a poller, which is the point of separating them from the transport.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_protocol.h>

#include <string.h>

/* ---- status words ---- */

static MunitResult test_status_word(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t success[] = {0x6f, 0x0c, 0x90, 0x00};
    uint16_t status_word = 0;

    munit_assert_true(seos_response_status(success, sizeof(success), &status_word));
    munit_assert_uint16(status_word, ==, 0x9000);

    const uint8_t failure[] = {0x6a, 0x82};
    munit_assert_true(seos_response_status(failure, sizeof(failure), &status_word));
    munit_assert_uint16(status_word, ==, 0x6a82);

    return MUNIT_OK;
}

/* A response with fewer than two bytes has no status word to read. Reading one
 * anyway means indexing from the end of a buffer that is shorter than the
 * step back. */
static MunitResult test_status_word_too_short(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t data[] = {0x90};
    uint16_t status_word = 0xffff;

    munit_assert_false(seos_response_status(data, 0, &status_word));
    munit_assert_false(seos_response_status(data, 1, &status_word));

    return MUNIT_OK;
}

/* ---- the first authenticate answer ---- */

/* 7c is the wrapper, 81 the card's challenge inside it. */
static const uint8_t ga1_answer[] = {
    0x7c, 0x0a, 0x81, 0x08, 0x31, 0x41, 0x59, 0x26, 0x53, 0x58, 0x97, 0x93, 0x90, 0x00};

static MunitResult test_ga1_response(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t rnd_icc[8];
    memset(rnd_icc, 0, sizeof(rnd_icc));

    munit_assert_true(
        seos_parse_ga1_response(ga1_answer, sizeof(ga1_answer), rnd_icc, sizeof(rnd_icc)));
    munit_assert_memory_equal(sizeof(rnd_icc), rnd_icc, ga1_answer + 4);

    return MUNIT_OK;
}

static MunitResult test_ga1_response_truncated(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t rnd_icc[8];

    /* Every prefix short of the whole challenge is incomplete. */
    for(size_t cut = 0; cut < 12; cut++) {
        munit_assert_false(seos_parse_ga1_response(ga1_answer, cut, rnd_icc, sizeof(rnd_icc)));
    }
    /* The challenge is complete at twelve bytes, status word or not. */
    munit_assert_true(seos_parse_ga1_response(ga1_answer, 12, rnd_icc, sizeof(rnd_icc)));

    return MUNIT_OK;
}

static MunitResult test_ga1_response_wrong_shape(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t rnd_icc[8];

    /* A wrapper of some other tag. */
    uint8_t wrong_wrapper[sizeof(ga1_answer)];
    memcpy(wrong_wrapper, ga1_answer, sizeof(ga1_answer));
    wrong_wrapper[0] = 0x7d;
    munit_assert_false(
        seos_parse_ga1_response(wrong_wrapper, sizeof(wrong_wrapper), rnd_icc, sizeof(rnd_icc)));

    /* The right wrapper carrying some other object. */
    uint8_t wrong_inner[sizeof(ga1_answer)];
    memcpy(wrong_inner, ga1_answer, sizeof(ga1_answer));
    wrong_inner[2] = 0x82;
    munit_assert_false(
        seos_parse_ga1_response(wrong_inner, sizeof(wrong_inner), rnd_icc, sizeof(rnd_icc)));

    /* A challenge of the wrong length is not the challenge. */
    const uint8_t short_challenge[] = {0x7c, 0x06, 0x81, 0x04, 0x01, 0x02, 0x03, 0x04, 0x90, 0x00};
    munit_assert_false(seos_parse_ga1_response(
        short_challenge, sizeof(short_challenge), rnd_icc, sizeof(rnd_icc)));

    return MUNIT_OK;
}

/* ---- the second authenticate answer ---- */

static void build_ga2_answer(uint8_t* out, size_t cryptogram_len) {
    out[0] = 0x7c;
    out[1] = (uint8_t)(cryptogram_len + 2);
    out[2] = 0x82;
    out[3] = (uint8_t)cryptogram_len;
    for(size_t i = 0; i < cryptogram_len; i++) {
        out[4 + i] = (uint8_t)(i * 3 + 5);
    }
    out[4 + cryptogram_len] = 0x90;
    out[5 + cryptogram_len] = 0x00;
}

static MunitResult test_ga2_response(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[6 + 40];
    build_ga2_answer(answer, 40);

    const uint8_t* cryptogram = NULL;
    size_t cryptogram_len = 0;
    munit_assert_true(
        seos_parse_ga2_response(answer, sizeof(answer), &cryptogram, &cryptogram_len));
    munit_assert_size(cryptogram_len, ==, 40);
    munit_assert_ptr_equal(cryptogram, answer + 4);

    return MUNIT_OK;
}

static MunitResult test_ga2_response_truncated(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[6 + 40];
    build_ga2_answer(answer, 40);

    const uint8_t* cryptogram = NULL;
    size_t cryptogram_len = 0;
    for(size_t cut = 0; cut < 44; cut++) {
        munit_assert_false(seos_parse_ga2_response(answer, cut, &cryptogram, &cryptogram_len));
    }
    munit_assert_true(seos_parse_ga2_response(answer, 44, &cryptogram, &cryptogram_len));

    return MUNIT_OK;
}

/* The caller decides what length it can verify; the parser reports it. */
static MunitResult test_ga2_response_reports_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[6 + 16];
    build_ga2_answer(answer, 16);

    const uint8_t* cryptogram = NULL;
    size_t cryptogram_len = 0;
    munit_assert_true(
        seos_parse_ga2_response(answer, sizeof(answer), &cryptogram, &cryptogram_len));
    munit_assert_size(cryptogram_len, ==, 16);

    return MUNIT_OK;
}

/* ---- the credential a read answers with ---- */

/* Tag ff00, then a BER length, then the credential. */
static size_t build_sio_answer(uint8_t* out, size_t sio_len) {
    size_t offset = 0;
    out[offset++] = 0xff;
    out[offset++] = 0x00;
    if(sio_len < 0x80) {
        out[offset++] = (uint8_t)sio_len;
    } else {
        out[offset++] = 0x81;
        out[offset++] = (uint8_t)sio_len;
    }
    for(size_t i = 0; i < sio_len; i++) {
        out[offset + i] = (uint8_t)(i * 11 + 3);
    }
    return offset + sio_len;
}

static MunitResult test_sio_response_short_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[4 + 64];
    size_t answer_len = build_sio_answer(answer, 64);

    uint8_t sio[256];
    size_t sio_len = 0;
    munit_assert_true(seos_parse_sio_response(answer, answer_len, sio, sizeof(sio), &sio_len));
    munit_assert_size(sio_len, ==, 64);
    munit_assert_memory_equal(64, sio, answer + 3);

    return MUNIT_OK;
}

/* A credential of 128 bytes or more carries its length in the long form, which
 * puts the value one byte further along. Reading it from the short-form offset
 * shifts every byte. */
static MunitResult test_sio_response_long_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[4 + 200];
    size_t answer_len = build_sio_answer(answer, 200);

    uint8_t sio[256];
    size_t sio_len = 0;
    munit_assert_true(seos_parse_sio_response(answer, answer_len, sio, sizeof(sio), &sio_len));
    munit_assert_size(sio_len, ==, 200);
    munit_assert_memory_equal(200, sio, answer + 4);

    return MUNIT_OK;
}

static MunitResult test_sio_response_will_not_fit(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[4 + 200];
    size_t answer_len = build_sio_answer(answer, 200);

    uint8_t sio[64];
    size_t sio_len = 0;
    munit_assert_false(seos_parse_sio_response(answer, answer_len, sio, sizeof(sio), &sio_len));

    return MUNIT_OK;
}

static MunitResult test_sio_response_truncated(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[4 + 200];
    size_t answer_len = build_sio_answer(answer, 200);

    uint8_t sio[256];
    size_t sio_len = 0;
    for(size_t cut = 0; cut < answer_len; cut++) {
        munit_assert_false(seos_parse_sio_response(answer, cut, sio, sizeof(sio), &sio_len));
    }
    munit_assert_true(seos_parse_sio_response(answer, answer_len, sio, sizeof(sio), &sio_len));

    return MUNIT_OK;
}

static MunitResult test_sio_response_wrong_tag(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t answer[4 + 32];
    size_t answer_len = build_sio_answer(answer, 32);
    answer[1] = 0x01; /* tag ff01, not the credential */

    uint8_t sio[256];
    size_t sio_len = 0;
    munit_assert_false(seos_parse_sio_response(answer, answer_len, sio, sizeof(sio), &sio_len));

    return MUNIT_OK;
}

static MunitTest test_reader_parse_cases[] = {
    {(char*)"/status/present", test_status_word, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/status/too-short",
     test_status_word_too_short,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/ga1/ok", test_ga1_response, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ga1/truncated", test_ga1_response_truncated, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ga1/wrong-shape",
     test_ga1_response_wrong_shape,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/ga2/ok", test_ga2_response, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ga2/truncated", test_ga2_response_truncated, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ga2/reports-length",
     test_ga2_response_reports_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/short-form",
     test_sio_response_short_form,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/long-form", test_sio_response_long_form, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sio/will-not-fit",
     test_sio_response_will_not_fit,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/truncated", test_sio_response_truncated, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sio/wrong-tag", test_sio_response_wrong_tag, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_reader_parse_suite = {
    (char*)"/reader-parse",
    test_reader_parse_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
