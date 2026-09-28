/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_MD5_H_
#define TINY_CRYPTO_MD5_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TC_MD5_DIGESTLEN 16
#define TC_MD5_BLOCKLEN 64

/* Legacy download checksums. MD5 has broken collision resistance; establish
 * download authenticity through trusted transport or provisioning. */
struct TC_MD5_ctx {
  uint64_t Count;
  uint32_t State[4];
  uint8_t BufLen;
  uint8_t Buf[TC_MD5_BLOCKLEN];
};

/* Initialize before use and before reusing a finalized context. Context storage
 * must be disjoint from input and digest buffers. One-shot input/output may
 * overlap. Failed argument checks preserve context and output. */
TC_status TC_MD5_init(struct TC_MD5_ctx* ctx);
TC_status TC_MD5_update(struct TC_MD5_ctx* ctx, const uint8_t* data, size_t length);
TC_status TC_MD5_final(struct TC_MD5_ctx* ctx, uint8_t digest[TC_MD5_DIGESTLEN]);
TC_status TC_MD5_digest(const uint8_t* data, size_t length, uint8_t digest[TC_MD5_DIGESTLEN]);
void TC_MD5_ctx_clear(struct TC_MD5_ctx* ctx);

#ifdef __cplusplus
}
#endif
#endif
