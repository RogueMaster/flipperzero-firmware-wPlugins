// Ported from MrClean.WireRoom.Tests DesfirePiccSimulatorConformanceTests /
// DesfirePiccSimulatorTests (EV1-native command subset).

#include "dfc_virtual_picc_test_helpers.h"

#include <string.h>

static void blank_picc(DfcCredential* credential, uint8_t picc_key_settings_1) {
    memset(credential, 0, sizeof(*credential));
    credential->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(
        credential->uid,
        ((uint8_t[]){0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}),
        DFC_DESFIRE_UID_LEN);
    credential->picc_key_settings_1 = picc_key_settings_1;
    credential->picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 1;
    credential->picc_auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
    // PICC key material lives in the shared pool, so it is reserved not assigned.
    munit_assert_true(dfc_credential_keys_resize(credential, NULL, 1, 16));
}

static DfcVirtualPiccSession* open_blank(uint8_t picc_key_settings_1, DfcCredential* credential) {
    blank_picc(credential, picc_key_settings_1);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    return session;
}

static void exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, cmd, cmd_len, response, response_capacity, response_len),
        ==,
        DfcVirtualPiccStatusOk);
}

// A PICC that records no key material at all: key count zero, nothing reserved in
// the pool. Section 1.4 says an absent key entry is the factory default, so this
// describes a card whose master key has never been changed.
static void keyless_picc(DfcCredential* credential) {
    memset(credential, 0, sizeof(*credential));
    credential->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(
        credential->uid,
        ((uint8_t[]){0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}),
        DFC_DESFIRE_UID_LEN);
    credential->picc_key_settings_1 = 0x0F;
    credential->picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 0;
    credential->picc_auth_command = DFC_CMD_AUTHENTICATE_LEGACY;
}

// The factory key authenticates a PICC that carries no key material, and the
// session it opens is a real one: GetCardUID answers only under authentication.
static MunitResult test_absent_key_is_factory_default(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    keyless_picc(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t factory[16] = {0};
    uint8_t response[64];
    size_t response_len = 0;
    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    authenticate_legacy_key0(
        session,
        factory,
        sizeof(factory),
        response,
        sizeof(response),
        &response_len,
        session_key,
        &session_key_len,
        DFC_STATUS_OK);

    const uint8_t get_card_uid[] = {0x90, 0x51, 0x00, 0x00, 0x00};
    exchange(session, get_card_uid, sizeof(get_card_uid), response, sizeof(response), &response_len);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static void assert_status(
    DfcVirtualPiccSession* session,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t expected_status) {
    uint8_t response[128];
    size_t response_len = 0;
    exchange(session, cmd, cmd_len, response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, expected_status);
}

static DfcVirtualPiccSession* open_file_policy(
    DfcCredential* credential,
    uint8_t key_settings_1,
    uint16_t access_rights,
    uint8_t comm_settings,
    const uint8_t* data,
    size_t data_len) {
    blank_picc(credential, 0x0F);
    const uint8_t aid[] = {0x01, 0x02, 0x03};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        credential, aid, key_settings_1, 0x01);
    munit_assert_not_null(app);
    DfcFile* file = dfc_credential_create_file(credential, 0, 0x01);
    munit_assert_not_null(file);
    file->type = 0x00;
    file->comm_settings = comm_settings;
    file->access_rights = access_rights;
    munit_assert_size(data_len, <=, DFC_MAX_FILE_DATA);
    munit_assert_true(dfc_file_resize(credential, file, data_len));
    if(data_len > 0) {
        uint8_t* bytes = dfc_file_data(credential, file);
        munit_assert_not_null(bytes);
        memcpy(bytes, data, data_len);
    }

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    return session;
}

static MunitResult test_unsupported_command(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    // AuthenticateEV2First is outside EV1 / earlier.
    const uint8_t cmd[] = {0x90, 0x71, 0x00, 0x00, 0x00};
    assert_status(session, cmd, sizeof(cmd), DFC_STATUS_ILLEGAL_COMMAND_CODE);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_truncated_payload(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    // Lc=2 but no payload bytes follow.
    const uint8_t cmd[] = {0x90, 0x6A, 0x00, 0x00, 0x02, 0x00};
    assert_status(session, cmd, sizeof(cmd), DFC_STATUS_LENGTH_ERROR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_authenticate_missing_key(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t cmd[] = {0x90, 0x0A, 0x00, 0x00, 0x01, 0x01, 0x00};
    assert_status(session, cmd, sizeof(cmd), DFC_STATUS_NO_SUCH_KEY);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

// A handshake the key type does not answer, and a key number beyond the key
// count, are refused with different statuses: the first is an authentication
// failure, the second a missing key.
static MunitResult test_authenticate_wrong_algorithm(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t wrong_cipher[] = {0x90, 0xAA, 0x00, 0x00, 0x01, 0x00, 0x00};
    assert_status(session, wrong_cipher, sizeof(wrong_cipher), DFC_STATUS_AUTHENTICATION_ERR);
    const uint8_t beyond_key_count[] = {0x90, 0x0A, 0x00, 0x00, 0x01, 0x04, 0x00};
    assert_status(session, beyond_key_count, sizeof(beyond_key_count), DFC_STATUS_NO_SUCH_KEY);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_get_version_frames(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    uint8_t response[32];
    size_t response_len = 0;

    const uint8_t get_version[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    exchange(session, get_version, sizeof(get_version), response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 9);
    munit_assert_uint8(response[7], ==, 0x91);
    munit_assert_uint8(response[8], ==, 0xAF);

    const uint8_t af[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    exchange(session, af, sizeof(af), response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 9);
    munit_assert_uint8(response[7], ==, 0x91);
    munit_assert_uint8(response[8], ==, 0xAF);
    munit_assert_uint8(response[4], ==, 0x03); // EV1 software type

    exchange(session, af, sizeof(af), response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 16);
    munit_assert_memory_equal(7, response, credential.uid);
    munit_assert_uint8(response[14], ==, 0x91);
    munit_assert_uint8(response[15], ==, 0x00);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_stray_additional_frame(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t cmd[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    assert_status(session, cmd, sizeof(cmd), DFC_STATUS_ILLEGAL_COMMAND_CODE);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_command_during_response_chaining(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    uint8_t file_data[80];
    for(size_t i = 0; i < sizeof(file_data); i++) file_data[i] = (uint8_t)i;
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEE, DFC_COMM_PLAIN, file_data, sizeof(file_data));

    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t read[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    exchange(session, read, sizeof(read), response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 56);
    munit_assert_uint8(response[54], ==, 0x91);
    munit_assert_uint8(response[55], ==, 0xAF);

    const uint8_t get_file_ids[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
    assert_status(session, get_file_ids, sizeof(get_file_ids), DFC_STATUS_COMMAND_ABORTED);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_write_past_file_end(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    uint8_t zeros[4] = {0};
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEE, DFC_COMM_PLAIN, zeros, sizeof(zeros));

    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);

    const uint8_t write[] = {
        0x90, 0x3D, 0x00, 0x00, 0x09, 0x01, 0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00};
    assert_status(session, write, sizeof(write), DFC_STATUS_BOUNDARY_ERROR);

    uint8_t response[16];
    size_t response_len = 0;
    const uint8_t read[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    exchange(session, read, sizeof(read), response, sizeof(response), &response_len);
    munit_assert_memory_equal(6, response, ((uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x91, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_read_past_file_end(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    uint8_t zeros[4] = {0};
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEE, DFC_COMM_PLAIN, zeros, sizeof(zeros));

    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);

    const uint8_t read[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
    assert_status(session, read, sizeof(read), DFC_STATUS_BOUNDARY_ERROR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_duplicate_file(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);

    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x01, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_OK);
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_DUPLICATE_ERROR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_restricted_app_directory(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    // KS1=0x0D: free create/delete, free directory access disabled.
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0D, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    const uint8_t get_file_ids[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
    assert_status(session, get_file_ids, sizeof(get_file_ids), DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_delete_application_requires_auth(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t del[] = {0x90, 0xDA, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, del, sizeof(del), DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_restricted_picc_create(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    // 0x09: free create/delete disabled on PICC master.
    DfcVirtualPiccSession* session = open_blank(0x09, &credential);
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_AUTHENTICATION_ERR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_restricted_picc_directory(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x09, &credential);
    const uint8_t get_aids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    assert_status(session, get_aids, sizeof(get_aids), DFC_STATUS_AUTHENTICATION_ERR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_file_requires_auth(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    // KS1=0x0B: free create/delete disabled.
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0B, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x01, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_delete_file_free_create_delete(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    uint8_t zeros[4] = {0};
    // Free create/delete; change-access nibble is non-free (0xF).
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEF, DFC_COMM_PLAIN, zeros, sizeof(zeros));
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    const uint8_t del[] = {0x90, 0xDF, 0x00, 0x00, 0x01, 0x01, 0x00};
    assert_status(session, del, sizeof(del), DFC_STATUS_OK);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_delete_file_requires_auth(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    uint8_t zeros[4] = {0};
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0B, 0xEEEE, DFC_COMM_PLAIN, zeros, sizeof(zeros));
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    const uint8_t del[] = {0x90, 0xDF, 0x00, 0x00, 0x01, 0x01, 0x00};
    assert_status(session, del, sizeof(del), DFC_STATUS_AUTHENTICATION_ERR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_29th_application(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    for(int index = 1; index <= 28; index++) {
        uint8_t create_app[] = {
            0x90, 0xCA, 0x00, 0x00, 0x05, (uint8_t)index, 0x00, 0x00, 0x0F, 0x01, 0x00};
        assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    }
    uint8_t create_29[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x1D, 0x00, 0x00, 0x0F, 0x01, 0x00};
    assert_status(session, create_29, sizeof(create_29), DFC_STATUS_COUNT_ERROR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_file_number_out_of_range(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x20, 0x00, 0xEE, 0xEE, 0x01, 0x00, 0x00, 0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_PARAMETER_ERROR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_application_fifteen_keys(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x0F, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_PARAMETER_ERROR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_file_beyond_capacity(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select, sizeof(select), DFC_STATUS_OK);
    // Size 0x002001 = 8193 bytes, above 8 KiB capacity.
    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x01, 0x00, 0xEE, 0xEE, 0x01, 0x20, 0x00, 0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_OUT_OF_EEPROM);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_application_and_list(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_DUPLICATE_ERROR);

    uint8_t response[16];
    size_t response_len = 0;
    const uint8_t get_aids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    exchange(session, get_aids, sizeof(get_aids), response, sizeof(response), &response_len);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0x01, 0x02, 0x03, 0x91, 0x00}));
    munit_assert_uint8(credential.apps[0].key_settings_1, ==, 0x0F);
    munit_assert_uint8(credential.apps[0].key_settings_2, ==, 0x21);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

// An application with an ISO file ID is listed whether or not it has a DF name.
// The record is then the AID and the file ID with nothing after them.
static MunitResult test_get_df_names_lists_unnamed(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    // ISO FID only, no DF name.
    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x07, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x10, 0xE1, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);

    const uint8_t get_df[] = {0x90, 0x6D, 0x00, 0x00, 0x00};
    uint8_t response[32];
    size_t response_len = 0;
    exchange(session, get_df, sizeof(get_df), response, sizeof(response), &response_len);
    const uint8_t expected[] = {0x01, 0x02, 0x03, 0x10, 0xE1, 0x91, DFC_STATUS_OK};
    munit_assert_size(response_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), response, expected);

    munit_assert_true(credential.apps[0].has_iso_file_id);
    munit_assert_uint16(credential.apps[0].iso_file_id, ==, 0xE110);
    munit_assert_size(credential.apps[0].iso_aid_len, ==, 0);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_iso_select_picc_df(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create_app[] = {0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    uint8_t response[8];
    size_t response_len = 0;
    const uint8_t iso_select[] = {
        0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00, 0x00};
    exchange(session, iso_select, sizeof(iso_select), response, sizeof(response), &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x90, 0x00}));

    const uint8_t get_aids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    exchange(session, get_aids, sizeof(get_aids), response, sizeof(response), &response_len);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0x01, 0x02, 0x03, 0x91, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_duplicate_iso_file_id(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create1[] = {
        0x90, 0xCA, 0x00, 0x00, 0x07, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x10, 0xE1, 0x00};
    assert_status(session, create1, sizeof(create1), DFC_STATUS_OK);
    const uint8_t create2[] = {
        0x90, 0xCA, 0x00, 0x00, 0x07, 0x04, 0x05, 0x06, 0x0F, 0x21, 0x10, 0xE1, 0x00};
    assert_status(session, create2, sizeof(create2), DFC_STATUS_DUPLICATE_ERROR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_duplicate_df_name(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create1[] = {
        0x90, 0xCA, 0x00, 0x00, 0x0B, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x10, 0xE1, 0xA0, 0x00, 0x00,
        0x01, 0x00, 0x00};
    assert_status(session, create1, sizeof(create1), DFC_STATUS_OK);
    const uint8_t create2[] = {
        0x90, 0xCA, 0x00, 0x00, 0x0B, 0x04, 0x05, 0x06, 0x0F, 0x21, 0x11, 0xE1, 0xA0, 0x00, 0x00,
        0x01, 0x00, 0x00};
    assert_status(session, create2, sizeof(create2), DFC_STATUS_DUPLICATE_ERROR);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_get_iso_file_ids_lists_file_fids(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);

    // KS2 bit5 (0x20) enables 2-byte ISO FIDs for files.
    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    // Empty list when no ISO FIDs exist.
    uint8_t response[32];
    size_t response_len = 0;
    const uint8_t get_iso[] = {0x90, 0x61, 0x00, 0x00, 0x00};
    exchange(session, get_iso, sizeof(get_iso), response, sizeof(response), &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_OK}));

    // CreateStdDataFile with optional ISO FID (LE): FileNo FID_lo FID_hi Comm AR_lo AR_hi Size(3)
    const uint8_t create_file[] = {
        0x90, 0xCD, 0x00, 0x00, 0x09, 0x01, 0xE1, 0x10, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_OK);
    munit_assert_true(credential.files[0].has_iso_file_id);
    munit_assert_uint16(credential.files[0].iso_file_id, ==, 0x10E1);

    exchange(session, get_iso, sizeof(get_iso), response, sizeof(response), &response_len);
    munit_assert_size(response_len, ==, 4);
    munit_assert_memory_equal(4, response, ((uint8_t[]){0xE1, 0x10, 0x91, DFC_STATUS_OK}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_create_std_data_file_iso_fid_requires_ks2_bit(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    // KS2 = 0x01: one key, ISO file FIDs disabled.
    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x01, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    const uint8_t create_with_fid[] = {
        0x90, 0xCD, 0x00, 0x00, 0x09, 0x01, 0xE1, 0x10, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    assert_status(session, create_with_fid, sizeof(create_with_fid), DFC_STATUS_PARAMETER_ERROR);
    munit_assert_size(credential.num_files, ==, 0);

    // Without FID still works.
    const uint8_t create_plain[] = {
        0x90, 0xCD, 0x00, 0x00, 0x07, 0x01, 0x00, 0xEE, 0xEE, 0x04, 0x00, 0x00, 0x00};
    assert_status(session, create_plain, sizeof(create_plain), DFC_STATUS_OK);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_free_access_full_read_treated_as_plain(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    uint8_t file_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    // Free access (0xEEEE) + Full: must not encrypt (EV3 free-access Full demotion).
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEE, DFC_COMM_ENCIPHERED, file_data, sizeof(file_data));

    uint8_t session_key[16] = {0};
    for(size_t i = 0; i < sizeof(session_key); i++) {
        session_key[i] = (uint8_t)(0x30 + i);
    }
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    session->emulator->auth_key_no = 0x00;

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t read_all[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    exchange(session, read_all, sizeof(read_all), response, sizeof(response), &response_len);

    // Plain data + 8-byte CMAC + 91 00 (not a ciphertext block).
    munit_assert_size(response_len, ==, 4 + 8 + 2);
    munit_assert_memory_equal(4, response, file_data);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_ev1_write_data_mac_requires_transmitted_cmac(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    uint8_t zeros[8] = {0};
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0xEEEE, DFC_COMM_MAC, zeros, sizeof(zeros));

    uint8_t session_key[16];
    for(size_t i = 0; i < sizeof(session_key); i++) {
        session_key[i] = (uint8_t)(0x40 + i);
    }
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    session->emulator->auth_key_no = 0x00;

    uint8_t response[64];
    size_t response_len = 0;

    // Write without MACt must fail integrity and drop auth.
    const uint8_t write_no_mac[] = {
        0x90, 0x3D, 0x00, 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x11, 0x22, 0x00};
    exchange(session, write_no_mac, sizeof(write_no_mac), response, sizeof(response), &response_len);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_INTEGRITY_ERROR}));
    munit_assert_null(session->emulator->secure_messaging);

    // Restore session for positive path.
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    session->emulator->auth_key_no = 0x00;

    uint8_t body[] = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x11, 0x22};
    // MACt = first 8 bytes of EV1 command CMAC (stored as IV after update).
    DfcSecureMessaging* twin = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    dfc_secure_messaging_update_ev1_command(twin, DFC_CMD_WRITE_DATA, body, sizeof(body));
    uint8_t mact[8];
    memcpy(mact, twin->iv, 8);

    uint8_t write_cmd[5 + sizeof(body) + 8 + 1];
    write_cmd[0] = 0x90;
    write_cmd[1] = 0x3D;
    write_cmd[2] = 0x00;
    write_cmd[3] = 0x00;
    write_cmd[4] = (uint8_t)(sizeof(body) + 8);
    memcpy(write_cmd + 5, body, sizeof(body));
    memcpy(write_cmd + 5 + sizeof(body), mact, 8);
    write_cmd[5 + sizeof(body) + 8] = 0x00;
    exchange(
        session, write_cmd, 6 + sizeof(body) + 8, response, sizeof(response), &response_len);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);
    munit_assert_memory_equal(2, dfc_file_data(&credential, &credential.files[0]), ((uint8_t[]){0x11, 0x22}));

    dfc_secure_messaging_free(twin);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_get_iso_file_ids_chains_when_many(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;
#if DFC_MAX_FILES < 28
    return MUNIT_SKIP;
#else
    DfcCredential credential;
    DfcVirtualPiccSession* session = open_blank(0x0F, &credential);
    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x21, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    // 28 files × 2-byte FID = 56 > DFC_EV1_MAX_FRAME_PAYLOAD (54) → AF + remainder.
    for(uint8_t n = 0; n < 28; n++) {
        DfcFile* file = dfc_credential_create_file(&credential, 0, n);
        munit_assert_not_null(file);
        file->type = 0x00;
        file->comm_settings = DFC_COMM_PLAIN;
        file->access_rights = 0xEEEE;
        file->has_iso_file_id = true;
        file->iso_file_id = (uint16_t)(0x1000 + n);
        munit_assert_true(dfc_file_resize(&credential, file, 1));
    }

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t get_iso[] = {0x90, 0x61, 0x00, 0x00, 0x00};
    exchange(session, get_iso, sizeof(get_iso), response, sizeof(response), &response_len);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, DFC_CMD_ADDITIONAL_FRAME);
    munit_assert_size(response_len, ==, DFC_EV1_MAX_FRAME_PAYLOAD + 2);

    const uint8_t af[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    exchange(session, af, sizeof(af), response, sizeof(response), &response_len);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);
    // Remaining: 56 - 54 = 2 bytes (one FID).
    munit_assert_size(response_len, ==, 2 + 2);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
#endif
}

static MunitResult test_read_data_ev1_enciphered_wraps_payload(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    uint8_t file_data[] = {0x30, 0x2F, 0x81, 0x02, 0x0C};
    // Key-gated read (nibble 0) so Full is not demoted by free-access rule.
    DfcVirtualPiccSession* session =
        open_file_policy(&credential, 0x0F, 0x0EEE, DFC_COMM_ENCIPHERED, file_data, sizeof(file_data));

    uint8_t session_key[16] = {0};
    for(size_t i = 0; i < sizeof(session_key); i++) {
        session_key[i] = (uint8_t)(0x20 + i);
    }
    session->emulator->secure_messaging = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    session->emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    session->emulator->auth_key_no = 0x00;

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t read_all[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    exchange(session, read_all, sizeof(read_all), response, sizeof(response), &response_len);

    // EV1 Full response: Enc(RespData||CRC32||pad) || 91 00 — no trailing CMAC.
    munit_assert_size(response_len, ==, 16 + 2);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);
    munit_assert_false(memcmp(response, file_data, sizeof(file_data)) == 0);

    // Reader: command CMAC sets IV, then decrypt the body (CRC covers data||status).
    DfcSecureMessaging* reader = dfc_secure_messaging_alloc(
        DFC_CMD_AUTHENTICATE_AES, session_key, sizeof(session_key), NULL);
    uint8_t file_no_off_len[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    dfc_secure_messaging_update_ev1_command(
        reader, DFC_CMD_READ_DATA, file_no_off_len, sizeof(file_no_off_len));

    uint8_t recovered[DFC_SM_MAX_SIZE];
    size_t recovered_len = dfc_secure_messaging_unwrap(
        reader, DFC_COMM_ENCIPHERED, DFC_STATUS_OK, response, 16, recovered);
    munit_assert_size(recovered_len, ==, sizeof(file_data));
    munit_assert_memory_equal(sizeof(file_data), recovered, file_data);

    dfc_secure_messaging_free(reader);
    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_can_change_key_policy(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    // Mirrors DesfireApplicationState.CanChangeKey policy cases.
    struct {
        uint8_t key_settings_1;
        uint8_t target;
        uint8_t auth;
        bool expected;
    } cases[] = {
        {0x0B, 0x01, 0x00, true},
        {0x0B, 0x01, 0x01, false},
        {0xFB, 0x01, 0x00, false},
        {0xEB, 0x01, 0x01, true},
        {0xEB, 0x01, 0x00, false},
        {0x2B, 0x01, 0x02, true},
        {0x2B, 0x01, 0x00, false},
        {0x0B, 0x00, 0x00, true},
        {0x0A, 0x00, 0x00, false},
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t ks1 = cases[i].key_settings_1;
        uint8_t target = cases[i].target;
        uint8_t auth = cases[i].auth;
        bool expected = cases[i].expected;
        bool allowed;
        if(target == 0x00) {
            allowed = ((ks1 & DFC_KS1_MASTER_KEY_CHANGEABLE) != 0) && auth == 0x00;
        } else {
            uint8_t change_access = (uint8_t)(ks1 >> DFC_KS1_CHANGE_KEY_ACCESS_SHIFT);
            if(change_access == DFC_CHANGE_KEY_ACCESS_FROZEN) {
                allowed = false;
            } else if(change_access == DFC_CHANGE_KEY_ACCESS_SAME) {
                allowed = auth == target;
            } else if(change_access == 0x00) {
                allowed = auth == 0x00;
            } else {
                allowed = auth == change_access;
            }
        }
        munit_assert_true(allowed == expected);
    }
    return MUNIT_OK;
}

// Access-rights nibbles are only distinguishable when the four of them name four
// different keys. These fixtures create the file over the air, so the wire octets
// are the input and `files[0].access_rights` is the logical value the engine must
// have derived from them.
static DfcVirtualPiccSession* open_rights_file_over_the_air(
    DfcCredential* credential,
    uint8_t rights_wire_low,
    uint8_t rights_wire_high) {
    DfcVirtualPiccSession* session = open_blank(0x0F, credential);
    // Four 2K3DES keys, so keys 0..3 are all available to authenticate as.
    const uint8_t create_app[] = {
        0x90, 0xCA, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x0F, 0x04, 0x00};
    assert_status(session, create_app, sizeof(create_app), DFC_STATUS_OK);
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    // CreateStdDataFile: FileNo Comm AR_lo AR_hi Size(3, LE).
    const uint8_t create_file[] = {
        0x90,
        0xCD,
        0x00,
        0x00,
        0x07,
        0x01,
        DFC_COMM_PLAIN,
        rights_wire_low,
        rights_wire_high,
        0x04,
        0x00,
        0x00,
        0x00};
    assert_status(session, create_file, sizeof(create_file), DFC_STATUS_OK);
    return session;
}

static void reselect_and_authenticate(DfcVirtualPiccSession* session, uint8_t key_no) {
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00};
    assert_status(session, select_app, sizeof(select_app), DFC_STATUS_OK);

    uint8_t zero_key[16] = {0};
    uint8_t response[64];
    size_t response_len = 0;
    authenticate_legacy_key(
        session, key_no, zero_key, 16, response, sizeof(response), &response_len, NULL, NULL,
        DFC_STATUS_OK);
}

static void assert_write_status(DfcVirtualPiccSession* session, uint8_t expected_status) {
    const uint8_t write_data[] = {
        0x90, 0x3D, 0x00, 0x00, 0x0B, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    assert_status(session, write_data, sizeof(write_data), expected_status);
}

static void assert_read_status(DfcVirtualPiccSession* session, uint8_t expected_status) {
    const uint8_t read_data[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    uint8_t response[64];
    size_t response_len = 0;
    exchange(session, read_data, sizeof(read_data), response, sizeof(response), &response_len);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, expected_status);
}

// Wire octets 30 12 mean logical 0x1230: read key 1, write key 2, read&write
// key 3, change key 0. Writing must therefore admit keys 2 and 3 and refuse
// keys 0 and 1 — in particular the change key must not be able to write.
static MunitResult test_write_honours_write_and_read_write_nibbles(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    DfcVirtualPiccSession* session = open_rights_file_over_the_air(&credential, 0x30, 0x12);
    munit_assert_uint16(credential.files[0].access_rights, ==, 0x1230);

    reselect_and_authenticate(session, 0x02);
    assert_write_status(session, DFC_STATUS_OK);

    reselect_and_authenticate(session, 0x03);
    assert_write_status(session, DFC_STATUS_OK);

    reselect_and_authenticate(session, 0x00);
    assert_write_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    reselect_and_authenticate(session, 0x01);
    assert_write_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

// Same file: reading must admit keys 1 and 3 and refuse keys 0 and 2.
static MunitResult test_read_honours_read_and_read_write_nibbles(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    DfcVirtualPiccSession* session = open_rights_file_over_the_air(&credential, 0x30, 0x12);
    munit_assert_uint16(credential.files[0].access_rights, ==, 0x1230);

    reselect_and_authenticate(session, 0x01);
    assert_read_status(session, DFC_STATUS_OK);

    reselect_and_authenticate(session, 0x03);
    assert_read_status(session, DFC_STATUS_OK);

    reselect_and_authenticate(session, 0x02);
    assert_read_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    reselect_and_authenticate(session, 0x00);
    assert_read_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

// Wire octets 30 FF mean logical 0xFF30: read and write both denied outright, so
// the read&write nibble (key 3) is the only path to either direction.
static MunitResult test_read_write_nibble_alone_authorises_both_directions(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    DfcVirtualPiccSession* session = open_rights_file_over_the_air(&credential, 0x30, 0xFF);
    munit_assert_uint16(credential.files[0].access_rights, ==, 0xFF30);

    reselect_and_authenticate(session, 0x03);
    assert_write_status(session, DFC_STATUS_OK);
    assert_read_status(session, DFC_STATUS_OK);

    reselect_and_authenticate(session, 0x01);
    assert_write_status(session, DFC_STATUS_AUTHENTICATION_ERR);
    assert_read_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

// ChangeFileSettings is gated by the change nibble, which for logical 0x1230 is
// key 0. Confirms the change nibble is the low one, not one of the read/write pair.
static MunitResult test_change_file_settings_round_trips_logical_rights(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    DfcVirtualPiccSession* session = open_rights_file_over_the_air(&credential, 0x30, 0x12);

    // Rewrite the rights to logical 0x2130 with wire octets 30 21.
    const uint8_t change[] = {
        0x90, 0x5F, 0x00, 0x00, 0x04, 0x01, DFC_COMM_PLAIN, 0x30, 0x21, 0x00};
    reselect_and_authenticate(session, 0x00);
    assert_status(session, change, sizeof(change), DFC_STATUS_OK);
    munit_assert_uint16(credential.files[0].access_rights, ==, 0x2130);

    // Read is now key 2, write key 1.
    reselect_and_authenticate(session, 0x02);
    assert_read_status(session, DFC_STATUS_OK);
    assert_write_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    reselect_and_authenticate(session, 0x01);
    assert_write_status(session, DFC_STATUS_OK);
    assert_read_status(session, DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/write-honours-write-and-read-write-nibbles",
     test_write_honours_write_and_read_write_nibbles,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-honours-read-and-read-write-nibbles",
     test_read_honours_read_and_read_write_nibbles,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-write-nibble-alone-authorises-both-directions",
     test_read_write_nibble_alone_authorises_both_directions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/change-file-settings-round-trips-logical-rights",
     test_change_file_settings_round_trips_logical_rights,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/unsupported-command", test_unsupported_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/truncated-payload", test_truncated_payload, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/authenticate-missing-key",
     test_authenticate_missing_key,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/authenticate-wrong-algorithm",
     test_authenticate_wrong_algorithm,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/absent-key-is-factory-default",
     test_absent_key_is_factory_default,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-version-frames", test_get_version_frames, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/stray-additional-frame",
     test_stray_additional_frame,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/command-during-chaining",
     test_command_during_response_chaining,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/write-past-file-end", test_write_past_file_end, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/read-past-file-end", test_read_past_file_end, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/create-duplicate-file",
     test_create_duplicate_file,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/restricted-app-directory",
     test_restricted_app_directory,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/delete-application-requires-auth",
     test_delete_application_requires_auth,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/restricted-picc-create",
     test_restricted_picc_create,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/restricted-picc-directory",
     test_restricted_picc_directory,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-file-requires-auth",
     test_create_file_requires_auth,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/delete-file-free-create-delete",
     test_delete_file_free_create_delete,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/delete-file-requires-auth",
     test_delete_file_requires_auth,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-29th-application",
     test_create_29th_application,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/file-number-out-of-range",
     test_file_number_out_of_range,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-application-fifteen-keys",
     test_create_application_fifteen_keys,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-file-beyond-capacity",
     test_create_file_beyond_capacity,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-application-and-list",
     test_create_application_and_list,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-df-names-lists-unnamed",
     test_get_df_names_lists_unnamed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/iso-select-picc-df", test_iso_select_picc_df, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/duplicate-iso-file-id",
     test_duplicate_iso_file_id,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/duplicate-df-name", test_duplicate_df_name, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/can-change-key-policy",
     test_can_change_key_policy,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-iso-file-ids-lists-file-fids",
     test_get_iso_file_ids_lists_file_fids,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-data-ev1-enciphered-wraps-payload",
     test_read_data_ev1_enciphered_wraps_payload,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/create-std-data-file-iso-fid-requires-ks2-bit",
     test_create_std_data_file_iso_fid_requires_ks2_bit,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/free-access-full-read-treated-as-plain",
     test_free_access_full_read_treated_as_plain,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ev1-write-data-mac-requires-transmitted-cmac",
     test_ev1_write_data_mac_requires_transmitted_cmac,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-iso-file-ids-chains-when-many",
     test_get_iso_file_ids_chains_when_many,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_ev1_conformance",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
