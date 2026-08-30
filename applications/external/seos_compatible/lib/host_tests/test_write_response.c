/* What a reader accepts as proof that a card stored what it was sent.
 *
 * The answer to a write is protected, so the status word in the clear settles
 * nothing on its own: anything in the field can send one. The checksum over
 * the protected status is what decides it.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_protocol.h>
#include <seos_sm_command.h>

#include <string.h>

#define BUFFER_CAPACITY 512

static AuthParameters write_params(void) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x41 + i);
    for(size_t i = 0; i < sizeof(params.UID); i++)
        params.UID[i] = (uint8_t)(0x82 + i);
    return params;
}

/* A reader and a card that agree on a session, and the card's answer to one
 * write. Both counters have been stepped for the command. */
typedef struct {
    SecureMessaging* reader;
    SecureMessaging* card;
    BitBuffer* answer;
    SeosCredential credential;
} WriteExchange;

static void write_exchange_begin(WriteExchange* exchange, size_t sio_len) {
    AuthParameters params = write_params();
    exchange->reader = secure_messaging_alloc(&params);
    exchange->card = secure_messaging_alloc(&params);
    munit_assert_not_null(exchange->reader);
    munit_assert_not_null(exchange->card);

    memset(&exchange->credential, 0, sizeof(exchange->credential));

    uint8_t sio[256];
    for(size_t i = 0; i < sio_len; i++)
        sio[i] = (uint8_t)(i * 5 + 9);

    /* The command names the object then carries it. */
    uint8_t message[300];
    size_t message_len = 0;
    message[message_len++] = 0xff;
    message[message_len++] = 0x00;
    message[message_len++] = (uint8_t)sio_len;
    memcpy(message + message_len, sio, sio_len);
    message_len += sio_len;

    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        exchange->reader,
        message,
        message_len,
        (uint8_t*)SEOS_SM_PUT_HEADER,
        sizeof(SEOS_SM_PUT_HEADER),
        false,
        wire));

    exchange->answer = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(seos_sm_command_handle(
        exchange->card,
        &exchange->credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        SEOS_SM_MAX_FRAME,
        exchange->answer,
        NULL,
        NULL));

    bit_buffer_free(wire);
}

static void write_exchange_end(WriteExchange* exchange) {
    secure_messaging_free(exchange->reader);
    secure_messaging_free(exchange->card);
    bit_buffer_free(exchange->answer);
}

/* The card really did store it, and says so under the session key. */
static MunitResult test_accepts_a_protected_success(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    WriteExchange exchange;
    write_exchange_begin(&exchange, 32);

    munit_assert_true(seos_reader_write_accepted(exchange.reader, exchange.answer));
    /* The card stored what it was sent. */
    munit_assert_size(exchange.credential.sio_len, ==, 32);

    write_exchange_end(&exchange);
    return MUNIT_OK;
}

/* Two bytes in the clear are what an unprotected card, or anything else in
 * the field, would answer. */
static MunitResult test_refuses_a_bare_status_word(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    WriteExchange exchange;
    write_exchange_begin(&exchange, 32);

    BitBuffer* bare = bit_buffer_alloc(BUFFER_CAPACITY);
    const uint8_t success[] = {0x90, 0x00};
    bit_buffer_copy_bytes(bare, success, sizeof(success));

    munit_assert_false(seos_reader_write_accepted(exchange.reader, bare));

    bit_buffer_free(bare);
    write_exchange_end(&exchange);
    return MUNIT_OK;
}

/* The answer closes with a status word in the clear, which the checksum does
 * not cover. Everything ahead of it does, so changing any of it must be
 * refused. */
static MunitResult test_refuses_a_tampered_answer(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    WriteExchange exchange;
    write_exchange_begin(&exchange, 32);

    size_t answer_len = bit_buffer_get_size_bytes(exchange.answer);
    munit_assert_size(answer_len, >, sizeof(uint16_t));
    size_t protected_len = answer_len - sizeof(uint16_t);

    for(size_t i = 0; i < protected_len; i++) {
        /* A fresh session each time: a refused answer leaves the counters
         * where they were, and the next attempt needs them in step. */
        WriteExchange tampered;
        write_exchange_begin(&tampered, 32);

        uint8_t raw[BUFFER_CAPACITY];
        memcpy(raw, bit_buffer_get_data(tampered.answer), answer_len);
        raw[i] ^= 0xff;

        BitBuffer* altered = bit_buffer_alloc(BUFFER_CAPACITY);
        bit_buffer_copy_bytes(altered, raw, answer_len);
        munit_assert_false(seos_reader_write_accepted(tampered.reader, altered));

        bit_buffer_free(altered);
        write_exchange_end(&tampered);
    }

    write_exchange_end(&exchange);
    return MUNIT_OK;
}

/* The status word in the clear is not what decides it. Anything in the field
 * can set that, so the protected one has to be what is believed -- in both
 * directions. */
static MunitResult test_ignores_the_cleartext_status(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    WriteExchange exchange;
    write_exchange_begin(&exchange, 32);

    size_t answer_len = bit_buffer_get_size_bytes(exchange.answer);
    uint8_t raw[BUFFER_CAPACITY];
    memcpy(raw, bit_buffer_get_data(exchange.answer), answer_len);

    /* A failure claimed in the clear over a protected success. */
    raw[answer_len - 2] = 0x6a;
    raw[answer_len - 1] = 0x84;

    BitBuffer* altered = bit_buffer_alloc(BUFFER_CAPACITY);
    bit_buffer_copy_bytes(altered, raw, answer_len);
    munit_assert_true(seos_reader_write_accepted(exchange.reader, altered));

    bit_buffer_free(altered);
    write_exchange_end(&exchange);
    return MUNIT_OK;
}

/* A card that answers something other than success, under a checksum that
 * verifies, is still a refusal. */
static MunitResult test_refuses_a_protected_failure(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = write_params();
    SecureMessaging* reader = secure_messaging_alloc(&params);
    SecureMessaging* card = secure_messaging_alloc(&params);

    /* A real command first, so both counters step together and the refusal
     * below is about the status word rather than the counters drifting. */
    uint8_t message[] = {0xff, 0x00, 0x01, 0x5a};
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        reader,
        message,
        sizeof(message),
        (uint8_t*)SEOS_SM_PUT_HEADER,
        sizeof(SEOS_SM_PUT_HEADER),
        false,
        wire));
    munit_assert_true(secure_messaging_unwrap_apdu(card, wire));

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_rapdu(card, NULL, 0, SEOS_SW_NOT_ENOUGH_ROOM, answer));

    munit_assert_false(seos_reader_write_accepted(reader, answer));

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    secure_messaging_free(reader);
    secure_messaging_free(card);
    return MUNIT_OK;
}

/* Reading the answer steps the counter for it. If it did not, the next
 * command would be wrapped under the wrong one and the card would refuse it. */
static MunitResult test_steps_the_counter(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    WriteExchange exchange;
    write_exchange_begin(&exchange, 16);

    munit_assert_true(seos_reader_write_accepted(exchange.reader, exchange.answer));

    /* A second command, wrapped by the reader and read by the card, only
     * verifies if both counters moved the same way. */
    uint8_t follow_up[] = {0x5c, 0x02, 0xff, 0x00};
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        exchange.reader,
        follow_up,
        sizeof(follow_up),
        (uint8_t*)SEOS_SM_HEADER,
        sizeof(SEOS_SM_HEADER),
        true,
        wire));

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(seos_sm_command_handle(
        exchange.card,
        &exchange.credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        SEOS_SM_MAX_FRAME,
        answer,
        NULL,
        NULL));

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    write_exchange_end(&exchange);
    return MUNIT_OK;
}

static MunitTest test_write_response_cases[] = {
    {(char*)"/accepts-protected-success",
     test_accepts_a_protected_success,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/refuses-bare-status",
     test_refuses_a_bare_status_word,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/refuses-tampered",
     test_refuses_a_tampered_answer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/ignores-cleartext-status",
     test_ignores_the_cleartext_status,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/refuses-protected-failure",
     test_refuses_a_protected_failure,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/steps-the-counter", test_steps_the_counter, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_write_response_suite = {
    (char*)"/write-response",
    test_write_response_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
