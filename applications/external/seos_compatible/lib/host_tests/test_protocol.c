/* The Seos exchange, both halves against each other.
 *
 * The reader and the card each hold half the handshake. Running them against
 * one another is the only way to show the two agree, which until now nothing
 * did: the code lived inside the NFC poller and listener callbacks.
 */
#include "munit.h"
#include "test_helpers.h"

#include <keys.h>
#include <seos_protocol.h>

#define TX_CAPACITY 128

static SeosCredential test_credential(void) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    credential.use_hardcoded = false;
    credential.diversifier_len = 8;
    for(size_t i = 0; i < credential.diversifier_len; i++)
        credential.diversifier[i] = (uint8_t)(0x50 + i);
    credential.adf_oid_len = SEOS_ADF_OID_LEN;
    memcpy(credential.adf_oid, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    return credential;
}

static MunitResult test_select_aid_response(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t aid[] = {0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};
    uint8_t expected[] = {
        0x6f, 0x0c, 0x84, 0x0a, 0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};

    BitBuffer* tx = bit_buffer_alloc(TX_CAPACITY);
    seos_emulator_select_aid(tx, aid, sizeof(aid));

    munit_assert_size(bit_buffer_get_size_bytes(tx), ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), bit_buffer_get_data(tx), expected);

    bit_buffer_free(tx);
    return MUNIT_OK;
}

/* The first step hands the reader the card's challenge. */
static MunitResult test_authenticate_1_carries_challenge(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x90 + i);

    BitBuffer* tx = bit_buffer_alloc(TX_CAPACITY);
    seos_emulator_general_authenticate_1(tx, params);

    uint8_t expected_header[] = {0x7c, 0x0a, 0x81, 0x08};
    munit_assert_size(
        bit_buffer_get_size_bytes(tx), ==, sizeof(expected_header) + sizeof(params.rndICC));
    munit_assert_memory_equal(sizeof(expected_header), bit_buffer_get_data(tx), expected_header);
    munit_assert_memory_equal(
        sizeof(params.rndICC), bit_buffer_get_data(tx) + sizeof(expected_header), params.rndICC);

    bit_buffer_free(tx);
    return MUNIT_OK;
}

/* Builds the second authenticate command around a reader cryptogram. */
static size_t build_authenticate_2(uint8_t key_no, const uint8_t* cryptogram, uint8_t* frame) {
    uint8_t prefix[] = {0x00, 0x87, 0x00, key_no, 0x2c, 0x7c, 0x2a, 0x82, 0x28};
    memcpy(frame, prefix, sizeof(prefix));
    memcpy(frame + sizeof(prefix), cryptogram, 40);
    return sizeof(prefix) + 40;
}

/* Runs the whole handshake and checks both ends reach the same session keys.
 * If either half disagreed, the derived keys would differ. */
static void mutual_authentication(uint8_t cipher, uint8_t hash, uint8_t key_no) {
    SeosCredential credential = test_credential();

    AuthParameters card;
    memset(&card, 0, sizeof(card));
    card.cipher = cipher;
    card.hash = hash;
    for(size_t i = 0; i < sizeof(card.rndICC); i++)
        card.rndICC[i] = (uint8_t)(0x11 * (i + 1));
    for(size_t i = 0; i < sizeof(card.rNonce); i++)
        card.rNonce[i] = (uint8_t)(0x70 + i);

    /* The reader learns the challenge from the first step. */
    BitBuffer* tx = bit_buffer_alloc(TX_CAPACITY);
    seos_emulator_general_authenticate_1(tx, card);

    AuthParameters reader;
    memset(&reader, 0, sizeof(reader));
    reader.cipher = cipher;
    reader.hash = hash;
    reader.key_no = key_no;
    memcpy(reader.rndICC, bit_buffer_get_data(tx) + 4, sizeof(reader.rndICC));
    for(size_t i = 0; i < sizeof(reader.UID); i++)
        reader.UID[i] = (uint8_t)(0x21 + i);
    for(size_t i = 0; i < sizeof(reader.cNonce); i++)
        reader.cNonce[i] = (uint8_t)(0x31 + i);

    uint8_t cryptogram[40];
    seos_reader_generate_cryptogram(&credential, &reader, cryptogram);

    uint8_t frame[64];
    size_t frame_len = build_authenticate_2(key_no, cryptogram, frame);

    bit_buffer_reset(tx);
    munit_assert_true(
        seos_emulator_general_authenticate_2(frame, frame_len, &credential, &card, tx));

    /* The card accepted, so it recovered what the reader sent. */
    munit_assert_memory_equal(sizeof(card.UID), card.UID, reader.UID);
    munit_assert_memory_equal(sizeof(card.cNonce), card.cNonce, reader.cNonce);

    /* And the reader accepts the card's answer, learning its nonce. */
    munit_assert_size(bit_buffer_get_size_bytes(tx), >=, 44);
    munit_assert_true(seos_reader_verify_cryptogram(&reader, bit_buffer_get_data(tx) + 4));
    munit_assert_memory_equal(sizeof(reader.rNonce), reader.rNonce, card.rNonce);

    /* Both sides now derive the same session. */
    SecureMessaging* card_session = secure_messaging_alloc(&card);
    SecureMessaging* reader_session = secure_messaging_alloc(&reader);
    munit_assert_not_null(card_session);
    munit_assert_not_null(reader_session);
    munit_assert_memory_equal(
        sizeof(card_session->PrivacyKey), card_session->PrivacyKey, reader_session->PrivacyKey);
    munit_assert_memory_equal(
        sizeof(card_session->CMACKey), card_session->CMACKey, reader_session->CMACKey);

    secure_messaging_free(card_session);
    secure_messaging_free(reader_session);
    bit_buffer_free(tx);
}

static MunitResult test_mutual_authentication_des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    mutual_authentication(TWO_KEY_3DES_CBC_MODE, SHA1, 0x01);
    return MUNIT_OK;
}

static MunitResult test_mutual_authentication_aes(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    mutual_authentication(AES_128_CBC, SHA256, 0x01);
    return MUNIT_OK;
}

/* Key number two selects the write key, so the two slots must not agree. */
static MunitResult test_write_keyslot_differs(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = test_credential();

    AuthParameters read_params;
    memset(&read_params, 0, sizeof(read_params));
    read_params.cipher = AES_128_CBC;
    read_params.hash = SHA256;
    read_params.key_no = 0x01;
    AuthParameters write_params = read_params;
    write_params.key_no = 0x02;

    uint8_t read_cryptogram[40];
    uint8_t write_cryptogram[40];
    seos_reader_generate_cryptogram(&credential, &read_params, read_cryptogram);
    seos_reader_generate_cryptogram(&credential, &write_params, write_cryptogram);

    munit_assert_memory_not_equal(
        sizeof(read_params.priv_key), read_params.priv_key, write_params.priv_key);
    munit_assert_memory_not_equal(sizeof(read_cryptogram), read_cryptogram, write_cryptogram);
    return MUNIT_OK;
}

/* A frame shorter than the cryptogram it claims must be refused, not read
 * past. The length used to be ignored entirely. */
static MunitResult test_rejects_short_authenticate(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = test_credential();
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;

    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    BitBuffer* tx = bit_buffer_alloc(TX_CAPACITY);

    for(size_t len = 0; len < 49; len++) {
        munit_assert_false(
            seos_emulator_general_authenticate_2(frame, len, &credential, &params, tx));
    }

    bit_buffer_free(tx);
    return MUNIT_OK;
}

/* A cryptogram the card cannot authenticate must be refused. */
static MunitResult test_rejects_bad_cryptogram(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = test_credential();

    AuthParameters card;
    memset(&card, 0, sizeof(card));
    card.cipher = AES_128_CBC;
    card.hash = SHA256;
    for(size_t i = 0; i < sizeof(card.rndICC); i++)
        card.rndICC[i] = (uint8_t)(0x11 * (i + 1));

    AuthParameters reader = card;
    reader.key_no = 0x01;

    uint8_t cryptogram[40];
    seos_reader_generate_cryptogram(&credential, &reader, cryptogram);
    cryptogram[0] ^= 0x01;

    uint8_t frame[64];
    size_t frame_len = build_authenticate_2(0x01, cryptogram, frame);

    BitBuffer* tx = bit_buffer_alloc(TX_CAPACITY);
    munit_assert_false(
        seos_emulator_general_authenticate_2(frame, frame_len, &credential, &card, tx));

    bit_buffer_free(tx);
    return MUNIT_OK;
}

/* An answer that gives nothing away still has to look like an answer. */
static MunitResult test_shill_select_looks_real(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* first = bit_buffer_alloc(TX_CAPACITY);
    BitBuffer* second = bit_buffer_alloc(TX_CAPACITY);
    seos_emulator_shill_select_adf(first);
    seos_emulator_shill_select_adf(second);

    /* The algorithm pair, a cryptogram object, a checksum object, success. */
    munit_assert_uint8(bit_buffer_get_byte(first, 0), ==, 0xcd);
    munit_assert_uint8(bit_buffer_get_byte(first, 1), ==, 0x02);
    munit_assert_uint8(bit_buffer_get_byte(first, 4), ==, 0x85);
    size_t len = bit_buffer_get_size_bytes(first);
    munit_assert_uint8(bit_buffer_get_byte(first, len - 12), ==, 0x8e);
    munit_assert_uint8(bit_buffer_get_byte(first, len - 2), ==, 0x90);
    munit_assert_uint8(bit_buffer_get_byte(first, len - 1), ==, 0x00);

    /* And two of them must not be the same, or it is a fingerprint. */
    munit_assert_size(bit_buffer_get_size_bytes(second), ==, len);
    munit_assert_memory_not_equal(len, bit_buffer_get_data(first), bit_buffer_get_data(second));

    bit_buffer_free(first);
    bit_buffer_free(second);
    return MUNIT_OK;
}

static MunitResult test_shill_authenticate_looks_real(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    BitBuffer* first = bit_buffer_alloc(TX_CAPACITY);
    BitBuffer* second = bit_buffer_alloc(TX_CAPACITY);
    seos_emulator_shill_authenticate(first);
    seos_emulator_shill_authenticate(second);

    uint8_t expected_header[] = {0x7c, 0x2a, 0x82, 0x28};
    munit_assert_memory_equal(
        sizeof(expected_header), bit_buffer_get_data(first), expected_header);
    size_t len = bit_buffer_get_size_bytes(first);
    munit_assert_size(len, ==, 4 + 0x28 + 2);
    munit_assert_uint8(bit_buffer_get_byte(first, len - 2), ==, 0x90);

    munit_assert_memory_not_equal(len, bit_buffer_get_data(first), bit_buffer_get_data(second));

    bit_buffer_free(first);
    bit_buffer_free(second);
    return MUNIT_OK;
}

/* Two select answers for the same credential must differ, or the card is
 * recognisable from its answer alone. */
static MunitResult test_select_response_varies(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = test_credential();

    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;

    uint8_t oid_list[64];
    size_t oid_list_len = 0;
    oid_list[oid_list_len++] = 0x06;
    oid_list[oid_list_len++] = (uint8_t)SEOS_ADF_OID_LEN;
    memcpy(oid_list + oid_list_len, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
    oid_list_len += SEOS_ADF_OID_LEN;

    BitBuffer* first = bit_buffer_alloc(TX_CAPACITY);
    BitBuffer* second = bit_buffer_alloc(TX_CAPACITY);
    munit_assert_true(
        seos_emulator_select_adf(oid_list, oid_list_len, &params, &credential, first));
    munit_assert_true(
        seos_emulator_select_adf(oid_list, oid_list_len, &params, &credential, second));

    munit_assert_size(bit_buffer_get_size_bytes(first), ==, bit_buffer_get_size_bytes(second));
    munit_assert_memory_not_equal(
        bit_buffer_get_size_bytes(first), bit_buffer_get_data(first), bit_buffer_get_data(second));

    bit_buffer_free(first);
    bit_buffer_free(second);
    return MUNIT_OK;
}

/* The authenticate command is told apart by its header and body, never by the
 * keyset number, which is a parameter a reader chooses. */
static MunitResult test_authenticate_matchers(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t built[SEOS_GENERAL_AUTHENTICATE_1_LEN];

    for(uint8_t key_no = 0; key_no < 4; key_no++) {
        seos_build_general_authenticate_1(key_no, built);

        uint8_t expected[] = {0x00, 0x87, 0x00, key_no, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};
        munit_assert_memory_equal(sizeof(expected), built, expected);

        munit_assert_true(seos_is_general_authenticate_1(built, sizeof(built)));
        munit_assert_false(seos_is_general_authenticate_2(built, sizeof(built)));
    }

    /* The second step shares the header but carries a cryptogram instead. */
    uint8_t step_two[] = {0x00, 0x87, 0x00, 0x02, 0x2c, 0x7c, 0x2a, 0x82, 0x28};
    munit_assert_true(seos_is_general_authenticate_2(step_two, sizeof(step_two)));
    munit_assert_false(seos_is_general_authenticate_1(step_two, sizeof(step_two)));

    /* Neither matches another command, or a frame too short to tell. */
    uint8_t other[] = {0x00, 0xa4, 0x04, 0x00, 0x04, 0x7c, 0x02, 0x81, 0x00, 0x00};
    munit_assert_false(seos_is_general_authenticate_1(other, sizeof(other)));
    munit_assert_false(seos_is_general_authenticate_2(other, sizeof(other)));
    munit_assert_false(seos_is_general_authenticate_1(step_two, 3));
    munit_assert_false(seos_is_general_authenticate_2(step_two, 3));
    return MUNIT_OK;
}

static MunitTest test_protocol_cases[] = {
    {(char*)"/shill/select", test_shill_select_looks_real, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/shill/authenticate",
     test_shill_authenticate_looks_real,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/select-adf/varies",
     test_select_response_varies,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/authenticate/matchers",
     test_authenticate_matchers,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/select-aid", test_select_aid_response, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/authenticate-1/challenge",
     test_authenticate_1_carries_challenge,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/mutual/des", test_mutual_authentication_des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/mutual/aes", test_mutual_authentication_aes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/keyslot/write-differs",
     test_write_keyslot_differs,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/authenticate-2/short-frame",
     test_rejects_short_authenticate,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/authenticate-2/bad-cryptogram",
     test_rejects_bad_cryptogram,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_protocol_suite = {
    (char*)"/protocol",
    test_protocol_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
