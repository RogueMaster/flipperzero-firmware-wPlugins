/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * GHASH for AES-GCM (NIST SP 800-38D section 6.4). Multiplication in
 * GF(2^128) uses the bit-reflected GCM convention and the reduction constant
 * R = 0xe1 || 0^120.
 *
 * TC_AES_GCM_GHASH_MODE selects one multiplier at build time:
 * - BITWISE: constant-time byte loop with the smallest code and no table.
 * - WIDE: constant-time 64-bit shift loop when uint64_t is available.
 * - FAST_TABLE: 16-entry nibble table in the context (256 bytes of RAM),
 *   rebuilt for each key. Table lookups are indexed by message nibbles.
 * - HARDWARE: TC_AES_GCM_hardware_multiply supplied by the platform.
 * - AUTO: WIDE when TC_AES_WIDE_OPS is set, otherwise BITWISE. */
#include "aes_internal.h"
#include "aes_ghash_internal.h"

#if TC_AES_ENABLE_GCM

#if (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_BITWISE) ||                                    \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO &&                                        \
     (!TC_AES_WIDE_OPS || !defined(UINT64_MAX))) ||                                                \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE) ||                                 \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE && !defined(UINT64_MAX))
static void tc_aes_gcm_multiply_x(uint8_t value[TC_AES_BLOCKLEN])
{
  uint8_t carry = 0;
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i) {
    const uint8_t next_carry = (uint8_t)(value[i] & 1u);
    value[i] = (uint8_t)((value[i] >> 1) | (carry << 7));
    carry = next_carry;
  }
  value[0] ^= (uint8_t)(0xe1u & (uint8_t)(0u - carry));
}

/* Constant-time bytewise multiplication in GF(2^128). */
static void tc_aes_gcm_multiply_bitwise(uint8_t* result, const uint8_t* left, const uint8_t* right)
{
  uint8_t z[TC_AES_BLOCKLEN] = {0};
  uint8_t v[TC_AES_BLOCKLEN];
  unsigned bit;

  memcpy(v, right, TC_AES_BLOCKLEN);
  for (bit = 0; bit < 128; ++bit) {
    const uint8_t bit_mask = (uint8_t)(0u - (uint8_t)((left[bit / 8u] >> (7u - (bit % 8u))) & 1u));
    unsigned i;

    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      z[i] ^= (uint8_t)(v[i] & bit_mask);

    tc_aes_gcm_multiply_x(v);
  }
  memcpy(result, z, TC_AES_BLOCKLEN);
  TC_secure_zero(z, sizeof(z));
  TC_secure_zero(v, sizeof(v));
}
#endif

#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE ||                                         \
    ((TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO) && TC_AES_WIDE_OPS)
#if defined(UINT64_MAX)
static void tc_aes_gcm_multiply_wide(uint8_t* result, const uint8_t* left, const uint8_t* right)
{
  uint64_t xh = tc_internal_load_be64(left);
  uint64_t xl = tc_internal_load_be64(left + 8);
  uint64_t zh = 0;
  uint64_t zl = 0;
  uint64_t vh = tc_internal_load_be64(right);
  uint64_t vl = tc_internal_load_be64(right + 8);
  unsigned bit;

  for (bit = 0; bit < 128; ++bit) {
    const uint64_t bit_mask = 0u - (xh >> 63);
    const uint64_t reduction = 0xe100000000000000ULL & (0u - (vl & 1u));
    zh ^= vh & bit_mask;
    zl ^= vl & bit_mask;
    vl = (vl >> 1) | (vh << 63);
    vh = (vh >> 1) ^ reduction;
    xh = (xh << 1) | (xl >> 63);
    xl <<= 1;
  }
  tc_internal_store_be64(result, zh);
  tc_internal_store_be64(result + 8, zl);
}
#endif
#endif

#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
void tc_aes_gcm_init_table(struct TC_AES_GCM_ctx* ctx)
{
  uint8_t input[TC_AES_BLOCKLEN] = {0};
  uint8_t entry;

  for (entry = 0; entry < 16; ++entry) {
    input[0] = (uint8_t)(entry << 4);
    tc_aes_gcm_multiply_bitwise(ctx->ghash_table[entry], input, ctx->h);
  }
  TC_secure_zero(input, sizeof(input));
}

static void tc_aes_gcm_multiply_fast_table(uint8_t* result, const uint8_t* left,
                                           const struct TC_AES_GCM_ctx* ctx)
{
  uint8_t value[TC_AES_BLOCKLEN] = {0};
  uint8_t position = 32;
  uint8_t i;

  /* Horner evaluation runs from the least-significant nibble toward the
   * most-significant one. Each x^4 step advances the accumulated field power. */
  while (position > 0) {
    const uint8_t nibble_position = (uint8_t)(--position);
    const uint8_t nibble =
        (uint8_t)((nibble_position & 1u) == 0u ? left[nibble_position / 2u] >> 4
                                               : left[nibble_position / 2u] & 0x0fu);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      value[i] ^= ctx->ghash_table[nibble][i];
  }
  memcpy(result, value, TC_AES_BLOCKLEN);
  TC_secure_zero(value, sizeof(value));
}
#endif

static void tc_aes_gcm_multiply(uint8_t* result, const uint8_t* left,
                                const struct TC_AES_GCM_ctx* ctx)
{
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_HARDWARE
  TC_AES_GCM_hardware_multiply(result, left, ctx->h);
#elif TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  tc_aes_gcm_multiply_fast_table(result, left, ctx);
#elif TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE ||                                       \
    ((TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO) && TC_AES_WIDE_OPS)
#if defined(UINT64_MAX)
  tc_aes_gcm_multiply_wide(result, left, ctx->h);
#else
  tc_aes_gcm_multiply_bitwise(result, left, ctx->h);
#endif
#else
  tc_aes_gcm_multiply_bitwise(result, left, ctx->h);
#endif
}

void tc_aes_gcm_ghash_block(struct TC_AES_GCM_ctx* ctx, const uint8_t* block)
{
  uint8_t value[TC_AES_BLOCKLEN];
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    value[i] = (uint8_t)(ctx->s[i] ^ block[i]);
  tc_aes_gcm_multiply(ctx->s, value, ctx);
  /* S xor block depends on H. Wipe it so it does not outlive the call. */
  TC_secure_zero(value, sizeof(value));
}

void tc_aes_gcm_hash_bytes(struct TC_AES_GCM_ctx* ctx, const uint8_t* data, size_t length)
{
  uint8_t block[TC_AES_BLOCKLEN] = {0};

  while (length >= TC_AES_BLOCKLEN) {
    tc_aes_gcm_ghash_block(ctx, data);
    data += TC_AES_BLOCKLEN;
    length -= TC_AES_BLOCKLEN;
  }
  if (length != 0) {
    memcpy(block, data, length);
    tc_aes_gcm_ghash_block(ctx, block);
  }
}

#endif
