/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * GHASH over the running tag state of a GCM context. H must already hold
 * AES_K(0^128), and FAST_TABLE builds must call tc_aes_gcm_init_table after
 * setting H. The GCM entry points validate the context, lengths and phase
 * before calling these helpers. */
#ifndef TC_AES_GHASH_INTERNAL_H
#define TC_AES_GHASH_INTERNAL_H
#include "aes_internal.h"

#if TC_AES_ENABLE_GCM
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
/* Build ghash_table[n] = n * H for each 4-bit n. The table depends on the key
 * and is wiped with the context. */
void tc_aes_gcm_init_table(struct TC_AES_GCM_ctx* ctx);
#endif

/* S = (S xor block) * H for one full 16-byte block. The block may alias
 * ctx->ghash, the partial-block buffer, because it is read before S changes. */
void tc_aes_gcm_ghash_block(struct TC_AES_GCM_ctx* ctx, const uint8_t* block);

/* Absorb data into S, zero-padding the final partial block. Used for a
 * non-96-bit IV when deriving J0 (SP 800-38D section 7.1 step 2). */
void tc_aes_gcm_hash_bytes(struct TC_AES_GCM_ctx* ctx, const uint8_t* data, size_t length);
#endif

#endif
