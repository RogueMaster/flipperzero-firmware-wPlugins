#include "zk_crypto.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t test_uid[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

const uint8_t* furi_hal_version_uid(void) {
    return test_uid;
}

size_t furi_hal_version_uid_size(void) {
    return sizeof(test_uid);
}

static void test_key_derivation(void) {
    static const uint8_t expected[ZK_KEY_SIZE] = {
        0xc3, 0x2f, 0x42, 0xe1, 0xde, 0xe7, 0x64, 0x24,
        0x0b, 0xb7, 0xb8, 0x8e, 0x2b, 0x36, 0x1d, 0xb0,
        0x30, 0xbd, 0x28, 0x53, 0x4d, 0xe5, 0x9b, 0xed,
        0xde, 0xa6, 0x4e, 0xcc, 0x47, 0x8e, 0x34, 0x75,
    };
    uint8_t key[ZK_KEY_SIZE];
    zk_crypto_derive_device_key(key);
    assert(memcmp(key, expected, sizeof(key)) == 0);
}

static void test_seal_open_and_tamper(void) {
    uint8_t key[ZK_KEY_SIZE];
    uint8_t nonce[ZK_NONCE_SIZE] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const uint8_t plain[] = "Tr0ub4dor&correct-horse";
    uint8_t cipher[sizeof(plain) - 1];
    uint8_t opened[sizeof(plain) - 1];
    uint8_t tag[ZK_TAG_SIZE];

    zk_crypto_derive_device_key(key);
    zk_crypto_seal(key, nonce, plain, sizeof(plain) - 1, cipher, tag);
    assert(memcmp(cipher, plain, sizeof(cipher)) != 0);
    assert(zk_crypto_open(key, nonce, cipher, sizeof(cipher), tag, opened));
    assert(memcmp(opened, plain, sizeof(opened)) == 0);

    cipher[3] ^= 0x80;
    memset(opened, 0xA5, sizeof(opened));
    assert(!zk_crypto_open(key, nonce, cipher, sizeof(cipher), tag, opened));
    for(size_t i = 0; i < sizeof(opened); i++) assert(opened[i] == 0xA5);
}

static void test_wipe(void) {
    uint8_t secret[17];
    memset(secret, 0x5A, sizeof(secret));
    zk_crypto_wipe(secret, sizeof(secret));
    for(size_t i = 0; i < sizeof(secret); i++) assert(secret[i] == 0);
}

int main(void) {
    test_key_derivation();
    test_seal_open_and_tamper();
    test_wipe();
    puts("crypto tests passed");
    return 0;
}
