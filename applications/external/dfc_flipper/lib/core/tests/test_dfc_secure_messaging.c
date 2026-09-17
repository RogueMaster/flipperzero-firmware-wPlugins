#include <stdio.h>
#include "../tests/munit/munit.h"
#include "dfc_secure_messaging.h"
#include <string.h>

static void hex_to_bytes(const char* hex, uint8_t* out) {
    size_t len = strlen(hex) / 2;
    for(size_t i = 0; i < len; i++) {
        unsigned int byte;
        sscanf(hex + i * 2, "%2x", &byte);
        out[i] = (uint8_t)byte;
    }
}

static DfcSecureMessaging* alloc_session(uint8_t cipher, size_t key_len) {
    uint8_t key[DFC_MAX_KEY_LEN];
    for(size_t i = 0; i < key_len; i++)
        key[i] = (uint8_t)(0x10 + i);
    return dfc_secure_messaging_alloc(cipher, key, key_len, NULL);
}

static uint32_t test_crc32_dfc(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0 - (crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return crc;
}

// Reader (PCD) side: wrap() an outgoing command, then verify_command() (the emulator/PICC
// side function) should accept it and recover the original plaintext - these are meant to
// be exact inverses of each other.
static MunitResult
    round_trip_wrap_verify_command(uint8_t cipher, uint8_t comm_mode, size_t key_len) {
    DfcSecureMessaging* sm_reader = alloc_session(cipher, key_len);
    sm_reader->pcd = true;
    DfcSecureMessaging* sm_emulator = alloc_session(cipher, key_len);

    // Cmd || FileNo || Offset(3) || Length(3), then the payload it describes.
    uint8_t header[] = {DFC_CMD_WRITE_DATA, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
    uint8_t plain[] = {0xde, 0xad, 0xbe, 0xef};

    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_wrap(
        sm_reader, comm_mode, header, sizeof(header), plain, sizeof(plain), wrapped);
    munit_assert_size(wrapped_len, >=, sizeof(plain));

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_verify_command(
        sm_emulator, comm_mode, header, sizeof(header), wrapped, wrapped_len, recovered);

    munit_assert_size(recovered_len, ==, sizeof(plain));
    munit_assert_memory_equal(sizeof(plain), recovered, plain);

    dfc_secure_messaging_free(sm_reader);
    dfc_secure_messaging_free(sm_emulator);
    return MUNIT_OK;
}

// Emulator (PICC) side: generate_response() a reply, then unwrap() (the reader side
// function) should accept it and recover the original plaintext.
static MunitResult
    round_trip_generate_response_unwrap(uint8_t cipher, uint8_t comm_mode, size_t key_len) {
    DfcSecureMessaging* sm_emulator = alloc_session(cipher, key_len);
    DfcSecureMessaging* sm_reader = alloc_session(cipher, key_len);
    sm_reader->pcd = true;

    uint8_t plain[] = {0x30, 0x0b, 0x81, 0x01, 0x01};
    uint8_t status = DFC_STATUS_OK;

    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_response(
        sm_emulator, comm_mode, status, plain, sizeof(plain), wrapped);
    munit_assert_size(wrapped_len, >=, sizeof(plain));

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len =
        dfc_secure_messaging_unwrap(sm_reader, comm_mode, status, wrapped, wrapped_len, recovered);

    munit_assert_size(recovered_len, ==, sizeof(plain));
    munit_assert_memory_equal(sizeof(plain), recovered, plain);

    dfc_secure_messaging_free(sm_emulator);
    dfc_secure_messaging_free(sm_reader);
    return MUNIT_OK;
}

static MunitResult test_plain_legacy(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_PLAIN, 8);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_PLAIN, 8);
    return MUNIT_OK;
}

static MunitResult test_mac_legacy_single_des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_MAC, 8);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_MAC, 8);
    return MUNIT_OK;
}

static MunitResult test_mac_legacy_2k3des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_MAC, 16);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_MAC, 16);
    return MUNIT_OK;
}

static MunitResult test_mac_iso_2k3des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_MAC, 16);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_MAC, 16);
    return MUNIT_OK;
}

static MunitResult test_mac_iso_3k3des(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_MAC, 24);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_MAC, 24);
    return MUNIT_OK;
}

static MunitResult test_mac_aes(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_AES, DFC_COMM_MAC, 16);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_AES, DFC_COMM_MAC, 16);
    return MUNIT_OK;
}

static MunitResult test_enciphered_legacy(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_ENCIPHERED, 8);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_LEGACY, DFC_COMM_ENCIPHERED, 8);
    return MUNIT_OK;
}

static MunitResult test_enciphered_iso(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_ENCIPHERED, 16);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_ISO, DFC_COMM_ENCIPHERED, 16);
    return MUNIT_OK;
}

static MunitResult test_enciphered_aes(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    round_trip_wrap_verify_command(DFC_CMD_AUTHENTICATE_AES, DFC_COMM_ENCIPHERED, 16);
    round_trip_generate_response_unwrap(DFC_CMD_AUTHENTICATE_AES, DFC_COMM_ENCIPHERED, 16);
    return MUNIT_OK;
}

// A corrupted MAC/ciphertext must be rejected (return 0), not silently accepted.
static MunitResult test_mac_tamper_rejected(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    DfcSecureMessaging* sm_reader = alloc_session(DFC_CMD_AUTHENTICATE_AES, 16);
    sm_reader->pcd = true;
    DfcSecureMessaging* sm_emulator = alloc_session(DFC_CMD_AUTHENTICATE_AES, 16);

    uint8_t header[] = {DFC_CMD_WRITE_DATA, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
    uint8_t plain[] = {0xde, 0xad, 0xbe, 0xef};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_wrap(
        sm_reader, DFC_COMM_MAC, header, sizeof(header), plain, sizeof(plain), wrapped);

    wrapped[wrapped_len - 1] ^= 0xFF; // corrupt the last MAC byte

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_verify_command(
        sm_emulator, DFC_COMM_MAC, header, sizeof(header), wrapped, wrapped_len, recovered);

    munit_assert_size(recovered_len, ==, 0);

    dfc_secure_messaging_free(sm_reader);
    dfc_secure_messaging_free(sm_emulator);
    return MUNIT_OK;
}

static MunitResult
    test_ev1_get_file_settings_response_is_mac_protected(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    DfcSecureMessaging* sm = alloc_session(DFC_CMD_AUTHENTICATE_ISO, 16);

    munit_assert_false(dfc_secure_messaging_applies_ev1(sm, DFC_CMD_SELECT_APPLICATION));
    munit_assert_false(dfc_secure_messaging_applies_ev1(sm, DFC_CMD_AUTHENTICATE_ISO));
    munit_assert_true(dfc_secure_messaging_applies_ev1(sm, DFC_CMD_GET_FILE_SETTINGS));

    uint8_t file_no[] = {0x0f};
    dfc_secure_messaging_update_ev1_command(
        sm, DFC_CMD_GET_FILE_SETTINGS, file_no, sizeof(file_no));

    uint8_t settings[] = {0x00, 0x01, 0x12, 0x30, 0x35, 0x00, 0x00};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_ev1_response(
        sm, DFC_STATUS_OK, settings, sizeof(settings), wrapped);

    munit_assert_size(wrapped_len, ==, sizeof(settings) + 8);
    munit_assert_memory_equal(sizeof(settings), wrapped, settings);

    dfc_secure_messaging_free(sm);
    return MUNIT_OK;
}

static MunitResult
    test_ev1_response_unwrap_requires_command_state(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[16];
    for(size_t i = 0; i < sizeof(key); i++)
        key[i] = (uint8_t)(0x20 + i);

    DfcSecureMessaging* sm_reader =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_ISO, key, sizeof(key), NULL);
    DfcSecureMessaging* sm_emulator =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_ISO, key, sizeof(key), NULL);
    DfcSecureMessaging* sm_stale_reader =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_ISO, key, sizeof(key), NULL);

    uint8_t read_params[] = {0x0f, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00};
    dfc_secure_messaging_update_ev1_command(
        sm_reader, DFC_CMD_READ_DATA, read_params, sizeof(read_params));
    dfc_secure_messaging_update_ev1_command(
        sm_emulator, DFC_CMD_READ_DATA, read_params, sizeof(read_params));

    uint8_t plain[] = {0x30, 0x2f, 0x81, 0x02, 0x0c};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_ev1_response(
        sm_emulator, DFC_STATUS_OK, plain, sizeof(plain), wrapped);

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_unwrap_ev1_response(
        sm_reader, DFC_STATUS_OK, wrapped, wrapped_len, recovered);
    munit_assert_size(recovered_len, ==, sizeof(plain));
    munit_assert_memory_equal(sizeof(plain), recovered, plain);

    recovered_len = dfc_secure_messaging_unwrap_ev1_response(
        sm_stale_reader, DFC_STATUS_OK, wrapped, wrapped_len, recovered);
    munit_assert_size(recovered_len, ==, SIZE_MAX);

    dfc_secure_messaging_free(sm_reader);
    dfc_secure_messaging_free(sm_emulator);
    dfc_secure_messaging_free(sm_stale_reader);
    return MUNIT_OK;
}

static MunitResult test_ev1_initial_iv_is_used(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[16];
    uint8_t initial_iv[16];
    for(size_t i = 0; i < sizeof(key); i++) {
        key[i] = (uint8_t)(0x40 + i);
        initial_iv[i] = (uint8_t)(0xa0 + i);
    }

    DfcSecureMessaging* sm =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_AES, key, sizeof(key), initial_iv);
    munit_assert_memory_equal(sizeof(initial_iv), sm->iv, initial_iv);

    dfc_secure_messaging_free(sm);
    return MUNIT_OK;
}

static MunitResult
    test_ev1_get_file_settings_matches_reference_vector(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;

    uint8_t session_key[16];
    hex_to_bytes("5675F868C28F4D5DAB636D2076B3C764", session_key);
    DfcSecureMessaging* sm = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_ISO, session_key, sizeof(session_key), NULL);

    uint8_t file_no[] = {0x0f};
    dfc_secure_messaging_update_ev1_command(
        sm, DFC_CMD_GET_FILE_SETTINGS, file_no, sizeof(file_no));

    uint8_t settings[] = {0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_ev1_response(
        sm, DFC_STATUS_OK, settings, sizeof(settings), wrapped);

    uint8_t expected[] = {
        0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00, 0xcd, 0x69, 0x3f, 0x57, 0x7b, 0x9a, 0x04, 0xf4};

    munit_assert_size(wrapped_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), wrapped, expected);

    dfc_secure_messaging_free(sm);
    return MUNIT_OK;
}

static MunitResult
    test_ev1_get_file_settings_matches_second_reference_vector(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;

    uint8_t session_key[16];
    hex_to_bytes("7DCA9F5BD5FEEBE1D837A927ADAD0227", session_key);
    DfcSecureMessaging* sm = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_ISO, session_key, sizeof(session_key), NULL);

    uint8_t file_no[] = {0x0f};
    dfc_secure_messaging_update_ev1_command(
        sm, DFC_CMD_GET_FILE_SETTINGS, file_no, sizeof(file_no));

    uint8_t settings[] = {0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_ev1_response(
        sm, DFC_STATUS_OK, settings, sizeof(settings), wrapped);

    uint8_t expected[] = {
        0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00, 0x27, 0xde, 0x15, 0x26, 0xf2, 0xbf, 0x84, 0x0f};

    munit_assert_size(wrapped_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), wrapped, expected);

    dfc_secure_messaging_free(sm);
    return MUNIT_OK;
}

static MunitResult test_ev1_transmitted_command_mac_round_trip(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[16];
    for(size_t i = 0; i < sizeof(key); i++)
        key[i] = (uint8_t)(0x50 + i);

    DfcSecureMessaging* writer =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_AES, key, sizeof(key), NULL);
    DfcSecureMessaging* reader =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_AES, key, sizeof(key), NULL);

    uint8_t body[] = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xDE, 0xAD};
    dfc_secure_messaging_update_ev1_command(writer, DFC_CMD_WRITE_DATA, body, sizeof(body));
    uint8_t mact[8];
    memcpy(mact, writer->iv, 8);

    uint8_t framed[sizeof(body) + 8];
    memcpy(framed, body, sizeof(body));
    memcpy(framed + sizeof(body), mact, 8);

    size_t clear_len = dfc_secure_messaging_verify_ev1_transmitted_command_mac(
        reader, DFC_CMD_WRITE_DATA, framed, sizeof(framed));
    munit_assert_size(clear_len, ==, sizeof(body));
    munit_assert_memory_equal(16, reader->iv, writer->iv);

    framed[0] ^= 0x01;
    DfcSecureMessaging* reader2 =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_AES, key, sizeof(key), NULL);
    munit_assert_size(
        dfc_secure_messaging_verify_ev1_transmitted_command_mac(
            reader2, DFC_CMD_WRITE_DATA, framed, sizeof(framed)),
        ==,
        SIZE_MAX);

    dfc_secure_messaging_free(writer);
    dfc_secure_messaging_free(reader);
    dfc_secure_messaging_free(reader2);
    return MUNIT_OK;
}

static MunitResult
    test_ev1_enciphered_response_crc_covers_plain_and_status(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;

    uint8_t key[16];
    for(size_t i = 0; i < sizeof(key); i++)
        key[i] = (uint8_t)(0x10 + i);

    DfcSecureMessaging* sm =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_AES, key, sizeof(key), NULL);

    uint8_t plain[] = {0x30, 0x2F, 0x81, 0x02, 0x0C};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_generate_response(
        sm, DFC_COMM_ENCIPHERED, DFC_STATUS_OK, plain, sizeof(plain), wrapped);

    munit_assert_size(wrapped_len, ==, 16);

    uint8_t iv[16] = {0};
    uint8_t clear[16];
    dfc_worker_aes_cbc_decrypt(key, sizeof(key), iv, wrapped_len, wrapped, clear);

    uint8_t crc_input[sizeof(plain) + 1];
    memcpy(crc_input, plain, sizeof(plain));
    crc_input[sizeof(plain)] = DFC_STATUS_OK;
    uint32_t expected_crc = test_crc32_dfc(crc_input, sizeof(crc_input));

    munit_assert_memory_equal(sizeof(plain), clear, plain);
    munit_assert_uint8(clear[sizeof(plain)], ==, (uint8_t)(expected_crc & 0xFF));
    munit_assert_uint8(clear[sizeof(plain) + 1], ==, (uint8_t)((expected_crc >> 8) & 0xFF));
    munit_assert_uint8(clear[sizeof(plain) + 2], ==, (uint8_t)((expected_crc >> 16) & 0xFF));
    munit_assert_uint8(clear[sizeof(plain) + 3], ==, (uint8_t)((expected_crc >> 24) & 0xFF));

    dfc_secure_messaging_free(sm);
    return MUNIT_OK;
}

// An EV1 session covers the command header, so redirecting a write to another
// file, offset or length must not verify. Without header coverage the payload
// alone authenticates and the header is free to rewrite in flight.
static MunitResult header_tamper_rejected(uint8_t comm_mode) {
    DfcSecureMessaging* sm_reader = alloc_session(DFC_CMD_AUTHENTICATE_AES, 16);
    sm_reader->pcd = true;
    DfcSecureMessaging* sm_emulator = alloc_session(DFC_CMD_AUTHENTICATE_AES, 16);

    uint8_t header[] = {DFC_CMD_WRITE_DATA, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
    uint8_t plain[] = {0xde, 0xad, 0xbe, 0xef};
    uint8_t wrapped[DFC_SM_MAX_SIZE];
    size_t wrapped_len = dfc_secure_messaging_wrap(
        sm_reader, comm_mode, header, sizeof(header), plain, sizeof(plain), wrapped);

    uint8_t tampered[sizeof(header)];
    memcpy(tampered, header, sizeof(header));
    tampered[1] = 0x02; // same payload, different file

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_verify_command(
        sm_emulator, comm_mode, tampered, sizeof(tampered), wrapped, wrapped_len, recovered);

    munit_assert_size(recovered_len, ==, 0);

    dfc_secure_messaging_free(sm_reader);
    dfc_secure_messaging_free(sm_emulator);
    return MUNIT_OK;
}

static MunitResult test_header_tamper_rejected_mac(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    return header_tamper_rejected(DFC_COMM_MAC);
}

static MunitResult test_header_tamper_rejected_enciphered(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    return header_tamper_rejected(DFC_COMM_ENCIPHERED);
}

static MunitTest tests[] = {
    {"/tamper/header_mac",
     test_header_tamper_rejected_mac,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/tamper/header_enciphered",
     test_header_tamper_rejected_enciphered,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/plain/legacy", test_plain_legacy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/legacy_single_des", test_mac_legacy_single_des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/legacy_2k3des", test_mac_legacy_2k3des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/iso_2k3des", test_mac_iso_2k3des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/iso_3k3des", test_mac_iso_3k3des, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/aes", test_mac_aes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/enciphered/legacy", test_enciphered_legacy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/enciphered/iso", test_enciphered_iso, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/enciphered/aes", test_enciphered_aes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/mac/tamper_rejected", test_mac_tamper_rejected, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/ev1/get_file_settings_response",
     test_ev1_get_file_settings_response_is_mac_protected,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1/response_unwrap_requires_command_state",
     test_ev1_response_unwrap_requires_command_state,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1/initial_iv", test_ev1_initial_iv_is_used, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/ev1/get_file_settings_reference_vector",
     test_ev1_get_file_settings_matches_reference_vector,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1/get_file_settings_second_reference_vector",
     test_ev1_get_file_settings_matches_second_reference_vector,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1/enciphered_response_crc_plain_and_status",
     test_ev1_enciphered_response_crc_covers_plain_and_status,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1/transmitted_command_mac_round_trip",
     test_ev1_transmitted_command_mac_round_trip,
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
