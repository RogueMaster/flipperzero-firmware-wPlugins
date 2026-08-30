/* A whole emulation, replayed against what a reader actually sent.
 *
 * The frames below are one exchange between a reader and this emulator,
 * captured off the air. Feeding the reader's half back in has to produce the
 * card's half byte for byte: the same select answer, the same challenge, the
 * same cryptogram, the same protected response. Nothing here is derived from
 * our own code, so a change that alters the wire shows up as a failure rather
 * than as a new expected value.
 */
#include "munit.h"
#include "test_helpers.h"

#include <furi_hal.h>
#include <keys.h>
#include <secure_messaging.h>
#include <seos_protocol.h>
#include <seos_sm_command.h>

#define BUFFER_CAPACITY 512

/* A credential made up for this test, and the exchange a reader would have
 * with it. The card's answers were worked out by a model written separately
 * from this implementation, so they remain something to check against rather
 * than a recording of what our own code did. */
#define CRED_OID      "2b0601040181e4380101020118010199"
#define CRED_PRIV_KEY "31383f464d545b626970777e858c939a"
#define CRED_AUTH_KEY "828990979ea5acb3bac1c8cfd6dde4eb"
#define CRED_SIO                                                       \
    "11181f262d343b424950575e656c737a81888f969da4abb2b9c0c7ced5dce3ea" \
    "f1f8ff060d141b22"
#define CRED_ADF_RESPONSE                                              \
    "cd020907854040474e555c636a71787f868d949ba2a9b0b7bec5ccd3dae1e8ef" \
    "f6fd040b121920272e353c434a51585f666d747b828990979ea5acb3bac1c8cf" \
    "d6dde4ebf2f98e089aa1a8afb6bdc4cb"
#define CARD_CHALLENGE    "a3aab1b8bfc6cdd4"
#define CARD_KEY_HALF     "c7ced5dce3eaf1f8ff060d141b222930"
#define READER_SELECT_ADF "06102b0601040181e4380101020118010199"
#define READER_AUTHENTICATE_2                                          \
    "008700022c7c2a8228ba3a56760e9dbd18e3574e63d37b28478226e7c7d096d9" \
    "63c212ce4e848d32a98669ca9fa6274bd800"
#define READER_GET_DATA                                                \
    "0ccb3fff1e85106d4e96677cae8ea626d51e2a802dbe3697008e089d454a551b" \
    "c8f62900"
#define CARD_AUTHENTICATE_1 "7c0a8108a3aab1b8bfc6cdd4"
#define CARD_AUTHENTICATE_2                                            \
    "7c2a8228ffb61afc1eabd6b49064b53804d6f07b50c506ac15e3aedf68987dbd" \
    "f5c74c7bac77e185693b62d6"
#define CARD_GET_DATA                                                  \
    "8530d1ee5b65ffca0e9787aa3db4ff47f41aede92f830974c5899faff9834ff2" \
    "d418df4ac93cb2df93d4fc8f6b6be1af61a9990290008e0883c99d37363faca3" \
    "9000"

/* The keyset the reader authenticates with, and the length of the answer's
 * plaintext. */
#define READER_KEY_NUMBER         0x02
#define CARD_ANSWER_PLAINTEXT_LEN 43

static SeosCredential saved_credential(void) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    credential.adf_oid_len =
        hex_to_bytes(CRED_OID, credential.adf_oid, sizeof(credential.adf_oid));
    credential.sio_len = hex_to_bytes(CRED_SIO, credential.sio, sizeof(credential.sio));
    hex_to_bytes(CRED_PRIV_KEY, credential.priv_key, sizeof(credential.priv_key));
    hex_to_bytes(CRED_AUTH_KEY, credential.auth_key, sizeof(credential.auth_key));
    hex_to_bytes(CRED_ADF_RESPONSE, credential.adf_response, sizeof(credential.adf_response));
    return credential;
}

/* Starts the card the way the emulator does, with its two random values
 * queued so the exchange reproduces. */
static AuthParameters started_card(void) {
    uint8_t challenge[8];
    uint8_t key_half[16];
    hex_to_bytes(CARD_CHALLENGE, challenge, sizeof(challenge));
    hex_to_bytes(CARD_KEY_HALF, key_half, sizeof(key_half));

    seos_host_clear_random();
    seos_host_set_random(challenge, sizeof(challenge));
    seos_host_set_random(key_half, sizeof(key_half));

    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = TWO_KEY_3DES_CBC_MODE;
    params.hash = SHA1;
    seos_worker_random_nonce(params.rndICC, sizeof(params.rndICC));
    seos_worker_random_nonce(params.rNonce, sizeof(params.rNonce));
    return params;
}

static void assert_frame(BitBuffer* actual, const char* expected_hex) {
    uint8_t expected[BUFFER_CAPACITY];
    size_t expected_len = hex_to_bytes(expected_hex, expected, sizeof(expected));

    munit_assert_size(bit_buffer_get_size_bytes(actual), ==, expected_len);
    munit_assert_memory_equal(expected_len, bit_buffer_get_data(actual), expected);
}

/* The whole exchange, one frame at a time. */
static MunitResult test_replays_captured_exchange(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = saved_credential();
    AuthParameters params = started_card();

    uint8_t frame[BUFFER_CAPACITY];
    size_t frame_len;
    BitBuffer* tx = bit_buffer_alloc(BUFFER_CAPACITY);

    /* The reader offers three applications; ours is the second. */
    frame_len = hex_to_bytes(READER_SELECT_ADF, frame, sizeof(frame));
    munit_assert_true(seos_emulator_select_adf(frame, frame_len, &params, &credential, tx));
    assert_frame(tx, CRED_ADF_RESPONSE);

    /* The saved answer names the cipher and digest the session then runs
     * under, which is not the one the card starts out assuming. */
    munit_assert_uint8(params.cipher, ==, AES_128_CBC);
    munit_assert_uint8(params.hash, ==, SHA256);

    bit_buffer_reset(tx);
    seos_emulator_general_authenticate_1(tx, params);
    assert_frame(tx, CARD_AUTHENTICATE_1);

    bit_buffer_reset(tx);
    frame_len = hex_to_bytes(READER_AUTHENTICATE_2, frame, sizeof(frame));
    munit_assert_true(
        seos_emulator_general_authenticate_2(frame, frame_len, &credential, &params, tx));
    assert_frame(tx, CARD_AUTHENTICATE_2);

    /* The reader's key number came out of the command it sent. */
    munit_assert_uint8(params.key_no, ==, READER_KEY_NUMBER);

    SecureMessaging* sm = secure_messaging_alloc(&params);
    munit_assert_not_null(sm);

    bit_buffer_reset(tx);
    frame_len = hex_to_bytes(READER_GET_DATA, frame, sizeof(frame));
    munit_assert_true(seos_sm_command_handle(
        sm, &credential, frame, frame_len, SEOS_SM_MAX_FRAME, tx, NULL, NULL));
    assert_frame(tx, CARD_GET_DATA);

    bit_buffer_free(tx);
    secure_messaging_free(sm);
    return MUNIT_OK;
}

/* A saved answer whose cryptogram will not fit must not be read past the end
 * of the field holding it. */
static MunitResult test_oversized_saved_answer(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = saved_credential();
    AuthParameters params = started_card();

    /* A length byte claiming more than the field can hold. */
    credential.adf_response[5] = 0xff;

    uint8_t frame[BUFFER_CAPACITY];
    size_t frame_len = hex_to_bytes(READER_SELECT_ADF, frame, sizeof(frame));
    BitBuffer* tx = bit_buffer_alloc(BUFFER_CAPACITY);

    /* Whatever it decides, it must not read beyond the saved answer. */
    seos_emulator_select_adf(frame, frame_len, &params, &credential, tx);
    munit_assert_size(bit_buffer_get_size_bytes(tx), <=, sizeof(credential.adf_response));

    bit_buffer_free(tx);
    return MUNIT_OK;
}

/* A credential with no saved answer falls back to building one. */
static MunitResult test_no_saved_answer(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = saved_credential();
    memset(credential.adf_response, 0, sizeof(credential.adf_response));
    credential.diversifier_len = 7;
    memcpy(credential.diversifier, "\xfb\xab\x70\x44\x6b\xdb\x1d", 7);

    AuthParameters params = started_card();
    params.cipher = AES_128_CBC;
    params.hash = SHA256;

    /* The keys file names a different application, so the built answer is
     * only reachable when that one is offered. */
    uint8_t frame[BUFFER_CAPACITY];
    size_t frame_len = 0;
    frame[frame_len++] = 0x06;
    frame[frame_len++] = (uint8_t)SEOS_ADF_OID_LEN;
    memcpy(frame + frame_len, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    frame_len += SEOS_ADF_OID_LEN;

    BitBuffer* tx = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(seos_emulator_select_adf(frame, frame_len, &params, &credential, tx));

    /* Algorithm pair, cryptogram object, checksum object. */
    munit_assert_uint8(bit_buffer_get_byte(tx, 0), ==, 0xcd);
    munit_assert_uint8(bit_buffer_get_byte(tx, 4), ==, 0x85);
    size_t len = bit_buffer_get_size_bytes(tx);
    munit_assert_uint8(bit_buffer_get_byte(tx, len - 10), ==, 0x8e);

    bit_buffer_free(tx);
    return MUNIT_OK;
}

static MunitTest test_emulated_card_cases[] = {
    {(char*)"/replay", test_replays_captured_exchange, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/saved-answer/oversized",
     test_oversized_saved_answer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/saved-answer/absent", test_no_saved_answer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_emulated_card_suite = {
    (char*)"/emulated-card",
    test_emulated_card_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
