/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Cipher suites used by eMRTD access control and Secure Messaging.
 * ICAO Doc 9303 part 11, section 9.7 and 9.8.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The symmetric cipher family a session runs on. */
typedef enum {
    EmrtdCipherTdes, /**< Two key 3DES, Retail MAC. Used by BAC. */
    EmrtdCipherAes128, /**< AES-128 CBC, AES-CMAC. */
    EmrtdCipherAes192, /**< AES-192 CBC, AES-CMAC. */
    EmrtdCipherAes256, /**< AES-256 CBC, AES-CMAC. */
    EmrtdCipherCount,
} EmrtdCipher;

/** Longest key any supported cipher needs. */
#define EMRTD_KEY_MAX_SIZE   32
/** Longest block any supported cipher uses, and therefore the largest SSC. */
#define EMRTD_BLOCK_MAX_SIZE 16
/** Secure Messaging always truncates the checksum to eight bytes. */
#define EMRTD_MAC_SIZE       8

/** Key length in bytes. */
size_t emrtd_cipher_key_size(EmrtdCipher cipher);

/** Block length in bytes; also the size of the Send Sequence Counter (9.8.2). */
size_t emrtd_cipher_block_size(EmrtdCipher cipher);

/** Human readable name, for the UI and the export report. */
const char* emrtd_cipher_name(EmrtdCipher cipher);

#ifdef __cplusplus
}
#endif
