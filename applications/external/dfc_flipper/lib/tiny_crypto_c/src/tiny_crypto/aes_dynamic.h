/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_AES_DYNAMIC_H_
#define TINY_CRYPTO_AES_DYNAMIC_H_
#include <tiny_crypto/aes.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Key length is selected per context. The fixed-size AES API is unchanged. */
typedef struct {
  uint8_t round_key[240];
  uint8_t rounds;
} TC_AES_dynamic_key;
typedef struct {
  TC_AES_dynamic_key key;
  uint8_t mac[16], buffer[16], k1[16], k2[16], used;
} TC_AES_dynamic_CMAC;

/* Keys are 16, 24, or 32 bytes. Input keys must not overlap the context.
 * Failure leaves the context unchanged. Clear is unconditional. */
TC_status TC_AES_dynamic_key_init(TC_AES_dynamic_key* ctx, const uint8_t* key, size_t key_len);
void TC_AES_dynamic_key_clear(TC_AES_dynamic_key* ctx);
TC_status TC_AES_dynamic_encrypt(const TC_AES_dynamic_key* ctx, uint8_t block[16]);
TC_status TC_AES_dynamic_decrypt(const TC_AES_dynamic_key* ctx, uint8_t block[16]);

/* CBC operates in place, without padding, and updates iv for the next call.
 * ctx, iv, and buffer must be disjoint. A zero-length buffer may be NULL. */
TC_status TC_AES_dynamic_CBC_encrypt(const TC_AES_dynamic_key* ctx, uint8_t iv[16], uint8_t* buffer, size_t length);
TC_status TC_AES_dynamic_CBC_decrypt(const TC_AES_dynamic_key* ctx, uint8_t iv[16], uint8_t* buffer, size_t length);

TC_status TC_AES_dynamic_CMAC_init(TC_AES_dynamic_CMAC* ctx, const uint8_t* key, size_t key_len);
TC_status TC_AES_dynamic_CMAC_update(TC_AES_dynamic_CMAC* ctx, const uint8_t* data, size_t length);
/* Produces the full tag and clears the context. Truncation is the caller's choice. */
TC_status TC_AES_dynamic_CMAC_final(TC_AES_dynamic_CMAC* ctx, uint8_t tag[16]);
void TC_AES_dynamic_CMAC_clear(TC_AES_dynamic_CMAC* ctx);
#ifdef __cplusplus
}
#endif
#endif
