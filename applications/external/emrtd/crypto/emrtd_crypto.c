/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_crypto.h"

size_t emrtd_cipher_key_size(EmrtdCipher cipher) {
    switch(cipher) {
    case EmrtdCipherTdes:
        return 16;
    case EmrtdCipherAes128:
        return 16;
    case EmrtdCipherAes192:
        return 24;
    case EmrtdCipherAes256:
        return 32;
    default:
        return 0;
    }
}

size_t emrtd_cipher_block_size(EmrtdCipher cipher) {
    return cipher == EmrtdCipherTdes ? 8 : 16;
}

const char* emrtd_cipher_name(EmrtdCipher cipher) {
    switch(cipher) {
    case EmrtdCipherTdes:
        return "3DES";
    case EmrtdCipherAes128:
        return "AES-128";
    case EmrtdCipherAes192:
        return "AES-192";
    case EmrtdCipherAes256:
        return "AES-256";
    default:
        return "?";
    }
}
