#include <stdio.h>
#include "../tests/munit/munit.h"
#include "dfc_common.h"
#include <string.h>

static void hex_to_bytes(const char* hex, uint8_t* out) {
    size_t len = strlen(hex) / 2;
    for(size_t i = 0; i < len; i++) {
        unsigned int byte;
        sscanf(hex + i * 2, "%2x", &byte);
        out[i] = (uint8_t)byte;
    }
}

static MunitResult test_rotate_left(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t buf[8] = {0x5e, 0xc0, 0x44, 0xf1, 0x84, 0xfa, 0x03, 0xae};
    uint8_t expected[8] = {0xc0, 0x44, 0xf1, 0x84, 0xfa, 0x03, 0xae, 0x5e};
    dfc_rotate_left(buf, sizeof(buf));
    munit_assert_memory_equal(sizeof(buf), buf, expected);
    return MUNIT_OK;
}

static MunitResult test_detectable_uid_accepts_supported_lengths_starting_with_04(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    uint8_t valid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t short_uid[] = {0x04, 0x11, 0x22, 0x33};
    uint8_t wrong_prefix[] = {0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t long_uid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

    munit_assert_true(dfc_desfire_uid_is_detectable(valid, sizeof(valid)));
    munit_assert_false(dfc_desfire_uid_is_detectable(short_uid, sizeof(short_uid)));
    munit_assert_false(dfc_desfire_uid_is_detectable(wrong_prefix, sizeof(wrong_prefix)));
    munit_assert_true(dfc_desfire_uid_is_detectable(long_uid, sizeof(long_uid)));

    return MUNIT_OK;
}

static MunitResult
    test_standard_data_file_settings_are_desfire_layout(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    // Access rights are held logically — read key 1, write key 2, read&write key 3,
    // change key 0 — and travel least-significant octet first, so 0x1230 encodes
    // as 30 12.
    uint8_t settings[7];
    uint8_t expected[] = {0x00, 0x01, 0x30, 0x12, 0x35, 0x00, 0x00};
    dfc_encode_standard_data_file_settings(0x00, DFC_COMM_MAC, 0x1230, 0x35, settings);

    munit_assert_memory_equal(sizeof(expected), settings, expected);
    return MUNIT_OK;
}

static MunitResult test_authentication_mode_names_accept_d40_and_legacy_aliases(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    uint8_t auth_command = 0;
    munit_assert_string_equal(dfc_authentication_mode_to_string(DFC_CMD_AUTHENTICATE_LEGACY), "D40");
    munit_assert_true(dfc_authentication_mode_from_string("D40", &auth_command));
    munit_assert_uint8(auth_command, ==, DFC_CMD_AUTHENTICATE_LEGACY);
    munit_assert_true(dfc_authentication_mode_from_string("Legacy", &auth_command));
    munit_assert_uint8(auth_command, ==, DFC_CMD_AUTHENTICATE_LEGACY);
    munit_assert_true(dfc_authentication_mode_from_string("NativeD40", &auth_command));
    munit_assert_uint8(auth_command, ==, DFC_CMD_AUTHENTICATE_LEGACY);
    munit_assert_true(dfc_authentication_mode_from_string("Native-D40", &auth_command));
    munit_assert_uint8(auth_command, ==, DFC_CMD_AUTHENTICATE_LEGACY);

    return MUNIT_OK;
}

static MunitResult
    test_authentication_mode_names_reject_invalid_input(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    uint8_t auth_command = DFC_CMD_AUTHENTICATE_ISO;
    munit_assert_false(dfc_authentication_mode_from_string("invalid", &auth_command));
    munit_assert_uint8(auth_command, ==, DFC_CMD_AUTHENTICATE_ISO);

    return MUNIT_OK;
}

// Protocol regression vector for D40 DES authentication/session derivation.
static MunitResult test_d40_des_reference_vector(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    uint8_t key[8] = {0};
    uint8_t enc_rndb[8], expected_rndb[8], rnda[8], rndb_rot[8], expected_plain[16];
    uint8_t enc_rnda_rot[8], expected_rnda_rot[8], expected_session_key[8];
    uint8_t iv[16];

    hex_to_bytes("2c1b3c101d9fb44f", enc_rndb);
    hex_to_bytes("5ec044f184fa03ae", expected_rndb);
    hex_to_bytes("11ddb59ed8b00fd7", rnda);
    hex_to_bytes("11ddb59ed8b00fd7c044f184fa03ae5e", expected_plain);
    hex_to_bytes("deb3e8fa0da0646a", enc_rnda_rot);
    hex_to_bytes("ddb59ed8b00fd711", expected_rnda_rot);
    hex_to_bytes("11ddb59e5ec044f1", expected_session_key);

    // Decrypt card's RndB
    uint8_t rndb[8];
    memset(iv, 0, sizeof(iv));
    dfc_worker_des_cbc_decrypt(key, sizeof(key), iv, 8, enc_rndb, rndb);
    munit_assert_memory_equal(8, rndb, expected_rndb);

    // Rotate RndB left by one byte
    memcpy(rndb_rot, rndb, 8);
    dfc_rotate_left(rndb_rot, 8);

    // Build RndA||RndB' and encrypt it (RndA is fixed here to match the captured vector)
    uint8_t plain[16];
    memcpy(plain, rnda, 8);
    memcpy(plain + 8, rndb_rot, 8);
    munit_assert_memory_equal(16, plain, expected_plain);

    uint8_t encrypted[16];
    memset(iv, 0, sizeof(iv));
    dfc_worker_des_cbc_encrypt(key, sizeof(key), iv, 16, plain, encrypted);
    uint8_t expected_encrypted[16];
    hex_to_bytes("12f2f42157176d12b707c4e5b0b3f33c", expected_encrypted);
    munit_assert_memory_equal(16, encrypted, expected_encrypted);

    // Decrypt the card's step-2 response and confirm it's RndA rotated left by one byte
    uint8_t rnda_rot_recv[8];
    memset(iv, 0, sizeof(iv));
    dfc_worker_des_cbc_decrypt(key, sizeof(key), iv, 8, enc_rnda_rot, rnda_rot_recv);
    munit_assert_memory_equal(8, rnda_rot_recv, expected_rnda_rot);

    uint8_t expected_rnda_rot_computed[8];
    memcpy(expected_rnda_rot_computed, rnda, 8);
    dfc_rotate_left(expected_rnda_rot_computed, 8);
    munit_assert_memory_equal(8, rnda_rot_recv, expected_rnda_rot_computed);

    // Session key formula for single-DES (8-byte key) sessions: RndA[0:4] || RndB[0:4]
    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_LEGACY, key, 8, rnda, rndb, session_key, &session_key_len);
    munit_assert_size(session_key_len, ==, 8);
    munit_assert_memory_equal(8, session_key, expected_session_key);

    return MUNIT_OK;
}

static MunitResult test_derive_session_key_aes(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    // AES session key = RndA[0:4] || RndB[0:4] || RndA[12:16] || RndB[12:16]
    uint8_t rnda[16], rndb[16], expected[16];
    hex_to_bytes("000102030405060708090a0b0c0d0e0f", rnda);
    hex_to_bytes("101112131415161718191a1b1c1d1e1f", rndb);
    // RndA[0:4]=00010203, RndB[0:4]=10111213, RndA[12:16]=0c0d0e0f, RndB[12:16]=1c1d1e1f
    hex_to_bytes("00010203101112130c0d0e0f1c1d1e1f", expected);

    uint8_t aes_key[16];
    hex_to_bytes("00112233445566778899aabbccddeeff", aes_key);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_AES, aes_key, 16, rnda, rndb, session_key, &session_key_len);
    munit_assert_size(session_key_len, ==, 16);
    munit_assert_memory_equal(16, session_key, expected);
    return MUNIT_OK;
}

static MunitResult test_derive_session_key_2k3des(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    // 2K3DES session key = RndA[0:4] || RndB[0:4] || RndA[4:8] || RndB[4:8].
    // The halves of the key must differ: a 16-octet key that is the same 8 octets
    // twice is single DES and yields the 8-octet session key instead.
    uint8_t rnda[8], rndb[8], expected[16], key_2k3des[16];
    hex_to_bytes("0001020304050607", rnda);
    hex_to_bytes("1011121314151617", rndb);
    hex_to_bytes("00010203101112130405060714151617", expected);
    hex_to_bytes("0f0e0d0c0b0a09080706050403020100", key_2k3des);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_LEGACY, key_2k3des, 16, rnda, rndb, session_key, &session_key_len);
    munit_assert_size(session_key_len, ==, 16);
    munit_assert_memory_equal(16, session_key, expected);
    return MUNIT_OK;
}

static MunitResult
    test_iso_auth_derives_expected_session_key(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    uint8_t key[16], enc_rndb[8], reader_continuation[16], card_continuation[8];
    uint8_t expected_rndb[8], expected_clear[16], expected_session_key[16];
    hex_to_bytes("C6EF847CAE0A2A56C0192AA1C4E8D9FB", key);
    hex_to_bytes("9E1CE3C4AAD37362", enc_rndb);
    hex_to_bytes("11DF077736DD5EEA70CC7049693D235A", reader_continuation);
    hex_to_bytes("7E652549CA0DD081", card_continuation);
    hex_to_bytes("C28F4D5D76B3C764", expected_rndb);
    hex_to_bytes("5675F868AB636D208F4D5D76B3C764C2", expected_clear);
    hex_to_bytes("5675F868C28F4D5DAB636D2076B3C764", expected_session_key);

    uint8_t iv[16] = {0};
    uint8_t rndb[8];
    dfc_worker_des_cbc_decrypt(key, sizeof(key), iv, sizeof(enc_rndb), enc_rndb, rndb);
    munit_assert_memory_equal(sizeof(expected_rndb), rndb, expected_rndb);

    memcpy(iv, enc_rndb, sizeof(enc_rndb));
    uint8_t clear[16];
    dfc_worker_des_cbc_decrypt(
        key, sizeof(key), iv, sizeof(reader_continuation), reader_continuation, clear);
    munit_assert_memory_equal(sizeof(expected_clear), clear, expected_clear);

    uint8_t rnd_a_rot[8];
    memset(iv, 0, sizeof(iv));
    memcpy(iv, reader_continuation + 8, 8);
    dfc_worker_des_cbc_decrypt(
        key, sizeof(key), iv, sizeof(card_continuation), card_continuation, rnd_a_rot);

    uint8_t expected_rnd_a_rot[8];
    memcpy(expected_rnd_a_rot, clear, 8);
    dfc_rotate_left(expected_rnd_a_rot, sizeof(expected_rnd_a_rot));
    munit_assert_memory_equal(sizeof(expected_rnd_a_rot), rnd_a_rot, expected_rnd_a_rot);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_ISO, key, sizeof(key), clear, rndb, session_key, &session_key_len);
    munit_assert_size(session_key_len, ==, sizeof(expected_session_key));
    munit_assert_memory_equal(sizeof(expected_session_key), session_key, expected_session_key);

    return MUNIT_OK;
}

static MunitResult test_derive_session_key_3k3des(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    // 3K3DES session key = RndA[0:4]||RndB[0:4]||RndA[6:10]||RndB[6:10]||RndA[12:16]||RndB[12:16]
    uint8_t rnda[16], rndb[16], expected[24];
    hex_to_bytes("000102030405060708090a0b0c0d0e0f", rnda);
    hex_to_bytes("101112131415161718191a1b1c1d1e1f", rndb);
    // RndA[0:4]=00010203 RndB[0:4]=10111213 RndA[6:10]=06070809 RndB[6:10]=16171819
    // RndA[12:16]=0c0d0e0f RndB[12:16]=1c1d1e1f
    hex_to_bytes("000102031011121306070809161718190c0d0e0f1c1d1e1f", expected);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    uint8_t key_3k3des[24];
    hex_to_bytes("000102030405060708090a0b0c0d0e0f1011121314151617", key_3k3des);
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_ISO, key_3k3des, 24, rnda, rndb, session_key, &session_key_len);
    munit_assert_size(session_key_len, ==, 24);
    munit_assert_memory_equal(24, session_key, expected);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/rotate_left", test_rotate_left, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/detectable_uid",
        test_detectable_uid_accepts_supported_lengths_starting_with_04,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/standard_data_file_settings",
     test_standard_data_file_settings_are_desfire_layout,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/authentication_mode/d40_aliases",
     test_authentication_mode_names_accept_d40_and_legacy_aliases,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/authentication_mode/rejects_invalid",
     test_authentication_mode_names_reject_invalid_input,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/d40_des/reference_vector",
     test_d40_des_reference_vector,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/session_key/aes", test_derive_session_key_aes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/session_key/2k3des",
     test_derive_session_key_2k3des,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/session_key/iso_auth",
     test_iso_auth_derives_expected_session_key,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/session_key/3k3des",
     test_derive_session_key_3k3des,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
