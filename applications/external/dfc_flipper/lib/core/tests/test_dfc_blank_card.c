/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 CinderSocket
 *
 * Exercises dfc_credential_init_blank end to end.
 *
 * A blank card is what the firmware installs when a slot is first set to
 * DESFire, so it has to be genuinely usable straight out of init: detectable in
 * the field, selectable, authenticable with the default key, and -- the part
 * that regressed -- actually writable. WriteData bounds-checks against the
 * file's length and never grows it, so a blank file with no allocated capacity
 * rejects every write with BOUNDARY_ERROR.
 */
#include "dfc_virtual_picc_test_helpers.h"

/* SelectApplication / WriteData / ReadData, ISO7816-wrapped as a reader sends
 * them. The AID is the one init_blank creates. */
static const uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00};

static MunitResult
    test_blank_card_is_selectable_authenticable_and_writable(
        const MunitParameter params[],
        void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    dfc_credential_init_blank(&credential);

    /* init_blank must produce a card the RF layer will answer for at all. */
    munit_assert_true(dfc_credential_uid_is_detectable(&credential));
    munit_assert_size(credential.uid_len, ==, DFC_DESFIRE_UID_LEN);
    munit_assert_uint8(credential.uid[0], ==, DFC_DESFIRE_UID_FIRST_BYTE);
    munit_assert_size(credential.num_apps, ==, 1);
    munit_assert_size(credential.num_files, ==, 1);

    /* The regression: the single file must have real capacity. */
    DfcFile* file = dfc_credential_find_file_in_app(&credential, 0, 0x00);
    munit_assert_not_null(file);
    munit_assert_size(file->data_len, ==, DFC_BLANK_FILE_SIZE);
    munit_assert_size(file->data_offset, !=, DFC_FILE_POOL_NONE);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    munit_assert_not_null(session);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[128];
    size_t response_len = 0;

    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, DFC_STATUS_OK);

    /* Default key 0 is all zeroes. */
    uint8_t zero_key[24];
    memset(zero_key, 0, sizeof(zero_key));
    DfcApplication* app = dfc_credential_get_application(&credential, 0);
    munit_assert_not_null(app);
    authenticate_legacy_key0(
        session,
        zero_key,
        app->key_len,
        response,
        sizeof(response),
        &response_len,
        NULL,
        NULL,
        DFC_STATUS_OK);

    /* WriteData 4 bytes at offset 0 -- BOUNDARY_ERROR here is the bug. */
    const uint8_t write_data[] = {
        0x90, 0x3D, 0x00, 0x00, 0x0B, /* Lc = file + offset(3) + len(3) + data(4) */
        0x00, /* file 0 */
        0x00, 0x00, 0x00, /* offset 0 */
        0x04, 0x00, 0x00, /* length 4 */
        0xDE, 0xAD, 0xBE, 0xEF,
        0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, write_data, sizeof(write_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_OK);

    /* Read it back. */
    const uint8_t read_data[] = {
        0x90, 0xBD, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, read_data, sizeof(read_data), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 6);
    munit_assert_uint8(response[0], ==, 0xDE);
    munit_assert_uint8(response[1], ==, 0xAD);
    munit_assert_uint8(response[2], ==, 0xBE);
    munit_assert_uint8(response[3], ==, 0xEF);
    munit_assert_uint8(response[4], ==, 0x91);
    munit_assert_uint8(response[5], ==, DFC_STATUS_OK);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/blank_card_selectable_authenticable_writable",
     test_blank_card_is_selectable_authenticable_and_writable,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_blank_card", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
