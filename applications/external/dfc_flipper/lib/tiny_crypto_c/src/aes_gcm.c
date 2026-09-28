/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aes_internal.h"

#if defined(TC_AES_ENABLE_GCM) && (TC_AES_ENABLE_GCM == 1)

#define TC_AES_GCM_PHASE_AAD   0u
#define TC_AES_GCM_PHASE_TEXT  1u
#define TC_AES_GCM_PHASE_FINAL 2u
#define TC_AES_GCM_DIRECTION_NONE    0u
#define TC_AES_GCM_DIRECTION_ENCRYPT 1u
#define TC_AES_GCM_DIRECTION_DECRYPT 2u

#if (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_BITWISE) || \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO && \
     (!TC_AES_WIDE_OPS || !defined(UINT64_MAX))) || \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE) || \
    (TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE && !defined(UINT64_MAX))
static void tc_aes_gcm_multiply_x(uint8_t value[TC_AES_BLOCKLEN])
{
  uint8_t carry = 0;
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
  {
    const uint8_t next_carry = (uint8_t)(value[i] & 1u);
    value[i] = (uint8_t)((value[i] >> 1) | (carry << 7));
    carry = next_carry;
  }
  value[0] ^= (uint8_t)(0xe1u & (uint8_t)(0u - carry));
}

/* Constant-time bytewise multiplication in GF(2^128). */
static void tc_aes_gcm_multiply_bitwise(uint8_t* result, const uint8_t* left,
                                 const uint8_t* right)
{
  uint8_t z[TC_AES_BLOCKLEN] = { 0 };
  uint8_t v[TC_AES_BLOCKLEN];
  unsigned bit;

  tc_aes_copy_bytes(v, right, TC_AES_BLOCKLEN);
  for (bit = 0; bit < 128; ++bit)
  {
    const uint8_t bit_mask = (uint8_t)(0u -
      (uint8_t)((left[bit / 8u] >> (7u - (bit % 8u))) & 1u));
    unsigned i;

    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      z[i] ^= (uint8_t)(v[i] & bit_mask);

    tc_aes_gcm_multiply_x(v);
  }
  tc_aes_copy_bytes(result, z, TC_AES_BLOCKLEN);
#if TC_ZEROIZE
  TC_secure_zero(z, sizeof(z));
  TC_secure_zero(v, sizeof(v));
#endif
}
#endif

#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE || \
    ((TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO) && TC_AES_WIDE_OPS)
#if defined(UINT64_MAX)
static void tc_aes_gcm_multiply_wide(uint8_t* result, const uint8_t* left,
                              const uint8_t* right)
{
  uint64_t xh = tc_internal_load_be64(left);
  uint64_t xl = tc_internal_load_be64(left + 8);
  uint64_t zh = 0;
  uint64_t zl = 0;
  uint64_t vh = tc_internal_load_be64(right);
  uint64_t vl = tc_internal_load_be64(right + 8);
  unsigned bit;

  for (bit = 0; bit < 128; ++bit)
  {
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
static void tc_aes_gcm_init_table(struct TC_AES_GCM_ctx* ctx)
{
  uint8_t input[TC_AES_BLOCKLEN] = { 0 };
  uint8_t entry;

  for (entry = 0; entry < 16; ++entry)
  {
    input[0] = (uint8_t)(entry << 4);
    tc_aes_gcm_multiply_bitwise(ctx->ghash_table[entry], input, ctx->H);
  }
#if TC_ZEROIZE
  TC_secure_zero(input, sizeof(input));
#endif
}

static void tc_aes_gcm_multiply_fast_table(uint8_t* result, const uint8_t* left,
                                    const struct TC_AES_GCM_ctx* ctx)
{
  uint8_t value[TC_AES_BLOCKLEN] = { 0 };
  uint8_t position = 32;
  uint8_t i;

  /* Horner evaluation runs from the least-significant nibble toward the
   * most-significant one. Each x^4 step advances the accumulated field power. */
  while (position > 0)
  {
    const uint8_t nibble_position = (uint8_t)(--position);
    const uint8_t nibble = (uint8_t)((nibble_position & 1u) == 0u ?
      left[nibble_position / 2u] >> 4 :
      left[nibble_position / 2u] & 0x0fu);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    tc_aes_gcm_multiply_x(value);
    for (i = 0; i < TC_AES_BLOCKLEN; ++i)
      value[i] ^= ctx->ghash_table[nibble][i];
  }
  tc_aes_copy_bytes(result, value, TC_AES_BLOCKLEN);
#if TC_ZEROIZE
  TC_secure_zero(value, sizeof(value));
#endif
}
#endif

static void tc_aes_gcm_multiply(uint8_t* result, const uint8_t* left,
                         const struct TC_AES_GCM_ctx* ctx)
{
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_HARDWARE
  TC_AES_GCM_hardware_multiply(result, left, ctx->H);
#elif TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  tc_aes_gcm_multiply_fast_table(result, left, ctx);
#elif TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_WIDE || \
      ((TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_AUTO) && TC_AES_WIDE_OPS)
#if defined(UINT64_MAX)
  tc_aes_gcm_multiply_wide(result, left, ctx->H);
#else
  tc_aes_gcm_multiply_bitwise(result, left, ctx->H);
#endif
#else
  tc_aes_gcm_multiply_bitwise(result, left, ctx->H);
#endif
}

static void tc_aes_gcm_ghash_block(struct TC_AES_GCM_ctx* ctx, const uint8_t* block)
{
  uint8_t value[TC_AES_BLOCKLEN];
  unsigned i;

  for (i = 0; i < TC_AES_BLOCKLEN; ++i)
    value[i] = (uint8_t)(ctx->S[i] ^ block[i]);
  tc_aes_gcm_multiply(ctx->S, value, ctx);
}

static void tc_aes_gcm_hash_bytes(struct TC_AES_GCM_ctx* ctx, const uint8_t* data,
                           size_t length)
{
  uint8_t block[TC_AES_BLOCKLEN] = { 0 };

  while (length >= TC_AES_BLOCKLEN)
  {
    tc_aes_gcm_ghash_block(ctx, data);
    data += TC_AES_BLOCKLEN;
    length -= TC_AES_BLOCKLEN;
  }
  if (length != 0)
  {
    tc_aes_copy_bytes(block, data, length);
    tc_aes_gcm_ghash_block(ctx, block);
  }
}

static void tc_aes_gcm_make_j0(struct TC_AES_GCM_ctx* ctx, const uint8_t* iv,
                        size_t iv_len)
{
  uint8_t length_block[TC_AES_BLOCKLEN] = { 0 };

  memset(ctx->S, 0, TC_AES_BLOCKLEN);
  memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
  if (iv_len == 12)
  {
    memset(ctx->J0, 0, TC_AES_BLOCKLEN);
    tc_aes_copy_bytes(ctx->J0, iv, iv_len);
    ctx->J0[15] = 1;
  }
  else
  {
    tc_aes_gcm_hash_bytes(ctx, iv, iv_len);
    tc_internal_store_be64(length_block + 8, (uint64_t)iv_len * 8u);
    tc_aes_gcm_ghash_block(ctx, length_block);
    tc_aes_copy_bytes(ctx->J0, ctx->S, TC_AES_BLOCKLEN);
  }
  memset(ctx->S, 0, TC_AES_BLOCKLEN);
  memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
}

static int tc_aes_gcm_length_is_valid(uint64_t current, size_t additional,
                               uint64_t limit)
{
  return (uint64_t)additional <= (limit - current);
}

static void tc_aes_gcm_pad_ghash(struct TC_AES_GCM_ctx* ctx)
{
  if (ctx->ghash_len != 0)
  {
    memset(ctx->ghash + ctx->ghash_len, 0,
           TC_AES_BLOCKLEN - ctx->ghash_len);
    tc_aes_gcm_ghash_block(ctx, ctx->ghash);
    ctx->ghash_len = 0;
    memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
  }
}

static void tc_aes_gcm_absorb(struct TC_AES_GCM_ctx* ctx, const uint8_t* data,
                       size_t length)
{
  while (length != 0)
  {
    const size_t available = TC_AES_BLOCKLEN - ctx->ghash_len;
    const size_t count = length < available ? length : available;
    tc_aes_copy_bytes(ctx->ghash + ctx->ghash_len, data, count);
    ctx->ghash_len = (uint8_t)(ctx->ghash_len + count);
    data += count;
    length -= count;
    if (ctx->ghash_len == TC_AES_BLOCKLEN)
    {
      tc_aes_gcm_ghash_block(ctx, ctx->ghash);
      ctx->ghash_len = 0;
      memset(ctx->ghash, 0, TC_AES_BLOCKLEN);
    }
  }
}

static void tc_aes_gcm_increment_counter(uint8_t* counter)
{
  unsigned i;
  for (i = 0; i < 4; ++i)
  {
    const unsigned offset = 15u - i;
    if (counter[offset] != 0xffu)
    {
      ++counter[offset];
      break;
    }
    counter[offset] = 0;
  }
}

static void tc_aes_gcm_finish_ghash(struct TC_AES_GCM_ctx* ctx)
{
  uint8_t length_block[TC_AES_BLOCKLEN] = { 0 };

  tc_aes_gcm_pad_ghash(ctx);
  tc_internal_store_be64(length_block, ctx->aad_len * 8u);
  tc_internal_store_be64(length_block + 8, ctx->text_len * 8u);
  tc_aes_gcm_ghash_block(ctx, length_block);
}

/*
 * SP 800-38D §5.2.1.2: t ∈ {128,120,112,104,96,64,32} bits only.
 * Fixed for the key for the life of the context (set at init).
 */
static int tc_aes_gcm_tag_length_is_valid(size_t tag_len)
{
  return tag_len == 16 || tag_len == 15 || tag_len == 14 ||
         tag_len == 13 || tag_len == 12 || tag_len == 8 ||
         tag_len == 4;
}

/*
 * Appendix C packet bound for short tags only (most permissive table row).
 * 96–128 bit tags have no Appendix C size cap. Overflow-safe for MCU math.
 * Lifetime decryption-invocation limits are not tracked here (no NVRAM/key
 * store); the application must rotate keys per Appendix C.
 */
static int tc_aes_gcm_packet_length_ok(const struct TC_AES_GCM_ctx* ctx,
                                uint64_t extra_text)
{
  uint64_t limit;
  uint64_t used;

  if (ctx->tag_len != 4 && ctx->tag_len != 8)
    return 1;

  limit = (ctx->tag_len == 4) ? TC_AES_GCM_SHORT_TAG4_MAX_PACKET
                              : TC_AES_GCM_SHORT_TAG8_MAX_PACKET;
  if (extra_text > limit)
    return 0;
  used = ctx->aad_len + ctx->text_len;
  if (used > limit - extra_text)
    return 0;
  return 1;
}

static void tc_aes_gcm_invalidate(struct TC_AES_GCM_ctx* ctx)
{
  TC_AES_GCM_clear(ctx);
  ctx->phase = TC_AES_GCM_PHASE_FINAL;
}

static TC_status tc_aes_gcm_make_tag(const struct TC_AES_GCM_ctx* ctx, uint8_t* tag)
{
  uint8_t mask[TC_AES_BLOCKLEN];
  uint8_t hash[TC_AES_BLOCKLEN];
  uint8_t i;
  TC_status status;

  tc_aes_copy_bytes(mask, ctx->J0, TC_AES_BLOCKLEN);
  status = tc_aes_cipher((state_t*)mask, ctx->key.round_key);
  if (status != TC_OK) goto done;
  tc_aes_copy_bytes(hash, ctx->S, TC_AES_BLOCKLEN);
  /* MSBt truncation: leading tag_len bytes of the 128-bit block. */
  for (i = 0; i < ctx->tag_len; ++i)
    tag[i] = (uint8_t)(mask[i] ^ hash[i]);
done:
#if TC_ZEROIZE
  TC_secure_zero(mask, sizeof(mask));
  TC_secure_zero(hash, sizeof(hash));
#endif
  return status;
}

TC_status TC_AES_GCM_init(struct TC_AES_GCM_ctx* ctx, const uint8_t* key,
                 const uint8_t* iv, size_t iv_len, size_t tag_len)
{
  uint8_t zero[TC_AES_BLOCKLEN] = { 0 };

  if (ctx == NULL || key == NULL || iv == NULL || iv_len == 0 ||
      (uint64_t)iv_len > TC_AES_GCM_MAX_IV_BYTES ||
      !tc_aes_gcm_tag_length_is_valid(tag_len))
    return TC_ERROR;

  if (TC_AES_key_init(&ctx->key, key) != TC_OK)
    return TC_ERROR;
  tc_aes_copy_bytes(ctx->H, zero, TC_AES_BLOCKLEN);
  if (tc_aes_cipher((state_t*)ctx->H, ctx->key.round_key) != TC_OK) {
    tc_aes_gcm_invalidate(ctx);
    return TC_ERROR;
  }
#if TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
  tc_aes_gcm_init_table(ctx);
#endif
  tc_aes_gcm_make_j0(ctx, iv, iv_len);
  tc_aes_copy_bytes(ctx->counter, ctx->J0, TC_AES_BLOCKLEN);
  tc_aes_copy_bytes(ctx->S, zero, TC_AES_BLOCKLEN);
  tc_aes_copy_bytes(ctx->ghash, zero, TC_AES_BLOCKLEN);
  ctx->aad_len = 0;
  ctx->text_len = 0;
  ctx->stream_pos = TC_AES_BLOCKLEN;
  ctx->ghash_len = 0;
  ctx->tag_len = (uint8_t)tag_len;
  ctx->phase = TC_AES_GCM_PHASE_AAD;
  ctx->direction = TC_AES_GCM_DIRECTION_NONE;
  return TC_OK;
}

TC_status TC_AES_GCM_aad_update(struct TC_AES_GCM_ctx* ctx, const uint8_t* aad,
                       size_t length)
{
  if (ctx == NULL || ctx->phase != TC_AES_GCM_PHASE_AAD ||
      (length != 0 && aad == NULL) ||
      !tc_aes_gcm_length_is_valid(ctx->aad_len, length, TC_AES_GCM_MAX_AAD_BYTES))
    return TC_ERROR;

  if (!tc_aes_gcm_packet_length_ok(ctx, (uint64_t)length))
    return TC_ERROR;

  tc_aes_gcm_absorb(ctx, aad, length);
  ctx->aad_len += (uint64_t)length;
  return TC_OK;
}

static int tc_aes_gcm_update(struct TC_AES_GCM_ctx* ctx, uint8_t* buf, size_t length,
                      int decrypt)
{
  size_t i;
  const size_t total_length = length;
  uint8_t* const output = buf;
  const uint8_t direction = decrypt ? TC_AES_GCM_DIRECTION_DECRYPT :
                                     TC_AES_GCM_DIRECTION_ENCRYPT;

  if (ctx == NULL || ctx->phase == TC_AES_GCM_PHASE_FINAL ||
      (length != 0 && buf == NULL) ||
      !tc_aes_gcm_length_is_valid(ctx->text_len, length, TC_AES_GCM_MAX_PLAINTEXT_BYTES) ||
      !tc_aes_gcm_packet_length_ok(ctx, (uint64_t)length))
    return TC_ERROR;
  if (ctx->direction != TC_AES_GCM_DIRECTION_NONE &&
      ctx->direction != direction)
    return TC_ERROR;
  if (ctx->direction == TC_AES_GCM_DIRECTION_NONE)
    ctx->direction = direction;

  if (ctx->phase == TC_AES_GCM_PHASE_AAD)
  {
    tc_aes_gcm_pad_ghash(ctx);
    ctx->phase = TC_AES_GCM_PHASE_TEXT;
  }

  while (length >= TC_AES_BLOCKLEN && ctx->stream_pos == TC_AES_BLOCKLEN &&
         ctx->ghash_len == 0)
  {
    uint8_t j;

    tc_aes_gcm_increment_counter(ctx->counter);
    tc_aes_copy_bytes(ctx->stream, ctx->counter, TC_AES_BLOCKLEN);
    if (tc_aes_cipher((state_t*)ctx->stream, ctx->key.round_key) != TC_OK) goto failed;
    if (decrypt)
      tc_aes_gcm_absorb(ctx, buf, TC_AES_BLOCKLEN);
    for (j = 0; j < TC_AES_BLOCKLEN; ++j)
      buf[j] ^= ctx->stream[j];
    if (!decrypt)
      tc_aes_gcm_absorb(ctx, buf, TC_AES_BLOCKLEN);
    buf += TC_AES_BLOCKLEN;
    length -= TC_AES_BLOCKLEN;
  }

  while (length != 0)
  {
    size_t available;
    size_t count;

    if (ctx->stream_pos == TC_AES_BLOCKLEN)
    {
      tc_aes_gcm_increment_counter(ctx->counter);
      tc_aes_copy_bytes(ctx->stream, ctx->counter, TC_AES_BLOCKLEN);
      if (tc_aes_cipher((state_t*)ctx->stream, ctx->key.round_key) != TC_OK) goto failed;
      ctx->stream_pos = 0;
    }

    available = TC_AES_BLOCKLEN - ctx->stream_pos;
    count = length < available ? length : available;
    if (decrypt)
      tc_aes_gcm_absorb(ctx, buf, count);
    for (i = 0; i < count; ++i)
      buf[i] ^= ctx->stream[ctx->stream_pos + i];
    if (!decrypt)
      tc_aes_gcm_absorb(ctx, buf, count);
    buf += count;
    length -= count;
    ctx->stream_pos = (uint8_t)(ctx->stream_pos + count);
  }
  ctx->text_len += (uint64_t)total_length;
  return TC_OK;
failed:
  TC_secure_zero(output, total_length);
  tc_aes_gcm_invalidate(ctx);
  return TC_ERROR;
}

TC_status TC_AES_GCM_encrypt_update(struct TC_AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length)
{
  return tc_aes_gcm_update(ctx, buf, length, 0);
}

TC_status TC_AES_GCM_decrypt_update(struct TC_AES_GCM_ctx* ctx, uint8_t* buf,
                           size_t length)
{
  return tc_aes_gcm_update(ctx, buf, length, 1);
}

TC_status TC_AES_GCM_encrypt_finish(struct TC_AES_GCM_ctx* ctx, uint8_t* tag)
{
  if (ctx == NULL || tag == NULL || ctx->phase == TC_AES_GCM_PHASE_FINAL ||
      !tc_aes_gcm_packet_length_ok(ctx, 0))
    return TC_ERROR;
  if (ctx->direction == TC_AES_GCM_DIRECTION_DECRYPT)
    return TC_ERROR;
  if (ctx->phase == TC_AES_GCM_PHASE_AAD)
  {
    tc_aes_gcm_pad_ghash(ctx);
    ctx->phase = TC_AES_GCM_PHASE_TEXT;
  }
  tc_aes_gcm_finish_ghash(ctx);
  if (tc_aes_gcm_make_tag(ctx, tag) != TC_OK) {
    tc_aes_gcm_invalidate(ctx);
    return TC_ERROR;
  }
  ctx->phase = TC_AES_GCM_PHASE_FINAL;
  return TC_OK;
}

TC_status TC_AES_GCM_decrypt_finish(struct TC_AES_GCM_ctx* ctx, const uint8_t* tag)
{
  uint8_t expected[TC_AES_BLOCKLEN];
  TC_status status;

  if (ctx == NULL || tag == NULL || ctx->phase == TC_AES_GCM_PHASE_FINAL ||
      !tc_aes_gcm_packet_length_ok(ctx, 0))
    return TC_ERROR;
  if (ctx->direction == TC_AES_GCM_DIRECTION_ENCRYPT)
    return TC_ERROR;
  if (ctx->phase == TC_AES_GCM_PHASE_AAD)
  {
    tc_aes_gcm_pad_ghash(ctx);
    ctx->phase = TC_AES_GCM_PHASE_TEXT;
  }
  tc_aes_gcm_finish_ghash(ctx);
  status = tc_aes_gcm_make_tag(ctx, expected);
  /* Authentication tags contain secrets, so comparison time must not reveal
   * the first byte that differs. */
  if (status == TC_OK) {
    status = TC_ct_equal(expected, tag, ctx->tag_len);
    ctx->phase = TC_AES_GCM_PHASE_FINAL;
  } else tc_aes_gcm_invalidate(ctx);
#if TC_ZEROIZE
  TC_secure_zero(expected, sizeof(expected));
#endif
  return status;
}

void TC_AES_GCM_clear(struct TC_AES_GCM_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_AES_GCM_encrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* plaintext, size_t plaintext_len,
                    uint8_t* ciphertext, uint8_t* tag, size_t tag_len)
{
  struct TC_AES_GCM_ctx ctx;
  int status;

  if (key == NULL || iv == NULL || iv_len == 0 ||
      (aad_len != 0 && aad == NULL) ||
      (plaintext_len != 0 && (plaintext == NULL || ciphertext == NULL)) ||
      tag == NULL ||
      !tc_aes_gcm_tag_length_is_valid(tag_len) ||
      (uint64_t)iv_len > TC_AES_GCM_MAX_IV_BYTES ||
      (uint64_t)aad_len > TC_AES_GCM_MAX_AAD_BYTES ||
      (uint64_t)plaintext_len > TC_AES_GCM_MAX_PLAINTEXT_BYTES ||
      !tc_aes_buffers_ok(plaintext, plaintext_len, ciphertext, plaintext_len) ||
      !tc_aes_buffers_disjoint(ciphertext, plaintext_len, tag, tag_len))
    return TC_ERROR;

  /* Short-tag Appendix C packet bound before any output write. */
  if (tag_len == 4 || tag_len == 8)
  {
    const uint64_t limit = (tag_len == 4) ? TC_AES_GCM_SHORT_TAG4_MAX_PACKET
                                          : TC_AES_GCM_SHORT_TAG8_MAX_PACKET;
    if ((uint64_t)aad_len > limit ||
        (uint64_t)plaintext_len > limit - (uint64_t)aad_len)
      return TC_ERROR;
  }

  if (TC_AES_GCM_init(&ctx, key, iv, iv_len, tag_len) != TC_OK)
    return TC_ERROR;
  if (TC_AES_GCM_aad_update(&ctx, aad, aad_len) != TC_OK)
  {
#if TC_ZEROIZE
    TC_AES_GCM_clear(&ctx);
#endif
    return TC_ERROR;
  }

  /* Copy only after all length/overlap checks and AAD accept. */
  if (plaintext != ciphertext && plaintext_len != 0)
    tc_aes_copy_bytes(ciphertext, plaintext, plaintext_len);

  status = TC_AES_GCM_encrypt_update(&ctx, ciphertext, plaintext_len);
  if (status == TC_OK)
    status = TC_AES_GCM_encrypt_finish(&ctx, tag);
  if (status != TC_OK && plaintext_len != 0)
    TC_secure_zero(ciphertext, plaintext_len);

#if TC_ZEROIZE
  TC_AES_GCM_clear(&ctx);
#endif
  return status;
}

TC_status TC_AES_GCM_decrypt(const uint8_t* key,
                    const uint8_t* iv, size_t iv_len,
                    const uint8_t* aad, size_t aad_len,
                    const uint8_t* ciphertext, size_t ciphertext_len,
                    const uint8_t* tag, size_t tag_len,
                    uint8_t* plaintext)
{
  struct TC_AES_GCM_ctx ctx;
  uint8_t expected[TC_AES_BLOCKLEN] = { 0 };
  TC_status status = TC_ERROR;

  if (key == NULL || iv == NULL || iv_len == 0 ||
      (aad_len != 0 && aad == NULL) ||
      (ciphertext_len != 0 && (ciphertext == NULL || plaintext == NULL)) ||
      tag == NULL ||
      !tc_aes_gcm_tag_length_is_valid(tag_len) ||
      (uint64_t)iv_len > TC_AES_GCM_MAX_IV_BYTES ||
      (uint64_t)aad_len > TC_AES_GCM_MAX_AAD_BYTES ||
      (uint64_t)ciphertext_len > TC_AES_GCM_MAX_PLAINTEXT_BYTES ||
      !tc_aes_buffers_ok(ciphertext, ciphertext_len, plaintext, ciphertext_len) ||
      !tc_aes_buffers_disjoint(plaintext, ciphertext_len, tag, tag_len))
    return TC_ERROR;

  if (tag_len == 4 || tag_len == 8)
  {
    const uint64_t limit = (tag_len == 4) ? TC_AES_GCM_SHORT_TAG4_MAX_PACKET
                                          : TC_AES_GCM_SHORT_TAG8_MAX_PACKET;
    if ((uint64_t)aad_len > limit ||
        (uint64_t)ciphertext_len > limit - (uint64_t)aad_len)
      return TC_ERROR;
  }

  if (TC_AES_GCM_init(&ctx, key, iv, iv_len, tag_len) != TC_OK)
    return TC_ERROR;
  if (TC_AES_GCM_aad_update(&ctx, aad, aad_len) != TC_OK)
    goto done;

  /* Absorb ciphertext into GHASH without decrypting (auth-before-release). */
  if (ctx.phase == TC_AES_GCM_PHASE_AAD)
  {
    tc_aes_gcm_pad_ghash(&ctx);
    ctx.phase = TC_AES_GCM_PHASE_TEXT;
  }
  if (!tc_aes_gcm_length_is_valid(ctx.text_len, ciphertext_len,
                           TC_AES_GCM_MAX_PLAINTEXT_BYTES) ||
      !tc_aes_gcm_packet_length_ok(&ctx, (uint64_t)ciphertext_len))
    goto done;
  tc_aes_gcm_absorb(&ctx, ciphertext, ciphertext_len);
  ctx.text_len += (uint64_t)ciphertext_len;
  ctx.direction = TC_AES_GCM_DIRECTION_DECRYPT;

  tc_aes_gcm_finish_ghash(&ctx);
  if (tc_aes_gcm_make_tag(&ctx, expected) != TC_OK) goto done;
  status = TC_ct_equal(expected, tag, ctx.tag_len);
  ctx.phase = TC_AES_GCM_PHASE_FINAL;

  if (status != TC_OK)
  {
    /* In-place callers supplied ciphertext in this buffer. Wipe it so an
     * authentication failure cannot leave unauthenticated data behind. */
    if (plaintext != NULL && plaintext == ciphertext && ciphertext_len != 0)
      TC_secure_zero(plaintext, ciphertext_len);
    goto done;
  }

  /* Tag OK: produce keystream and decrypt. Reset counter path from J0. */
  {
    uint8_t counter[TC_AES_BLOCKLEN];
    uint8_t stream[TC_AES_BLOCKLEN];
    size_t offset = 0;

    tc_aes_copy_bytes(counter, ctx.J0, TC_AES_BLOCKLEN);
    while (offset < ciphertext_len)
    {
      const size_t count = ciphertext_len - offset < TC_AES_BLOCKLEN ?
                           ciphertext_len - offset : TC_AES_BLOCKLEN;
      uint8_t j;

      tc_aes_gcm_increment_counter(counter);
      tc_aes_copy_bytes(stream, counter, TC_AES_BLOCKLEN);
      status = tc_aes_cipher((state_t*)stream, ctx.key.round_key);
      if (status != TC_OK) break;
      for (j = 0; j < (uint8_t)count; ++j)
        plaintext[offset + j] =
          (uint8_t)(ciphertext[offset + j] ^ stream[j]);
      offset += count;
    }
#if TC_ZEROIZE
    TC_secure_zero(counter, sizeof(counter));
    TC_secure_zero(stream, sizeof(stream));
#endif
  }
  if (status != TC_OK && ciphertext_len != 0)
    TC_secure_zero(plaintext, ciphertext_len);

done:
#if TC_ZEROIZE
  TC_AES_GCM_clear(&ctx);
  TC_secure_zero(expected, sizeof(expected));
#endif
  return status;
}

#endif
