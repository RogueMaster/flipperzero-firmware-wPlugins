/* Session key derivation, and the freshness the session rests on. */
#include "munit.h"
#include "test_helpers.h"

#include <furi_hal.h>
#include <secure_messaging.h>

static AuthParameters derivation_params(uint8_t cipher, uint8_t hash) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = cipher;
    params.hash = hash;
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x10 + i);
    for(size_t i = 0; i < sizeof(params.UID); i++)
        params.UID[i] = (uint8_t)(0x20 + i);
    for(size_t i = 0; i < sizeof(params.cNonce); i++)
        params.cNonce[i] = (uint8_t)(0x30 + i);
    for(size_t i = 0; i < sizeof(params.rNonce); i++)
        params.rNonce[i] = (uint8_t)(0x40 + i);
    return params;
}

/* Pins the derivation against a model of it built independently, so a change
 * to the construction cannot pass unnoticed. */
static void check_keys(uint8_t cipher, uint8_t hash, const char* privacy, const char* checksum) {
    AuthParameters params = derivation_params(cipher, hash);
    SecureMessaging* sm = secure_messaging_alloc(&params);
    munit_assert_not_null(sm);

    uint8_t expected[16];
    hex_to_bytes(privacy, expected, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), sm->PrivacyKey, expected);
    hex_to_bytes(checksum, expected, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), sm->CMACKey, expected);

    secure_messaging_free(sm);
}

static MunitResult test_aes_sha256_keys(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_keys(
        AES_128_CBC,
        SHA256,
        "ed241d0479918bca12abe4cdb4747617",
        "b2b734dff1c968abdc99a4f54c7ea4c3");
    return MUNIT_OK;
}

/* SHA-1 produces twenty bytes a round, so the checksum key spans two of them. */
static MunitResult test_des_sha1_keys(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    check_keys(
        TWO_KEY_3DES_CBC_MODE,
        SHA1,
        "875eb3b1285f90dd9f9d42a223f01fff",
        "17bdaf5eec3695f533fb9cf83fb6a721");
    return MUNIT_OK;
}

/* The counter is seeded from the nonces, so it must differ between sessions. */
static MunitResult test_counter_follows_nonces(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = derivation_params(AES_128_CBC, SHA256);
    SecureMessaging* first = secure_messaging_alloc(&params);

    params.rndICC[0] ^= 0xff;
    SecureMessaging* second = secure_messaging_alloc(&params);

    munit_assert_memory_not_equal(
        sizeof(first->aesContext), first->aesContext, second->aesContext);
    munit_assert_memory_not_equal(
        sizeof(first->PrivacyKey), first->PrivacyKey, second->PrivacyKey);

    secure_messaging_free(first);
    secure_messaging_free(second);
    return MUNIT_OK;
}

/* A hash the derivation does not know left the step at zero, which spun the
 * loop forever. It must refuse instead. */
static MunitResult test_rejects_unknown_hash(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = derivation_params(AES_128_CBC, 0x42);
    munit_assert_null(secure_messaging_alloc(&params));
    return MUNIT_OK;
}

static MunitResult test_rejects_unknown_cipher(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = derivation_params(0x77, SHA256);
    munit_assert_null(secure_messaging_alloc(&params));
    return MUNIT_OK;
}

/* Nonces must come from the random source, not a constant. Two draws from a
 * source with different octets queued must differ. */
static MunitResult test_nonces_are_random(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t first[8];
    uint8_t second[8];

    uint8_t queued[] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04};
    seos_host_clear_random();
    seos_host_set_random(queued, sizeof(queued));
    seos_worker_random_nonce(first, sizeof(first));
    munit_assert_memory_equal(sizeof(queued), first, queued);
    munit_assert_size(seos_host_random_remaining(), ==, 0);

    seos_worker_random_nonce(second, sizeof(second));
    munit_assert_memory_not_equal(sizeof(first), first, second);
    return MUNIT_OK;
}

/* The cipher wrappers each end up at the same place; a round trip through
 * every one shows they agree on direction and key. */
static MunitResult test_cipher_wrappers(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t key[16];
    for(size_t i = 0; i < sizeof(key); i++)
        key[i] = (uint8_t)(0x90 + i);

    uint8_t clear[32];
    for(size_t i = 0; i < sizeof(clear); i++)
        clear[i] = (uint8_t)(i * 11 + 3);

    uint8_t encrypted[32];
    uint8_t recovered[32];

    munit_assert_true(seos_worker_aes_encrypt(key, sizeof(clear), clear, encrypted));
    munit_assert_memory_not_equal(sizeof(clear), encrypted, clear);
    munit_assert_true(seos_worker_aes_decrypt(key, sizeof(clear), encrypted, recovered));
    munit_assert_memory_equal(sizeof(clear), recovered, clear);

    munit_assert_true(seos_worker_des_encrypt(key, sizeof(clear), clear, encrypted));
    munit_assert_memory_not_equal(sizeof(clear), encrypted, clear);
    munit_assert_true(seos_worker_des_decrypt(key, sizeof(clear), encrypted, recovered));
    munit_assert_memory_equal(sizeof(clear), recovered, clear);

    /* A length that is not whole blocks is refused rather than half done. */
    munit_assert_false(seos_worker_aes_encrypt(key, 17, clear, encrypted));
    munit_assert_false(seos_worker_des_encrypt(key, 5, clear, encrypted));

    /* And a cipher nobody agreed on is refused by name. */
    munit_assert_size(seos_cipher_block_size(0x77), ==, 0);
    munit_assert_false(seos_cipher_encrypt(0x77, key, sizeof(clear), clear, encrypted));
    munit_assert_false(seos_cipher_decrypt(0x77, key, sizeof(clear), clear, recovered));
    munit_assert_false(seos_cipher_cmac(0x77, key, sizeof(key), clear, sizeof(clear), encrypted));
    return MUNIT_OK;
}

static MunitTest test_kdf_cases[] = {
    {(char*)"/keys/aes-sha256", test_aes_sha256_keys, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/keys/des-sha1", test_des_sha1_keys, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/keys/follow-nonces",
     test_counter_follows_nonces,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reject/unknown-hash",
     test_rejects_unknown_hash,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reject/unknown-cipher",
     test_rejects_unknown_cipher,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/cipher/wrappers", test_cipher_wrappers, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/nonce/random", test_nonces_are_random, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_kdf_suite = {
    (char*)"/kdf",
    test_kdf_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
