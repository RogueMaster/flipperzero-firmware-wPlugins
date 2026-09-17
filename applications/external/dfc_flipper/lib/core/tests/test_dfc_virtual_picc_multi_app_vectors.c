#include "dfc_virtual_picc_test_helpers.h"

static void init_blank_picc(DfcCredential* credential) {
    dfc_credential_clear(credential);
    credential->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(
        credential->uid,
        ((uint8_t[]){0x04, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75}),
        DFC_DESFIRE_UID_LEN);
}

static void exchange_ok(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t* response_len) {
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, command, command_len, response, 128, response_len),
        ==,
        DfcVirtualPiccStatusOk);
}

static MunitResult
    test_multi_app_file_pool_and_dirty_mutations(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    init_blank_picc(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[128];
    size_t response_len = 0;
    munit_assert_false(credential.dirty);

    const uint8_t create_app_a[] = {
        0x90, DFC_CMD_CREATE_APPLICATION, 0x00, 0x00, 0x05, 0xA1, 0xB2, 0xC3, 0x0F, 0x01, 0x00};
    exchange_ok(session, create_app_a, sizeof(create_app_a), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_apps, ==, 1);
    dfc_credential_clear_dirty(&credential);

    exchange_ok(session, create_app_a, sizeof(create_app_a), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_DUPLICATE_ERROR}));
    munit_assert_false(credential.dirty);

    const uint8_t create_app_b[] = {
        0x90, DFC_CMD_CREATE_APPLICATION, 0x00, 0x00, 0x05, 0xA2, 0xB2, 0xC3, 0x0F, 0x01, 0x00};
    exchange_ok(session, create_app_b, sizeof(create_app_b), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_apps, ==, 2);
    dfc_credential_clear_dirty(&credential);

    const uint8_t get_application_ids[] = {0x90, DFC_CMD_GET_APPLICATION_IDS, 0x00, 0x00, 0x00};
    exchange_ok(
        session, get_application_ids, sizeof(get_application_ids), response, &response_len);
    munit_assert_memory_equal(
        8, response, ((uint8_t[]){0xA1, 0xB2, 0xC3, 0xA2, 0xB2, 0xC3, 0x91, DFC_STATUS_OK}));

    const uint8_t select_app_a[] = {
        0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0xA1, 0xB2, 0xC3, 0x00};
    exchange_ok(session, select_app_a, sizeof(select_app_a), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));

    const uint8_t create_file_04_a[] = {
        0x90,
        DFC_CMD_CREATE_STD_DATA_FILE,
        0x00,
        0x00,
        0x07,
        0x04,
        0x00,
        0xEE,
        0xEE,
        0x02,
        0x00,
        0x00,
        0x00};
    exchange_ok(session, create_file_04_a, sizeof(create_file_04_a), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_files, ==, 1);
    munit_assert_size(credential.files[0].app_index, ==, 0);
    dfc_credential_clear_dirty(&credential);

    const uint8_t select_app_b[] = {
        0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0xA2, 0xB2, 0xC3, 0x00};
    exchange_ok(session, select_app_b, sizeof(select_app_b), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));

    const uint8_t get_file_ids[] = {0x90, DFC_CMD_GET_FILE_IDS, 0x00, 0x00, 0x00};
    exchange_ok(session, get_file_ids, sizeof(get_file_ids), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));

    const uint8_t create_file_04_b[] = {
        0x90,
        DFC_CMD_CREATE_STD_DATA_FILE,
        0x00,
        0x00,
        0x07,
        0x04,
        0x00,
        0xEE,
        0xEE,
        0x03,
        0x00,
        0x00,
        0x00};
    exchange_ok(session, create_file_04_b, sizeof(create_file_04_b), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_files, ==, 2);
    munit_assert_size(credential.files[1].app_index, ==, 1);
    dfc_credential_clear_dirty(&credential);

    exchange_ok(session, get_file_ids, sizeof(get_file_ids), response, &response_len);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x04, 0x91, DFC_STATUS_OK}));

    exchange_ok(session, select_app_a, sizeof(select_app_a), response, &response_len);
    exchange_ok(session, get_file_ids, sizeof(get_file_ids), response, &response_len);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x04, 0x91, DFC_STATUS_OK}));

    const uint8_t select_picc[] = {
        0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
    exchange_ok(session, select_picc, sizeof(select_picc), response, &response_len);

    uint8_t zero_key[8] = {0};
    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    authenticate_legacy_key0(
        session,
        zero_key,
        sizeof(zero_key),
        response,
        sizeof(response),
        &response_len,
        session_key,
        &session_key_len,
        DFC_STATUS_OK);

    const uint8_t delete_app_a[] = {
        0x90, DFC_CMD_DELETE_APPLICATION, 0x00, 0x00, 0x03, 0xA1, 0xB2, 0xC3, 0x00};
    exchange_ok(session, delete_app_a, sizeof(delete_app_a), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_apps, ==, 1);
    munit_assert_size(credential.num_files, ==, 1);
    munit_assert_memory_equal(3, credential.apps[0].aid, ((uint8_t[]){0xC3, 0xB2, 0xA2}));
    munit_assert_size(credential.files[0].app_index, ==, 0);
    dfc_credential_clear_dirty(&credential);

    authenticate_legacy_key0(
        session,
        zero_key,
        sizeof(zero_key),
        response,
        sizeof(response),
        &response_len,
        session_key,
        &session_key_len,
        DFC_STATUS_OK);

    const uint8_t format_picc[] = {0x90, DFC_CMD_FORMAT_PICC, 0x00, 0x00, 0x00};
    exchange_ok(session, format_picc, sizeof(format_picc), response, &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));
    munit_assert_true(credential.dirty);
    munit_assert_size(credential.num_apps, ==, 0);
    munit_assert_size(credential.num_files, ==, 0);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/multi-app-file-pool-and-dirty-mutations",
     test_multi_app_file_pool_and_dirty_mutations,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite =
    {"/dfc_virtual_picc_multi_app_vectors", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
