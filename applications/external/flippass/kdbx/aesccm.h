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

#ifndef __AESCCM_H__
#define __AESCCM_H__

#include <stddef.h>
#include <stdint.h>

#include "aes.h"

AES_RETURN aes_ccm_encrypt(
    aes_encrypt_ctx* encrypt_ctx,
    const uint8_t* nonce,
    size_t nonce_len,
    const uint8_t* adata,
    size_t adata_len,
    const uint8_t* plaintext,
    size_t plaintext_len,
    size_t mac_len,
    uint8_t* ciphertext);

AES_RETURN aes_ccm_decrypt(
    aes_encrypt_ctx* encrypt_ctx,
    const uint8_t* nonce,
    size_t nonce_len,
    const uint8_t* adata,
    size_t adata_len,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    size_t mac_len,
    uint8_t* plaintext);

#endif
