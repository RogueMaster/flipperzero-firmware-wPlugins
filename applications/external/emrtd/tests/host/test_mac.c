/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Padding, the Retail MAC and AES-CMAC.
 *
 * Sources of the vectors:
 *   - RFC 4493 section 4, AES-128 CMAC
 *   - NIST SP 800-38B appendix D.2/D.3, AES-192 and AES-256 CMAC
 *   - ICAO Doc 9303 part 11 appendix D.3, the Retail MAC inside BAC
 */

#include "emrtd_test.h"

#include "../../crypto/emrtd_mac.h"

static void test_padding(void) {
    emrtd_test_begin("ISO 9797-1 method 2 padding is always added");

    uint8_t buf[32];
    memset(buf, 0xAA, sizeof(buf));
    size_t len = emrtd_pad_iso9797_m2(buf, 3, 8);
    TEST_EQ_INT(len, 8);
    TEST_EQ_HEX(buf, len, "AAAAAA8000000000");

    /* An already aligned input still grows by a whole block. */
    memset(buf, 0xAA, sizeof(buf));
    len = emrtd_pad_iso9797_m2(buf, 8, 8);
    TEST_EQ_INT(len, 16);
    TEST_EQ_HEX(buf, len, "AAAAAAAAAAAAAAAA80000000 00000000");

    memset(buf, 0xBB, sizeof(buf));
    len = emrtd_pad_iso9797_m2(buf, 16, 16);
    TEST_EQ_INT(len, 32);

    emrtd_test_begin("padding is removed again");
    uint8_t padded[16];
    size_t out_len = 0;
    emrtd_test_hex("AABBCC800000000000000000 00000000", padded, sizeof(padded));
    TEST_CHECK(emrtd_unpad_iso9797_m2(padded, 16, &out_len));
    TEST_EQ_INT(out_len, 3);

    emrtd_test_begin("a missing 0x80 marker is rejected");
    emrtd_test_hex("AABBCC000000000000000000 00000000", padded, sizeof(padded));
    TEST_CHECK(!emrtd_unpad_iso9797_m2(padded, 16, &out_len));
}

static void test_retail_mac(void) {
    emrtd_test_begin("Retail MAC, ICAO 9303-11 appendix D.3");

    /*
     * KMAC and the padded E.IFD of the worked BAC example; the expected
     * result is M.IFD.
     */
    uint8_t key[16];
    uint8_t data[64];
    uint8_t mac[8];
    emrtd_test_hex("7962D9ECE03D1ACD4C76089DCE131543", key, sizeof(key));
    const size_t len = emrtd_test_hex(
        "72C29C2371CC9BDB65B779B8E8D37B29ECC154AA56A8799FAE2F498F76ED92F2"
        "8000000000000000",
        data,
        sizeof(data));
    TEST_EQ_INT(len, 40);
    TEST_CHECK(emrtd_retail_mac(key, data, len, mac));
    TEST_EQ_HEX(mac, sizeof(mac), "5F1448EEA8AD90A7");

    emrtd_test_begin("Retail MAC rejects unpadded input");
    TEST_CHECK(!emrtd_retail_mac(key, data, 39, mac));
}

static void test_aes_cmac(void) {
    uint8_t key[32];
    uint8_t msg[64];
    uint8_t mac[16];

    emrtd_test_begin("AES-128 CMAC, RFC 4493");
    emrtd_test_hex("2B7E151628AED2A6ABF7158809CF4F3C", key, sizeof(key));

    /* Empty message. */
    TEST_CHECK(emrtd_aes_cmac(key, 16, msg, 0, mac, 16));
    TEST_EQ_HEX(mac, 16, "BB1D6929E95937287FA37D129B756746");

    /* One full block. */
    size_t len = emrtd_test_hex("6BC1BEE22E409F96E93D7E117393172A", msg, sizeof(msg));
    TEST_CHECK(emrtd_aes_cmac(key, 16, msg, len, mac, 16));
    TEST_EQ_HEX(mac, 16, "070A16B46B4D4144F79BDD9DD04A287C");

    /* A partial last block, which exercises the K2 branch. */
    len = emrtd_test_hex(
        "6BC1BEE22E409F96E93D7E117393172AAE2D8A571E03AC9C9EB76FAC45AF8E51"
        "30C81C46A35CE411",
        msg,
        sizeof(msg));
    TEST_CHECK(emrtd_aes_cmac(key, 16, msg, len, mac, 16));
    TEST_EQ_HEX(mac, 16, "DFA66747DE9AE63030CA32611497C827");

    /* Four full blocks. */
    len = emrtd_test_hex(
        "6BC1BEE22E409F96E93D7E117393172AAE2D8A571E03AC9C9EB76FAC45AF8E51"
        "30C81C46A35CE411E5FBC1191A0A52EFF69F2445DF4F9B17AD2B417BE66C3710",
        msg,
        sizeof(msg));
    TEST_CHECK(emrtd_aes_cmac(key, 16, msg, len, mac, 16));
    TEST_EQ_HEX(mac, 16, "51F0BEBF7E3B9D92FC49741779363CFE");

    emrtd_test_begin("AES-192 CMAC, NIST SP 800-38B D.2");
    emrtd_test_hex("8E73B0F7DA0E6452C810F32B809079E562F8EAD2522C6B7B", key, sizeof(key));
    TEST_CHECK(emrtd_aes_cmac(key, 24, msg, 0, mac, 16));
    TEST_EQ_HEX(mac, 16, "D17DDF46ADAACDE531CAC483DE7A9367");
    len = emrtd_test_hex("6BC1BEE22E409F96E93D7E117393172A", msg, sizeof(msg));
    TEST_CHECK(emrtd_aes_cmac(key, 24, msg, len, mac, 16));
    TEST_EQ_HEX(mac, 16, "9E99A7BF31E710900662F65E617C5184");

    emrtd_test_begin("AES-256 CMAC, NIST SP 800-38B D.3");
    emrtd_test_hex(
        "603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4", key, sizeof(key));
    TEST_CHECK(emrtd_aes_cmac(key, 32, msg, 0, mac, 16));
    TEST_EQ_HEX(mac, 16, "028962F61B7BF89EFC6B551F4667D983");
    len = emrtd_test_hex("6BC1BEE22E409F96E93D7E117393172A", msg, sizeof(msg));
    TEST_CHECK(emrtd_aes_cmac(key, 32, msg, len, mac, 16));
    TEST_EQ_HEX(mac, 16, "28A7023F452E8F82BD4BF28D8C37C35C");

    emrtd_test_begin("Secure Messaging truncates the CMAC to eight bytes");
    emrtd_test_hex("2B7E151628AED2A6ABF7158809CF4F3C", key, sizeof(key));
    TEST_CHECK(emrtd_aes_cmac(key, 16, msg, 0, mac, EMRTD_MAC_SIZE));
    TEST_EQ_HEX(mac, EMRTD_MAC_SIZE, "BB1D6929E9593728");
}

void test_suite_mac(void) {
    test_padding();
    test_retail_mac();
    test_aes_cmac();
}
