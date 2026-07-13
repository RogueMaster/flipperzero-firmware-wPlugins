#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZK_KEY_SIZE 32
#define ZK_NONCE_SIZE 12
#define ZK_TAG_SIZE 16

void zk_crypto_derive_device_key(uint8_t key[ZK_KEY_SIZE]);

void zk_crypto_seal(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* plain,
    size_t length,
    uint8_t* cipher,
    uint8_t tag[ZK_TAG_SIZE]);

bool zk_crypto_open(
    const uint8_t key[ZK_KEY_SIZE],
    const uint8_t nonce[ZK_NONCE_SIZE],
    const uint8_t* cipher,
    size_t length,
    const uint8_t tag[ZK_TAG_SIZE],
    uint8_t* plain);

void zk_crypto_wipe(void* data, size_t length);

