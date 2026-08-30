/* Whole sessions, end to end.
 *
 * Each case takes a session's nonces, derives the keys, builds the command a
 * reader would send and unwraps the answer a card would give. Everything the
 * secure channel does is exercised at once: the derivation, the sequence
 * counter, the cryptogram, the checksum and the lengths.
 *
 * The expected values come from a model written separately from this
 * implementation, so they check it against something other than itself.
 */
#include "munit.h"
#include "test_helpers.h"

#include <secure_messaging.h>
#include <seos_protocol.h>
#include <seos_sm_command.h>

#define BUFFER_CAPACITY 512

typedef struct {
    uint8_t cipher;
    uint8_t hash;
    const char* card_nonce; /* the card's challenge */
    const char* reader_nonce; /* the reader's challenge */
    const char* card_key_half; /* the card's key contribution */
    const char* reader_key_half; /* the reader's key contribution */
    const char* privacy_key; /* expected, derived */
    const char* checksum_key; /* expected, derived */
    const char* cryptogram; /* expected, for the command below */
    const char* response; /* what the card answered */
    size_t response_plaintext_len;
} SessionVector;

/* Reading the credential's metadata object. */
static const char* read_command = "5c02ff41";

static const SessionVector aes_session = {
    .cipher = AES_128_CBC,
    .hash = SHA256,
    .card_nonce = "21282f363d444b52",
    .reader_nonce = "51585f666d747b82",
    .card_key_half = "91989fa6adb4bbc2c9d0d7dee5ecf3fa",
    .reader_key_half = "71787f868d949ba2a9b0b7bec5ccd3da",
    .privacy_key = "0bc1df915339a200e3986f652c9a42bf",
    .checksum_key = "5230643a393e676a3815d2582eec22a7",
    .cryptogram = "d3934f7b80f90b15b858c770f64ab7c9",
    .response = "8581a08d057fc61270a341f4d44cf7468219e37fa3eefc011e7995ede5bee3c8"
                "b0d99b12fbd6bd85caf5d55badadfcd13052bd86d3281dbf6aa8fc5addab344c"
                "fcc725d35fa2c3ad791742972692bbb8413ce41b604b7f4fe17dd25e6d77d3c2"
                "2097b51ac6e946714d70137113e8900a71673870604a40aedf354756f2a6fa36"
                "e4f6dc7d256d69f9b3d467ce330fa808fbcd9fba79cba39f9806de82a4d75e89"
                "b6a327990290008e083678c01f07f91cdb9000",
    .response_plaintext_len = 144,
};

static const SessionVector des_session = {
    .cipher = TWO_KEY_3DES_CBC_MODE,
    .hash = SHA1,
    .card_nonce = "777e858c939aa1a8",
    .reader_nonce = "a7aeb5bcc3cad1d8",
    .card_key_half = "e7eef5fc030a11181f262d343b424950",
    .reader_key_half = "c7ced5dce3eaf1f8ff060d141b222930",
    .privacy_key = "ce1e7147b721cfa0ac5948b1e47c1725",
    .checksum_key = "24eca46b64aa0d05f0d9d80f1f9d13f9",
    .cryptogram = "6f81f76b3114173e",
    .response = "854084f9147e17fece05946aa405cc5a4303cef88e54866fde4fa437e444339d"
                "f62fd132c6ad421bf00553c1055df53c41d084ef0e0628147ce2619e272948c5"
                "ce9f990290008e084abd495fe480ff149000",
    .response_plaintext_len = 63,
};

static void run_session(const SessionVector* v) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = v->cipher;
    params.hash = v->hash;
    hex_to_bytes(v->card_nonce, params.rndICC, sizeof(params.rndICC));
    hex_to_bytes(v->reader_nonce, params.UID, sizeof(params.UID));
    hex_to_bytes(v->reader_key_half, params.cNonce, sizeof(params.cNonce));
    hex_to_bytes(v->card_key_half, params.rNonce, sizeof(params.rNonce));

    SecureMessaging* sm = secure_messaging_alloc(&params);
    munit_assert_not_null(sm);

    /* The session the two ends agreed on. */
    uint8_t expected[16];
    hex_to_bytes(v->privacy_key, expected, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), sm->PrivacyKey, expected);
    hex_to_bytes(v->checksum_key, expected, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), sm->CMACKey, expected);

    /* The counter starts from the two challenges, most of the card's first. */
    size_t block_size = seos_cipher_block_size(v->cipher);
    uint8_t* counter = v->cipher == AES_128_CBC ? sm->aesContext : sm->desContext;
    uint8_t card_nonce[8];
    uint8_t reader_nonce[8];
    hex_to_bytes(v->card_nonce, card_nonce, sizeof(card_nonce));
    hex_to_bytes(v->reader_nonce, reader_nonce, sizeof(reader_nonce));
    munit_assert_memory_equal(block_size / 2, counter, card_nonce);
    munit_assert_memory_equal(block_size / 2, counter + block_size / 2, reader_nonce);

    /* The command the reader sent. */
    uint8_t command[8];
    size_t command_len = hex_to_bytes(read_command, command, sizeof(command));

    BitBuffer* tx = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        sm, command, command_len, (uint8_t*)SEOS_SM_HEADER, sizeof(SEOS_SM_HEADER), true, tx));

    uint8_t cryptogram[32];
    size_t cryptogram_len = hex_to_bytes(v->cryptogram, cryptogram, sizeof(cryptogram));

    /* Header, length, then the cryptogram object. */
    munit_assert_uint8(bit_buffer_get_byte(tx, 5), ==, 0x85);
    munit_assert_uint8(bit_buffer_get_byte(tx, 6), ==, (uint8_t)cryptogram_len);
    munit_assert_memory_equal(cryptogram_len, bit_buffer_get_data(tx) + 7, cryptogram);

    /* The answer the card gave, which the same session must accept. */
    uint8_t raw[BUFFER_CAPACITY];
    size_t raw_len = hex_to_bytes(v->response, raw, sizeof(raw));

    BitBuffer* rx = bit_buffer_alloc(BUFFER_CAPACITY);
    bit_buffer_copy_bytes(rx, raw, raw_len);

    munit_assert_true(secure_messaging_unwrap_rapdu(sm, rx));
    munit_assert_size(bit_buffer_get_size_bytes(rx), ==, v->response_plaintext_len);
    munit_assert_uint16(sm->last_response_sw, ==, SEOS_SW_SUCCESS_VALUE);

    /* The object the reader asked for is the one that came back. */
    munit_assert_uint8(bit_buffer_get_byte(rx, 0), ==, 0xff);
    munit_assert_uint8(bit_buffer_get_byte(rx, 1), ==, 0x41);

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    secure_messaging_free(sm);
}

static MunitResult test_aes_session(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    run_session(&aes_session);
    return MUNIT_OK;
}

static MunitResult test_des_session(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    run_session(&des_session);
    return MUNIT_OK;
}

/* The card answers a long object with a long-form length, so the reader has to
 * read one. */
static MunitResult test_response_uses_long_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t raw[BUFFER_CAPACITY];
    hex_to_bytes(aes_session.response, raw, sizeof(raw));

    munit_assert_uint8(raw[0], ==, 0x85);
    munit_assert_uint8(raw[1], ==, 0x81);
    /* The long form is only correct for a length a single byte cannot carry. */
    munit_assert_uint8(raw[2], >=, 0x80);
    return MUNIT_OK;
}

static MunitTest test_session_vector_cases[] = {
    {(char*)"/aes", test_aes_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des", test_des_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/long-form-response",
     test_response_uses_long_form,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_session_vectors_suite = {
    (char*)"/session-vectors",
    test_session_vector_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
