/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES-CMAC (NIST SP 800-38B): one-shot, streaming and dynamic-key forms,
 * over the shared MAC core. */
#include "aes_mac_core_internal.h"

#if TC_AES_ENABLE_CMAC || TC_AES_ENABLE_DYNAMIC
static TC_status tc_aes_cmac_update(const uint8_t* key, uint8_t rounds, uint8_t mac[16],
                                    uint8_t buffer[16], uint8_t* used, const uint8_t* data,
                                    size_t length)
{
  const tc_aes_block_key mac_key = {key, rounds};
  const tc_block_cipher cipher = tc_aes_block_cipher(&mac_key);
  return tc_mac_cbc_update(&cipher, mac, buffer, used, data, length, 1);
}

static TC_status tc_aes_cmac_final(const uint8_t* key, uint8_t rounds, uint8_t mac[16],
                                   uint8_t buffer[16], uint8_t used, const uint8_t k1[16],
                                   const uint8_t k2[16], uint8_t tag[16])
{
  const tc_aes_block_key mac_key = {key, rounds};
  const tc_block_cipher cipher = tc_aes_block_cipher(&mac_key);
  return tc_mac_cmac_final(&cipher, mac, buffer, used, k1, k2, tag);
}
#endif

#if TC_AES_ENABLE_DYNAMIC
TC_status TC_AES_dynamic_CMAC_init(TC_AES_dynamic_CMAC* ctx, TC_bytes key)
{
  if (!ctx)
    return TC_ERROR;
  const int disjoint = tc_internal_ranges_disjoint(ctx, sizeof *ctx, key.data, key.length);
  TC_AES_dynamic_CMAC_clear(ctx);
  if (!disjoint || TC_AES_dynamic_key_init(&ctx->key, key) != TC_OK)
    return TC_ERROR;
  if (tc_aes_cmac_generate_subkeys(ctx->key.round_key, ctx->key.rounds, ctx->k1, ctx->k2) !=
      TC_OK) {
    TC_AES_dynamic_CMAC_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_dynamic_CMAC_update(TC_AES_dynamic_CMAC* ctx, TC_bytes data)
{
  if (!tc_block_mode_args(ctx, sizeof *ctx, data.data, data.length, 1) ||
      !tc_aes_dynamic_key_valid(&ctx->key) || ctx->used > TC_AES_BLOCKLEN)
    return TC_ERROR;
  if (tc_aes_cmac_update(ctx->key.round_key, ctx->key.rounds, ctx->mac, ctx->buffer, &ctx->used,
                         data.data, data.length) != TC_OK) {
    TC_AES_dynamic_CMAC_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_dynamic_CMAC_final(TC_AES_dynamic_CMAC* ctx, TC_buffer tag)
{
  TC_status status;
  if (tag.capacity < TC_AES_BLOCKLEN ||
      !tc_block_mode_args(ctx, sizeof *ctx, tag.data, TC_AES_BLOCKLEN, 1) ||
      !tc_aes_dynamic_key_valid(&ctx->key) || ctx->used > TC_AES_BLOCKLEN)
    return TC_ERROR;
  status = tc_aes_cmac_final(ctx->key.round_key, ctx->key.rounds, ctx->mac, ctx->buffer, ctx->used,
                             ctx->k1, ctx->k2, tag.data);
  TC_AES_dynamic_CMAC_clear(ctx);
  return status;
}

void TC_AES_dynamic_CMAC_clear(TC_AES_dynamic_CMAC* ctx)
{
  if (ctx)
    TC_secure_zero(ctx, sizeof *ctx);
}
#endif

#if TC_AES_ENABLE_CMAC
/* One-shot CMAC over the streaming context. The tag is the leading tag_len
 * bytes of T (SP 800-38B section 6.2 step 7). short_tag selects the lengths
 * below TC_MIN_TAG_LEN (Appendix A.2). */
static TC_status tc_aes_cmac_oneshot(TC_bytes key, TC_bytes msg, TC_buffer tag, int short_tag)
{
  struct TC_AES_CMAC_ctx ctx;
  uint8_t full[TC_AES_BLOCKLEN];
  TC_status status;

  if (!tc_internal_span_valid(tag.data, tag.capacity) ||
      !tc_internal_tag_length_allowed(tag.capacity, TC_AES_CMAC_TAG_MAX, short_tag) ||
      !tc_internal_span_valid(msg.data, msg.length))
    return TC_ERROR;

  status = TC_AES_CMAC_init(&ctx, key);
  if (status == TC_OK)
    status = TC_AES_CMAC_update(&ctx, msg);
  if (status == TC_OK)
    status = TC_AES_CMAC_final(&ctx, (TC_buffer){full, sizeof full});
  if (status == TC_OK)
    memcpy(tag.data, full, tag.capacity);
  TC_secure_zero(full, sizeof(full));
  TC_AES_CMAC_ctx_clear(&ctx);
  return status;
}

static TC_status tc_aes_cmac_verify_oneshot(TC_bytes key, TC_bytes msg, TC_bytes tag, int short_tag)
{
  uint8_t computed[TC_AES_CMAC_TAG_MAX];
  if (tag.data == NULL ||
      !tc_internal_tag_length_allowed(tag.length, TC_AES_CMAC_TAG_MAX, short_tag))
    return TC_ERROR;
  return tc_internal_verify_tag(
      tc_aes_cmac_oneshot(key, msg, (TC_buffer){computed, tag.length}, short_tag), computed,
      sizeof computed, tag.data, tag.length);
}

TC_status TC_AES_CMAC(TC_bytes key, TC_bytes msg, TC_buffer tag)
{
  return tc_aes_cmac_oneshot(key, msg, tag, 0);
}

TC_status TC_AES_CMAC_verify(TC_bytes key, TC_bytes msg, TC_bytes tag)
{
  return tc_aes_cmac_verify_oneshot(key, msg, tag, 0);
}

TC_status TC_AES_CMAC_short_tag(TC_bytes key, TC_bytes msg, TC_buffer tag)
{
  return tc_aes_cmac_oneshot(key, msg, tag, 1);
}

TC_status TC_AES_CMAC_verify_short_tag(TC_bytes key, TC_bytes msg, TC_bytes tag)
{
  return tc_aes_cmac_verify_oneshot(key, msg, tag, 1);
}

TC_status TC_AES_CMAC_init(struct TC_AES_CMAC_ctx* ctx, TC_bytes key)
{
  if (!ctx)
    return TC_ERROR;
  /* Check the whole context before clearing it. Clearing first would wipe a
   * key staged in k1, k2, mac or buf and then key the context with zeros. */
  const int disjoint = key.length == TC_AES_KEYLEN &&
                       tc_internal_ranges_disjoint(ctx, sizeof *ctx, key.data, key.length);
  TC_AES_CMAC_ctx_clear(ctx);
  if (!disjoint || TC_AES_key_init(&ctx->key, key) != TC_OK)
    return TC_ERROR;
  if (tc_aes_cmac_generate_subkeys(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->k1, ctx->k2) !=
      TC_OK) {
    TC_AES_CMAC_ctx_clear(ctx);
    return TC_ERROR;
  }
  ctx->active = 1;
  return TC_OK;
}

TC_status TC_AES_CMAC_update(struct TC_AES_CMAC_ctx* ctx, TC_bytes data)
{
  if (!tc_block_mode_args(ctx, sizeof *ctx, data.data, data.length, 1) || ctx->active != 1 ||
      ctx->buf_len > TC_AES_BLOCKLEN)
    return TC_ERROR;
  if (tc_aes_cmac_update(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->mac, ctx->buf, &ctx->buf_len,
                         data.data, data.length) != TC_OK) {
    TC_AES_CMAC_ctx_clear(ctx);
    return TC_ERROR;
  }
  return TC_OK;
}

TC_status TC_AES_CMAC_final(struct TC_AES_CMAC_ctx* ctx, TC_buffer tag)
{
  TC_status status;
  if (tag.capacity < TC_AES_CMAC_TAG_MAX ||
      !tc_block_mode_args(ctx, sizeof *ctx, tag.data, TC_AES_CMAC_TAG_MAX, 1) || ctx->active != 1 ||
      ctx->buf_len > TC_AES_BLOCKLEN)
    return TC_ERROR;
  status = tc_aes_cmac_final(ctx->key.round_key, TC_AES_FIXED_ROUNDS, ctx->mac, ctx->buf,
                             ctx->buf_len, ctx->k1, ctx->k2, tag.data);
  /* The context is one-shot: it is cleared after success and failure. */
  TC_AES_CMAC_ctx_clear(ctx);
  return status;
}

void TC_AES_CMAC_ctx_clear(struct TC_AES_CMAC_ctx* ctx)
{
  if (ctx)
    TC_secure_zero(ctx, sizeof(*ctx));
}

#endif /* CMAC */
