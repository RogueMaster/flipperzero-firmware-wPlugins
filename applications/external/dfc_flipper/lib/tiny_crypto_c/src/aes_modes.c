/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES confidentiality modes (NIST SP 800-38A): ECB, CBC, CTR and OFB, plus
 * single blocks and CBC over dynamic-length keys. CBC, CTR and OFB run on the
 * shared cores in block_modes.c. */
#include <tiny_crypto/aes.h>
#if TC_ENABLE_AES
#include "aes_internal.h"

/* Every fixed-key entry validates once: tc_block_mode_args on the buffer,
 * then an active key schedule. An argument error leaves buf and ctx
 * unchanged. */
#define AES_MODE_VALID(ctx, key_ctx, buf, length, alignment)                                       \
  (tc_block_mode_args((ctx), sizeof *(ctx), (buf), (length), (alignment)) && (key_ctx)->active == 1)

#if TC_AES_HAVE_IV
/* The IV modes also need a loaded IV. TC_AES_init leaves none, so a caller
 * that skips TC_AES_set_iv gets TC_ERROR and never encrypts under a fixed
 * all-zero IV. SP 800-38A Appendix C requires an unpredictable CBC IV and a
 * unique OFB IV per message, and Appendix B unique CTR counter blocks. */
#define AES_IV_MODE_VALID(ctx, buf, length, alignment)                                             \
  (AES_MODE_VALID((ctx), &(ctx)->key, (buf), (length), (alignment)) && (ctx)->iv_loaded == 1)

/* The fixed schedule of an AES context, borrowed for one call. */
static tc_aes_block_key tc_aes_ctx_key(const struct TC_AES_ctx* ctx)
{
  const tc_aes_block_key key = {ctx->key.round_key, TC_AES_FIXED_ROUNDS};
  return key;
}
#endif

/*****************************************************************************/
/* Public functions:                                                         */
/*****************************************************************************/
#if TC_AES_ENABLE_ECB || TC_AES_ENABLE_DYNAMIC
/* Single-block rule: a cipher failure wipes the block. The key schedule is
 * const and stays. */
static TC_status tc_aes_block_result(TC_status status, uint8_t* buf)
{
  if (status != TC_OK)
    TC_secure_zero(buf, TC_AES_BLOCKLEN);
  return status;
}
#endif

#if TC_AES_ENABLE_ECB

TC_status TC_AES_ECB_encrypt(const struct TC_AES_key_ctx* ctx, TC_buffer buf)
{
  if (buf.capacity != TC_AES_BLOCKLEN || !AES_MODE_VALID(ctx, ctx, buf.data, buf.capacity, 1))
    return TC_ERROR;
  return tc_aes_block_result(tc_aes_cipher((state_t*)buf.data, ctx->round_key), buf.data);
}

TC_status TC_AES_ECB_decrypt(const struct TC_AES_key_ctx* ctx, TC_buffer buf)
{
  if (buf.capacity != TC_AES_BLOCKLEN || !AES_MODE_VALID(ctx, ctx, buf.data, buf.capacity, 1))
    return TC_ERROR;
  return tc_aes_block_result(
      tc_aes_inverse_rounds((state_t*)buf.data, ctx->round_key, TC_AES_FIXED_ROUNDS), buf.data);
}

#endif

/* A cipher failure part way through wipes buf and the chaining value in the
 * shared core, and clears the context here. */
#if TC_AES_ENABLE_CBC
TC_status TC_AES_CBC_encrypt(struct TC_AES_ctx* ctx, TC_buffer buffer)
{
  if (!AES_IV_MODE_VALID(ctx, buffer.data, buffer.capacity, TC_AES_BLOCKLEN))
    return TC_ERROR;
  const tc_aes_block_key key = tc_aes_ctx_key(ctx);
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  if (tc_block_cbc_encrypt(&cipher, ctx->iv, buffer.data, buffer.capacity) != TC_OK) {
    TC_AES_ctx_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_CBC_decrypt(struct TC_AES_ctx* ctx, TC_buffer buffer)
{
  if (!AES_IV_MODE_VALID(ctx, buffer.data, buffer.capacity, TC_AES_BLOCKLEN))
    return TC_ERROR;
  const tc_aes_block_key key = tc_aes_ctx_key(ctx);
  const tc_block_cipher cipher = tc_aes_block_cipher_inverse(&key);
  if (tc_block_cbc_decrypt(&cipher, ctx->iv, buffer.data, buffer.capacity) != TC_OK) {
    TC_AES_ctx_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}
#endif /* CBC */

#if TC_AES_ENABLE_CTR
TC_status TC_AES_CTR_crypt(struct TC_AES_ctx* ctx, TC_buffer buf)
{
  if (!AES_IV_MODE_VALID(ctx, buf.data, buf.capacity, 1))
    return TC_ERROR;
  const tc_block_ctr_state state = {ctx->iv, ctx->ctr_stream, &ctx->ctr_pos, &ctx->ctr_exhausted};
  if (!tc_block_ctr_request_ok(&state, TC_AES_BLOCKLEN, buf.capacity))
    return TC_ERROR;
  const tc_aes_block_key key = tc_aes_ctx_key(ctx);
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  if (tc_block_ctr_crypt(&cipher, &state, buf.data, buf.capacity) != TC_OK) {
    TC_AES_ctx_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}
#endif /* CTR */

#if TC_AES_ENABLE_OFB
TC_status TC_AES_OFB_crypt(struct TC_AES_ctx* ctx, TC_buffer buf)
{
  if (!AES_IV_MODE_VALID(ctx, buf.data, buf.capacity, 1) || ctx->ofb_pos > TC_AES_BLOCKLEN)
    return TC_ERROR;
  const tc_aes_block_key key = tc_aes_ctx_key(ctx);
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  if (tc_block_ofb_crypt(&cipher, ctx->iv, &ctx->ofb_pos, buf.data, buf.capacity) != TC_OK) {
    TC_AES_ctx_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}
#endif /* OFB */

#if TC_AES_ENABLE_DYNAMIC
TC_status TC_AES_dynamic_encrypt(const TC_AES_dynamic_key* ctx, TC_buffer block)
{
  if (block.capacity != TC_AES_BLOCKLEN ||
      !tc_block_mode_args(ctx, sizeof *ctx, block.data, block.capacity, 1) ||
      !tc_aes_dynamic_key_valid(ctx))
    return TC_ERROR;
  return tc_aes_block_result(
      tc_aes_cipher_rounds((state_t*)block.data, ctx->round_key, ctx->rounds), block.data);
}

TC_status TC_AES_dynamic_decrypt(const TC_AES_dynamic_key* ctx, TC_buffer block)
{
  if (block.capacity != TC_AES_BLOCKLEN ||
      !tc_block_mode_args(ctx, sizeof *ctx, block.data, block.capacity, 1) ||
      !tc_aes_dynamic_key_valid(ctx))
    return TC_ERROR;
  return tc_aes_block_result(
      tc_aes_inverse_rounds((state_t*)block.data, ctx->round_key, ctx->rounds), block.data);
}

/* The key, IV and buffer are pairwise disjoint. */
static int tc_aes_dynamic_cbc_valid(const TC_AES_dynamic_key* ctx, const uint8_t* iv,
                                    const uint8_t* buffer, size_t length)
{
  return tc_block_mode_args(ctx, sizeof *ctx, buffer, length, TC_AES_BLOCKLEN) &&
         tc_block_mode_args(ctx, sizeof *ctx, iv, TC_AES_BLOCKLEN, 1) &&
         tc_internal_ranges_disjoint(iv, TC_AES_BLOCKLEN, buffer, length) &&
         tc_aes_dynamic_key_valid(ctx);
}

TC_status TC_AES_dynamic_CBC_encrypt(const TC_AES_dynamic_key* ctx, TC_buffer iv, TC_buffer buffer)
{
  if (iv.capacity != TC_AES_BLOCKLEN ||
      !tc_aes_dynamic_cbc_valid(ctx, iv.data, buffer.data, buffer.capacity))
    return TC_ERROR;
  const tc_aes_block_key key = {ctx->round_key, ctx->rounds};
  const tc_block_cipher cipher = tc_aes_block_cipher(&key);
  return tc_block_cbc_encrypt(&cipher, iv.data, buffer.data, buffer.capacity);
}

TC_status TC_AES_dynamic_CBC_decrypt(const TC_AES_dynamic_key* ctx, TC_buffer iv, TC_buffer buffer)
{
  if (iv.capacity != TC_AES_BLOCKLEN ||
      !tc_aes_dynamic_cbc_valid(ctx, iv.data, buffer.data, buffer.capacity))
    return TC_ERROR;
  const tc_aes_block_key key = {ctx->round_key, ctx->rounds};
  const tc_block_cipher cipher = tc_aes_block_cipher_inverse(&key);
  return tc_block_cbc_decrypt(&cipher, iv.data, buffer.data, buffer.capacity);
}
#endif
#endif
