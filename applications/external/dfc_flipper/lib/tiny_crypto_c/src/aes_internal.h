/* SPDX-License-Identifier: GPL-2.0-or-later
 * Private AES interface. Only the block cipher crosses translation units. */
#ifndef TC_AES_INTERNAL_H
#define TC_AES_INTERNAL_H
#include <tiny_crypto/aes.h>
#if TC_AES_ENABLE_DYNAMIC
#include <tiny_crypto/aes_dynamic.h>
#endif
#include "internal.h"

typedef uint8_t state_t[4][4];
TC_status tc_aes_cipher(state_t* state, const uint8_t* round_key);
TC_status tc_aes_cipher_rounds(state_t* state, const uint8_t* round_key, uint8_t rounds);
#define TC_AES_FIXED_ROUNDS (TC_AES_KEY_BITS / 32 + 6)

static inline void tc_aes_copy_bytes(uint8_t* dst, const uint8_t* src, size_t length)
{
  memcpy(dst, src, length);
}

#if (defined(TC_AES_ENABLE_GCM) && (TC_AES_ENABLE_GCM == 1)) || (defined(TC_AES_ENABLE_CCM) && (TC_AES_ENABLE_CCM == 1)) || \
    (defined(TC_AES_ENABLE_EAX) && (TC_AES_ENABLE_EAX == 1)) || (defined(TC_AES_ENABLE_EAX_PRIME) && (TC_AES_ENABLE_EAX_PRIME == 1)) || \
    (defined(TC_AES_ENABLE_SIV) && (TC_AES_ENABLE_SIV == 1))
/*
 * Completely disjoint buffers (exact alias is not disjoint).
 * Empty lengths are always treated as disjoint.
 *
 * Uses uintptr_t subtraction (not relational pointer compares or
 * pa+len) for C portability across unrelated objects / MCU ABIs.
 */
static inline int tc_aes_buffers_disjoint(const void* a, size_t a_len,
                                const void* b, size_t b_len)
{
  return tc_internal_ranges_disjoint(a, a_len, b, b_len);
}

/*
 * Buffer relationship for one-shot in/out pairs:
 *   exact alias (same pointer) — OK
 *   completely disjoint — OK
 *   partial overlap — not OK (TC_ERROR)
 * Empty lengths are always OK.
 */
static inline int tc_aes_buffers_ok(const void* a, size_t a_len,
                          const void* b, size_t b_len)
{
  const uintptr_t pa = (uintptr_t)a;
  const uintptr_t pb = (uintptr_t)b;

  if (a_len == 0 || b_len == 0 || pa == pb)
    return 1;
  return tc_aes_buffers_disjoint(a, a_len, b, b_len);
}
#endif

#endif
