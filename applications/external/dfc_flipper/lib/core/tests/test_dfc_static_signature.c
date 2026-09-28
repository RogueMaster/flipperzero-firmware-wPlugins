#include "munit/munit.h"
#include "dfc_build_config.h"
#include "dfc_der.h"
#include "dfc_text.h"
#include "dfc_virtual_picc.h"

#include <string.h>

#if DFC_ENABLE_STATIC_SIGNATURE
static void read_signature(DfcCredential* credential, const uint8_t* apdu, size_t apdu_len,
                           uint8_t* response, size_t* response_len) {
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(credential);
    munit_assert_not_null(session);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, apdu, apdu_len, response, 64, response_len),
        ==,
        DfcVirtualPiccStatusOk);
    dfc_virtual_picc_session_free(session);
}

static MunitResult test_ev2_signature_round_trip_and_command(
    const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    munit_assert_true(dfc_build_capabilities().static_signature);
    static DfcCredential original, from_text, from_der;
    munit_assert_true(dfc_credential_clear(&original));
    original.card.generation = DfcGenerationEv2;
    original.card.storage = 4096;
    original.uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(original.uid, "\x04\x11\x22\x33\x44\x55\x66", DFC_DESFIRE_UID_LEN);
    original.picc_key_settings_1 = 0x0F;
    original.picc_key_settings_2 = DFC_KEY_TYPE_AES | 1;
    original.picc_auth_command = DFC_CMD_AUTHENTICATE_AES;
    munit_assert_true(dfc_credential_keys_resize(&original, NULL, 1, DFC_AES_KEY_LENGTH));
    original.picc_has_static_signature = true;
    for(size_t i = 0; i < DFC_STATIC_SIGNATURE_LENGTH; i++) {
        original.picc_static_signature[i] = (uint8_t)i;
    }

    static char text[DFC_TEXT_MAX_SIZE];
    size_t text_len = 0;
    munit_assert_int(
        dfc_text_write(&original, text, sizeof(text), &text_len), ==, DfcTextOk);
    DfcTextError detail = {0};
    munit_assert_int(dfc_text_parse(&from_text, text, text_len, &detail), ==, DfcTextOk);
    munit_assert_true(from_text.picc_has_static_signature);
    munit_assert_memory_equal(
        DFC_STATIC_SIGNATURE_LENGTH, from_text.picc_static_signature, original.picc_static_signature);

    static uint8_t der[DFC_DER_MAX_SIZE];
    size_t der_len = 0;
    munit_assert_int(dfc_der_encode(&from_text, der, sizeof(der), &der_len), ==, DfcDerOk);
    munit_assert_int(dfc_der_decode(&from_der, der, der_len), ==, DfcDerOk);
    munit_assert_true(from_der.picc_has_static_signature);
    munit_assert_memory_equal(
        DFC_STATIC_SIGNATURE_LENGTH, from_der.picc_static_signature, original.picc_static_signature);

    const uint8_t request[] = {0x90, DFC_CMD_READ_SIGNATURE, 0, 0, 1, 0, 0};
    uint8_t response[64];
    size_t response_len = 0;
    read_signature(&from_der, request, sizeof(request), response, &response_len);
    munit_assert_size(response_len, ==, DFC_STATIC_SIGNATURE_LENGTH + 2);
    munit_assert_memory_equal(
        DFC_STATIC_SIGNATURE_LENGTH, response, original.picc_static_signature);
    munit_assert_uint8(response[DFC_STATIC_SIGNATURE_LENGTH], ==, 0x91);
    munit_assert_uint8(response[DFC_STATIC_SIGNATURE_LENGTH + 1], ==, DFC_STATUS_SPECIAL_SUCCESS);

    const uint8_t wrong_address[] = {0x90, DFC_CMD_READ_SIGNATURE, 0, 0, 1, 1, 0};
    read_signature(&from_der, wrong_address, sizeof(wrong_address), response, &response_len);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_PARAMETER_ERROR);
    const uint8_t missing_address[] = {0x90, DFC_CMD_READ_SIGNATURE, 0, 0, 0};
    read_signature(&from_der, missing_address, sizeof(missing_address), response, &response_len);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_LENGTH_ERROR);

    from_der.picc_has_static_signature = false;
    read_signature(&from_der, request, sizeof(request), response, &response_len);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_ILLEGAL_COMMAND_CODE);

    from_der.picc_has_static_signature = true;
    from_der.card.generation = DfcGenerationEv1;
    read_signature(&from_der, request, sizeof(request), response, &response_len);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_ILLEGAL_COMMAND_CODE);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/ev2_signature_round_trip_and_command", test_ev2_signature_round_trip_and_command,
     NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
#else
static MunitTest tests[] = {{NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
#endif

static const MunitSuite suite = {"/dfc_static_signature", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
