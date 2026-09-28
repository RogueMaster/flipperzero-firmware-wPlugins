#include "munit/munit.h"
#include "dfc_der.h"
#include "dfc_emulator.h"
#include "dfc_port_host.h"
#include "dfc_text.h"
#include "dfc_virtual_picc.h"

#include <stdio.h>
#include <string.h>

#if DFC_ENABLE_EV2_SECURE_MESSAGING
static size_t
    command(DfcEmulator* emulator, const uint8_t* input, size_t input_len, uint8_t output[64]) {
    DfcByteBuf* tx = dfc_bytebuf_alloc(64);
    munit_assert_not_null(tx);
    munit_assert_true(dfc_emulator_handle_command(emulator, input, input_len, tx, NULL));
    size_t len = dfc_bytebuf_get_size_bytes(tx);
    munit_assert_size(len, <=, 64);
    memcpy(output, dfc_bytebuf_get_data(tx), len);
    dfc_bytebuf_free(tx);
    return len;
}

static void credential_init(DfcCredential* credential) {
    munit_assert_true(dfc_credential_clear(credential));
    credential->card.generation = DfcGenerationEv3;
    credential->card.storage = 4096;
    credential->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(credential->uid, "\x04\x11\x22\x33\x44\x55\x66", DFC_DESFIRE_UID_LEN);
    credential->picc_key_settings_2 = DFC_KEY_TYPE_AES | 1;
    credential->picc_key_settings_1 = 0x0F;
    credential->picc_auth_command = DFC_CMD_AUTHENTICATE_AES;
    munit_assert_true(dfc_credential_keys_resize(credential, NULL, 1, DFC_AES_KEY_LENGTH));
}

static uint8_t hex_nibble(char c) {
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    munit_error("invalid hexadecimal test vector");
    return 0;
}

static size_t decode_hex(const char* hex, uint8_t* bytes, size_t capacity) {
    size_t digits = strlen(hex);
    munit_assert_size(digits % 2, ==, 0);
    munit_assert_size(digits / 2, <=, capacity);
    for(size_t i = 0; i < digits / 2; i++) {
        bytes[i] = (uint8_t)((hex_nibble(hex[2 * i]) << 4) | hex_nibble(hex[2 * i + 1]));
    }
    return digits / 2;
}

static void assert_apdu(DfcVirtualPiccSession* session, const char* request, const char* expected) {
    uint8_t input[64], wanted[64], actual[64];
    size_t input_len = decode_hex(request, input, sizeof(input));
    size_t wanted_len = decode_hex(expected, wanted, sizeof(wanted));
    size_t actual_len = 0;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, input, input_len, actual, sizeof(actual), &actual_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(actual_len, ==, wanted_len);
    munit_assert_memory_equal(wanted_len, actual, wanted);
}

static void authenticate_with_key(
    DfcEmulator* emulator,
    const uint8_t* reader_capabilities,
    size_t reader_len,
    const uint8_t expected_card[DFC_EV2_CAPABILITY_LENGTH],
    const uint8_t key[DFC_AES_KEY_LENGTH]) {
    uint8_t request[3 + 8] = {DFC_CMD_AUTHENTICATE_EV2_FIRST, 0, (uint8_t)reader_len};
    if(reader_len) memcpy(request + 3, reader_capabilities, reader_len);
    uint8_t response[64];
    size_t len = command(emulator, request, 3 + reader_len, response);
    munit_assert_size(len, ==, 1 + DFC_EV2_RANDOM_LENGTH);
    munit_assert_uint8(response[0], ==, DFC_CMD_ADDITIONAL_FRAME);

    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t random_b[DFC_EV2_RANDOM_LENGTH];
    dfc_worker_aes_cbc_decrypt(
        key, DFC_AES_KEY_LENGTH, iv, sizeof(random_b), response + 1, random_b);
    uint8_t clear[DFC_EV2_RANDOM_LENGTH * 2];
    for(size_t i = 0; i < DFC_EV2_RANDOM_LENGTH; i++)
        clear[i] = (uint8_t)(0x10 + i);
    for(size_t i = 0; i < DFC_EV2_RANDOM_LENGTH; i++) {
        clear[DFC_EV2_RANDOM_LENGTH + i] = random_b[(i + 1) % DFC_EV2_RANDOM_LENGTH];
    }
    uint8_t continuation[1 + sizeof(clear)] = {DFC_CMD_ADDITIONAL_FRAME};
    memset(iv, 0, sizeof(iv));
    dfc_worker_aes_cbc_encrypt(
        key, DFC_AES_KEY_LENGTH, iv, sizeof(clear), clear, continuation + 1);
    len = command(emulator, continuation, sizeof(continuation), response);
    munit_assert_size(len, ==, 1 + DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH);
    munit_assert_uint8(response[0], ==, DFC_STATUS_OK);

    uint8_t decoded[DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH];
    memset(iv, 0, sizeof(iv));
    dfc_worker_aes_cbc_decrypt(
        key, DFC_AES_KEY_LENGTH, iv, sizeof(decoded), response + 1, decoded);
    for(size_t i = 0; i < DFC_EV2_RANDOM_LENGTH; i++) {
        munit_assert_uint8(
            decoded[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH + i],
            ==,
            clear[(i + 1) % DFC_EV2_RANDOM_LENGTH]);
    }
    munit_assert_memory_equal(DFC_EV2_CAPABILITY_LENGTH, decoded + 20, expected_card);
    uint8_t expected_reader[DFC_EV2_CAPABILITY_LENGTH] = {0};
    size_t copied = reader_len < sizeof(expected_reader) ? reader_len : sizeof(expected_reader);
    if(copied) memcpy(expected_reader, reader_capabilities, copied);
    munit_assert_memory_equal(DFC_EV2_CAPABILITY_LENGTH, decoded + 26, expected_reader);
}

static void authenticate(
    DfcEmulator* emulator,
    const uint8_t* reader_capabilities,
    size_t reader_len,
    const uint8_t expected_card[DFC_EV2_CAPABILITY_LENGTH]) {
    const uint8_t zero_key[DFC_AES_KEY_LENGTH] = {0};
    authenticate_with_key(emulator, reader_capabilities, reader_len, expected_card, zero_key);
}

static MunitResult test_v6_maximal_capability_exchange(
    const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential from_text, from_binary;
    static char text[DFC_TEXT_MAX_SIZE];
    FILE* file = fopen("../examples/credentials/v6-maximal.dfc", "rb");
    munit_assert_not_null(file);
    size_t text_len = fread(text, 1, sizeof(text), file);
    munit_assert_int(ferror(file), ==, 0);
    munit_assert_int(fgetc(file), ==, EOF);
    munit_assert_int(fclose(file), ==, 0);
    DfcTextError detail = {0};
    munit_assert_int(dfc_text_parse(&from_text, text, text_len, &detail), ==, DfcTextOk);

    static uint8_t binary[DFC_DER_MAX_SIZE];
    size_t binary_len = 0;
    munit_assert_int(
        dfc_der_encode(&from_text, binary, sizeof(binary), &binary_len), ==, DfcDerOk);
    munit_assert_int(dfc_der_decode(&from_binary, binary, binary_len), ==, DfcDerOk);

    const uint8_t reader_caps[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
    const uint8_t picc_caps[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t app_caps[6] = {5, 6, 7, 8, 9, 10};
    uint8_t picc_key[DFC_AES_KEY_LENGTH];
    uint8_t app_key[DFC_AES_KEY_LENGTH];
    for(size_t i = 0; i < DFC_AES_KEY_LENGTH; i++) {
        picc_key[i] = (uint8_t)i;
        app_key[i] = (uint8_t)(0x70 + i);
    }

    DfcCredential* credentials[] = {&from_text, &from_binary};
    for(size_t i = 0; i < sizeof(credentials) / sizeof(credentials[0]); i++) {
        DfcEmulator* emulator = dfc_emulator_alloc(credentials[i]);
        munit_assert_not_null(emulator);
        authenticate_with_key(emulator, reader_caps, sizeof(reader_caps), picc_caps, picc_key);
        const uint8_t select_app[] = {DFC_CMD_SELECT_APPLICATION, 3, 2, 1};
        uint8_t response[64];
        size_t len = command(emulator, select_app, sizeof(select_app), response);
        munit_assert_size(len, ==, 1);
        munit_assert_uint8(response[0], ==, DFC_STATUS_OK);
        authenticate_with_key(emulator, reader_caps, sizeof(reader_caps), app_caps, app_key);
        dfc_emulator_free(emulator);
    }
    return MUNIT_OK;
}

static MunitResult test_card_and_reader_capabilities(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    credential_init(&credential);
    const uint8_t card_caps[6] = {0, 0, 0, 0, 0xA5, 0x5A};
    const uint8_t reader_caps[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t zeros[6] = {0};
    for(size_t n = 0; n <= sizeof(reader_caps); n++) {
        DfcEmulator* emulator = dfc_emulator_alloc(&credential);
        munit_assert_not_null(emulator);
        authenticate(emulator, reader_caps, n, zeros);
        dfc_emulator_free(emulator);
    }
    credential.picc_has_ev2_capabilities = true;
    memcpy(credential.picc_ev2_capabilities, card_caps, sizeof(card_caps));
    DfcEmulator* emulator = dfc_emulator_alloc(&credential);
    munit_assert_not_null(emulator);
    authenticate(emulator, reader_caps, 6, card_caps);
    dfc_emulator_free(emulator);

#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    const uint8_t aid[3] = {1, 2, 3};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        &credential, aid, 0x0F, DFC_KEY_TYPE_AES | 1);
    munit_assert_not_null(app);
    emulator = dfc_emulator_alloc(&credential);
    munit_assert_not_null(emulator);
    emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    emulator->selected_app_index = 0;
    authenticate(emulator, reader_caps, 2, zeros);
    app->has_capability_data = true;
    memcpy(app->capability_data + DFC_APPLICATION_CARD_CAPABILITY_OFFSET, card_caps, 6);
    authenticate(emulator, reader_caps, 2, card_caps);
    dfc_emulator_free(emulator);
#endif
    return MUNIT_OK;
}

static MunitResult test_wrapped_apdu_after_credential_round_trips(
    const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential original, from_text, from_der;
    credential_init(&original);
    original.picc_has_ev2_capabilities = true;
    memcpy(original.picc_ev2_capabilities, "\x00\x00\x00\x00\xA5\x5A", 6);

    static char text[DFC_TEXT_MAX_SIZE];
    size_t text_len = 0;
    munit_assert_int(
        dfc_text_write(&original, text, sizeof(text), &text_len), ==, DfcTextOk);
    DfcTextError detail = {0};
    munit_assert_int(dfc_text_parse(&from_text, text, text_len, &detail), ==, DfcTextOk);
    munit_assert_true(from_text.picc_has_ev2_capabilities);
    munit_assert_memory_equal(6, from_text.picc_ev2_capabilities, original.picc_ev2_capabilities);

    static uint8_t der[DFC_DER_MAX_SIZE];
    size_t der_len = 0;
    munit_assert_int(dfc_der_encode(&from_text, der, sizeof(der), &der_len), ==, DfcDerOk);
    munit_assert_int(dfc_der_decode(&from_der, der, der_len), ==, DfcDerOk);
    munit_assert_true(from_der.picc_has_ev2_capabilities);
    munit_assert_memory_equal(6, from_der.picc_ev2_capabilities, original.picc_ev2_capabilities);

    static const struct {
        const char* request;
        const char* final_response;
    } vectors[] = {
        {"9071000002000000",
         "4D693FA018614D600133425883AE77E27E888514CBB6195AD99B1E6B9512EEAD9100"},
        {"9071000008000601020304050600",
         "4D693FA018614D600133425883AE77E2D1796CC80A0A7BF756479D07782A67A99100"},
        {"907100000A0008010203040506070800",
         "4D693FA018614D600133425883AE77E2D1796CC80A0A7BF756479D07782A67A99100"},
    };
    for(size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&from_der);
        munit_assert_not_null(session);
        DfcVirtualPiccActivation activation;
        munit_assert_int(
            dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
        uint8_t random[20];
        decode_hex("000102030405060708090A0B0C0D0E0FA1B2C3D4", random, sizeof(random));
        dfc_host_set_random_buffer(random, sizeof(random));
        assert_apdu(
            session, vectors[i].request, "7ACA0FD9BCD6EC7C9F97466616E6A28291AF");
        assert_apdu(
            session,
            "90AF000020358D5B59ADB65D04107676586F4734468AE4FC4E8630324D15ABDA99C0D0202C00",
            vectors[i].final_response);
        munit_assert_size(dfc_host_random_remaining(), ==, 0);
        dfc_virtual_picc_session_free(session);
    }
    return MUNIT_OK;
}

static MunitResult test_non_first_has_no_capability_exchange(
    const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    credential_init(&credential);
    credential.picc_has_ev2_capabilities = true;
    memset(credential.picc_ev2_capabilities, 0xA5, DFC_EV2_CAPABILITY_LENGTH);
    DfcEmulator* emulator = dfc_emulator_alloc(&credential);
    munit_assert_not_null(emulator);
    const uint8_t reader[6] = {1, 2, 3, 4, 5, 6};
    authenticate(emulator, reader, sizeof(reader), credential.picc_ev2_capabilities);

    const uint8_t request[] = {DFC_CMD_AUTHENTICATE_EV2_NON_FIRST, 0};
    uint8_t response[64];
    size_t len = command(emulator, request, sizeof(request), response);
    munit_assert_size(len, ==, 1 + DFC_EV2_RANDOM_LENGTH);
    munit_assert_uint8(response[0], ==, DFC_CMD_ADDITIONAL_FRAME);
    uint8_t zero_key[DFC_AES_KEY_LENGTH] = {0};
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t random_b[DFC_EV2_RANDOM_LENGTH];
    dfc_worker_aes_cbc_decrypt(
        zero_key, sizeof(zero_key), iv, sizeof(random_b), response + 1, random_b);
    uint8_t clear[DFC_EV2_RANDOM_LENGTH * 2];
    for(size_t i = 0; i < DFC_EV2_RANDOM_LENGTH; i++) {
        clear[i] = (uint8_t)(0x20 + i);
        clear[DFC_EV2_RANDOM_LENGTH + i] = random_b[(i + 1) % DFC_EV2_RANDOM_LENGTH];
    }
    uint8_t continuation[1 + sizeof(clear)] = {DFC_CMD_ADDITIONAL_FRAME};
    memset(iv, 0, sizeof(iv));
    dfc_worker_aes_cbc_encrypt(
        zero_key, sizeof(zero_key), iv, sizeof(clear), clear, continuation + 1);
    len = command(emulator, continuation, sizeof(continuation), response);
    munit_assert_size(len, ==, 1 + DFC_EV2_RANDOM_LENGTH);
    munit_assert_uint8(response[0], ==, DFC_STATUS_OK);
    uint8_t decoded[DFC_EV2_RANDOM_LENGTH];
    memset(iv, 0, sizeof(iv));
    dfc_worker_aes_cbc_decrypt(
        zero_key, sizeof(zero_key), iv, sizeof(decoded), response + 1, decoded);
    for(size_t i = 0; i < DFC_EV2_RANDOM_LENGTH; i++) {
        munit_assert_uint8(decoded[i], ==, clear[(i + 1) % DFC_EV2_RANDOM_LENGTH]);
    }
    dfc_emulator_free(emulator);
    return MUNIT_OK;
}

static MunitResult
    test_bad_lengths_do_not_start_authentication(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    credential_init(&credential);
    DfcEmulator* emulator = dfc_emulator_alloc(&credential);
    munit_assert_not_null(emulator);
    const uint8_t malformed[][5] = {
        {DFC_CMD_AUTHENTICATE_EV2_FIRST, 0},
        {DFC_CMD_AUTHENTICATE_EV2_FIRST, 0, 2, 1},
        {DFC_CMD_AUTHENTICATE_EV2_FIRST, 0, 0, 1},
        {DFC_CMD_AUTHENTICATE_EV2_NON_FIRST, 0, 0},
    };
    const size_t lengths[] = {2, 4, 4, 3};
    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        uint8_t response[64];
        size_t len = command(emulator, malformed[i], lengths[i], response);
        munit_assert_size(len, ==, 1);
        munit_assert_uint8(response[0], ==, DFC_STATUS_LENGTH_ERROR);
        munit_assert_false(emulator->ev2_authentication_pending);
    }
    dfc_emulator_free(emulator);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/v6_maximal_capability_exchange",
     test_v6_maximal_capability_exchange,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/card_and_reader_capabilities",
     test_card_and_reader_capabilities,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/bad_lengths",
     test_bad_lengths_do_not_start_authentication,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/wrapped_apdu_after_credential_round_trips",
     test_wrapped_apdu_after_credential_round_trips,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/non_first_has_no_capability_exchange",
     test_non_first_has_no_capability_exchange,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
#else
static MunitTest tests[] = {{NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
#endif

static const MunitSuite suite = {"/dfc_ev2_capabilities", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
