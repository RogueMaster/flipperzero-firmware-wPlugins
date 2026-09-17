/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 CinderSocket
 *
 * A legacy (D40) authentication captured off the wire, replayed against the
 * engine. The credential is the exact octets a device held, and the reader
 * frames are the exact octets an access reader sent, so nothing here is
 * reconstructed from our own understanding of the handshake: if the engine and
 * the reader disagree, this test says so without either of them agreeing with
 * itself.
 *
 * The reader's RndB is pinned so the exchange is deterministic. Its value is the
 * one the capture implies: the card answered ek(RndB) = 2A2CD1A3B4848209, and
 * ek is a 2K3DES CBC encryption under application key 1 with a zero IV, so
 * RndB = DEC(key1, 2A2CD1A3B4848209) = B2094AA93343754F.
 */
#include "dfc_virtual_picc_test_helpers.h"
#include "dfc_port_host.h"
#include "dfc_der.h"
#include "dfc_emulator_i.h"

/* Embedded credential so the replay does not depend on reading a file: UID
 * 04DBD4960E2E46, one application 53494F with legacy authentication and four
 * 16-octet keys, one standard data file 0F. */
static const uint8_t kDeviceCredential[] = {
    0x60, 0x81, 0xf6, 0x80, 0x01, 0x04, 0xa1, 0x13, 0x80, 0x01, 0x01, 0x81, 0x02, 0x10, 0x00, 0x82,
    0x07, 0x04, 0xdb, 0xd4, 0x96, 0x0e, 0x2e, 0x46, 0x83, 0x01, 0x00, 0xa2, 0x0d, 0x80, 0x01, 0x0f,
    0x81, 0x01, 0x01, 0x82, 0x01, 0x00, 0xa9, 0x00, 0xaa, 0x00, 0xa3, 0x81, 0xcc, 0x30, 0x81, 0xc9,
    0x80, 0x03, 0x53, 0x49, 0x4f, 0x83, 0x01, 0x0b, 0x84, 0x01, 0x04, 0x85, 0x01, 0x00, 0xa6, 0x68,
    0x30, 0x18, 0x80, 0x01, 0x00, 0x81, 0x10, 0xed, 0x23, 0x5b, 0x68, 0x33, 0x11, 0x9a, 0x6e, 0x05,
    0x5a, 0x22, 0x79, 0xb5, 0x8f, 0x2b, 0xa0, 0x82, 0x01, 0x00, 0x30, 0x18, 0x80, 0x01, 0x01, 0x81,
    0x10, 0xea, 0x55, 0xe3, 0x68, 0x30, 0x50, 0x9b, 0x79, 0xd3, 0x31, 0x8f, 0x3b, 0x59, 0x11, 0xe0,
    0xf1, 0x82, 0x01, 0x00, 0x30, 0x18, 0x80, 0x01, 0x02, 0x81, 0x10, 0xc2, 0x41, 0x9f, 0x40, 0x86,
    0x5d, 0xf8, 0x03, 0x13, 0x03, 0xe8, 0x9f, 0xde, 0x4e, 0x2d, 0xdb, 0x82, 0x01, 0x00, 0x30, 0x18,
    0x80, 0x01, 0x03, 0x81, 0x10, 0x6a, 0x2f, 0xff, 0x65, 0x0c, 0x58, 0xbe, 0x9a, 0x23, 0xaf, 0x3d,
    0x4f, 0x4d, 0x6f, 0xea, 0x07, 0x82, 0x01, 0x00, 0xa7, 0x4f, 0x30, 0x4d, 0x80, 0x01, 0x0f, 0x81,
    0x01, 0x00, 0x82, 0x01, 0x01, 0x83, 0x02, 0x12, 0x30, 0xa5, 0x3e, 0x80, 0x01, 0x36, 0x81, 0x36,
    0x30, 0x32, 0x81, 0x05, 0x01, 0xb2, 0xb7, 0xad, 0x15, 0xa5, 0x02, 0x05, 0x00, 0xa6, 0x08, 0x81,
    0x01, 0x01, 0x04, 0x03, 0x03, 0x00, 0x09, 0xa7, 0x17, 0x85, 0x15, 0x52, 0xfe, 0xd5, 0x69, 0x68,
    0x32, 0xc1, 0x6e, 0xbf, 0x79, 0x9b, 0x18, 0x7b, 0xc4, 0xd9, 0x33, 0xd5, 0xff, 0x92, 0x20, 0xae,
    0xa9, 0x02, 0x05, 0x00, 0x05, 0x00, 0x82, 0x01, 0xff};

/* RndB the card must have generated, given the ek(RndB) it sent. */
static const uint8_t kRndB[] = {0xb2, 0x09, 0x4a, 0xa9, 0x33, 0x43, 0x75, 0x4f};

/* Captured: the card's answer to AUTHENTICATE, ek(RndB) followed by 0xAF. */
static const uint8_t kEncRndB[] = {0x2a, 0x2c, 0xd1, 0xa3, 0xb4, 0x84, 0x82, 0x09};

/* Captured: the reader's second frame, ek(RndA) || ek(RndB' xor previous). */
static const uint8_t kReaderResponse[] = {0xd3, 0xf8, 0xf6, 0x02, 0x6a, 0x2b, 0x65, 0x80,
                                          0x4a, 0x99, 0xa7, 0xb6, 0x42, 0xa0, 0x40, 0xd1};

static MunitResult test_captured_d40_authentication(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    DfcCredential credential;
    munit_assert_int(
        dfc_der_decode(&credential, kDeviceCredential, sizeof(kDeviceCredential)), ==, DfcDerOk);
    munit_assert_size(credential.num_apps, ==, 1);
    munit_assert_size(credential.apps[0].num_keys, ==, 4);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);

    uint8_t response[64];
    size_t response_len = 0;

    /* The reader's real prologue, in order. It probes the well-known DF name,
     * then asks for the field application, which this credential does not carry,
     * and only then selects the one it does. Replaying it matters: a refused
     * selection resets the session, and whether the selection that follows
     * leaves the right application current is exactly what the handshake then
     * depends on. */
    const uint8_t iso_select[] = {
        0x00, 0xa4, 0x04, 0x00, 0x07, 0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, iso_select, sizeof(iso_select), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x90, 0x00}));

    const uint8_t select_field[] = {0x90, 0x5a, 0x00, 0x00, 0x03, 0x4f, 0x49, 0xd3, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_field, sizeof(select_field), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_memory_equal(
        2, response, ((uint8_t[]){0x91, DFC_STATUS_APPLICATION_NOT_FOUND}));

    /* Select the application, least significant AID octet first on the wire. */
    const uint8_t select_app[] = {0x90, 0x5a, 0x00, 0x00, 0x03, 0x4f, 0x49, 0x53, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, select_app, sizeof(select_app), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 2);
    munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

    /* AUTHENTICATE (legacy) on key 1, with the card's RndB pinned to the value
     * the capture implies. The answer must be the captured ek(RndB): if it is
     * not, the card enciphered RndB differently from the reader that read it,
     * and the handshake cannot then succeed whatever step 2 does. */
    dfc_host_set_random_buffer(kRndB, sizeof(kRndB));
    const uint8_t authenticate[] = {0x90, 0x0a, 0x00, 0x00, 0x01, 0x01, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, authenticate, sizeof(authenticate), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, sizeof(kEncRndB) + 2);
    munit_assert_memory_equal(sizeof(kEncRndB), response, kEncRndB);

    /* The reader's real answer must be accepted. */
    uint8_t frame[5 + sizeof(kReaderResponse) + 1];
    frame[0] = 0x90;
    frame[1] = 0xaf;
    frame[2] = 0x00;
    frame[3] = 0x00;
    frame[4] = (uint8_t)sizeof(kReaderResponse);
    memcpy(frame + 5, kReaderResponse, sizeof(kReaderResponse));
    frame[5 + sizeof(kReaderResponse)] = 0x00;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, frame, sizeof(frame), response, sizeof(response), &response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_uint8(response[response_len - 1], ==, DFC_STATUS_OK);

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/captured-d40-authentication",
     test_captured_d40_authentication,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/dfc_d40_capture", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
