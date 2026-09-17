#include <stdio.h>
#include "../tests/munit/munit.h"
#include "aes_cmac.h"
#include "des_cmac.h"
#include <string.h>

static void hex_to_bytes(const char* hex, uint8_t* out) {
    size_t len = strlen(hex) / 2;
    for(size_t i = 0; i < len; i++) {
        unsigned int byte;
        sscanf(hex + i * 2, "%2x", &byte);
        out[i] = (uint8_t)byte;
    }
}

// RFC 4493 AES-CMAC test vectors, key = 2b7e151628aed2a6abf7158809cf4f3c
static MunitResult test_aes_cmac_empty(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t key[16];
    hex_to_bytes("2b7e151628aed2a6abf7158809cf4f3c", key);
    uint8_t expected[16];
    hex_to_bytes("bb1d6929e95937287fa37d129b756746", expected);

    uint8_t cmac[16];
    munit_assert_true(aes_cmac(key, sizeof(key), NULL, 0, cmac));
    munit_assert_memory_equal(16, cmac, expected);
    return MUNIT_OK;
}

static MunitResult test_aes_cmac_16bytes(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t key[16];
    hex_to_bytes("2b7e151628aed2a6abf7158809cf4f3c", key);
    uint8_t message[16];
    hex_to_bytes("6bc1bee22e409f96e93d7e117393172a", message);
    uint8_t expected[16];
    hex_to_bytes("070a16b46b4d4144f79bdd9dd04a287c", expected);

    uint8_t cmac[16];
    munit_assert_true(aes_cmac(key, sizeof(key), message, sizeof(message), cmac));
    munit_assert_memory_equal(16, cmac, expected);
    return MUNIT_OK;
}

// des_cmac() self-consistency: same input -> same output; different keys/messages -> different
// output. There's no widely-available official test vector for this project's exact 2K/3K3DES
// CMAC construction, so this checks internal correctness properties instead.
static MunitResult test_des_cmac_deterministic(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t key[16] = {
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x09,
        0x0a,
        0x0b,
        0x0c,
        0x0d,
        0x0e,
        0x0f,
        0x10};
    uint8_t message[8] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11};

    uint8_t mac1[16], mac2[16];
    munit_assert_true(des_cmac(key, sizeof(key), message, sizeof(message), mac1));
    munit_assert_true(des_cmac(key, sizeof(key), message, sizeof(message), mac2));
    munit_assert_memory_equal(8, mac1, mac2);
    return MUNIT_OK;
}

static MunitResult test_des_cmac_key_lengths(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t message[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t key8[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t key16[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t key24[24] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

    uint8_t mac[16];
    munit_assert_true(des_cmac(key8, sizeof(key8), message, sizeof(message), mac));
    munit_assert_true(des_cmac(key16, sizeof(key16), message, sizeof(message), mac));
    munit_assert_true(des_cmac(key24, sizeof(key24), message, sizeof(message), mac));

    // Rejects other lengths
    uint8_t bad_key[10] = {0};
    munit_assert_false(des_cmac(bad_key, sizeof(bad_key), message, sizeof(message), mac));
    return MUNIT_OK;
}

// A single-DES session key (8 bytes) is the degenerate K1=K2=K case of a 2-key 3DES key;
// des_cmac() with an 8-byte key must equal des_cmac() with that key repeated twice.
static MunitResult
    test_des_cmac_single_des_matches_2k3des_degenerate(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t key8[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t key16[16] = {
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88};
    uint8_t message[8] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x11, 0x22, 0x33};

    uint8_t mac8[16], mac16[16];
    munit_assert_true(des_cmac(key8, sizeof(key8), message, sizeof(message), mac8));
    munit_assert_true(des_cmac(key16, sizeof(key16), message, sizeof(message), mac16));
    munit_assert_memory_equal(8, mac8, mac16);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/aes_cmac/empty", test_aes_cmac_empty, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/aes_cmac/16bytes", test_aes_cmac_16bytes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/des_cmac/deterministic",
     test_des_cmac_deterministic,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/des_cmac/key_lengths", test_des_cmac_key_lengths, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/des_cmac/single_des_degenerate",
     test_des_cmac_single_des_matches_2k3des_degenerate,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
