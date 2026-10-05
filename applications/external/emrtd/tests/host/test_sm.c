/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Secure Messaging.
 *
 * The centrepiece is the worked example of ICAO Doc 9303 part 11 appendix D.4,
 * replayed command by command. It pins both directions of the envelope to the
 * exact bytes the standard prints: a change anywhere in the padding, the Send
 * Sequence Counter, the checksum input or the data object framing moves one of
 * these strings, which is the only cheap way to find such a change before a
 * real chip does.
 *
 * The AES half of the suite covers what appendix D cannot, since the worked
 * example is 3DES only: the initialisation vector derived from the counter,
 * the sixteen byte counter, and the refusals.
 */

#include "emrtd_test.h"

#include "../../crypto/emrtd_sm.h"
#include "../../transport/emrtd_transceiver.h"

/* The session appendix D.3 ends with, which appendix D.4 then uses. */
#define D4_KS_ENC "979EC13B1CBFE9DCD01AB0FED307EAE5"
#define D4_KS_MAC "F1CB1F1FB5ADF208806B89DC579DC1F8"
#define D4_SSC    "887022120C06C226"

static void sm_from_hex(
    EmrtdSm* sm,
    EmrtdCipher cipher,
    const char* ks_enc_hex,
    const char* ks_mac_hex,
    const char* ssc_hex) {
    uint8_t ks_enc[EMRTD_KEY_MAX_SIZE];
    uint8_t ks_mac[EMRTD_KEY_MAX_SIZE];
    uint8_t ssc[EMRTD_BLOCK_MAX_SIZE];
    memset(ssc, 0, sizeof(ssc));
    emrtd_test_hex(ks_enc_hex, ks_enc, sizeof(ks_enc));
    emrtd_test_hex(ks_mac_hex, ks_mac, sizeof(ks_mac));
    if(ssc_hex != NULL) {
        emrtd_test_hex(ssc_hex, ssc, sizeof(ssc));
    }
    emrtd_sm_init(sm, cipher, ks_enc, ks_mac, ssc);
}

static void d4_session(EmrtdSm* sm) {
    sm_from_hex(sm, EmrtdCipherTdes, D4_KS_ENC, D4_KS_MAC, D4_SSC);
}

/** SELECT EF.COM: no Le, so the protected command carries no DO'97'. */
static void select_ef_com(EmrtdCommandApdu* command, const uint8_t fid[2]) {
    command->cla = 0x00;
    command->ins = 0xA4;
    command->p1 = 0x02;
    command->p2 = 0x0C;
    command->data = fid;
    command->data_len = 2;
    command->le = EMRTD_LE_NONE;
}

/** READ BINARY with the offset in P1P2, which is how appendix D.4 reads EF.COM. */
static void read_binary(EmrtdCommandApdu* command, uint16_t offset, int length) {
    command->cla = 0x00;
    command->ins = 0xB0;
    command->p1 = (uint8_t)((offset >> 8) & 0x7F);
    command->p2 = (uint8_t)(offset & 0xFF);
    command->data = NULL;
    command->data_len = 0;
    command->le = length;
}

static void test_appendix_d4(void) {
    emrtd_test_begin("appendix D.4: SELECT EF.COM is wrapped byte for byte");

    EmrtdSm sm;
    d4_session(&sm);

    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    const uint8_t fid[2] = {0x01, 0x1E};
    EmrtdCommandApdu command;

    select_ef_com(&command, fid);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(out, out_len, "0CA4020C158709016375432908C044F68E08BF8B92D635FF24F800");

    emrtd_test_begin("appendix D.4: the answer to SELECT verifies and is empty");
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    uint8_t plain[EMRTD_APDU_MAX_SIZE];
    EmrtdResponseApdu parsed;
    size_t response_len =
        emrtd_test_hex("990290008E08FA855A5D4C50A8ED9000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, response_len, plain, sizeof(plain), &parsed),
        EmrtdErrorNone);
    TEST_EQ_INT(parsed.sw, 0x9000);
    TEST_EQ_INT(parsed.data_len, 0);

    emrtd_test_begin("appendix D.4: the first READ BINARY and its four bytes");
    read_binary(&command, 0, 4);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(out, out_len, "0CB000000D9701048E08ED6705417E96BA5500");

    response_len = emrtd_test_hex(
        "8709019FF0EC34F9922651990290008E08AD55CC17140B2DED9000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, response_len, plain, sizeof(plain), &parsed),
        EmrtdErrorNone);
    TEST_EQ_INT(parsed.sw, 0x9000);
    TEST_EQ_HEX(parsed.data, parsed.data_len, "60145F01");

    emrtd_test_begin("appendix D.4: the second READ BINARY and the rest of EF.COM");
    read_binary(&command, 4, 18);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(out, out_len, "0CB000040D9701128E082EA28A70F3C7B53500");

    response_len = emrtd_test_hex(
        "871901FB9235F4E4037F2327DCC8964F1F9B8C30F42C8E2FFF224A990290008E08C8B2787EAEA07D749000",
        response,
        sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, response_len, plain, sizeof(plain), &parsed),
        EmrtdErrorNone);
    TEST_EQ_HEX(parsed.data, parsed.data_len, "04303130365F36063034303030305C026175");

    emrtd_sm_clear(&sm);
}

static void test_unprotect_over_the_response_buffer(void) {
    emrtd_test_begin("a response can be decrypted over itself");

    /*
     * The worker reads into one buffer and hands the same one back as the
     * destination, which keeps a second kilobyte off an already small heap.
     * This is the appendix D.4 read again, with the buffers aliased.
     */
    EmrtdSm sm;
    d4_session(&sm);

    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    const uint8_t fid[2] = {0x01, 0x1E};
    EmrtdCommandApdu command;
    select_ef_com(&command, fid);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);

    uint8_t buffer[EMRTD_APDU_MAX_SIZE];
    EmrtdResponseApdu parsed;
    size_t len = emrtd_test_hex("990290008E08FA855A5D4C50A8ED9000", buffer, sizeof(buffer));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, buffer, len, buffer, sizeof(buffer), &parsed), EmrtdErrorNone);

    read_binary(&command, 0, 4);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    len = emrtd_test_hex(
        "8709019FF0EC34F9922651990290008E08AD55CC17140B2DED9000", buffer, sizeof(buffer));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, buffer, len, buffer, sizeof(buffer), &parsed), EmrtdErrorNone);
    TEST_EQ_HEX(parsed.data, parsed.data_len, "60145F01");

    emrtd_sm_clear(&sm);
}

/* An AES-128 session with obvious keys, for the tests that only need framing. */
static void aes_session(EmrtdSm* sm) {
    uint8_t ks_enc[16];
    uint8_t ks_mac[16];
    uint8_t ssc[16];
    for(size_t i = 0; i < 16; i++) {
        ks_enc[i] = (uint8_t)i;
        ks_mac[i] = (uint8_t)(16 + i);
    }
    memset(ssc, 0, sizeof(ssc));
    emrtd_sm_init(sm, EmrtdCipherAes128, ks_enc, ks_mac, ssc);
}

static void test_aes_framing(void) {
    emrtd_test_begin("a protected command raises the secure messaging class bit");

    EmrtdSm sm;
    aes_session(&sm);

    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    const uint8_t fid[2] = {0x01, 0x1E};
    EmrtdCommandApdu command;
    select_ef_com(&command, fid);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_CHECK((out[0] & 0x0C) == 0x0C);
    /* The answer needs room for an envelope of its own, so Le is always 256. */
    TEST_EQ_INT(out[out_len - 1], 0x00);

    emrtd_test_begin("chaining survives the envelope");
    /* PACE sends its General Authenticate commands chained; CLA 10 becomes 1C. */
    command.cla = 0x10;
    command.ins = 0x86;
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_INT(out[0], 0x1C);

    emrtd_test_begin("the same counter always produces the same command");
    EmrtdSm first;
    EmrtdSm second;
    uint8_t out_a[EMRTD_APDU_MAX_SIZE];
    uint8_t out_b[EMRTD_APDU_MAX_SIZE];
    size_t len_a = 0;
    size_t len_b = 0;
    aes_session(&first);
    aes_session(&second);
    read_binary(&command, 0, 4);
    TEST_EQ_INT(emrtd_sm_protect(&first, &command, out_a, sizeof(out_a), &len_a), EmrtdErrorNone);
    TEST_EQ_INT(emrtd_sm_protect(&second, &command, out_b, sizeof(out_b), &len_b), EmrtdErrorNone);
    TEST_EQ_INT(len_a, len_b);
    TEST_CHECK(memcmp(out_a, out_b, len_a) == 0);

    emrtd_sm_clear(&sm);
    emrtd_sm_clear(&first);
    emrtd_sm_clear(&second);
}

static void test_counter(void) {
    emrtd_test_begin("the counter advances once per command");

    EmrtdSm sm;
    aes_session(&sm);
    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    EmrtdCommandApdu command;
    read_binary(&command, 0, 4);

    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000001");
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000002");

    emrtd_test_begin("the counter carries across every byte and wraps");
    uint8_t ks[16];
    uint8_t ssc[16];
    memset(ks, 0, sizeof(ks));
    memset(ssc, 0xFF, sizeof(ssc));
    ssc[15] = 0xFE;
    emrtd_sm_init(&sm, EmrtdCipherAes128, ks, ks, ssc);
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(sm.ssc, 16, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000000");

    emrtd_test_begin("a 3DES session keeps an eight byte counter");
    EmrtdSm tdes;
    d4_session(&tdes);
    TEST_EQ_INT(emrtd_sm_protect(&tdes, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(tdes.ssc, 8, "887022120C06C227");

    emrtd_sm_clear(&sm);
    emrtd_sm_clear(&tdes);
}

static void test_refusals(void) {
    emrtd_test_begin("a response with no checksum names the card's status word");

    EmrtdSm sm;
    aes_session(&sm);
    uint8_t response[EMRTD_APDU_MAX_SIZE];
    uint8_t plain[EMRTD_APDU_MAX_SIZE];
    EmrtdResponseApdu parsed;

    /*
     * 6A82 arriving in the clear is what a chip sends when the file is simply
     * not there. Reporting it as a missing file rather than as a broken
     * session is the difference between a useful message and a dead end.
     */
    size_t len = emrtd_test_hex("6A82", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorFileNotFound);
    TEST_EQ_INT(parsed.sw, 0x6A82);
    /* The counter is untouched, so the caller may still use the session. */
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000000");

    emrtd_test_begin("a successful response without a checksum is still refused");
    len = emrtd_test_hex("990290009000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("a forged checksum is refused");
    len = emrtd_test_hex("990290008E080000000000000000 9000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("a response too short to hold a status word is refused");
    len = emrtd_test_hex("90", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&sm, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("a checksum that is not the last object is refused");
    /*
     * Were a trailing object accepted, a chip could append whatever it liked
     * behind the checksum and have it read as part of the answer.
     */
    EmrtdSm d4;
    d4_session(&d4);
    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    const uint8_t fid[2] = {0x01, 0x1E};
    EmrtdCommandApdu command;
    select_ef_com(&command, fid);
    TEST_EQ_INT(emrtd_sm_protect(&d4, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    len = emrtd_test_hex("990290008E08FA855A5D4C50A8ED 990200009000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&d4, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("an unknown padding indicator is refused before decryption");
    d4_session(&d4);
    select_ef_com(&command, fid);
    TEST_EQ_INT(emrtd_sm_protect(&d4, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    len = emrtd_test_hex("990290008E08FA855A5D4C50A8ED9000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&d4, response, len, plain, sizeof(plain), &parsed), EmrtdErrorNone);
    read_binary(&command, 0, 4);
    TEST_EQ_INT(emrtd_sm_protect(&d4, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    /* The same response as appendix D.4, with indicator 02 in place of 01. */
    len = emrtd_test_hex(
        "8709029FF0EC34F9922651990290008E08AD55CC17140B2DED9000", response, sizeof(response));
    TEST_EQ_INT(
        emrtd_sm_unprotect(&d4, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("an unopened session refuses to work");
    EmrtdSm empty;
    memset(&empty, 0, sizeof(empty));
    TEST_EQ_INT(
        emrtd_sm_protect(&empty, &command, out, sizeof(out), &out_len), EmrtdErrorSecureMessaging);
    TEST_EQ_INT(
        emrtd_sm_unprotect(&empty, response, len, plain, sizeof(plain), &parsed),
        EmrtdErrorSecureMessaging);

    emrtd_test_begin("a buffer too small leaves the counter where it was");
    /*
     * The counter may only move when a command really goes to the chip. Were
     * it advanced before the buffer was measured, a caller that recovered from
     * the refusal would find the session silently dead.
     */
    aes_session(&sm);
    uint8_t tiny[8];
    TEST_EQ_INT(
        emrtd_sm_protect(&sm, &command, tiny, sizeof(tiny), &out_len), EmrtdErrorBufferTooSmall);
    TEST_EQ_INT(out_len, 0);
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000000");
    /* The session is still usable afterwards. */
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    TEST_EQ_HEX(sm.ssc, 16, "00000000000000000000000000000001");

    emrtd_sm_clear(&sm);
    emrtd_sm_clear(&d4);
}

static void test_keys_are_wiped(void) {
    emrtd_test_begin("clearing a session leaves no key material behind");

    EmrtdSm sm;
    d4_session(&sm);
    TEST_CHECK(sm.established);
    emrtd_sm_clear(&sm);
    TEST_CHECK(!sm.established);

    uint8_t zero[EMRTD_KEY_MAX_SIZE];
    memset(zero, 0, sizeof(zero));
    TEST_CHECK(memcmp(sm.ks_enc, zero, sizeof(sm.ks_enc)) == 0);
    TEST_CHECK(memcmp(sm.ks_mac, zero, sizeof(sm.ks_mac)) == 0);
    TEST_CHECK(memcmp(sm.ssc, zero, sizeof(sm.ssc)) == 0);

    emrtd_test_begin("a session cannot be opened on a cipher that does not exist");
    uint8_t key[EMRTD_KEY_MAX_SIZE];
    memset(key, 0x11, sizeof(key));
    emrtd_sm_init(&sm, EmrtdCipherCount, key, key, NULL);
    TEST_CHECK(!sm.established);
}

static void test_overhead(void) {
    emrtd_test_begin("the envelope's cost is reported without understating it");

    EmrtdSm sm;
    aes_session(&sm);

    /* No data: DO'97' and DO'8E' only. */
    TEST_EQ_INT(emrtd_sm_command_overhead(&sm, 0), 3 + 10);

    /*
     * Two bytes of data grow into a sixteen byte cryptogram plus the tag, the
     * length and the padding indicator, so the envelope adds 19 - 2 on top of
     * DO'97' and DO'8E'.
     */
    TEST_EQ_INT(emrtd_sm_command_overhead(&sm, 2), (3 + 16 - 2) + 3 + 10);

    /* Past 0x80 the length field grows a byte, and the report grows with it. */
    TEST_EQ_INT(emrtd_sm_command_overhead(&sm, 128), (4 + 144 - 128) + 3 + 10);

    EmrtdSm tdes;
    d4_session(&tdes);
    TEST_EQ_INT(emrtd_sm_command_overhead(&tdes, 2), (3 + 8 - 2) + 3 + 10);

    /* A command really does fit in what the report promises. */
    uint8_t data[64];
    memset(data, 0x5A, sizeof(data));
    const EmrtdCommandApdu command = {
        .cla = 0x00,
        .ins = 0xA4,
        .p1 = 0x02,
        .p2 = 0x0C,
        .data = data,
        .data_len = sizeof(data),
        .le = EMRTD_LE_MAX,
    };
    uint8_t out[EMRTD_APDU_MAX_SIZE];
    size_t out_len = 0;
    TEST_EQ_INT(emrtd_sm_protect(&sm, &command, out, sizeof(out), &out_len), EmrtdErrorNone);
    /* Four header bytes, one Lc, the envelope around the data, and one Le. */
    TEST_EQ_INT(out_len, 4 + 1 + sizeof(data) + emrtd_sm_command_overhead(&sm, sizeof(data)) + 1);

    emrtd_sm_clear(&sm);
    emrtd_sm_clear(&tdes);
}

void test_suite_sm(void) {
    test_appendix_d4();
    test_unprotect_over_the_response_buffer();
    test_aes_framing();
    test_counter();
    test_refusals();
    test_keys_are_wiped();
    test_overhead();
}
