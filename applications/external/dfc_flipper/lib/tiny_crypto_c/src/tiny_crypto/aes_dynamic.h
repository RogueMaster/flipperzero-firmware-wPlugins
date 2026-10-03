/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* AES with the key length chosen per context: 128, 192 or 256-bit keys for
 * single blocks, CBC and CMAC. aes.h provides the fixed-size AES API.
 * Standards: FIPS 197, SP 800-38A (CBC), SP 800-38B (CMAC).
 * Configuration: TC_ENABLE_AES and TC_AES_ENABLE_DYNAMIC.
 * Work: every function charges no work budget. The functions return TC_OK
 * or TC_ERROR.
 * Contracts: docs/api.md, including its block-mode rules. */
#ifndef TINY_CRYPTO_AES_DYNAMIC_H_
#define TINY_CRYPTO_AES_DYNAMIC_H_
#include <tiny_crypto/aes.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Expanded key schedule for 10, 12 or 14 rounds. Fields are private. */
typedef struct {
  uint8_t round_key[240];
  uint8_t rounds;
} TC_AES_dynamic_key;
/* Streaming CMAC state. Fields are private. */
typedef struct {
  TC_AES_dynamic_key key;
  uint8_t mac[16], buffer[16], k1[16], k2[16], used;
} TC_AES_dynamic_CMAC;

#if TC_ENABLE_AES && TC_AES_ENABLE_DYNAMIC
/* Expand a 16, 24 or 32-byte key (FIPS 197 section 5.2). key must be
 * disjoint from ctx. Returns TC_OK, or TC_ERROR for a NULL argument, another
 * key length, a key that overlaps ctx, or, in the runtime S-box profile, a
 * call before TC_AES_init_sbox. A NULL ctx is left alone. Every other
 * failure wipes ctx, so no earlier key stays usable. */
TC_status TC_AES_dynamic_key_init(TC_AES_dynamic_key* ctx, TC_bytes key);
/* Wipe the key schedule. NULL is ignored. */
void TC_AES_dynamic_key_clear(TC_AES_dynamic_key* ctx);
/* Encrypt or decrypt one 16-byte block in place (FIPS 197 sections 5.1 and
 * 5.3). block must be disjoint from ctx. Returns TC_OK, or TC_ERROR with
 * block unchanged for a NULL argument, an overlap or an unkeyed ctx. A
 * cipher failure wipes block. */
TC_status TC_AES_dynamic_encrypt(const TC_AES_dynamic_key* ctx, TC_buffer block);
TC_status TC_AES_dynamic_decrypt(const TC_AES_dynamic_key* ctx, TC_buffer block);

/* CBC (SP 800-38A section 6.2) in place, without padding. length is a
 * multiple of 16. iv holds the chaining value and advances to the last
 * ciphertext block, so the next call continues the message. ctx, iv and
 * buffer must be pairwise disjoint. A zero-length buffer may be NULL.
 * Returns TC_OK, or TC_ERROR with buffer and iv unchanged for a NULL ctx or
 * iv, a NULL buffer with a nonzero length, an unaligned length, an overlap
 * or an unkeyed ctx. A cipher failure part way through wipes buffer and
 * iv. */
TC_status TC_AES_dynamic_CBC_encrypt(const TC_AES_dynamic_key* ctx, TC_buffer iv, TC_buffer buffer);
TC_status TC_AES_dynamic_CBC_decrypt(const TC_AES_dynamic_key* ctx, TC_buffer iv, TC_buffer buffer);

/* Streaming AES-CMAC (SP 800-38B sections 6.1 and 6.2).
 * init keys ctx with a 16, 24 or 32-byte key disjoint from ctx and derives
 * the subkeys. It returns TC_OK, or TC_ERROR under the
 * TC_AES_dynamic_key_init conditions. A NULL ctx is left alone, and every
 * other failure leaves ctx wiped.
 * update absorbs data, which may be NULL when length is 0. It returns TC_OK,
 * or TC_ERROR with ctx unchanged for a NULL ctx, a NULL data with a nonzero
 * length, data that overlaps ctx, or an unkeyed ctx. A cipher failure clears
 * ctx.
 * final writes the full 16-byte tag, which must be disjoint from ctx.
 * Callers may truncate it to its leading bytes (section 6.2 step 7). It
 * returns TC_OK, or TC_ERROR with ctx unchanged for a NULL argument, an
 * overlap or an unkeyed ctx. Otherwise final wipes ctx, on success and on
 * failure.
 * clear wipes ctx and ignores NULL. */
TC_status TC_AES_dynamic_CMAC_init(TC_AES_dynamic_CMAC* ctx, TC_bytes key);
TC_status TC_AES_dynamic_CMAC_update(TC_AES_dynamic_CMAC* ctx, TC_bytes data);
TC_status TC_AES_dynamic_CMAC_final(TC_AES_dynamic_CMAC* ctx, TC_buffer tag);
void TC_AES_dynamic_CMAC_clear(TC_AES_dynamic_CMAC* ctx);
#endif
#ifdef __cplusplus
}
#endif
#endif
