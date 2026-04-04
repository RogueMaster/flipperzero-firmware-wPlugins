/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <furi.h>
#include "hmac.h"
#include "memzero.h"

void hmac_sha256_Init(HMAC_SHA256_CTX* hctx, const uint8_t* key, const uint32_t keylen) {
    static uint8_t i_key_pad[SHA256_BLOCK_LENGTH];
    memzero(i_key_pad, SHA256_BLOCK_LENGTH);
    if(keylen > SHA256_BLOCK_LENGTH) {
        sha256_Raw(key, keylen, i_key_pad);
    } else {
        memcpy(i_key_pad, key, keylen);
    }
    for(int i = 0; i < SHA256_BLOCK_LENGTH; i++) {
        hctx->o_key_pad[i] = i_key_pad[i] ^ 0x5c;
        i_key_pad[i] ^= 0x36;
    }
    sha256_Init(&(hctx->ctx));
    sha256_Update(&(hctx->ctx), i_key_pad, SHA256_BLOCK_LENGTH);
    memzero(i_key_pad, sizeof(i_key_pad));
}

void hmac_sha256_Update(HMAC_SHA256_CTX* hctx, const uint8_t* msg, const uint32_t msglen) {
    sha256_Update(&(hctx->ctx), msg, msglen);
}

void hmac_sha256_Final(HMAC_SHA256_CTX* hctx, uint8_t* hmac) {
    sha256_Final(&(hctx->ctx), hmac);
    sha256_Init(&(hctx->ctx));
    sha256_Update(&(hctx->ctx), hctx->o_key_pad, SHA256_BLOCK_LENGTH);
    sha256_Update(&(hctx->ctx), hmac, SHA256_DIGEST_LENGTH);
    sha256_Final(&(hctx->ctx), hmac);
    memzero(hctx, sizeof(HMAC_SHA256_CTX));
}

void hmac_sha256(
    const uint8_t* key,
    const uint32_t keylen,
    const uint8_t* msg,
    const uint32_t msglen,
    uint8_t* hmac) {
    static HMAC_SHA256_CTX hctx;
    hmac_sha256_Init(&hctx, key, keylen);
    hmac_sha256_Update(&hctx, msg, msglen);
    hmac_sha256_Final(&hctx, hmac);
}

void hmac_sha256_prepare(
    const uint8_t* key,
    const uint32_t keylen,
    uint32_t* opad_digest,
    uint32_t* ipad_digest) {
    UNUSED(key);
    UNUSED(keylen);
    UNUSED(opad_digest);
    UNUSED(ipad_digest);
    // empty function
}

void hmac_sha512_Init(HMAC_SHA512_CTX* hctx, const uint8_t* key, const uint32_t keylen) {
    static uint8_t i_key_pad[SHA512_BLOCK_LENGTH];
    memzero(i_key_pad, SHA512_BLOCK_LENGTH);
    if(keylen > SHA512_BLOCK_LENGTH) {
        sha512_Raw(key, keylen, i_key_pad);
    } else {
        memcpy(i_key_pad, key, keylen);
    }
    for(int i = 0; i < SHA512_BLOCK_LENGTH; i++) {
        hctx->o_key_pad[i] = i_key_pad[i] ^ 0x5c;
        i_key_pad[i] ^= 0x36;
    }
    sha512_Init(&(hctx->ctx));
    sha512_Update(&(hctx->ctx), i_key_pad, SHA512_BLOCK_LENGTH);
    memzero(i_key_pad, sizeof(i_key_pad));
}

void hmac_sha512_Update(HMAC_SHA512_CTX* hctx, const uint8_t* msg, const uint32_t msglen) {
    sha512_Update(&(hctx->ctx), msg, msglen);
}

void hmac_sha512_Final(HMAC_SHA512_CTX* hctx, uint8_t* hmac) {
    sha512_Final(&(hctx->ctx), hmac);
    sha512_Init(&(hctx->ctx));
    sha512_Update(&(hctx->ctx), hctx->o_key_pad, SHA512_BLOCK_LENGTH);
    sha512_Update(&(hctx->ctx), hmac, SHA512_DIGEST_LENGTH);
    sha512_Final(&(hctx->ctx), hmac);
    memzero(hctx, sizeof(HMAC_SHA512_CTX));
}

void hmac_sha512(
    const uint8_t* key,
    const uint32_t keylen,
    const uint8_t* msg,
    const uint32_t msglen,
    uint8_t* hmac) {
    HMAC_SHA512_CTX hctx = {0};
    hmac_sha512_Init(&hctx, key, keylen);
    hmac_sha512_Update(&hctx, msg, msglen);
    hmac_sha512_Final(&hctx, hmac);
}

void hmac_sha512_prepare(
    const uint8_t* key,
    const uint32_t keylen,
    uint64_t* opad_digest,
    uint64_t* ipad_digest) {
    UNUSED(key);
    UNUSED(keylen);
    UNUSED(opad_digest);
    UNUSED(ipad_digest);
    // empty function
}
