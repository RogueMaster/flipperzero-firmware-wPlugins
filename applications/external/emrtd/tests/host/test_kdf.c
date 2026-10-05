/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Key derivation, ICAO Doc 9303 part 11 section 9.7.
 * Vectors from appendix D.1 (3DES) and appendix G.1 (AES, PACE).
 */

#include "emrtd_test.h"

#include "../../crypto/emrtd_kdf.h"

void test_suite_kdf(void) {
    uint8_t secret[32];
    uint8_t enc[EMRTD_KEY_MAX_SIZE];
    uint8_t mac[EMRTD_KEY_MAX_SIZE];

    emrtd_test_begin("3DES keys from Kseed, appendix D.1");
    size_t len = emrtd_test_hex("239AB9CB282DAF66231DC5A4DF6BFBAE", secret, sizeof(secret));
    TEST_CHECK(emrtd_kdf_enc_mac(EmrtdCipherTdes, secret, len, enc, mac));
    TEST_EQ_HEX(enc, 16, "AB94FDECF2674FDFB9B391F85D7F76F2");
    TEST_EQ_HEX(mac, 16, "7962D9ECE03D1ACD4C76089DCE131543");

    emrtd_test_begin("3DES session keys from K.IFD xor K.IC, appendix D.3");
    /* K.IFD 0B795240... xor K.IC 0B4F8032... */
    len = emrtd_test_hex("0036D272F5C350ACAC50C3F572D23600", secret, sizeof(secret));
    TEST_CHECK(emrtd_kdf_enc_mac(EmrtdCipherTdes, secret, len, enc, mac));
    TEST_EQ_HEX(enc, 16, "979EC13B1CBFE9DCD01AB0FED307EAE5");
    TEST_EQ_HEX(mac, 16, "F1CB1F1FB5ADF208806B89DC579DC1F8");

    emrtd_test_begin("AES-128 session keys from the PACE shared secret, appendix G.1");
    len = emrtd_test_hex(
        "28768D20701247DAE81804C9E780EDE582A9996DB4A315020B2733197DB84925",
        secret,
        sizeof(secret));
    TEST_CHECK(emrtd_kdf_enc_mac(EmrtdCipherAes128, secret, len, enc, mac));
    TEST_EQ_HEX(enc, 16, "F5F0E35C0D7161EE6724EE513A0D9A7F");
    TEST_EQ_HEX(mac, 16, "FE251C7858B356B24514B3BD5F4297D1");

    emrtd_test_begin("the PACE password key uses counter 3");
    /* Kpi for the MRZ of the appendix G example. */
    len = emrtd_test_hex("7E2D2A41C74EA0B38CD36F863939BFA8E9032AAD", secret, sizeof(secret));
    TEST_CHECK(emrtd_kdf(EmrtdCipherAes128, secret, len, EMRTD_KDF_COUNTER_PACE, enc));
    TEST_EQ_HEX(enc, 16, "89DED1B26624EC1E634C1989302849DD");

    emrtd_test_begin("AES-192 and AES-256 switch the hash to SHA-256");
    len = emrtd_test_hex(
        "28768D20701247DAE81804C9E780EDE582A9996DB4A315020B2733197DB84925",
        secret,
        sizeof(secret));
    TEST_CHECK(emrtd_kdf(EmrtdCipherAes192, secret, len, EMRTD_KDF_COUNTER_ENC, enc));
    TEST_CHECK(emrtd_kdf(EmrtdCipherAes256, secret, len, EMRTD_KDF_COUNTER_ENC, mac));
    /* The first 24 bytes of the 256 bit derivation are the 192 bit key. */
    TEST_CHECK(memcmp(enc, mac, 24) == 0);
    TEST_EQ_INT(emrtd_cipher_key_size(EmrtdCipherAes192), 24);
    TEST_EQ_INT(emrtd_cipher_key_size(EmrtdCipherAes256), 32);

    emrtd_test_begin("DES parity is adjusted to odd");
    uint8_t key[16];
    memset(key, 0x00, sizeof(key));
    emrtd_des_adjust_parity(key, sizeof(key));
    TEST_EQ_HEX(key, 16, "01010101010101010101010101010101");
    memset(key, 0xFF, sizeof(key));
    emrtd_des_adjust_parity(key, sizeof(key));
    TEST_EQ_HEX(key, 16, "FEFEFEFEFEFEFEFEFEFEFEFEFEFEFEFE");

    emrtd_test_begin("block and SSC sizes follow the cipher");
    TEST_EQ_INT(emrtd_cipher_block_size(EmrtdCipherTdes), 8);
    TEST_EQ_INT(emrtd_cipher_block_size(EmrtdCipherAes128), 16);
    TEST_EQ_STR(emrtd_cipher_name(EmrtdCipherAes256), "AES-256");
}
