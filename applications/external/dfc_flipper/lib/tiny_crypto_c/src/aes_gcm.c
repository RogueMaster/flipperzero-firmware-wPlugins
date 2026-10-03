/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES-GCM authenticated encryption (NIST SP 800-38D): streaming and one-shot
 * encryption, one-shot decryption and the short-tag packet limits of
 * appendix C. GHASH lives in aes_ghash.c. */
#include "aes_internal.h"
#include "aes_ghash_internal.h"

#if TC_AES_ENABLE_GCM

#define TC_AES_GCM_PHASE_UNINIT 0u
#define TC_AES_GCM_PHASE_AAD 1u
#define TC_AES_GCM_PHASE_TEXT 2u
#define TC_AES_GCM_PHASE_FINAL 3u

static void tc_aes_gcm_make_j0(struct TC_AES_GCM_ctx* ctx, const uint8_t* iv, size_t iv_len)
{
  uint8_t length_block[TC_AES_BLOCKLEN] = {0};

  memset(ctx->s, 0, TC_AES_BLOCKLEN);
  memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
  if (iv_len == 12) {
    memset(ctx->j0, 0, TC_AES_BLOCKLEN);
    memcpy(ctx->j0, iv, iv_len);
    ctx->j0[15] = 1;
  } else {
    tc_aes_gcm_hash_bytes(ctx, iv, iv_len);
    tc_internal_store_be64(length_block + 8, (uint64_t)iv_len * 8u);
    tc_aes_gcm_ghash_block(ctx, length_block);
    memcpy(ctx->j0, ctx->s, TC_AES_BLOCKLEN);
  }
  memset(ctx->s, 0, TC_AES_BLOCKLEN);
  memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
}

static int tc_aes_gcm_length_is_valid(uint64_t current, size_t additional, uint64_t limit)
{
  return (uint64_t)additional <= (limit - current);
}

static void tc_aes_gcm_pad_ghash(struct TC_AES_GCM_ctx* ctx)
{
  if (ctx->ghash_len != 0) {
    memset(ctx->ghash + ctx->ghash_len, 0, TC_AES_BLOCKLEN - ctx->ghash_len);
    tc_aes_gcm_ghash_block(ctx, ctx->ghash);
    ctx->ghash_len = 0;
    memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
  }
}

static void tc_aes_gcm_start_text(struct TC_AES_GCM_ctx* ctx)
{
  if (ctx->phase != TC_AES_GCM_PHASE_AAD)
    return;
  tc_aes_gcm_pad_ghash(ctx);
  ctx->phase = TC_AES_GCM_PHASE_TEXT;
}

static void tc_aes_gcm_absorb(struct TC_AES_GCM_ctx* ctx, const uint8_t* data, size_t length)
{
  while (length != 0) {
    if (ctx->ghash_len == 0 && length >= TC_AES_BLOCKLEN) {
      tc_aes_gcm_ghash_block(ctx, data);
      data += TC_AES_BLOCKLEN;
      length -= TC_AES_BLOCKLEN;
      continue;
    }
    const size_t available = TC_AES_BLOCKLEN - ctx->ghash_len;
    const size_t count = length < available ? length : available;
    memcpy(ctx->ghash + ctx->ghash_len, data, count);
    ctx->ghash_len = (uint8_t)(ctx->ghash_len + count);
    data += count;
    length -= count;
    if (ctx->ghash_len == TC_AES_BLOCKLEN) {
      tc_aes_gcm_ghash_block(ctx, ctx->ghash);
      ctx->ghash_len = 0;
      memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
    }
  }
}

/* inc32 (SP 800-38D section 6.2): only the low 32 bits of the counter block
 * change, and they wrap. */
static void tc_aes_gcm_increment32(uint8_t* counter)
{
  /* inc32 wraps by definition, so the carry is discarded. */
  (void)tc_internal_increment_be(counter + 12, 4);
}

/* GCTR (SP 800-38D section 6.5): XOR the counter keystream into buf. A
 * partial keystream block left by the previous call is used first. */
static TC_status tc_aes_gcm_gctr(struct TC_AES_GCM_ctx* ctx, uint8_t* buf, size_t length)
{
  while (length != 0) {
    size_t available;
    size_t count;
    size_t i;

    if (ctx->stream_pos == TC_AES_BLOCKLEN) {
      tc_aes_gcm_increment32(ctx->counter);
      memcpy(ctx->stream, ctx->counter, TC_AES_BLOCKLEN);
      if (tc_aes_cipher((state_t*)ctx->stream, ctx->key.round_key) != TC_OK)
        return TC_ERROR;
      ctx->stream_pos = 0;
    }
    available = TC_AES_BLOCKLEN - ctx->stream_pos;
    count = length < available ? length : available;
    for (i = 0; i < count; ++i)
      buf[i] ^= ctx->stream[ctx->stream_pos + i];
    buf += count;
    length -= count;
    ctx->stream_pos = (uint8_t)(ctx->stream_pos + count);
  }
  return TC_OK;
}

static void tc_aes_gcm_finish_ghash(struct TC_AES_GCM_ctx* ctx)
{
  uint8_t length_block[TC_AES_BLOCKLEN] = {0};

  tc_aes_gcm_pad_ghash(ctx);
  tc_internal_store_be64(length_block, ctx->aad_len * 8u);
  tc_internal_store_be64(length_block + 8, ctx->text_len * 8u);
  tc_aes_gcm_ghash_block(ctx, length_block);
}

/* SP 800-38D §5.2.1.2 permits these lengths. Short tags need explicit use. */
static int tc_aes_gcm_tag_length_is_allowed(size_t tag_len, int short_tag)
{
  return short_tag ? (tag_len == 4 || tag_len == 8) : (tag_len >= 12 && tag_len <= 16);
}

/*
 * Appendix C packet bound for short tags only (most permissive table row).
 * 96–128 bit tags have no Appendix C size cap. Overflow-safe for MCU math.
 * The application tracks lifetime decryption-invocation limits and rotates
 * keys per Appendix C. The library has no NVRAM or key store.
 */
static int tc_aes_gcm_packet_lengths_ok(size_t tag_len, uint64_t aad_len, uint64_t text_len,
                                        uint64_t extra_text)
{
  uint64_t limit;

  if (tag_len != 4 && tag_len != 8)
    return 1;

  limit = (tag_len == 4) ? TC_AES_GCM_SHORT_TAG4_MAX_PACKET : TC_AES_GCM_SHORT_TAG8_MAX_PACKET;
  if (aad_len > limit || text_len > limit - aad_len)
    return 0;
  return extra_text <= limit - aad_len - text_len;
}

static int tc_aes_gcm_packet_length_ok(const struct TC_AES_GCM_ctx* ctx, uint64_t extra_text)
{
  return tc_aes_gcm_packet_lengths_ok(ctx->tag_len, ctx->aad_len, ctx->text_len, extra_text);
}

static void tc_aes_gcm_invalidate(struct TC_AES_GCM_ctx* ctx)
{
  TC_AES_GCM_ctx_clear(ctx);
  ctx->phase = TC_AES_GCM_PHASE_FINAL;
}

static TC_status tc_aes_gcm_make_tag(const struct TC_AES_GCM_ctx* ctx, uint8_t* tag)
{
  uint8_t mask[TC_AES_BLOCKLEN];
  uint8_t hash[TC_AES_BLOCKLEN];
  uint8_t i;
  TC_status status;

  memcpy(mask, ctx->j0, TC_AES_BLOCKLEN);
  status = tc_aes_cipher((state_t*)mask, ctx->key.round_key);
  if (status != TC_OK)
    goto done;
  memcpy(hash, ctx->s, TC_AES_BLOCKLEN);
  /* MSBt truncation: leading tag_len bytes of the 128-bit block. */
  for (i = 0; i < ctx->tag_len; ++i)
    tag[i] = (uint8_t)(mask[i] ^ hash[i]);
done:
  TC_secure_zero(mask, sizeof(mask));
  TC_secure_zero(hash, sizeof(hash));
  return status;
}

static TC_status tc_aes_gcm_init_impl(struct TC_AES_GCM_ctx* ctx, TC_bytes key, TC_bytes nonce,
                                      size_t tag_len, int short_tag)
{
  uint8_t zero[TC_AES_BLOCKLEN] = {0};
  const uint8_t* iv = nonce.data;
  const size_t iv_len = nonce.length;

  if (ctx == NULL)
    return TC_ERROR;
  if ((key.data != NULL && !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), key.data, key.length)) ||
      (iv != NULL && iv_len <= TC_AES_GCM_MAX_IV_BYTES &&
       !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), iv, iv_len))) {
    TC_AES_GCM_ctx_clear(ctx);
    return TC_ERROR;
  }
  TC_AES_GCM_ctx_clear(ctx);
  if (key.length != TC_AES_KEYLEN || key.data == NULL || iv == NULL || iv_len == 0 ||
      (uint64_t)iv_len > TC_AES_GCM_MAX_IV_BYTES ||
      !tc_aes_gcm_tag_length_is_allowed(tag_len, short_tag))
    return TC_ERROR;

  if (TC_AES_key_init(&ctx->key, key) != TC_OK)
    return TC_ERROR;
  memcpy(ctx->h, zero, TC_AES_BLOCKLEN);
  if (tc_aes_cipher((state_t*)ctx->h, ctx->key.round_key) != TC_OK) {
    tc_aes_gcm_invalidate(ctx);
    return TC_ERROR;
  }
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  tc_aes_gcm_init_table(ctx);
#endif
  tc_aes_gcm_make_j0(ctx, iv, iv_len);
  memcpy(ctx->counter, ctx->j0, TC_AES_BLOCKLEN);
  memcpy(ctx->s, zero, TC_AES_BLOCKLEN);
  memcpy(ctx->ghash, zero, TC_AES_BLOCKLEN);
  ctx->aad_len = 0;
  ctx->text_len = 0;
  ctx->stream_pos = TC_AES_BLOCKLEN;
  ctx->ghash_len = 0;
  ctx->tag_len = (uint8_t)tag_len;
  ctx->phase = TC_AES_GCM_PHASE_AAD;
  return TC_OK;
}

TC_status TC_AES_GCM_init(struct TC_AES_GCM_ctx* ctx, TC_bytes key, TC_bytes iv, size_t tag_len)
{
  return tc_aes_gcm_init_impl(ctx, key, iv, tag_len, 0);
}

TC_status TC_AES_GCM_init_short_tag(struct TC_AES_GCM_ctx* ctx, TC_bytes key, TC_bytes iv,
                                    size_t tag_len)
{
  return tc_aes_gcm_init_impl(ctx, key, iv, tag_len, 1);
}

TC_status TC_AES_GCM_aad_update(struct TC_AES_GCM_ctx* ctx, TC_bytes aad_span)
{
  const uint8_t* aad = aad_span.data;
  const size_t length = aad_span.length;
  if (ctx == NULL || ctx->phase != TC_AES_GCM_PHASE_AAD || (length != 0 && aad == NULL) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), aad, length) ||
      !tc_aes_gcm_length_is_valid(ctx->aad_len, length, TC_AES_GCM_MAX_AAD_BYTES))
    return TC_ERROR;

  if (!tc_aes_gcm_packet_length_ok(ctx, (uint64_t)length))
    return TC_ERROR;

  tc_aes_gcm_absorb(ctx, aad, length);
  ctx->aad_len += (uint64_t)length;
  return TC_OK;
}

TC_status TC_AES_GCM_encrypt_update(struct TC_AES_GCM_ctx* ctx, TC_buffer buffer)
{
  uint8_t* buf = buffer.data;
  const size_t length = buffer.capacity;
  if (ctx == NULL || ctx->phase == TC_AES_GCM_PHASE_UNINIT ||
      ctx->phase == TC_AES_GCM_PHASE_FINAL || (length != 0 && buf == NULL) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), buf, length) ||
      !tc_aes_gcm_length_is_valid(ctx->text_len, length, TC_AES_GCM_MAX_PLAINTEXT_BYTES) ||
      !tc_aes_gcm_packet_length_ok(ctx, (uint64_t)length))
    return TC_ERROR;

  tc_aes_gcm_start_text(ctx);
  if (tc_aes_gcm_gctr(ctx, buf, length) != TC_OK) {
    TC_secure_zero(buf, length);
    tc_aes_gcm_invalidate(ctx);
    return TC_ERROR;
  }
  tc_aes_gcm_absorb(ctx, buf, length);
  ctx->text_len += (uint64_t)length;
  return TC_OK;
}

TC_status TC_AES_GCM_encrypt_finish(struct TC_AES_GCM_ctx* ctx, TC_buffer output)
{
  uint8_t* tag = output.data;
  TC_status status;

  if (ctx == NULL || tag == NULL || ctx->phase == TC_AES_GCM_PHASE_UNINIT ||
      ctx->phase == TC_AES_GCM_PHASE_FINAL || output.capacity < ctx->tag_len ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), tag, ctx->tag_len) ||
      !tc_aes_gcm_packet_length_ok(ctx, 0))
    return TC_ERROR;
  tc_aes_gcm_start_text(ctx);
  tc_aes_gcm_finish_ghash(ctx);
  /* Finish consumes the context on success and on failure. */
  status = tc_aes_gcm_make_tag(ctx, tag);
  tc_aes_gcm_invalidate(ctx);
  return status;
}

void TC_AES_GCM_ctx_clear(struct TC_AES_GCM_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

/* Checks shared by one-shot encrypt and decrypt. tag is the tag storage in
 * either direction. */
static int tc_aes_gcm_oneshot_args_ok(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes input,
                                      TC_buffer output, TC_bytes tag, int short_tag)
{
  return key.data != NULL && key.length == TC_AES_KEYLEN && iv.data != NULL && iv.length != 0 &&
         tc_internal_span_valid(aad.data, aad.length) && tag.data != NULL &&
         tc_aes_gcm_tag_length_is_allowed(tag.length, short_tag) &&
         (uint64_t)iv.length <= TC_AES_GCM_MAX_IV_BYTES &&
         (uint64_t)aad.length <= TC_AES_GCM_MAX_AAD_BYTES &&
         (uint64_t)input.length <= TC_AES_GCM_MAX_PLAINTEXT_BYTES &&
         tc_aes_text_ok(input, output) &&
         tc_internal_ranges_disjoint(output.data, input.length, tag.data, tag.length) &&
         tc_aes_gcm_packet_lengths_ok(tag.length, (uint64_t)aad.length, (uint64_t)input.length, 0);
}

static TC_status tc_aes_gcm_encrypt_impl(TC_bytes key, TC_bytes iv, TC_bytes aad,
                                         TC_bytes plaintext, TC_buffer ciphertext, TC_buffer tag,
                                         int short_tag)
{
  struct TC_AES_GCM_ctx ctx;
  TC_status status;

  if (!tc_aes_gcm_oneshot_args_ok(key, iv, aad, plaintext, ciphertext,
                                  (TC_bytes){tag.data, tag.capacity}, short_tag))
    return TC_ERROR;

  status = tc_aes_gcm_init_impl(&ctx, key, iv, tag.capacity, short_tag);
  if (status == TC_OK)
    status = TC_AES_GCM_aad_update(&ctx, aad);
  if (status == TC_OK) {
    /* AAD is consumed before the text output is written, so it may overlap. */
    if (plaintext.data != ciphertext.data && plaintext.length != 0)
      memcpy(ciphertext.data, plaintext.data, plaintext.length);
    status = TC_AES_GCM_encrypt_update(&ctx, (TC_buffer){ciphertext.data, plaintext.length});
  }
  if (status == TC_OK)
    status = TC_AES_GCM_encrypt_finish(&ctx, tag);
  if (status != TC_OK && plaintext.length != 0)
    TC_secure_zero(ciphertext.data, plaintext.length);

  TC_AES_GCM_ctx_clear(&ctx);
  return status;
}

/* SP 800-38D section 7.2. The tag is computed over the AAD and ciphertext
 * and compared before GCTR writes any plaintext. The one-shot contract keeps
 * the input unchanged for the call, so each pass reads the same ciphertext. */
static TC_status tc_aes_gcm_decrypt_impl(TC_bytes key, TC_bytes iv, TC_bytes aad,
                                         TC_bytes ciphertext, TC_bytes tag, TC_buffer plaintext,
                                         int short_tag)
{
  struct TC_AES_GCM_ctx ctx;
  uint8_t expected[TC_AES_BLOCKLEN] = {0};
  TC_status status;
  const size_t length = ciphertext.length;

  if (!tc_aes_gcm_oneshot_args_ok(key, iv, aad, ciphertext, plaintext, tag, short_tag))
    return TC_ERROR;

  status = tc_aes_gcm_init_impl(&ctx, key, iv, tag.length, short_tag);
  if (status == TC_OK)
    status = TC_AES_GCM_aad_update(&ctx, aad);
  if (status == TC_OK) {
    tc_aes_gcm_start_text(&ctx);
    tc_aes_gcm_absorb(&ctx, ciphertext.data, length);
    ctx.text_len = (uint64_t)length;
    tc_aes_gcm_finish_ghash(&ctx);
    status = tc_aes_gcm_make_tag(&ctx, expected);
  }
  /* Tags are secret until checked. The comparison runs in constant time. */
  if (status == TC_OK)
    status = TC_ct_equal((TC_bytes){expected, ctx.tag_len}, tag);
  if (status == TC_OK && length != 0) {
    if (plaintext.data != ciphertext.data)
      memcpy(plaintext.data, ciphertext.data, length);
    status = tc_aes_gcm_gctr(&ctx, plaintext.data, length);
  }
  if (status != TC_OK && length != 0)
    TC_secure_zero(plaintext.data, length);

  TC_AES_GCM_ctx_clear(&ctx);
  TC_secure_zero(expected, sizeof(expected));
  return status;
}

TC_status TC_AES_GCM_encrypt(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes plaintext,
                             TC_buffer ciphertext, TC_buffer tag)
{
  return tc_aes_gcm_encrypt_impl(key, iv, aad, plaintext, ciphertext, tag, 0);
}

TC_status TC_AES_GCM_encrypt_short_tag(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes plaintext,
                                       TC_buffer ciphertext, TC_buffer tag)
{
  return tc_aes_gcm_encrypt_impl(key, iv, aad, plaintext, ciphertext, tag, 1);
}

TC_status TC_AES_GCM_decrypt(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes ciphertext,
                             TC_bytes tag, TC_buffer plaintext)
{
  return tc_aes_gcm_decrypt_impl(key, iv, aad, ciphertext, tag, plaintext, 0);
}

TC_status TC_AES_GCM_decrypt_short_tag(TC_bytes key, TC_bytes iv, TC_bytes aad, TC_bytes ciphertext,
                                       TC_bytes tag, TC_buffer plaintext)
{
  return tc_aes_gcm_decrypt_impl(key, iv, aad, ciphertext, tag, plaintext, 1);
}

#endif
