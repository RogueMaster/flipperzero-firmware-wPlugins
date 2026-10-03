/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * DES and TDEA confidentiality modes (NIST SP 800-38A): ECB, CBC, CTR, CFB1,
 * CFB8, CFB64 and OFB over the block cipher in des.c. One context serves
 * single DES and TDEA. The key length chosen at init sets ctx->triple.
 */

#include <string.h>
#include <tiny_crypto/des.h>
#include "internal.h"
#include "des_internal.h"

/* Every mode entry validates once, before taking addresses of context
 * members: tc_block_mode_args on the byte span, then an active context. The
 * mode cores assume both. */
#define DES_MODE_REQUIRE_ARGS(ctx, buf, bytes, alignment)                                          \
  do {                                                                                             \
    if (!tc_block_mode_args((ctx), sizeof *(ctx), (buf), (bytes), (alignment)) ||                  \
        (ctx)->active != 1)                                                                        \
      return TC_ERROR;                                                                             \
  } while (0)

/* The IV modes also need a loaded IV. TC_DES_init leaves none, so a caller
 * that skips TC_DES_set_iv gets TC_ERROR and never encrypts under a fixed
 * all-zero IV. SP 800-38A section 5.3 and Appendix C require an
 * unpredictable CBC and CFB IV and a unique OFB IV per message, and
 * Appendix B unique CTR counter blocks. */
#define DES_IV_MODE_REQUIRE_ARGS(ctx, buf, bytes, alignment)                                       \
  do {                                                                                             \
    DES_MODE_REQUIRE_ARGS((ctx), (buf), (bytes), (alignment));                                     \
    if ((ctx)->iv_loaded != 1)                                                                     \
      return TC_ERROR;                                                                             \
  } while (0)

#if TC_DES_ENABLE_ECB || TC_DES_ENABLE_CBC || TC_DES_ENABLE_CTR || TC_DES_ENABLE_OFB ||            \
    TC_DES_ENABLE_CFB1 || TC_DES_ENABLE_CFB8 || TC_DES_ENABLE_CFB64
/* The context's schedule, borrowed for one call. */
static tc_des_block_key tc_des_ctx_key(const struct TC_DES_ctx* ctx)
{
  const tc_des_block_key key = {ctx->schedule, ctx->triple};
  return key;
}
#endif

#if TC_DES_ENABLE_CFB1 || TC_DES_ENABLE_CFB8 || TC_DES_ENABLE_CFB64
/* CFB keystream block: CIPH_K(I_j) of the feedback register (SP 800-38A
 * section 6.3). The DES cipher cannot fail. */
static void tc_des_cfb_keystream(const struct TC_DES_ctx* ctx, uint8_t keystream[TC_DES_BLOCKLEN])
{
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  memcpy(keystream, ctx->iv, TC_DES_BLOCKLEN);
  (void)tc_des_block_encrypt(&key, keystream); /* DES has no failure path. */
}
#endif

#if TC_DES_ENABLE_CFB64
/* A short final CFB-64 segment shifts only its ciphertext bytes into the
 * feedback register. */
static void tc_des_cfb64_shift_iv(uint8_t* iv, const uint8_t* ct, size_t length)
{
  if (length >= TC_DES_BLOCKLEN) {
    memcpy(iv, ct, TC_DES_BLOCKLEN);
    return;
  }
  memmove(iv, iv + length, TC_DES_BLOCKLEN - length);
  memcpy(iv + TC_DES_BLOCKLEN - length, ct, length);
}

static TC_status tc_des_mode_cfb64(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length, int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  uint8_t saved[TC_DES_BLOCKLEN];
  size_t offset = 0;

  /* SP 800-38A section 5.2 defines a CFB message as whole s-bit segments.
   * The one short final segment accepted here ends the message. */
  if (ctx->cfb64_finished)
    return TC_ERROR;

  while (offset < length) {
    const size_t segment = length - offset < TC_DES_BLOCKLEN ? length - offset : TC_DES_BLOCKLEN;

    if (decrypt)
      memcpy(saved, buf + offset, segment);
    tc_des_cfb_keystream(ctx, keystream);
    tc_internal_xor(buf + offset, keystream, segment);
    tc_des_cfb64_shift_iv(ctx->iv, decrypt ? saved : buf + offset, segment);
    offset += segment;
  }
  ctx->cfb64_finished = (uint8_t)((length % TC_DES_BLOCKLEN) != 0);
  TC_secure_zero(keystream, sizeof(keystream));
  TC_secure_zero(saved, sizeof(saved));
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CFB8
static TC_status tc_des_mode_cfb8(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length, int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  size_t i;

  for (i = 0; i < length; ++i) {
    const uint8_t ciphertext = buf[i];
    tc_des_cfb_keystream(ctx, keystream);
    buf[i] ^= keystream[0];
    memmove(ctx->iv, ctx->iv + 1, TC_DES_BLOCKLEN - 1);
    ctx->iv[TC_DES_BLOCKLEN - 1] = decrypt ? ciphertext : buf[i];
  }
  TC_secure_zero(keystream, sizeof(keystream));
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CFB1
static void tc_des_cfb1_shift_iv(uint8_t* iv, uint8_t ciphertext_bit)
{
  uint8_t j;
  for (j = 0; j < TC_DES_BLOCKLEN - 1; ++j)
    iv[j] = (uint8_t)((iv[j] << 1) | (iv[j + 1] >> 7));
  iv[TC_DES_BLOCKLEN - 1] = (uint8_t)((iv[TC_DES_BLOCKLEN - 1] << 1) | ciphertext_bit);
}

static TC_status tc_des_mode_cfb1(struct TC_DES_ctx* ctx, uint8_t* buf, size_t bit_length,
                                  int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  size_t i;

  for (i = 0; i < bit_length; ++i) {
    const size_t byte_index = i / 8;
    const uint8_t shift = (uint8_t)(7 - (i % 8));
    const uint8_t input_bit = (uint8_t)((buf[byte_index] >> shift) & 1u);
    uint8_t output_bit;
    uint8_t ciphertext_bit;

    tc_des_cfb_keystream(ctx, keystream);
    output_bit = (uint8_t)((input_bit ^ (keystream[0] >> 7)) & 1u);
    ciphertext_bit = decrypt ? input_bit : output_bit;
    buf[byte_index] =
        (uint8_t)((buf[byte_index] & ~(1u << shift)) | ((unsigned)output_bit << shift));
    tc_des_cfb1_shift_iv(ctx->iv, ciphertext_bit);
  }
  TC_secure_zero(keystream, sizeof(keystream));
  return TC_OK;
}
#endif

/*****************************************************************************/
/* Public Functions: key setup                                               */
/*****************************************************************************/

void TC_DES_ctx_clear(struct TC_DES_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

/* 8 bytes selects single DES. 16 and 24 bytes select TDEA when it is built. */
static int tc_des_ctx_keylen_supported(size_t keylen)
{
#if TC_DES_ENABLE_TDES
  if (keylen == TC_DES_KEYLEN_2KEY || keylen == TC_DES_KEYLEN_3KEY)
    return 1;
#endif
  return keylen == TC_DES_KEYLEN;
}

#if TC_DES_NEEDS_IV
/* Start a new message: load the IV and reset every per-message stream state. */
static void tc_des_ctx_start_message(struct TC_DES_ctx* ctx, const uint8_t iv[TC_DES_BLOCKLEN])
{
  memcpy(ctx->iv, iv, TC_DES_BLOCKLEN);
  ctx->iv_loaded = 1;
#if TC_DES_ENABLE_CTR
  ctx->ctr_pos = TC_DES_BLOCKLEN;
  ctx->ctr_exhausted = 0;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_CFB64
  ctx->cfb64_finished = 0;
#endif
}
#endif

TC_status TC_DES_init(struct TC_DES_ctx* ctx, TC_bytes key)
{
  if (ctx == NULL)
    return TC_ERROR;
  TC_DES_ctx_clear(ctx);
  if (!tc_internal_span_valid(key.data, key.length) || !tc_des_ctx_keylen_supported(key.length) ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, key.data, key.length))
    return TC_ERROR;
#if TC_DES_REJECT_WEAK_KEYS
  if (tc_des_bundle_is_rejected(key.data, key.length))
    return TC_ERROR;
#endif
  tc_des_schedule_key(ctx->schedule, key.data, key.length);
  ctx->triple = (uint8_t)(key.length != TC_DES_KEYLEN);
  /* iv_loaded stays 0 from the clear, so the IV modes fail until
   * TC_DES_set_iv starts a message. */
  ctx->active = 1;
  return TC_OK;
}

#if TC_DES_NEEDS_IV
TC_status TC_DES_set_iv(struct TC_DES_ctx* ctx, TC_bytes iv)
{
  if (iv.length != TC_DES_BLOCKLEN)
    return TC_ERROR;
  DES_MODE_REQUIRE_ARGS(ctx, iv.data, iv.length, 1);
  tc_des_ctx_start_message(ctx, iv.data);
  return TC_OK;
}
#endif

/*****************************************************************************/
/* Public Functions: modes                                                   */
/*****************************************************************************/

#if TC_DES_ENABLE_ECB
TC_status TC_DES_ECB_encrypt(const struct TC_DES_ctx* ctx, TC_buffer buf)
{
  if (buf.capacity != TC_DES_BLOCKLEN)
    return TC_ERROR;
  DES_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  return tc_des_block_encrypt(&key, buf.data);
}

TC_status TC_DES_ECB_decrypt(const struct TC_DES_ctx* ctx, TC_buffer buf)
{
  if (buf.capacity != TC_DES_BLOCKLEN)
    return TC_ERROR;
  DES_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  return tc_des_block_decrypt(&key, buf.data);
}
#endif

#if TC_DES_ENABLE_CBC
TC_status TC_DES_CBC_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, TC_DES_BLOCKLEN);
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  const tc_block_cipher cipher = tc_des_block_cipher(&key);
  return tc_block_cbc_encrypt(&cipher, ctx->iv, buf.data, buf.capacity);
}

TC_status TC_DES_CBC_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, TC_DES_BLOCKLEN);
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  const tc_block_cipher cipher = tc_des_block_cipher_inverse(&key);
  return tc_block_cbc_decrypt(&cipher, ctx->iv, buf.data, buf.capacity);
}
#endif

#if TC_DES_ENABLE_CTR
TC_status TC_DES_CTR_crypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  const tc_block_cipher cipher = tc_des_block_cipher(&key);
  const tc_block_ctr_state state = {ctx->iv, ctx->ctr_stream, &ctx->ctr_pos, &ctx->ctr_exhausted};
  if (!tc_block_ctr_request_ok(&state, TC_DES_BLOCKLEN, buf.capacity))
    return TC_ERROR;
  return tc_block_ctr_crypt(&cipher, &state, buf.data, buf.capacity);
}
#endif

#if TC_DES_ENABLE_CFB64
TC_status TC_DES_CFB64_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  return tc_des_mode_cfb64(ctx, buf.data, buf.capacity, 0);
}

TC_status TC_DES_CFB64_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  /* CFB decryption uses the cipher's forward direction. */
  return tc_des_mode_cfb64(ctx, buf.data, buf.capacity, 1);
}
#endif

#if TC_DES_ENABLE_CFB8
TC_status TC_DES_CFB8_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  return tc_des_mode_cfb8(ctx, buf.data, buf.capacity, 0);
}

TC_status TC_DES_CFB8_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  return tc_des_mode_cfb8(ctx, buf.data, buf.capacity, 1);
}
#endif

#if TC_DES_ENABLE_CFB1
/* Bytes that hold bit_length packed bits, without overflow near SIZE_MAX. */
static size_t tc_des_cfb1_bytes(size_t bit_length)
{
  return bit_length / 8 + (bit_length % 8 != 0);
}

TC_status TC_DES_CFB1_encrypt(struct TC_DES_ctx* ctx, TC_buffer buf, size_t bit_length)
{
  if (buf.capacity < tc_des_cfb1_bytes(bit_length))
    return TC_ERROR;
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, tc_des_cfb1_bytes(bit_length), 1);
  return tc_des_mode_cfb1(ctx, buf.data, bit_length, 0);
}

TC_status TC_DES_CFB1_decrypt(struct TC_DES_ctx* ctx, TC_buffer buf, size_t bit_length)
{
  if (buf.capacity < tc_des_cfb1_bytes(bit_length))
    return TC_ERROR;
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, tc_des_cfb1_bytes(bit_length), 1);
  return tc_des_mode_cfb1(ctx, buf.data, bit_length, 1);
}
#endif

#if TC_DES_ENABLE_OFB
TC_status TC_DES_OFB_crypt(struct TC_DES_ctx* ctx, TC_buffer buf)
{
  DES_IV_MODE_REQUIRE_ARGS(ctx, buf.data, buf.capacity, 1);
  if (ctx->ofb_pos > TC_DES_BLOCKLEN)
    return TC_ERROR;
  const tc_des_block_key key = tc_des_ctx_key(ctx);
  const tc_block_cipher cipher = tc_des_block_cipher(&key);
  return tc_block_ofb_crypt(&cipher, ctx->iv, &ctx->ofb_pos, buf.data, buf.capacity);
}
#endif
