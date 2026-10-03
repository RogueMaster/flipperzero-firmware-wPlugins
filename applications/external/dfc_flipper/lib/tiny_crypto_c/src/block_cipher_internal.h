/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Block cipher descriptor shared by the AES and DES confidentiality modes
 * (block_modes.c) and the block-cipher MACs (mac_core.c), and the argument
 * rule every mode and MAC entry applies. */
#ifndef TC_BLOCK_CIPHER_INTERNAL_H_
#define TC_BLOCK_CIPHER_INTERNAL_H_
#include <tiny_crypto/common.h>
#include "internal.h"

/* Largest block the shared code handles (AES). */
enum { TC_BLOCK_MAX = 16 };

/* One block cipher bound to its key schedule. encrypt and decrypt transform
 * one block of block_size bytes in place and may leave it modified when they
 * fail. decrypt is NULL where no caller needs the inverse cipher, so a
 * forward-only mode does not link it. key is borrowed and outlives every call. */
typedef struct {
  size_t block_size;
  const void* key;
  TC_status (*encrypt)(const void* key, uint8_t* block);
  TC_status (*decrypt)(const void* key, uint8_t* block);
} tc_block_cipher;

/*
 * Argument rule for every AES and DES mode and MAC entry: a context, a span
 * that has storage unless empty, a length that is a multiple of alignment,
 * and a span disjoint from the context. A span inside the context would
 * change round keys, the feedback register or MAC state while the operation
 * reads them, and set_iv would copy between overlapping ranges. length is in
 * bytes. Callers check the context state after this succeeds.
 */
static inline int tc_block_mode_args(const void* ctx, size_t ctx_size, const void* span,
                                     size_t length, size_t alignment)
{
  return ctx != NULL && tc_internal_span_valid(span, length) && length % alignment == 0 &&
         tc_internal_ranges_disjoint(ctx, ctx_size, span, length);
}

/* Shared modes, built when an AES or DES mode that uses them is enabled. The
 * mode gates apply even when the family is disabled, because direct-source
 * builds compile every translation unit. */
#define TC_BLOCK_NEED_CBC (TC_AES_ENABLE_CBC || TC_AES_ENABLE_DYNAMIC || TC_DES_ENABLE_CBC)
#define TC_BLOCK_NEED_CTR (TC_AES_ENABLE_CTR || TC_DES_ENABLE_CTR)
#define TC_BLOCK_NEED_OFB (TC_AES_ENABLE_OFB || TC_DES_ENABLE_OFB)

/*
 * NIST SP 800-38A modes over a descriptor. buf is transformed in place and
 * must be disjoint from the cipher key and the mode state. Callers validate
 * the arguments and the mode state first, so the cores fail only when the
 * cipher fails. That failure wipes buf, and for CBC also iv, so neither
 * partial output nor a broken chaining value survives. The caller then
 * clears its context.
 */
#if TC_BLOCK_NEED_CBC
/* Section 6.2. length is a multiple of block_size. iv holds the chaining
 * value and is advanced to the last ciphertext block. Decryption needs the
 * descriptor's decrypt hook. */
TC_status tc_block_cbc_encrypt(const tc_block_cipher* cipher, uint8_t* iv, uint8_t* buf,
                               size_t length);
TC_status tc_block_cbc_decrypt(const tc_block_cipher* cipher, uint8_t* iv, uint8_t* buf,
                               size_t length);
#endif

#if TC_BLOCK_NEED_CTR
/* Section 6.5 stream state. counter is the big-endian counter block for the
 * next keystream block, keystream the current block, used the keystream bytes
 * already consumed (block_size when none remain), and exhausted is set once
 * the counter wrapped. */
typedef struct {
  uint8_t* counter;
  uint8_t* keystream;
  uint8_t* used;
  uint8_t* exhausted;
} tc_block_ctr_state;

/* A request is valid when used is at most block_size and the counter space
 * of the IV holds every block it needs (2^(8 * block_size) blocks per IV).
 * An empty request is always valid. Check it before any output. */
static inline int tc_block_ctr_request_ok(const tc_block_ctr_state* state, size_t block_size,
                                          size_t length)
{
  if (*state->used > block_size)
    return 0;
  return length == 0 || tc_internal_counter_has_blocks(
                            state->counter, block_size,
                            tc_internal_counter_blocks_needed(length, block_size, *state->used),
                            *state->exhausted);
}

/* The request must satisfy tc_block_ctr_request_ok. */
TC_status tc_block_ctr_crypt(const tc_block_cipher* cipher, const tc_block_ctr_state* state,
                             uint8_t* buf, size_t length);
#endif

#if TC_BLOCK_NEED_OFB
/* Section 6.4. feedback holds the current output block and used counts its
 * consumed bytes, at most block_size (block_size when none remain). */
TC_status tc_block_ofb_crypt(const tc_block_cipher* cipher, uint8_t* feedback, uint8_t* used,
                             uint8_t* buf, size_t length);
#endif

#endif
