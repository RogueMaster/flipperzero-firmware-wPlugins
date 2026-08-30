/* Selecting an application, from both ends.
 *
 * The card builds the answer and the reader takes it apart again, so the two
 * are checked against each other rather than against a recorded blob. The
 * refusals matter as much as the round trip: everything here is parsed out of
 * bytes a reader was handed.
 */
#include "munit.h"
#include "test_helpers.h"

#include <keys.h>
#include <seos_protocol.h>

#define BUFFER_CAPACITY 256

static SeosCredential credential_for_select(void) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    /* No saved answer, so the card builds one. */
    credential.diversifier_len = 7;
    for(size_t i = 0; i < credential.diversifier_len; i++)
        credential.diversifier[i] = (uint8_t)(0x40 + i);
    return credential;
}

/* The application list a reader offers, naming the one the keys describe. */
static size_t offer_known_application(uint8_t* out) {
    size_t len = 0;
    out[len++] = 0x06;
    out[len++] = (uint8_t)SEOS_ADF_OID_LEN;
    memcpy(out + len, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    return len + SEOS_ADF_OID_LEN;
}

/* What the card sends, with the status word a reader would see after it. */
static void card_answer(uint8_t cipher, uint8_t hash, SeosCredential* credential, BitBuffer* out) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = cipher;
    params.hash = hash;

    uint8_t offer[64];
    size_t offer_len = offer_known_application(offer);

    munit_assert_true(seos_emulator_select_adf(offer, offer_len, &params, credential, out));
    bit_buffer_append_bytes(out, SEOS_SW_SUCCESS, sizeof(SEOS_SW_SUCCESS));
}

static void round_trip(uint8_t cipher, uint8_t hash) {
    SeosCredential card = credential_for_select();
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    card_answer(cipher, hash, &card, answer);

    /* The answer names the cipher and digest, and carries the diversifier the
     * reader needs to work out the card's keys. */
    SeosCredential reader;
    memset(&reader, 0, sizeof(reader));
    AuthParameters params;
    memset(&params, 0, sizeof(params));

    munit_assert_true(seos_reader_select_adf_response(answer, 0, &reader, &params));
    munit_assert_uint8(params.cipher, ==, cipher);
    munit_assert_uint8(params.hash, ==, hash);
    munit_assert_size(reader.diversifier_len, ==, card.diversifier_len);
    munit_assert_memory_equal(card.diversifier_len, reader.diversifier, card.diversifier);

    bit_buffer_free(answer);
}

static MunitResult test_round_trip_aes(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip(AES_128_CBC, SHA256);
    return MUNIT_OK;
}

static MunitResult test_round_trip_des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip(TWO_KEY_3DES_CBC_MODE, SHA1);
    return MUNIT_OK;
}

/* Two answers for the same card must differ, or the card is recognisable. */
static MunitResult test_answer_varies_des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential card = credential_for_select();
    BitBuffer* first = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* second = bit_buffer_alloc(BUFFER_CAPACITY);
    card_answer(TWO_KEY_3DES_CBC_MODE, SHA1, &card, first);
    card_answer(TWO_KEY_3DES_CBC_MODE, SHA1, &card, second);

    munit_assert_memory_not_equal(
        bit_buffer_get_size_bytes(first), bit_buffer_get_data(first), bit_buffer_get_data(second));

    bit_buffer_free(first);
    bit_buffer_free(second);
    return MUNIT_OK;
}

/* An application the card does not carry is not selected. */
static MunitResult test_unknown_application(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential card = credential_for_select();
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;

    uint8_t offer[] = {0x06, 0x04, 0x2b, 0x06, 0x01, 0x99};
    BitBuffer* out = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_false(seos_emulator_select_adf(offer, sizeof(offer), &params, &card, out));

    bit_buffer_free(out);
    return MUNIT_OK;
}

/* Answers a reader must refuse rather than parse. */
static void assert_reader_refuses(const uint8_t* raw, size_t len) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    AuthParameters params;
    memset(&params, 0, sizeof(params));

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    bit_buffer_copy_bytes(answer, raw, len);
    munit_assert_false(seos_reader_select_adf_response(answer, 0, &credential, &params));
    bit_buffer_free(answer);
}

/* The answer may sit behind a byte of transport framing, so the caller says
 * where it starts. A buffer shorter than that offset plus the header it looks
 * for has nothing to compare, and a length counted from it would run
 * backwards past zero. */
static MunitResult test_reader_refuses_short_at_offset(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* A well-formed answer, one byte of framing ahead of it. */
    const uint8_t framed[] = {0x00, 0xcd, 0x02, 0x02, 0x06, 0x90, 0x00};

    for(size_t len = 0; len < sizeof(framed); len++) {
        SeosCredential credential;
        memset(&credential, 0, sizeof(credential));
        AuthParameters params;
        memset(&params, 0, sizeof(params));

        /* Sized to the answer so a read past the end lands outside the
         * allocation, where the sanitiser can see it. */
        BitBuffer* answer = bit_buffer_alloc(len > 0 ? len : 1);
        bit_buffer_copy_bytes(answer, framed, len);
        munit_assert_false(seos_reader_select_adf_response(answer, 1, &credential, &params));
        bit_buffer_free(answer);
    }

    return MUNIT_OK;
}

static MunitResult test_reader_refuses_malformed(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t too_short[] = {0xcd};
    assert_reader_refuses(too_short, sizeof(too_short));

    uint8_t wrong_header[] = {0xaa, 0xbb, 0x09, 0x07, 0x85, 0x40, 0x00, 0x90, 0x00};
    assert_reader_refuses(wrong_header, sizeof(wrong_header));
    return MUNIT_OK;
}

/* An answer that decrypts to something without the expected shape is refused
 * rather than read as though it had it. */
static MunitResult test_reader_refuses_bad_contents(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential card = credential_for_select();
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    card_answer(AES_128_CBC, SHA256, &card, answer);

    /* Disturb the initialisation vector, which changes the first block of the
     * recovered bytes, so they no longer describe an application. */
    uint8_t raw[BUFFER_CAPACITY];
    size_t len = bit_buffer_get_size_bytes(answer);
    memcpy(raw, bit_buffer_get_data(answer), len);
    raw[6] ^= 0xff;

    assert_reader_refuses(raw, len);
    bit_buffer_free(answer);
    return MUNIT_OK;
}

/* The card's answer is longer than the field a reader keeps it in only if
 * something is wrong; it must be clamped, not overrun. */
static MunitResult test_reader_clamps_long_answer(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential card = credential_for_select();
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    card_answer(AES_128_CBC, SHA256, &card, answer);

    uint8_t raw[BUFFER_CAPACITY];
    size_t len = bit_buffer_get_size_bytes(answer);
    memcpy(raw, bit_buffer_get_data(answer), len);
    /* Pad it out well past the field that holds it. */
    memset(raw + len, 0x00, 120);
    len += 120;

    SeosCredential reader;
    memset(&reader, 0, sizeof(reader));
    AuthParameters params;
    memset(&params, 0, sizeof(params));

    BitBuffer* padded = bit_buffer_alloc(BUFFER_CAPACITY);
    bit_buffer_copy_bytes(padded, raw, len);
    /* Whatever it decides, it must not have written past the field. */
    seos_reader_select_adf_response(padded, 0, &reader, &params);

    bit_buffer_free(padded);
    bit_buffer_free(answer);
    return MUNIT_OK;
}

/* ---- the command a reader sends ---- */

/* Header, the length the reader states, the list, and a trailing Le. */
static size_t build_select_adf_command(uint8_t* out, size_t list_len, uint8_t stated_len) {
    size_t len = 0;
    out[len++] = 0x80;
    out[len++] = 0xa5;
    out[len++] = 0x04;
    out[len++] = 0x00;
    out[len++] = stated_len;
    for(size_t i = 0; i < list_len; i++)
        out[len++] = (uint8_t)(0x10 + i);
    out[len++] = 0x00;
    return len;
}

static MunitResult test_select_adf_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_adf_command(apdu, 12, 12);

    const uint8_t* list = NULL;
    size_t list_len = 0;
    munit_assert_true(seos_parse_select_adf(apdu, apdu_len, &list, &list_len));
    munit_assert_size(list_len, ==, 12);
    munit_assert_ptr_equal(list, apdu + 5);

    return MUNIT_OK;
}

/* The length is the reader's claim, not a fact. A command naming more than it
 * carries must be refused rather than searched. */
static MunitResult test_select_adf_overlong_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_adf_command(apdu, 4, 200);

    const uint8_t* list = NULL;
    size_t list_len = 0;
    munit_assert_false(seos_parse_select_adf(apdu, apdu_len, &list, &list_len));

    return MUNIT_OK;
}

/* Every prefix short of the stated list is incomplete. */
static MunitResult test_select_adf_truncated(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_adf_command(apdu, 12, 12);

    const uint8_t* list = NULL;
    size_t list_len = 0;
    for(size_t cut = 0; cut < apdu_len - 1; cut++) {
        munit_assert_false(seos_parse_select_adf(apdu, cut, &list, &list_len));
    }
    /* Complete without the trailing Le. */
    munit_assert_true(seos_parse_select_adf(apdu, apdu_len - 1, &list, &list_len));

    return MUNIT_OK;
}

static MunitResult test_select_adf_wrong_header(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_adf_command(apdu, 8, 8);
    apdu[1] = 0xa4;

    const uint8_t* list = NULL;
    size_t list_len = 0;
    munit_assert_false(seos_parse_select_adf(apdu, apdu_len, &list, &list_len));

    return MUNIT_OK;
}

/* A command naming nothing has nothing to match. */
static MunitResult test_select_adf_empty_list(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_adf_command(apdu, 0, 0);

    const uint8_t* list = NULL;
    size_t list_len = 0;
    munit_assert_false(seos_parse_select_adf(apdu, apdu_len, &list, &list_len));

    return MUNIT_OK;
}

/* ---- selecting an application by identifier ---- */

static size_t build_select_aid_command(uint8_t* out, size_t aid_len, uint8_t stated_len) {
    size_t len = 0;
    out[len++] = 0x00;
    out[len++] = 0xa4;
    out[len++] = 0x04;
    out[len++] = 0x00;
    out[len++] = stated_len;
    for(size_t i = 0; i < aid_len; i++)
        out[len++] = (uint8_t)(0xa0 + i);
    out[len++] = 0x00;
    return len;
}

static MunitResult test_select_aid_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_aid_command(apdu, 10, 10);

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    munit_assert_true(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len));
    munit_assert_size(aid_len, ==, 10);
    munit_assert_ptr_equal(aid, apdu + 5);

    return MUNIT_OK;
}

/* The identifier is compared against several known ones, each ten bytes. A
 * command that stops before then must not be read that far. */
static MunitResult test_select_aid_truncated(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_aid_command(apdu, 10, 10);

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    for(size_t cut = 0; cut < apdu_len - 1; cut++) {
        munit_assert_false(seos_parse_select_aid(apdu, cut, &aid, &aid_len));
    }
    munit_assert_true(seos_parse_select_aid(apdu, apdu_len - 1, &aid, &aid_len));

    return MUNIT_OK;
}

static MunitResult test_select_aid_overlong(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_aid_command(apdu, 4, 200);

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    munit_assert_false(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len));

    return MUNIT_OK;
}

static MunitResult test_select_aid_wrong_header(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t apdu[64];
    size_t apdu_len = build_select_aid_command(apdu, 10, 10);
    apdu[1] = 0xa5;

    const uint8_t* aid = NULL;
    size_t aid_len = 0;
    munit_assert_false(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len));

    return MUNIT_OK;
}

static MunitTest test_select_adf_cases[] = {
    {(char*)"/aid/ok", test_select_aid_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aid/truncated", test_select_aid_truncated, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aid/overlong", test_select_aid_overlong, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aid/wrong-header",
     test_select_aid_wrong_header,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/command/ok", test_select_adf_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/command/overlong",
     test_select_adf_overlong_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/command/truncated",
     test_select_adf_truncated,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/command/wrong-header",
     test_select_adf_wrong_header,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/command/empty-list",
     test_select_adf_empty_list,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/round-trip/aes", test_round_trip_aes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/round-trip/des", test_round_trip_des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/varies/des", test_answer_varies_des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/unknown-application",
     test_unknown_application,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reader/malformed",
     test_reader_refuses_malformed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reader/short-at-offset",
     test_reader_refuses_short_at_offset,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reader/bad-contents",
     test_reader_refuses_bad_contents,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reader/long-answer",
     test_reader_clamps_long_answer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_select_adf_suite = {
    (char*)"/select-adf",
    test_select_adf_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
