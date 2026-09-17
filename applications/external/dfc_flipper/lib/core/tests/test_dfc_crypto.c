#include "munit.h"

#include "dfc_crypto.h"

#include <string.h>

static MunitResult test_aes_cbc_vector(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    const uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    const uint8_t clear[16] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
    const uint8_t expected[16] = {
        0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
        0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d};
    const uint8_t initial_iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t iv[16];
    uint8_t buffer[16];

    memcpy(iv, initial_iv, sizeof(iv));
    memcpy(buffer, clear, sizeof(buffer));
    munit_assert_true(dfc_crypto_aes_cbc(true, key, sizeof(key), iv, buffer, buffer, sizeof(buffer)));
    munit_assert_memory_equal(sizeof(expected), buffer, expected);
    munit_assert_memory_equal(sizeof(expected), iv, expected);

    memcpy(iv, initial_iv, sizeof(iv));
    munit_assert_true(dfc_crypto_aes_cbc(false, key, sizeof(key), iv, buffer, buffer, sizeof(buffer)));
    munit_assert_memory_equal(sizeof(clear), buffer, clear);
    return MUNIT_OK;
}

static MunitResult test_des_ecb_vector(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    const uint8_t key[8] = {0x13, 0x34, 0x57, 0x79, 0x9b, 0xbc, 0xdf, 0xf1};
    const uint8_t clear[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    const uint8_t expected[8] = {0x85, 0xe8, 0x13, 0x54, 0x0f, 0x0a, 0xb4, 0x05};
    uint8_t buffer[8];

    munit_assert_true(dfc_crypto_des_ecb(true, key, sizeof(key), clear, buffer));
    munit_assert_memory_equal(sizeof(expected), buffer, expected);
    munit_assert_true(dfc_crypto_des_ecb(false, key, sizeof(key), buffer, buffer));
    munit_assert_memory_equal(sizeof(clear), buffer, clear);
    return MUNIT_OK;
}

static MunitResult test_rejects_invalid_lengths(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t key[24] = {0};
    uint8_t iv[16] = {0};
    uint8_t buffer[16] = {0};

    munit_assert_false(dfc_crypto_aes_cbc(true, key, 24, iv, buffer, buffer, 16));
    munit_assert_false(dfc_crypto_aes_cbc(true, key, 16, iv, buffer, buffer, 15));
    munit_assert_false(dfc_crypto_des_cbc(true, key, 12, iv, buffer, buffer, 8));
    munit_assert_false(dfc_crypto_des_cbc(true, key, 8, iv, buffer, buffer, 7));
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {(char*)"/aes-cbc-vector", test_aes_cbc_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des-ecb-vector", test_des_ecb_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/invalid-lengths", test_rejects_invalid_lengths, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {(char*)"/crypto", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
