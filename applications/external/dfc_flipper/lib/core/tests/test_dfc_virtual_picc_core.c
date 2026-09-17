#include <stdio.h>
#include "dfc_virtual_picc_test_helpers.h"
#include "dfc_port_host.h"
#include "dfc_emulator_i.h"

static MunitResult
    test_scan_and_iso_wrapped_exchange(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.uid_len, ==, 7);
    munit_assert_uint8(activation.uid[0], ==, 0x04);
    munit_assert_uint8(activation.sak, ==, 0x20);

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t select_picc_df[] = {
        0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            select_picc_df,
            sizeof(select_picc_df),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x90);
    munit_assert_uint8(response[1], ==, 0x00);

    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, 0x00);

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
    munit_assert_size(response_len, ==, 9);
    // The fixture holds logical rights 0x1230, which go on the wire low octet first.
    munit_assert_memory_equal(
        7, response, ((uint8_t[]){0x00, 0x01, 0x30, 0x12, 0x03, 0x00, 0x00}));
    munit_assert_uint8(response[7], ==, 0x91);
    munit_assert_uint8(response[8], ==, 0x00);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_framed_unknown_iso_select_preserves_prefix_and_returns_not_found(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcEmulator* emulator = dfc_emulator_alloc(&credential);
    DfcByteBuf* tx_buffer = dfc_bytebuf_alloc(DFC_WORKER_MAX_BUFFER_SIZE);
    const uint8_t prefix[] = {0x0A, 0x00};
    dfc_bytebuf_append_bytes(tx_buffer, prefix, sizeof(prefix));

    const uint8_t unknown_select[] = {
        0x00,
        DFC_ISO7816_INS_SELECT,
        0x04,
        0x00,
        0x0A,
        0xA0,
        0x00,
        0x00,
        0x01,
        0x51,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00};
    bool app_selected = true;

    munit_assert_true(dfc_emulator_handle_iso7816_select(
        emulator,
        unknown_select,
        sizeof(unknown_select),
        tx_buffer,
        sizeof(prefix),
        &app_selected));
    munit_assert_false(app_selected);
    munit_assert_size(dfc_bytebuf_get_size_bytes(tx_buffer), ==, 4);
    munit_assert_memory_equal(
        4, dfc_bytebuf_get_data(tx_buffer), ((uint8_t[]){0x0A, 0x00, 0x6A, 0x82}));

    dfc_bytebuf_free(tx_buffer);
    dfc_emulator_free(emulator);
    return MUNIT_OK;
}

static MunitResult
    test_iso_dep_frame_exchange_wraps_apdu(const MunitParameter params[], void* user_data) {
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
    const uint8_t rats[] = {0xE0, 0x80};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, rats, sizeof(rats), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 5);
    munit_assert_memory_equal(5, response, ((uint8_t[]){0x05, 0x65, 0x81, 0x02, 0x80}));

    const uint8_t select_picc_df[] = {
        0x00, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_picc_df,
            sizeof(select_picc_df),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 3);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x00, 0x90, 0x00}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static void assert_repeated_reader_frame_cycle(DfcVirtualPiccSession* session) {
    // This fixture exercises ISO-DEP framing, not access-control policy.
    session->credential->files[0].access_rights = 0xEEEE;

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

    const uint8_t select_picc_df[] = {
        0x00, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_picc_df,
            sizeof(select_picc_df),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x00, 0x90, 0x00}));

    const uint8_t select_app[] = {0x01, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(3, response, ((uint8_t[]){0x01, 0x91, 0x00}));

    const uint8_t get_file_settings[] = {0x00, 0x90, 0xF5, 0x00, 0x00, 0x01, 0x0F, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            get_file_settings,
            sizeof(get_file_settings),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 10);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x00, 0x00}));

    // Length 0 = entire file (3 bytes on the standard fixture).
    const uint8_t read_file[] = {
        0x01, 0x90, 0xBD, 0x00, 0x00, 0x07, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, read_file, sizeof(read_file), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, >, 2);
    munit_assert_uint8(response[0], ==, 0x01);
    munit_assert_uint8(response[response_len - 2], ==, 0x91);
    munit_assert_uint8(response[response_len - 1], ==, 0x00);

    const uint8_t select_picc_native[] = {
        0x00, 0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            select_picc_native,
            sizeof(select_picc_native),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    const uint8_t deselect_with_cid[] = {0xCA, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session,
            deselect_with_cid,
            sizeof(deselect_with_cid),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0xEA, 0x00}));

    munit_assert_int(dfc_virtual_picc_field_off(session), ==, DfcVirtualPiccStatusOk);
}

static MunitResult test_reader_frame_cycle_repeats_after_field_off(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    credential.files[0].access_rights = 0xEEEE;
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    assert_repeated_reader_frame_cycle(session);
    assert_repeated_reader_frame_cycle(session);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_protocol_and_field_resets_reuse_allocations(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    munit_assert_not_null(session);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    DfcEmulator* emulator = session->emulator;
    DfcByteBuf* tx_buffer = emulator->tx_buffer;
    emulator->selected_application = DfcEmulatorSelectedApplicationApp;
    emulator->selected_app_index = 1;
    emulator->awaiting_step2 = true;
    session->iso_dep_selected = true;
    session->expected_pcd_sequence = 1;

    munit_assert_int(
        dfc_virtual_picc_reset_protocol(session), ==, DfcVirtualPiccStatusOk);
    munit_assert_ptr_equal(session->emulator, emulator);
    munit_assert_ptr_equal(session->emulator->tx_buffer, tx_buffer);
    munit_assert_true(session->activated);
    munit_assert_false(session->iso_dep_selected);
    munit_assert_uint8(session->expected_pcd_sequence, ==, 0);
    munit_assert_int(
        session->emulator->selected_application, ==, DfcEmulatorSelectedApplicationPicc);
    munit_assert_false(session->emulator->awaiting_step2);

    munit_assert_int(dfc_virtual_picc_field_off(session), ==, DfcVirtualPiccStatusOk);
    munit_assert_ptr_equal(session->emulator, emulator);
    munit_assert_ptr_equal(session->emulator->tx_buffer, tx_buffer);
    munit_assert_false(session->activated);
    munit_assert_false(session->random_uid_valid);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_scan_reports_on_device_desfire_activation_vector(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    munit_assert_size(activation.protocol_len, ==, 2);
    munit_assert_memory_equal(2, activation.protocol, ((uint8_t[]){0x02, 0x02}));
    munit_assert_size(activation.uid_len, ==, 7);
    munit_assert_uint8(activation.uid[0], ==, 0x04);
    munit_assert_size(activation.ats_len, ==, 5);
    munit_assert_memory_equal(5, activation.ats, ((uint8_t[]){0x05, 0x65, 0x81, 0x02, 0x80}));
    munit_assert_uint8(activation.sak, ==, 0x20);
    munit_assert_size(activation.atqa_len, ==, 2);
    munit_assert_memory_equal(2, activation.atqa, ((uint8_t[]){0x03, 0x44}));
    munit_assert_size(activation.rf_detail_len, ==, 3);
    munit_assert_memory_equal(3, activation.rf_detail, ((uint8_t[]){0x01, 0x51, 0x57}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_wrapped_get_version_returns_detection_frame(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t command[DFC_WRAPPED_GET_VERSION_FRAME_SIZE];
    dfc_build_wrapped_get_version_frame(command);

    uint8_t response[128];
    size_t response_len = 0;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, command, sizeof(command), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);

    uint8_t expected[] = {0x04, 0x01, 0x01, 0x01, 0x00, 0x1A, 0x05, 0x91, 0xAF};
    munit_assert_size(response_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), response, expected);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult
    test_unknown_native_aid_returns_to_picc_level(const MunitParameter params[], void* user_data) {
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
    const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_uint8(
        session->emulator->selected_application, ==, DfcEmulatorSelectedApplicationApp);

    const uint8_t select_missing_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0xEF, 0xBE, 0xAD, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            select_missing_app,
            sizeof(select_missing_app),
            response,
            sizeof(response),
            &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_APPLICATION_NOT_FOUND}));
    munit_assert_uint8(
        session->emulator->selected_application, ==, DfcEmulatorSelectedApplicationPicc);

    // No application is selected, so this is refused before any file access
    // rights are consulted and stays 0x9D.
    const uint8_t get_file_ids[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, get_file_ids, sizeof(get_file_ids), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, DFC_STATUS_PERMISSION_DENIED}));

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

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static void load_factory_d40_credential(DfcCredential* credential) {
    dfc_credential_clear(credential);
    memcpy(
        credential->uid,
        ((uint8_t[]){0x04, 0x02, 0xA5, 0xDA, 0x4F, 0x85, 0x5C}),
        DFC_DESFIRE_UID_LEN);
    credential->uid_len = DFC_DESFIRE_UID_LEN;

    const uint8_t factory_aid_desfire_order[] = {0x4F, 0x49, 0x53};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        credential, factory_aid_desfire_order, 0x0B, DFC_KEY_TYPE_DES_2K3DES | 4);
    munit_assert_not_null(app);
    const uint8_t iso_aid[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
    memcpy(app->iso_aid, iso_aid, sizeof(iso_aid));
    app->iso_aid_len = sizeof(iso_aid);
    app->auth_command = DFC_CMD_AUTHENTICATE_LEGACY;

    const uint8_t keys[][16] = {
        {
            0x5E,
            0xA0,
            0xA3,
            0xE3,
            0xA8,
            0xD2,
            0x20,
            0x6C,
            0xB3,
            0x96,
            0xC9,
            0xC8,
            0xDE,
            0xA9,
            0xC2,
            0xD9},
        {
            0xF9,
            0xF5,
            0xE5,
            0xBE,
            0x4D,
            0xEA,
            0x54,
            0xC6,
            0xB6,
            0xD2,
            0x5E,
            0x28,
            0xA8,
            0xBF,
            0xA1,
            0x56},
        {
            0x54,
            0x0A,
            0xF5,
            0x62,
            0x1B,
            0xD3,
            0x5C,
            0xB3,
            0x82,
            0x70,
            0xC2,
            0x50,
            0x06,
            0x3D,
            0x41,
            0x25},
        {
            0xC5,
            0x76,
            0x16,
            0x55,
            0x74,
            0xAC,
            0x9F,
            0x5E,
            0x84,
            0x5E,
            0x63,
            0xF3,
            0x3F,
            0x9F,
            0x4B,
            0x20},
    };
    for(size_t i = 0; i < 4; i++) {
        memcpy(dfc_credential_key(credential, app, i), keys[i], app->key_len);
    }

    DfcFile* file = dfc_credential_create_file(credential, 0, 0x0F);
    munit_assert_not_null(file);
    file->type = 0x00;
    file->comm_settings = DFC_COMM_MAC;
    // The captured card answers GetFileSettings with access-rights octets 12 03.
    // Those arrive least-significant octet first, so the logical value is 0x0312:
    // read key 0, write key 3, read&write key 1, change key 2.
    file->access_rights = 0x0312;
    static const uint8_t file_data[] = {
        0x30,
        0x2F,
        0x81,
        0x02,
        0x0C,
        0x03,
        0xA5,
        0x02,
        0x05,
        0x00,
        0xA6,
        0x08,
        0x81,
        0x01,
        0x01,
        0x04,
        0x03,
        0x03,
        0x00,
        0x09,
        0xA7,
        0x17,
        0x85,
        0x15,
        0xD2,
        0x0B,
        0x74,
        0x96,
        0x7B,
        0xE1,
        0x16,
        0x31,
        0xE9,
        0xC4,
        0x02,
        0x4B,
        0xD6,
        0xDB,
        0x18,
        0xF5,
        0x68,
        0x2B,
        0xE0,
        0x7D,
        0xA4,
        0xA9,
        0x02,
        0x05,
        0x00,
        0x05,
        0x00};
    munit_assert_true(dfc_file_resize(credential, file, sizeof(file_data)));
    memcpy(dfc_file_data(credential, file), file_data, sizeof(file_data));
}

static void assert_trace_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    const uint8_t* expected,
    size_t expected_len) {
    uint8_t response[128];
    size_t response_len = 0;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, command, command_len, response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    if(response_len != expected_len || memcmp(response, expected, expected_len) != 0) {
        printf("command=");
        for(size_t i = 0; i < command_len; i++) printf("%02X", command[i]);
        printf("\nexpected=");
        for(size_t i = 0; i < expected_len; i++) printf("%02X", expected[i]);
        printf("\nactual=");
        for(size_t i = 0; i < response_len; i++) printf("%02X", response[i]);
        printf("\n");
    }
    munit_assert_size(response_len, ==, expected_len);
    munit_assert_memory_equal(expected_len, response, expected);
}

static MunitResult test_factory_d40_trace_select_auth_read_path(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_factory_d40_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00}),
        12,
        ((uint8_t[]){0x90, 0x00}),
        2);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0x4F, 0x49, 0xD3, 0x00}),
        9,
        ((uint8_t[]){0x91, DFC_STATUS_APPLICATION_NOT_FOUND}),
        2);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0x4F, 0x49, 0x53, 0x00}),
        9,
        ((uint8_t[]){0x91, DFC_STATUS_OK}),
        2);
    munit_assert_uint8(
        session->emulator->selected_application, ==, DfcEmulatorSelectedApplicationApp);

    const uint8_t captured_rnd_b[] = {0x5E, 0xE1, 0x1A, 0x07, 0x43, 0xFB, 0x4E, 0xB5};
    dfc_host_set_random_buffer(captured_rnd_b, sizeof(captured_rnd_b));
    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_AUTHENTICATE_LEGACY, 0x00, 0x00, 0x01, 0x01, 0x00}),
        7,
        ((uint8_t[]){0x61, 0xBA, 0x91, 0xCD, 0x03, 0x13, 0x98, 0xD5, 0x91, DFC_CMD_ADDITIONAL_FRAME}),
        10);

    assert_trace_exchange(
        session,
        ((uint8_t[]){
            0x90,
            DFC_CMD_ADDITIONAL_FRAME,
            0x00,
            0x00,
            0x10,
            0xD3,
            0x59,
            0x19,
            0x7F,
            0x16,
            0xEF,
            0x09,
            0xE4,
            0xBE,
            0x33,
            0x3E,
            0x63,
            0xCC,
            0xF2,
            0xC1,
            0x1B,
            0x00}),
        22,
        ((uint8_t[]){0xA4, 0x4A, 0x65, 0xA8, 0x62, 0xF4, 0xA9, 0x54, 0x91, DFC_STATUS_OK}),
        10);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_GET_FILE_SETTINGS, 0x00, 0x00, 0x01, 0x0F, 0x00}),
        7,
        ((uint8_t[]){0x00, 0x01, 0x12, 0x03, 0x33, 0x00, 0x00, 0x91, DFC_STATUS_OK}),
        9);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_READ_DATA, 0x00, 0x00, 0x07, 0x0F, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00}),
        13,
        ((uint8_t[]){0x30, 0x2F, 0x81, 0x02, 0x0C, 0x1E, 0xA1, 0x78, 0x17, 0x91, DFC_STATUS_OK}),
        11);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_READ_DATA, 0x00, 0x00, 0x07, 0x0F, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00}),
        13,
        ((uint8_t[]){
            0x30,
            0x2F,
            0x81,
            0x02,
            0x0C,
            0x03,
            0xA5,
            0x02,
            0x05,
            0x00,
            0xA6,
            0x08,
            0x81,
            0x01,
            0x01,
            0x04,
            0x03,
            0x03,
            0x00,
            0x09,
            0xA7,
            0x17,
            0x85,
            0x15,
            0xD2,
            0x0B,
            0x74,
            0x96,
            0x7B,
            0xE1,
            0x16,
            0x31,
            0xE9,
            0xC4,
            0x02,
            0x4B,
            0xD6,
            0xDB,
            0x18,
            0xF5,
            0x68,
            0x2B,
            0xE0,
            0x7D,
            0xA4,
            0xA9,
            0x02,
            0x05,
            0x00,
            0xEF,
            0x5C,
            0x8D,
            0x4A,
            0x91,
            DFC_STATUS_OK}),
        55);

    assert_trace_exchange(
        session,
        ((uint8_t[]){0x90, DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00}),
        9,
        ((uint8_t[]){0x91, DFC_STATUS_OK}),
        2);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/scan-and-iso-wrapped-exchange",
     test_scan_and_iso_wrapped_exchange,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/framed-unknown-iso-select-preserves-prefix-and-returns-not-found",
     test_framed_unknown_iso_select_preserves_prefix_and_returns_not_found,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/iso-dep-frame-exchange-wraps-apdu",
     test_iso_dep_frame_exchange_wraps_apdu,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/reader-frame-cycle-repeats-after-field-off",
     test_reader_frame_cycle_repeats_after_field_off,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/protocol-and-field-resets-reuse-allocations",
     test_protocol_and_field_resets_reuse_allocations,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/scan-reports-on-device-desfire-activation-vector",
     test_scan_reports_on_device_desfire_activation_vector,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/wrapped-get-version-detection-frame",
     test_wrapped_get_version_returns_detection_frame,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/unknown-native-aid-returns-to-picc-level",
     test_unknown_native_aid_returns_to_picc_level,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/factory-d40-trace-select-auth-read-path",
     test_factory_d40_trace_select_auth_read_path,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_virtual_picc_core", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
