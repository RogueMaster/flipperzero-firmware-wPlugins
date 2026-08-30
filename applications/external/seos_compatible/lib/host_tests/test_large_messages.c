/* Wrapping and unwrapping messages larger than one frame's worth.
 *
 * The scratch buffers used to be fixed size arrays on the stack, which put a
 * hard ceiling on a message and charged every caller for the largest one.
 */
#include "munit.h"
#include "test_helpers.h"

#include <secure_messaging.h>
#include <seos_protocol.h>
#include <seos_sm_command.h>

#include <stdlib.h>
#include <string.h>

#define BUFFER_CAPACITY 4096

static AuthParameters large_params(uint8_t cipher, uint8_t hash) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = cipher;
    params.hash = hash;
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x33 + i);
    for(size_t i = 0; i < sizeof(params.UID); i++)
        params.UID[i] = (uint8_t)(0x77 + i);
    return params;
}

static void fill(uint8_t* out, size_t len) {
    for(size_t i = 0; i < len; i++)
        out[i] = (uint8_t)(i * 13 + 7);
}

/* A command of `len` bytes wrapped by one side and read by the other. */
static void assert_command_round_trips(uint8_t cipher, uint8_t hash, size_t len) {
    AuthParameters params = large_params(cipher, hash);
    SecureMessaging* sender = secure_messaging_alloc(&params);
    SecureMessaging* receiver = secure_messaging_alloc(&params);
    munit_assert_not_null(sender);
    munit_assert_not_null(receiver);

    uint8_t* message = malloc(len);
    munit_assert_not_null(message);
    fill(message, len);

    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        sender, message, len, (uint8_t*)SEOS_SM_HEADER, sizeof(SEOS_SM_HEADER), true, wire));

    munit_assert_true(secure_messaging_unwrap_apdu(receiver, wire));
    munit_assert_size(bit_buffer_get_size_bytes(wire), ==, len);
    munit_assert_memory_equal(len, bit_buffer_get_data(wire), message);

    bit_buffer_free(wire);
    free(message);
    secure_messaging_free(sender);
    secure_messaging_free(receiver);
}

/* A response of `len` bytes, the same way round. */
static void assert_response_round_trips(uint8_t cipher, uint8_t hash, size_t len) {
    AuthParameters params = large_params(cipher, hash);
    SecureMessaging* card = secure_messaging_alloc(&params);
    SecureMessaging* reader = secure_messaging_alloc(&params);

    uint8_t* message = malloc(len);
    munit_assert_not_null(message);
    fill(message, len);

    /* Wrapping steps the card's counter and unwrapping steps the reader's, so
     * the two stay level without a command in between. */
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(
        secure_messaging_wrap_rapdu(card, message, len, SEOS_SW_SUCCESS_VALUE, wire));

    munit_assert_true(secure_messaging_unwrap_rapdu(reader, wire));
    munit_assert_size(bit_buffer_get_size_bytes(wire), ==, len);
    munit_assert_memory_equal(len, bit_buffer_get_data(wire), message);

    bit_buffer_free(wire);
    free(message);
    secure_messaging_free(card);
    secure_messaging_free(reader);
}

/* Past what the fixed buffers used to hold, up to what one command can state
 * a length for. */
static MunitResult test_large_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t lengths[] = {200, 210, SECURE_MESSAGING_COMMAND_MAX};

    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        assert_command_round_trips(AES_128_CBC, SHA256, lengths[i]);
        assert_command_round_trips(TWO_KEY_3DES_CBC_MODE, SHA1, lengths[i]);
    }

    return MUNIT_OK;
}

/* A command states its own length in one byte. A message that would not fit
 * is refused rather than sent with a length that wrapped. */
static MunitResult test_command_too_long_is_refused(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t lengths[] = {256, 400, 1000};

    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        AuthParameters params = large_params(AES_128_CBC, SHA256);
        SecureMessaging* sender = secure_messaging_alloc(&params);

        uint8_t* message = malloc(lengths[i]);
        fill(message, lengths[i]);

        BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
        munit_assert_false(secure_messaging_wrap_apdu(
            sender,
            message,
            lengths[i],
            (uint8_t*)SEOS_SM_HEADER,
            sizeof(SEOS_SM_HEADER),
            true,
            wire));

        bit_buffer_free(wire);
        free(message);
        secure_messaging_free(sender);
    }

    return MUNIT_OK;
}

static MunitResult test_large_response(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t lengths[] = {200, 255, 256, 400, 1000};

    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        assert_response_round_trips(AES_128_CBC, SHA256, lengths[i]);
        assert_response_round_trips(TWO_KEY_3DES_CBC_MODE, SHA1, lengths[i]);
    }

    return MUNIT_OK;
}

/* Lengths on and either side of each boundary where the encoded length grows
 * an octet, and either side of a cipher block. */
static MunitResult test_length_boundaries(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t command_lengths[] = {1, 15, 16, 17, 127, 128, 129, 191, 192, 193, 223};
    static const size_t response_lengths[] = {1, 127, 128, 129, 254, 255, 256, 257};

    for(size_t i = 0; i < sizeof(command_lengths) / sizeof(command_lengths[0]); i++) {
        assert_command_round_trips(AES_128_CBC, SHA256, command_lengths[i]);
    }
    for(size_t i = 0; i < sizeof(response_lengths) / sizeof(response_lengths[0]); i++) {
        assert_response_round_trips(AES_128_CBC, SHA256, response_lengths[i]);
    }

    return MUNIT_OK;
}

/* Nothing is left over between calls: the same session wraps a long message
 * then a short one and both read back exactly. */
static MunitResult test_long_then_short(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = large_params(AES_128_CBC, SHA256);
    SecureMessaging* sender = secure_messaging_alloc(&params);
    SecureMessaging* receiver = secure_messaging_alloc(&params);

    uint8_t big[SECURE_MESSAGING_COMMAND_MAX];
    fill(big, sizeof(big));
    uint8_t small[4] = {1, 2, 3, 4};

    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        sender, big, sizeof(big), (uint8_t*)SEOS_SM_HEADER, sizeof(SEOS_SM_HEADER), true, wire));
    munit_assert_true(secure_messaging_unwrap_apdu(receiver, wire));
    munit_assert_size(bit_buffer_get_size_bytes(wire), ==, sizeof(big));

    bit_buffer_reset(wire);
    munit_assert_true(secure_messaging_wrap_apdu(
        sender, small, sizeof(small), (uint8_t*)SEOS_SM_HEADER, sizeof(SEOS_SM_HEADER), true, wire));
    munit_assert_true(secure_messaging_unwrap_apdu(receiver, wire));
    munit_assert_size(bit_buffer_get_size_bytes(wire), ==, sizeof(small));
    munit_assert_memory_equal(sizeof(small), bit_buffer_get_data(wire), small);

    bit_buffer_free(wire);
    secure_messaging_free(sender);
    secure_messaging_free(receiver);
    return MUNIT_OK;
}

static MunitTest test_large_messages_cases[] = {
    {(char*)"/command", test_large_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/command-too-long",
     test_command_too_long_is_refused,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/response", test_large_response, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/boundaries", test_length_boundaries, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/long-then-short", test_long_then_short, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_large_messages_suite = {
    (char*)"/large-messages",
    test_large_messages_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
