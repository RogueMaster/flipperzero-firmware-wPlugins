/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 CinderSocket
 *
 * The PICC configuration a credential carries has to change what the emulated
 * card does, not just survive a round trip. These tests drive the emulator and
 * read what activation presented and what commands answered, rather than
 * inspecting the credential.
 */
#include "dfc_virtual_picc_test_helpers.h"
#include "dfc_port_host.h"
#include "dfc_emulator_i.h"

static void base_credential(DfcCredential* credential) {
    dfc_credential_clear(credential);
    const uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    memcpy(credential->uid, uid, sizeof(uid));
    credential->uid_len = sizeof(uid);
}

/* Anti-collision presents a generated 4-octet identifier, not the stored UID,
 * and a different one on every activation. */
static MunitResult test_random_id_activation(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    credential.picc_random_id = true;
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    const uint8_t pinned[] = {0xAA, 0xBB, 0xCC};
    dfc_host_set_random_buffer(pinned, sizeof(pinned));

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.uid_len, ==, 4);
    munit_assert_uint8(activation.uid[0], ==, 0x08);
    munit_assert_memory_equal(4, activation.uid, "\x08\xAA\xBB\xCC");
    /* The stored UID is untouched, and is not what was presented. */
    munit_assert_memory_equal(7, credential.uid, "\x04\xA1\xB2\xC3\xD4\xE5\xF6");

    /* A second activation presents a different identifier. */
    dfc_virtual_picc_field_off(session);
    const uint8_t pinned2[] = {0x11, 0x22, 0x33};
    dfc_host_set_random_buffer(pinned2, sizeof(pinned2));
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(4, activation.uid, "\x08\x11\x22\x33");

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* GetVersion reports zeros where it would report the UID, and GetCardUID needs
 * an authenticated session, so the real UID is not readable in the clear. */
static MunitResult test_random_id_hides_uid(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    credential.picc_random_id = true;
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t get_version[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    const uint8_t additional[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    for(int frame = 0; frame < 3; frame++) {
        const uint8_t* apdu = frame == 0 ? get_version : additional;
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(
                session, apdu, 5, response, sizeof(response), &response_len),
            ==,
            DfcVirtualPiccStatusOk);
    }
    /* Third frame: 7 UID octets, 7 production octets, 91 00. */
    munit_assert_size(response_len, ==, 16);
    munit_assert_memory_equal(7, response, "\x00\x00\x00\x00\x00\x00\x00");
    munit_assert_uint8(response[14], ==, 0x91);
    munit_assert_uint8(response[15], ==, DFC_STATUS_OK);

    const uint8_t get_card_uid[] = {0x90, 0x51, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, get_card_uid, sizeof(get_card_uid), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, DFC_STATUS_AUTHENTICATION_ERR);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* Without random ID, GetVersion still reports the stored UID: the zeroing is
 * conditional, not a blanket change. */
static MunitResult test_no_random_id_reports_uid(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.uid_len, ==, 7);
    munit_assert_memory_equal(7, activation.uid, credential.uid);

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t get_version[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    const uint8_t additional[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    for(int frame = 0; frame < 3; frame++) {
        const uint8_t* apdu = frame == 0 ? get_version : additional;
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(
                session, apdu, 5, response, sizeof(response), &response_len),
            ==,
            DfcVirtualPiccStatusOk);
    }
    munit_assert_size(response_len, ==, 16);
    munit_assert_memory_equal(7, response, credential.uid);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* A stored ATS is the answer to RATS, in place of the built-in default. */
static MunitResult test_user_ats_answers_rats(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    /* Deliberately not the section 1.1 default, which has no TA byte. If this
     * ever equals the default the test stops proving the override is honoured. */
    const uint8_t ats[] = {0x06, 0x75, 0x00, 0x81, 0x02, 0x80};
    memcpy(credential.picc_ats, ats, sizeof(ats));
    credential.picc_ats_len = sizeof(ats);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);

    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.ats_len, ==, sizeof(ats));
    munit_assert_memory_equal(sizeof(ats), activation.ats, ats);

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t rats[] = {0xE0, 0x80};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_frame_exchange(
            session, rats, sizeof(rats), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, sizeof(ats));
    munit_assert_memory_equal(sizeof(ats), response, ats);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* A credential with no ATS still gets the engine's default. */
static MunitResult test_default_ats_without_override(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.ats_len, ==, 5);
    munit_assert_memory_equal(
        5, activation.ats, ((uint8_t[]){0x05, 0x65, 0x81, 0x02, 0x80}));

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* SAK and ATQA are presented as stored when the credential carries them. */
static MunitResult test_sak_and_atqa_override(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_uint8(activation.sak, ==, 0x20);
    munit_assert_memory_equal(2, activation.atqa, "\x03\x44");
    dfc_virtual_picc_session_free(session);

    credential.picc_has_sak = true;
    credential.picc_sak = 0x28;
    credential.picc_has_atqa = true;
    credential.picc_atqa[0] = 0x03;
    credential.picc_atqa[1] = 0x48;
    session = dfc_virtual_picc_session_alloc(&credential);
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_uint8(activation.sak, ==, 0x28);
    munit_assert_memory_equal(2, activation.atqa, "\x03\x48");

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* FormatPicc with format disabled is refused, with the status this command
 * already uses for a permission failure, and the credential survives. */
static MunitResult test_format_disabled_refuses(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    credential.picc_format_disabled = true;
    munit_assert_size(credential.num_apps, ==, 1);
    munit_assert_size(credential.num_files, ==, 1);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t master_key[8] = {0};
    authenticate_legacy_key0(
        session, master_key, sizeof(master_key), response, sizeof(response), &response_len, NULL,
        NULL, DFC_STATUS_OK);
    dfc_credential_clear_dirty(&credential);

    const uint8_t format[] = {0x90, 0xFC, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, format, sizeof(format), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[0], ==, 0x91);
    munit_assert_uint8(response[1], ==, DFC_STATUS_AUTHENTICATION_ERR);
    munit_assert_size(credential.num_apps, ==, 1);
    munit_assert_size(credential.num_files, ==, 1);
    munit_assert_false(credential.dirty);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* The same credential without the flag does get wiped, so the test above is
 * measuring the flag and not a broken FormatPicc. */
static MunitResult test_format_allowed_wipes(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    load_standard_credential(&credential);
    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[64];
    size_t response_len = 0;
    const uint8_t master_key[8] = {0};
    authenticate_legacy_key0(
        session, master_key, sizeof(master_key), response, sizeof(response), &response_len, NULL,
        NULL, DFC_STATUS_OK);

    const uint8_t format[] = {0x90, 0xFC, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, format, sizeof(format), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_uint8(response[1], ==, DFC_STATUS_OK);
    munit_assert_size(credential.num_apps, ==, 0);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

/* An ATS states its own length in its first octet. One that contradicts itself
 * is refused where a credential is admitted, rather than repaired. */
static MunitResult test_ats_consistency_check(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    DfcCredential credential;
    base_credential(&credential);

    /* No user ATS: nothing to check. */
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&credential));

    memcpy(credential.picc_ats, "\x06\x75\x00\x81\x02\x80", 6);
    credential.picc_ats_len = 6;
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&credential));

    credential.picc_ats[0] = 0x05; /* shorter than the stored octets */
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&credential));
    credential.picc_ats[0] = 0x14; /* longer, would run off the end */
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&credential));
    credential.picc_ats[0] = 0x00; /* describes no answer at all */
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&credential));

    credential.picc_ats[0] = 0x01;
    credential.picc_ats_len = 1;
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&credential));

    credential.picc_ats_len = DFC_PICC_ATS_MAX + 1;
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&credential));
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/random-id-activation", test_random_id_activation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/random-id-hides-uid", test_random_id_hides_uid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/no-random-id-reports-uid",
     test_no_random_id_reports_uid,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/user-ats-answers-rats",
     test_user_ats_answers_rats,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/default-ats-without-override",
     test_default_ats_without_override,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/sak-and-atqa-override",
     test_sak_and_atqa_override,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/format-disabled-refuses",
     test_format_disabled_refuses,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/format-allowed-wipes",
     test_format_allowed_wipes,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/ats-consistency-check",
     test_ats_consistency_check,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_picc_config", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
