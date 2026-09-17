#include "dfc_virtual_picc_test_helpers.h"

static MunitResult
    test_pcsc_blank_card_probe_vectors(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t select_picc[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_picc, sizeof(select_picc), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    const uint8_t get_key_settings[] = {0x90, 0x45, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_key_settings,
            sizeof(get_key_settings),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(4, response, ((uint8_t[]){0x0F, 0x01, 0x91, 0x00}));

    const uint8_t iso_manage_channel[] = {0x00, 0x70, 0x00, 0x40, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            iso_manage_channel,
            sizeof(iso_manage_channel),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(
        2,
        response,
        ((uint8_t[]){DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI, DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO}));

    const uint8_t wrapped_mifare_plus_probe[] = {
        0x90, 0x70, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            wrapped_mifare_plus_probe,
            sizeof(wrapped_mifare_plus_probe),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_ILLEGAL_COMMAND_CODE}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_trace_reader_sequence_handles_get_key_settings_and_mfp_probe(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcApplication* app = dfc_credential_get_primary_application(&credential);
    munit_assert_not_null(app);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t rats[] = {0xE0, 0x80};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, rats, sizeof(rats), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    const uint8_t select_unknown_df[] = {0x0A, 0x00, 0x00, 0xA4, 0x04, 0x00, 0x10,
                                         0xA0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01,
                                         0x01, 0x00, 0x01, 0x00, 0x00, 0x47, 0x05};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_unknown_df,
            sizeof(select_unknown_df),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(4, response, ((uint8_t[]){0x0A, 0x00, 0x6A, 0x82}));

    const uint8_t select_second_unknown_df[] = {
        0x0B,
        0x00,
        0x00,
        0xA4,
        0x04,
        0x00,
        0x0A,
        0xA0,
        0x00,
        0x00,
        0x04,
        0x40,
        0x00,
        0x01,
        0x01,
        0x00,
        0x01,
        0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_second_unknown_df,
            sizeof(select_second_unknown_df),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(4, response, ((uint8_t[]){0x0B, 0x00, 0x6A, 0x82}));

    const uint8_t select_missing_native_aid[] = {
        0x0A, 0x00, 0x90, 0x5A, 0x00, 0x00, 0x03, 0xE3, 0x84, 0xF4, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_missing_native_aid,
            sizeof(select_missing_native_aid),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(
        4, response, ((uint8_t[]){0x0A, 0x00, 0x91, DFC_STATUS_APPLICATION_NOT_FOUND}));
    munit_assert_uint8(
        session->emulator->selected_application, ==, DfcEmulatorSelectedApplicationPicc);

    const uint8_t select_app[] = {
        0x0B, 0x00, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(4, response, ((uint8_t[]){0x0B, 0x00, 0x91, 0x00}));

    const uint8_t get_key_settings[] = {0x0A, 0x00, 0x90, 0x45, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            get_key_settings,
            sizeof(get_key_settings),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 6);
    munit_assert_memory_equal(
        6,
        response,
        ((uint8_t[]){0x0A, 0x00, app->key_settings_1, app->key_settings_2, 0x91, 0x00}));

    const uint8_t mifare_plus_auth_first_probe[] = {0x0B, 0x00, 0x70, 0x00, 0x40, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            mifare_plus_auth_first_probe,
            sizeof(mifare_plus_auth_first_probe),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(
        3, response, ((uint8_t[]){0x0B, 0x00, DFC_STATUS_ILLEGAL_COMMAND_CODE}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/pcsc-blank-card-probe-vectors",
     test_pcsc_blank_card_probe_vectors,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/trace-reader-sequence-handles-get-key-settings-and-mfp-probe",
     test_trace_reader_sequence_handles_get_key_settings_and_mfp_probe,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite =
    {"/dfc_virtual_picc_probe_vectors", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
