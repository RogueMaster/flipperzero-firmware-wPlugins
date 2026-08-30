/* CMAC against known answers.
 *
 * The keys and the message are ours. The expected values were produced by two
 * implementations written separately from this one and compared, so a change
 * here that alters the result will show up rather than being recorded.
 */
#include "munit.h"
#include "test_helpers.h"

#include <cmac.h>

/* Cases take a prefix of this. */
static const char* sample_message = "05162738495a6b7c8d9eafc0d1e2f304"
                                    "15263748596a7b8c9daebfd0e1f20314"
                                    "25364758697a8b9cadbecfe0f1021324"
                                    "35465768798a9bacbdcedff001122334";

static void check_aes(const char* key_hex, size_t message_len, const char* expected_hex) {
    uint8_t key[16];
    uint8_t message[64];
    uint8_t expected[16];
    uint8_t cmac[16];

    hex_to_bytes(key_hex, key, sizeof(key));
    hex_to_bytes(sample_message, message, sizeof(message));
    hex_to_bytes(expected_hex, expected, sizeof(expected));

    munit_assert_true(aes_cmac(key, sizeof(key), message, message_len, cmac));
    munit_assert_memory_equal(sizeof(expected), cmac, expected);
}

static void check_des(const char* key_hex, size_t message_len, const char* expected_hex) {
    uint8_t key[16];
    uint8_t message[64];
    uint8_t expected[8];
    uint8_t cmac[8];

    hex_to_bytes(key_hex, key, sizeof(key));
    hex_to_bytes(sample_message, message, sizeof(message));
    hex_to_bytes(expected_hex, expected, sizeof(expected));

    munit_assert_true(des_cmac(key, sizeof(key), message, message_len, cmac));
    munit_assert_memory_equal(sizeof(expected), cmac, expected);
}

#define AES_KEY "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
#define DES_KEY "0123456789abcdeffedcba9876543210"

static MunitResult test_aes_empty(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_aes(AES_KEY, 0, "7b18d0e09efd73767544e5342f503faf");
    return MUNIT_OK;
}

static MunitResult test_aes_one_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_aes(AES_KEY, 16, "4ca6e56b374f1957235ef809b553564b");
    return MUNIT_OK;
}

/* A partial final block, which takes the padding path. */
static MunitResult test_aes_partial_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_aes(AES_KEY, 37, "59268dcffe5b24283a6ae3e8a382803b");
    return MUNIT_OK;
}

static MunitResult test_aes_four_blocks(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_aes(AES_KEY, 64, "51d6d6fe3f93a3156a2c6f1f491509b0");
    return MUNIT_OK;
}

static MunitResult test_aes_rejects_short_key(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[8] = {0};
    uint8_t cmac[16];
    munit_assert_false(aes_cmac(key, sizeof(key), NULL, 0, cmac));
    return MUNIT_OK;
}

static MunitResult test_des_empty(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_des(DES_KEY, 0, "5b560372570d37cb");
    return MUNIT_OK;
}

static MunitResult test_des_partial_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_des(DES_KEY, 5, "79593c2f9225a934");
    return MUNIT_OK;
}

static MunitResult test_des_one_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_des(DES_KEY, 8, "767c0c98aee37131");
    return MUNIT_OK;
}

static MunitResult test_des_three_blocks(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_des(DES_KEY, 24, "03a93d3a167b7499");
    return MUNIT_OK;
}

static MunitResult test_des_rejects_short_key(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[8] = {0};
    uint8_t cmac[8];
    munit_assert_false(des_cmac(key, sizeof(key), NULL, 0, cmac));
    return MUNIT_OK;
}

static MunitTest test_cmac_cases[] = {
    {(char*)"/aes/empty", test_aes_empty, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aes/one-block", test_aes_one_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aes/partial-block", test_aes_partial_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aes/four-blocks", test_aes_four_blocks, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/aes/short-key", test_aes_rejects_short_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des/empty", test_des_empty, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des/partial-block", test_des_partial_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des/one-block", test_des_one_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des/three-blocks", test_des_three_blocks, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/des/short-key", test_des_rejects_short_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_cmac_suite = {
    (char*)"/cmac",
    test_cmac_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
