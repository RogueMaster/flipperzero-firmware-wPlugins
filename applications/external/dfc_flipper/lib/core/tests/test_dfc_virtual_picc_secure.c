#include "dfc_virtual_picc_test_helpers.h"

static MunitResult test_iso_authentication_resets_ev1_secure_messaging_iv(
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
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    const uint8_t authenticate[] = {0x90, 0x1A, 0x00, 0x00, 0x01, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, authenticate, sizeof(authenticate), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 10);
    munit_assert_uint8(response[8], ==, 0x91);
    munit_assert_uint8(response[9], ==, 0xAF);

    const uint8_t* key = dfc_credential_key_const(&credential, app, 1);
    uint8_t iv[16] = {0};
    uint8_t rnd_b[16];
    dfc_worker_des_cbc_decrypt(key, app->key_len, iv, 8, response, rnd_b);

    uint8_t rnd_b_rot[16];
    memcpy(rnd_b_rot, rnd_b, 8);
    dfc_rotate_left(rnd_b_rot, 8);

    uint8_t rnd_a[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    uint8_t challenge[16];
    memcpy(challenge, rnd_a, 8);
    memcpy(challenge + 8, rnd_b_rot, 8);

    memcpy(iv, response, 8);
    uint8_t encrypted_challenge[16];
    dfc_worker_des_cbc_encrypt(
        key, app->key_len, iv, sizeof(challenge), challenge, encrypted_challenge);

    uint8_t additional_frame[22] = {0x90, 0xAF, 0x00, 0x00, 0x10};
    memcpy(additional_frame + 5, encrypted_challenge, sizeof(encrypted_challenge));
    additional_frame[21] = 0x00;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            additional_frame,
            sizeof(additional_frame),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 10);
    munit_assert_uint8(response[8], ==, 0x91);
    munit_assert_uint8(response[9], ==, 0x00);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(
        DFC_CMD_AUTHENTICATE_ISO, key, app->key_len, rnd_a, rnd_b, session_key, &session_key_len);
    DfcSecureMessaging* expected_sm =
        dfc_secure_messaging_alloc(DFC_CMD_AUTHENTICATE_ISO, session_key, session_key_len, NULL);

    const uint8_t get_file_settings[] = {0x90, 0xF5, 0x00, 0x00, 0x01, 0x0F, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_file_settings,
            sizeof(get_file_settings),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 17);
    munit_assert_uint8(response[15], ==, 0x91);
    munit_assert_uint8(response[16], ==, 0x00);

    uint8_t file_no[] = {0x0F};
    dfc_secure_messaging_update_ev1_command(
        expected_sm, DFC_CMD_GET_FILE_SETTINGS, file_no, sizeof(file_no));

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_unwrap_ev1_response(
        expected_sm, DFC_STATUS_OK, response, response_len - 2, recovered);

    // The fixture holds logical rights 0x1230, emitted low octet first.
    uint8_t expected_settings[] = {0x00, 0x01, 0x30, 0x12, 0x03, 0x00, 0x00};
    munit_assert_size(recovered_len, ==, sizeof(expected_settings));
    munit_assert_memory_equal(sizeof(expected_settings), recovered, expected_settings);

    dfc_secure_messaging_free(expected_sm);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_ev1_error_response_clears_authenticated_session(
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
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    const uint8_t authenticate[] = {0x90, 0x1A, 0x00, 0x00, 0x01, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, authenticate, sizeof(authenticate), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 10);

    const uint8_t* key = dfc_credential_key_const(&credential, app, 1);
    uint8_t iv[16] = {0};
    uint8_t rnd_b[16];
    dfc_worker_des_cbc_decrypt(key, app->key_len, iv, 8, response, rnd_b);

    uint8_t rnd_b_rot[16];
    memcpy(rnd_b_rot, rnd_b, 8);
    dfc_rotate_left(rnd_b_rot, 8);

    uint8_t rnd_a[16] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
    uint8_t challenge[16];
    memcpy(challenge, rnd_a, 8);
    memcpy(challenge + 8, rnd_b_rot, 8);

    memcpy(iv, response, 8);
    uint8_t encrypted_challenge[16];
    dfc_worker_des_cbc_encrypt(
        key, app->key_len, iv, sizeof(challenge), challenge, encrypted_challenge);

    uint8_t additional_frame[22] = {0x90, 0xAF, 0x00, 0x00, 0x10};
    memcpy(additional_frame + 5, encrypted_challenge, sizeof(encrypted_challenge));
    additional_frame[21] = 0x00;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            additional_frame,
            sizeof(additional_frame),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_not_null(session->emulator->secure_messaging);

    const uint8_t get_missing_file_settings[] = {0x90, 0xF5, 0x00, 0x00, 0x01, 0x7F, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_missing_file_settings,
            sizeof(get_missing_file_settings),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, DFC_STATUS_PARAMETER_ERROR);
    munit_assert_null(session->emulator->secure_messaging);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult
    test_read_data_respects_offset_and_length(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcApplication* app = dfc_credential_get_primary_application(&credential);
    munit_assert_not_null(app);
    credential.files[0].comm_settings = DFC_COMM_PLAIN;
    munit_assert_true(dfc_file_resize(&credential, &credential.files[0], 8));
    memcpy(
        dfc_file_data(&credential, &credential.files[0]),
        ((uint8_t[]){0x30, 0x2F, 0x81, 0x02, 0x0C, 0x03, 0xA5, 0x02}),
        8);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t session_key[8] = {0};
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_LEGACY, session_key, sizeof(session_key), NULL);
    session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    // Access rights 0x1230 require read key 0x01.
    session->emulator->auth_key_no = 0x01;

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t read_first_five[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_first_five,
            sizeof(read_first_five),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 7);
    munit_assert_memory_equal(
        7, response, ((uint8_t[]){0x30, 0x2F, 0x81, 0x02, 0x0C, 0x91, 0x00}));

    const uint8_t read_offset_three[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x0F, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_offset_three,
            sizeof(read_offset_three),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 5);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0x02, 0x0C, 0x03, 0x91, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_read_data_mac_file_uses_single_ev1_response_mac(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcApplication* app = dfc_credential_get_primary_application(&credential);
    munit_assert_not_null(app);
    credential.files[0].comm_settings = DFC_COMM_MAC;
    munit_assert_true(dfc_file_resize(&credential, &credential.files[0], 8));
    memcpy(
        dfc_file_data(&credential, &credential.files[0]),
        ((uint8_t[]){0x30, 0x2F, 0x81, 0x02, 0x0C, 0x03, 0xA5, 0x02}),
        8);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t session_key[16] = {0};
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_ISO, session_key, sizeof(session_key), NULL);
    session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    // Access rights 0x1230 require read key 0x01.
    session->emulator->auth_key_no = 0x01;

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t read_first_five[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_first_five,
            sizeof(read_first_five),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    munit_assert_size(response_len, ==, 15);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0x30, 0x2F, 0x81, 0x02, 0x0C}));
    munit_assert_uint8(response[13], ==, 0x91);
    munit_assert_uint8(response[14], ==, 0x00);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_get_key_settings_reports_selected_app_settings(
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
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);

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
    munit_assert_size(response_len, ==, 4);
    munit_assert_memory_equal(
        4, response, ((uint8_t[]){app->key_settings_1, app->key_settings_2, 0x91, 0x00}));

    const uint8_t get_key_version[] = {0x90, 0x64, 0x00, 0x00, 0x01, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_key_version,
            sizeof(get_key_version),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(3, response, ((uint8_t[]){app->key_versions[1], 0x91, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/iso-authentication-resets-ev1-secure-messaging-iv",
     test_iso_authentication_resets_ev1_secure_messaging_iv,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1-error-response-clears-authenticated-session",
     test_ev1_error_response_clears_authenticated_session,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-data-respects-offset-and-length",
     test_read_data_respects_offset_and_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-data-mac-file-uses-single-ev1-response-mac",
     test_read_data_mac_file_uses_single_ev1_response_mac,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-key-settings-reports-selected-app-settings",
     test_get_key_settings_reports_selected_app_settings,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite =
    {"/dfc_virtual_picc_secure", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
