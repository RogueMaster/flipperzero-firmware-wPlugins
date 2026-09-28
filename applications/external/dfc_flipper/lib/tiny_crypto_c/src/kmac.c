/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/kmac.h>

#if TC_ENABLE_KMAC256
#include "internal.h"

#define TC_KMAC_RATE 136u

static uint64_t tc_kmac_rotate(uint64_t value, unsigned shift)
{
  return shift ? (value << shift) | (value >> (64u - shift)) : value;
}

/* Rearrange lanes in place to save 200 bytes of stack. Generate the round
 * constants with an LFSR so AVR builds don't need a table in RAM. */
static void tc_kmac_permute(uint64_t* a)
{
  uint64_t c[5], t, d;
  unsigned round, x, y, i, j;
  uint8_t lfsr = 1;
  for (round = 0; round < 24; ++round) {
    for (x = 0; x < 5; ++x)
      c[x] = a[x] ^ a[x+5] ^ a[x+10] ^ a[x+15] ^ a[x+20];
    for (x = 0; x < 5; ++x) {
      d = c[(x+4)%5] ^ tc_kmac_rotate(c[(x+1)%5], 1);
      for (y = 0; y < 25; y += 5) a[y+x] ^= d;
    }
    x = 1; y = 0; t = a[1];
    for (i = 0; i < 24; ++i) {
      j = y; y = (2*x + 3*y)%5; x = j;
      d = a[x+5*y];
      a[x+5*y] = tc_kmac_rotate(t, ((i+1)*(i+2)/2)%64);
      t = d;
    }
    for (y = 0; y < 25; y += 5) {
      for (x = 0; x < 5; ++x) c[x] = a[y+x];
      for (x = 0; x < 5; ++x)
        a[y+x] = c[x] ^ ((~c[(x+1)%5]) & c[(x+2)%5]);
    }
    for (j = 0; j < 7; ++j) {
      if (lfsr & 1u) a[0] ^= UINT64_C(1) << ((1u << j)-1u);
      lfsr = (uint8_t)((lfsr << 1) ^ ((lfsr & 0x80u) ? 0x71u : 0u));
    }
  }
#if TC_ZEROIZE
  TC_secure_zero(c, sizeof(c));
#endif
}

static void tc_kmac_byte(struct TC_KMAC256_ctx* ctx, uint8_t byte)
{
  unsigned p = ctx->Position;
  ctx->State[p/8] ^= (uint64_t)byte << (8*(p%8));
  if (++ctx->Position == TC_KMAC_RATE) {
    tc_kmac_permute(ctx->State);
    ctx->Position = 0;
  }
}

static void tc_kmac_absorb(struct TC_KMAC256_ctx* ctx,
                           const uint8_t* data, size_t len)
{
  while (len--) tc_kmac_byte(ctx, *data++);
}

static void tc_kmac_encode(struct TC_KMAC256_ctx* ctx, uint64_t value, int right)
{
  uint64_t v = value;
  unsigned n = 1, i;
  while ((v >>= 8) != 0) ++n;
  if (!right) tc_kmac_byte(ctx, (uint8_t)n);
  for (i = n; i != 0; --i)
    tc_kmac_byte(ctx, (uint8_t)(value >> (8*(i-1))));
  if (right) tc_kmac_byte(ctx, (uint8_t)n);
}

static void tc_kmac_pad(struct TC_KMAC256_ctx* ctx)
{
  while (ctx->Position) tc_kmac_byte(ctx, 0);
}

static int tc_kmac_length(size_t len)
{
#if SIZE_MAX > UINT64_MAX / 8
  return len <= UINT64_MAX / 8;
#else
  (void)len;
  return 1;
#endif
}

TC_status TC_KMAC256_init(struct TC_KMAC256_ctx* ctx,
    const uint8_t* key, size_t key_len, const uint8_t* custom, size_t custom_len)
{
#if TC_STRICT
  if (!ctx || (!key && key_len) || (!custom && custom_len)) return TC_ERROR;
#endif
  if (!tc_kmac_length(key_len) || !tc_kmac_length(custom_len) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), key, key_len) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), custom, custom_len))
    return TC_ERROR;
  memset(ctx, 0, sizeof(*ctx));
  /* bytepad(encode_string("KMAC") || encode_string(S), rate). */
  tc_kmac_encode(ctx, TC_KMAC_RATE, 0);
  tc_kmac_encode(ctx, 32, 0);
  tc_kmac_byte(ctx, 'K'); tc_kmac_byte(ctx, 'M');
  tc_kmac_byte(ctx, 'A'); tc_kmac_byte(ctx, 'C');
  tc_kmac_encode(ctx, (uint64_t)custom_len*8, 0);
  tc_kmac_absorb(ctx, custom, custom_len);
  tc_kmac_pad(ctx);
  tc_kmac_encode(ctx, TC_KMAC_RATE, 0);
  tc_kmac_encode(ctx, (uint64_t)key_len*8, 0);
  tc_kmac_absorb(ctx, key, key_len);
  tc_kmac_pad(ctx);
  ctx->Active = 1;
  return TC_OK;
}

TC_status TC_KMAC256_update(struct TC_KMAC256_ctx* ctx,
    const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (!ctx || (!data && len)) return TC_ERROR;
#endif
  if (ctx->Active != 1 || ctx->Position >= TC_KMAC_RATE ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), data, len)) return TC_ERROR;
  tc_kmac_absorb(ctx, data, len);
  return TC_OK;
}

TC_status TC_KMAC256_final(struct TC_KMAC256_ctx* ctx,
    uint8_t* out, size_t out_len)
{
  size_t i;
  unsigned p = 0;
#if TC_STRICT
  if (!ctx || !out) return TC_ERROR;
#endif
  if (!out_len || !tc_kmac_length(out_len) || ctx->Active != 1 ||
      ctx->Position >= TC_KMAC_RATE ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), out, out_len)) return TC_ERROR;
  tc_kmac_encode(ctx, (uint64_t)out_len*8, 1);
  /* cSHAKE domain suffix followed by pad10*1. */
  ctx->State[ctx->Position/8] ^= UINT64_C(0x04) << (8*(ctx->Position%8));
  ctx->State[(TC_KMAC_RATE-1)/8] ^= UINT64_C(0x80) << 56;
  tc_kmac_permute(ctx->State);
  for (i = 0; i < out_len; ++i) {
    if (p == TC_KMAC_RATE) { tc_kmac_permute(ctx->State); p = 0; }
    out[i] = (uint8_t)(ctx->State[p/8] >> (8*(p%8)));
    ++p;
  }
#if TC_ZEROIZE
  TC_KMAC256_ctx_clear(ctx);
#else
  ctx->Active = 0;
#endif
  return TC_OK;
}

void TC_KMAC256_ctx_clear(struct TC_KMAC256_ctx* ctx)
{
  if (ctx) TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_KMAC256_digest(const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t len, const uint8_t* custom, size_t custom_len,
    uint8_t* out, size_t out_len)
{
  struct TC_KMAC256_ctx ctx;
  TC_status status;
  if ((!key && key_len) || (!data && len) || (!custom && custom_len) || !out ||
      !out_len || !tc_kmac_length(key_len) || !tc_kmac_length(custom_len) ||
      !tc_kmac_length(out_len)) return TC_ERROR;
  status = TC_KMAC256_init(&ctx, key, key_len, custom, custom_len);
  if (status == TC_OK) status = TC_KMAC256_update(&ctx, data, len);
  if (status == TC_OK) status = TC_KMAC256_final(&ctx, out, out_len);
#if TC_ZEROIZE
  TC_KMAC256_ctx_clear(&ctx);
#endif
  return status;
}
#endif
