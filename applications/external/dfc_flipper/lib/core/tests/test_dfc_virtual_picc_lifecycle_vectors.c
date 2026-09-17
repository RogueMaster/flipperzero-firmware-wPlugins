#include "dfc_virtual_picc_test_helpers.h"

static MunitResult test_pcsc_application_and_file_lifecycle_vectors(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    memset(&credential, 0, sizeof(credential));
    credential.uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(
        credential.uid,
        ((uint8_t[]){0x04, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}),
        DFC_DESFIRE_UID_LEN);
    credential.picc_key_settings_1 = 0x0F;
    credential.picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 1;
    credential.picc_auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
    // PICC key material lives in the shared pool, so it is reserved not assigned.
    munit_assert_true(dfc_credential_keys_resize(&credential, NULL, 1, 16));

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[128];
    size_t response_len = 0;
    uint8_t zero_key[16] = {0};

    const uint8_t select_picc[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_picc, sizeof(select_picc), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x05, 0xC3, 0xB2, 0xA1, 0x0F, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, create_app, sizeof(create_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, create_app, sizeof(create_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_DUPLICATE_ERROR}));

    const uint8_t get_application_ids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_application_ids,
            sizeof(get_application_ids),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0xC3, 0xB2, 0xA1, 0x91, 0x00}));

    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0xC3, 0xB2, 0xA1, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
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

    const uint8_t get_key_version[] = {0x90, 0x64, 0x00, 0x00, 0x01, 0x00, 0x00};
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
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x00, 0x91, 0x00}));

    const uint8_t get_file_ids[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, get_file_ids, sizeof(get_file_ids), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

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
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_PARAMETER_ERROR}));

    const uint8_t read_missing_file[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x7F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_missing_file,
            sizeof(read_missing_file),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_FILE_NOT_FOUND}));

    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x01, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, create_file, sizeof(create_file), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, create_file, sizeof(create_file), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_DUPLICATE_ERROR}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, get_file_ids, sizeof(get_file_ids), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x01, 0x91, 0x00}));

    const uint8_t get_file_settings[] = {0x90, 0xF5, 0x00, 0x00, 0x01, 0x01, 0x00};
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
    munit_assert_memory_equal(
        9, response, ((uint8_t[]){0x00, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x91, 0x00}));

    const uint8_t read_zero_len[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_zero_len,
            sizeof(read_zero_len),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(6, response, ((uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x91, 0x00}));

    const uint8_t read_at_end[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, read_at_end, sizeof(read_at_end), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_BOUNDARY_ERROR}));

    const uint8_t read_beyond_end[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            read_beyond_end,
            sizeof(read_beyond_end),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_BOUNDARY_ERROR}));

    const uint8_t write_data[] = {
        0x90,
        0x3D,
        0x00,
        0x00,
        0x0B,
        0x01,
        0x00,
        0x00,
        0x00,
        0x04,
        0x00,
        0x00,
        0x44,
        0x45,
        0x53,
        0x46,
        0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, write_data, sizeof(write_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    const uint8_t read_data[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, read_data, sizeof(read_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(6, response, ((uint8_t[]){0x44, 0x45, 0x53, 0x46, 0x91, 0x00}));

    const uint8_t write_beyond_end[] = {
        0x90, 0x3D, 0x00, 0x00, 0x08, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x99, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            write_beyond_end,
            sizeof(write_beyond_end),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_BOUNDARY_ERROR}));

    const uint8_t change_file_settings_free[] = {
        0x90, 0x5F, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            change_file_settings_free,
            sizeof(change_file_settings_free),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, read_data, sizeof(read_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    // ChangeFileSettings just set the access rights to 0x0000, so reading names
    // key 0 and the unauthenticated session is missing an authentication, not
    // denied outright.
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_AUTHENTICATION_ERR}));

    authenticate_legacy_key0(
        session,
        zero_key,
        credential.picc_key_len,
        response,
        sizeof(response),
        &response_len,
        NULL,
        NULL,
        DFC_STATUS_OK);
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, read_data, sizeof(read_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(6, response, ((uint8_t[]){0x44, 0x45, 0x53, 0x46, 0x91, 0x00}));

    const uint8_t delete_file[] = {0x90, 0xDF, 0x00, 0x00, 0x01, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, delete_file, sizeof(delete_file), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, get_file_ids, sizeof(get_file_ids), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    const uint8_t select_picc_again[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            select_picc_again,
            sizeof(select_picc_again),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    const uint8_t delete_app[] = {0x90, 0xDA, 0x00, 0x00, 0x03, 0xC3, 0xB2, 0xA1, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, delete_app, sizeof(delete_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_AUTHENTICATION_ERR}));

    authenticate_legacy_key0(
        session,
        zero_key,
        credential.picc_key_len,
        response,
        sizeof(response),
        &response_len,
        NULL,
        NULL,
        DFC_STATUS_OK);

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, delete_app, sizeof(delete_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            get_application_ids,
            sizeof(get_application_ids),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/pcsc-application-and-file-lifecycle-vectors",
     test_pcsc_application_and_file_lifecycle_vectors,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite =
    {"/dfc_virtual_picc_lifecycle_vectors", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
