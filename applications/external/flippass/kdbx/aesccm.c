/**
 * Copyright (c) 2023 Andrew Kozlik
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

/*
 * For a specification of AES-CCM see one of the following:
 * https://datatracker.ietf.org/doc/html/rfc3610
 * https://doi.org/10.6028/NIST.SP.800-38C
 */

#include <string.h>

#include "aesccm.h"
#include "memzero.h"

typedef struct {
    const aes_encrypt_ctx* encrypt_ctx;
    union {
        // Ensure 32-bit alignment.
        uint8_t state[AES_BLOCK_SIZE];
        uint32_t state32[AES_BLOCK_SIZE / 4];
    };
    // Next position in the state where data will be added.
    // Valid values are 0 to 15.
    uint8_t pos;
} cbc_mac_context;

// WARNING: Caller must ensure that encrypt_ctx remains valid for the lifetime
// of ctx.
static void cbc_mac_init(cbc_mac_context* ctx, const aes_encrypt_ctx* encrypt_ctx) {
    memzero(ctx, sizeof(cbc_mac_context));
    ctx->encrypt_ctx = encrypt_ctx;
}

static AES_RETURN cbc_mac_update(cbc_mac_context* ctx, const uint8_t* data, size_t size) {
    if(ctx->pos != 0) {
        while(size > 0 && ctx->pos < AES_BLOCK_SIZE) {
            ctx->state[ctx->pos] ^= *data;
            ctx->pos++;
            data++;
            size--;
        }

        if(ctx->pos != AES_BLOCK_SIZE) {
            return EXIT_SUCCESS;
        }

        if(aes_encrypt(ctx->state, ctx->state, ctx->encrypt_ctx) != EXIT_SUCCESS) {
            memzero(ctx, sizeof(*ctx));
            return EXIT_FAILURE;
        }
        ctx->pos = 0;
    }

    size_t block_count = size >> AES_BLOCK_SIZE_P2;
    size %= AES_BLOCK_SIZE;

    if(!ALIGN_OFFSET(data, 4)) {
        while(block_count != 0) {
            ctx->state32[0] ^= ((uint32_t*)data)[0];
            ctx->state32[1] ^= ((uint32_t*)data)[1];
            ctx->state32[2] ^= ((uint32_t*)data)[2];
            ctx->state32[3] ^= ((uint32_t*)data)[3];
            if(aes_encrypt(ctx->state, ctx->state, ctx->encrypt_ctx) != EXIT_SUCCESS) {
                memzero(ctx, sizeof(*ctx));
                return EXIT_FAILURE;
            }
            data += AES_BLOCK_SIZE;
            block_count--;
        }
    } else {
        while(block_count != 0) {
            ctx->state[0] ^= data[0];
            ctx->state[1] ^= data[1];
            ctx->state[2] ^= data[2];
            ctx->state[3] ^= data[3];
            ctx->state[4] ^= data[4];
            ctx->state[5] ^= data[5];
            ctx->state[6] ^= data[6];
            ctx->state[7] ^= data[7];
            ctx->state[8] ^= data[8];
            ctx->state[9] ^= data[9];
            ctx->state[10] ^= data[10];
            ctx->state[11] ^= data[11];
            ctx->state[12] ^= data[12];
            ctx->state[13] ^= data[13];
            ctx->state[14] ^= data[14];
            ctx->state[15] ^= data[15];
            if(aes_encrypt(ctx->state, ctx->state, ctx->encrypt_ctx) != EXIT_SUCCESS) {
                memzero(ctx, sizeof(*ctx));
                return EXIT_FAILURE;
            }
            data += AES_BLOCK_SIZE;
            block_count--;
        }
    }

    while(size > 0) {
        ctx->state[ctx->pos] ^= *data;
        ctx->pos++;
        data++;
        size--;
    }

    return EXIT_SUCCESS;
}

static AES_RETURN cbc_mac_update_zero_padding(cbc_mac_context* ctx) {
    if(ctx->pos != 0) {
        if(aes_encrypt(ctx->state, ctx->state, ctx->encrypt_ctx) != EXIT_SUCCESS) {
            memzero(ctx, sizeof(*ctx));
            return EXIT_FAILURE;
        }
        ctx->pos = 0;
    }
    return EXIT_SUCCESS;
}

static AES_RETURN cbc_mac_final(cbc_mac_context* ctx, uint8_t* mac, size_t mac_len) {
    if(ctx->pos != 0 || mac_len > AES_BLOCK_SIZE) {
        memzero(ctx, sizeof(*ctx));
        return EXIT_FAILURE;
    }
    memcpy(mac, ctx->state, mac_len);
    memzero(ctx, sizeof(*ctx));
    return EXIT_SUCCESS;
}

static AES_RETURN aes_ccm_init(
    aes_encrypt_ctx* encrypt_ctx,
    const uint8_t* nonce,
    size_t nonce_len,
    const uint8_t* adata,
    size_t adata_len,
    size_t plaintext_len,
    size_t mac_len,
    cbc_mac_context* cbc_ctx,
    uint8_t* ctr_block) {
    if(mac_len < 4 || mac_len > AES_BLOCK_SIZE || mac_len % 2 != 0) {
        return EXIT_FAILURE;
    }

    if(nonce_len < 7 || nonce_len > 13) {
        return EXIT_FAILURE;
    }

    // Length of the binary representation of plaintext_len.
    const size_t q = 15 - nonce_len;

    uint8_t flags = (adata_len != 0) << 6;
    flags |= ((mac_len - 2) / 2) << 3;
    flags |= q - 1;

    // Encode the first block.
    uint8_t block[AES_BLOCK_SIZE] = {0};
    block[0]                      = flags;
    memcpy(&block[1], nonce, nonce_len);
    size_t shifted_len = plaintext_len;
    for(size_t i = 0; i < q; ++i) {
        block[15 - i] = shifted_len & 0xff;
        shifted_len >>= 8;
    }
    if(shifted_len != 0) {
        // plaintext_len does not fit into q octets.
        return EXIT_FAILURE;
    }

    aes_mode_reset(encrypt_ctx);
    cbc_mac_init(cbc_ctx, encrypt_ctx);
    if(cbc_mac_update(cbc_ctx, block, sizeof(block)) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Format the associated data length.
    if(adata_len != 0) {
        size_t block_size = 0;
        if(adata_len < 65536 - 256) {
            block[0]   = adata_len >> 8;
            block[1]   = adata_len & 0xff;
            block_size = 2;
        } else {
            shifted_len = adata_len;
            block[5]    = shifted_len & 0xff;
            shifted_len >>= 8;
            block[4] = shifted_len & 0xff;
            shifted_len >>= 8;
            block[3] = shifted_len & 0xff;
            shifted_len >>= 8;
            block[2]   = shifted_len & 0xff;
            block[1]   = 0xfe;
            block[0]   = 0xff;
            block_size = 6;
            if((shifted_len >> 8) != 0) {
                // Associated data over 4 GB not supported.
                return EXIT_FAILURE;
            }
        }

        if(cbc_mac_update(cbc_ctx, block, block_size) != EXIT_SUCCESS ||
           cbc_mac_update(cbc_ctx, adata, adata_len) != EXIT_SUCCESS ||
           cbc_mac_update_zero_padding(cbc_ctx) != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
    }

    // Initialize counter.
    memzero(ctr_block, AES_BLOCK_SIZE);
    ctr_block[0] = q - 1;
    memcpy(&ctr_block[1], nonce, nonce_len);

    return EXIT_SUCCESS;
}

// The length of data written to the ciphertext array is plaintext_len +
// mac_len.
AES_RETURN aes_ccm_encrypt(
    aes_encrypt_ctx* encrypt_ctx,
    const uint8_t* nonce,
    size_t nonce_len,
    const uint8_t* adata,
    size_t adata_len,
    const uint8_t* plaintext,
    size_t plaintext_len,
    size_t mac_len,
    uint8_t* ciphertext) {
    cbc_mac_context cbc_ctx           = {0};
    uint8_t ctr_block[AES_BLOCK_SIZE] = {0};
    if(aes_ccm_init(
           encrypt_ctx,
           nonce,
           nonce_len,
           adata,
           adata_len,
           plaintext_len,
           mac_len,
           &cbc_ctx,
           ctr_block) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if(cbc_mac_update(&cbc_ctx, plaintext, plaintext_len) != EXIT_SUCCESS ||
       cbc_mac_update_zero_padding(&cbc_ctx) != EXIT_SUCCESS ||
       cbc_mac_final(&cbc_ctx, &ciphertext[plaintext_len], mac_len) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    uint8_t s0[AES_BLOCK_SIZE] = {0};
    if(aes_ecb_encrypt(ctr_block, s0, AES_BLOCK_SIZE, encrypt_ctx) != EXIT_SUCCESS) {
        memzero(s0, sizeof(s0));
        return EXIT_FAILURE;
    }

    for(size_t i = 0; i < mac_len; ++i) {
        ciphertext[plaintext_len + i] ^= s0[i];
    }
    memzero(s0, sizeof(s0));

    ctr_block[AES_BLOCK_SIZE - 1] = 1;
    if(aes_ctr_crypt(
           plaintext, ciphertext, plaintext_len, ctr_block, aes_ctr_cbuf_inc, encrypt_ctx) !=
       EXIT_SUCCESS) {
        memzero(ciphertext, plaintext_len + mac_len);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// The length of data written to the plaintext array is ciphertext_len -
// mac_len.
AES_RETURN aes_ccm_decrypt(
    aes_encrypt_ctx* encrypt_ctx,
    const uint8_t* nonce,
    size_t nonce_len,
    const uint8_t* adata,
    size_t adata_len,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    size_t mac_len,
    uint8_t* plaintext) {
    cbc_mac_context cbc_ctx           = {0};
    uint8_t ctr_block[AES_BLOCK_SIZE] = {0};
    size_t plaintext_len              = ciphertext_len - mac_len;
    if(ciphertext_len < mac_len || aes_ccm_init(
                                       encrypt_ctx,
                                       nonce,
                                       nonce_len,
                                       adata,
                                       adata_len,
                                       plaintext_len,
                                       mac_len,
                                       &cbc_ctx,
                                       ctr_block) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    uint8_t s0[AES_BLOCK_SIZE] = {0};
    if(aes_ecb_encrypt(ctr_block, s0, AES_BLOCK_SIZE, encrypt_ctx) != EXIT_SUCCESS) {
        memzero(&cbc_ctx, sizeof(cbc_ctx));
        return EXIT_FAILURE;
    }

    ctr_block[AES_BLOCK_SIZE - 1] = 1;
    if(aes_ctr_crypt(
           ciphertext, plaintext, plaintext_len, ctr_block, aes_ctr_cbuf_inc, encrypt_ctx) !=
       EXIT_SUCCESS) {
        memzero(&cbc_ctx, sizeof(cbc_ctx));
        memzero(s0, sizeof(s0));
        memzero(plaintext, plaintext_len);
        return EXIT_FAILURE;
    }

    uint8_t cbc_mac[AES_BLOCK_SIZE] = {0};
    if(cbc_mac_update(&cbc_ctx, plaintext, plaintext_len) != EXIT_SUCCESS ||
       cbc_mac_update_zero_padding(&cbc_ctx) != EXIT_SUCCESS ||
       cbc_mac_final(&cbc_ctx, cbc_mac, mac_len) != EXIT_SUCCESS) {
        memzero(s0, sizeof(s0));
        memzero(plaintext, plaintext_len);
        return EXIT_FAILURE;
    }

    uint8_t diff = 0;
    for(size_t i = 0; i < mac_len; ++i) {
        diff |= ciphertext[plaintext_len + i] ^ s0[i] ^ cbc_mac[i];
    }
    memzero(cbc_mac, sizeof(cbc_mac));
    memzero(s0, sizeof(s0));

    if(diff != 0) {
        memzero(plaintext, plaintext_len);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
