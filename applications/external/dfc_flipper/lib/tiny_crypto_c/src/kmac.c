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
 * constants with an LFSR so AVR builds need no table in RAM. */
static void tc_kmac_permute(uint64_t* a)
{
  uint64_t c[5], t, d;
  unsigned round, x, y, i, j;
  uint8_t lfsr = 1;
  for (round = 0; round < 24; ++round) {
    for (x = 0; x < 5; ++x)
      c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
    for (x = 0; x < 5; ++x) {
      d = c[(x + 4) % 5] ^ tc_kmac_rotate(c[(x + 1) % 5], 1);
      for (y = 0; y < 25; y += 5)
        a[y + x] ^= d;
    }
    x = 1;
    y = 0;
    t = a[1];
    for (i = 0; i < 24; ++i) {
      j = y;
      y = (2 * x + 3 * y) % 5;
      x = j;
      d = a[x + 5 * y];
      a[x + 5 * y] = tc_kmac_rotate(t, ((i + 1) * (i + 2) / 2) % 64);
      t = d;
    }
    for (y = 0; y < 25; y += 5) {
      for (x = 0; x < 5; ++x)
        c[x] = a[y + x];
      for (x = 0; x < 5; ++x)
        a[y + x] = c[x] ^ ((~c[(x + 1) % 5]) & c[(x + 2) % 5]);
    }
    for (j = 0; j < 7; ++j) {
      if (lfsr & 1u)
        a[0] ^= UINT64_C(1) << ((1u << j) - 1u);
      lfsr = (uint8_t)((lfsr << 1) ^ ((lfsr & 0x80u) ? 0x71u : 0u));
    }
  }
  TC_secure_zero(c, sizeof(c));
}

static void tc_kmac_byte(struct TC_KMAC256_ctx* ctx, uint8_t byte)
{
  unsigned p = ctx->position;
  ctx->state[p / 8] ^= (uint64_t)byte << (8 * (p % 8));
  if (++ctx->position == TC_KMAC_RATE) {
    tc_kmac_permute(ctx->state);
    ctx->position = 0;
  }
}

static void tc_kmac_absorb(struct TC_KMAC256_ctx* ctx, const uint8_t* data, size_t len)
{
  while (len--)
    tc_kmac_byte(ctx, *data++);
}

static void tc_kmac_encode(struct TC_KMAC256_ctx* ctx, uint64_t value, int right)
{
  uint64_t v = value;
  unsigned n = 1, i;
  while ((v >>= 8) != 0)
    ++n;
  if (!right)
    tc_kmac_byte(ctx, (uint8_t)n);
  for (i = n; i != 0; --i)
    tc_kmac_byte(ctx, (uint8_t)(value >> (8 * (i - 1))));
  if (right)
    tc_kmac_byte(ctx, (uint8_t)n);
}

static void tc_kmac_pad(struct TC_KMAC256_ctx* ctx)
{
  while (ctx->position)
    tc_kmac_byte(ctx, 0);
}

static int tc_kmac_length(size_t len)
{
#if SIZE_MAX > UINT64_MAX / 8
  return len <= UINT64_MAX / 8;
#else
  /* size_t cannot exceed the limit, so len needs no comparison. */
  (void)len;
  return 1;
#endif
}

/* A span is usable when it has storage or is empty, and its bit length fits
 * the 64-bit encodings of SP 800-185 section 2.3.1. */
static int tc_kmac_span(const void* data, size_t length)
{
  return tc_internal_span_valid(data, length) && tc_kmac_length(length);
}

/* A context input must stay outside the context, which init rewrites. */
static int tc_kmac_input(const struct TC_KMAC256_ctx* ctx, TC_bytes input)
{
  return tc_kmac_span(input.data, input.length) &&
         tc_internal_ranges_disjoint(ctx, sizeof(*ctx), input.data, input.length);
}

static int tc_kmac_output(TC_buffer out)
{
  return out.data != NULL && out.capacity != 0 && tc_kmac_length(out.capacity);
}

TC_status TC_KMAC256_init(struct TC_KMAC256_ctx* ctx, TC_bytes key, TC_bytes custom)
{
  if (!ctx)
    return TC_ERROR;
  if (!tc_kmac_input(ctx, key) || !tc_kmac_input(ctx, custom)) {
    TC_KMAC256_ctx_clear(ctx);
    return TC_ERROR;
  }
  memset(ctx, 0, sizeof(*ctx));
  /* SP 800-185 section 4.3: newX = bytepad(encode_string(K), 136) || X,
   * absorbed by cSHAKE256 with N = "KMAC" and S = custom. The cSHAKE prefix
   * is bytepad(encode_string(N) || encode_string(S), 136) (section 3.3). */
  tc_kmac_encode(ctx, TC_KMAC_RATE, 0);
  tc_kmac_encode(ctx, 32, 0);
  tc_kmac_byte(ctx, 'K');
  tc_kmac_byte(ctx, 'M');
  tc_kmac_byte(ctx, 'A');
  tc_kmac_byte(ctx, 'C');
  tc_kmac_encode(ctx, (uint64_t)custom.length * 8, 0);
  tc_kmac_absorb(ctx, custom.data, custom.length);
  tc_kmac_pad(ctx);
  tc_kmac_encode(ctx, TC_KMAC_RATE, 0);
  tc_kmac_encode(ctx, (uint64_t)key.length * 8, 0);
  tc_kmac_absorb(ctx, key.data, key.length);
  tc_kmac_pad(ctx);
  ctx->active = 1;
  return TC_OK;
}

static int tc_kmac_live(const struct TC_KMAC256_ctx* ctx)
{
  return ctx->active == 1 && ctx->position < TC_KMAC_RATE;
}

TC_status TC_KMAC256_update(struct TC_KMAC256_ctx* ctx, TC_bytes data)
{
  if (!ctx || !tc_kmac_live(ctx) || !tc_internal_span_valid(data.data, data.length) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), data.data, data.length))
    return TC_ERROR;
  tc_kmac_absorb(ctx, data.data, data.length);
  return TC_OK;
}

static TC_status tc_kmac_final(struct TC_KMAC256_ctx* ctx, TC_buffer out, int short_tag)
{
  size_t i;
  unsigned p = 0;
  if (!ctx || !tc_kmac_output(out) ||
      !tc_internal_tag_length_allowed(out.capacity, SIZE_MAX, short_tag) || !tc_kmac_live(ctx) ||
      !tc_internal_ranges_disjoint(ctx, sizeof(*ctx), out.data, out.capacity))
    return TC_ERROR;
  /* X || right_encode(L), then the cSHAKE domain suffix and pad10*1. */
  tc_kmac_encode(ctx, (uint64_t)out.capacity * 8, 1);
  ctx->state[ctx->position / 8] ^= UINT64_C(0x04) << (8 * (ctx->position % 8));
  ctx->state[(TC_KMAC_RATE - 1) / 8] ^= UINT64_C(0x80) << 56;
  tc_kmac_permute(ctx->state);
  for (i = 0; i < out.capacity; ++i) {
    if (p == TC_KMAC_RATE) {
      tc_kmac_permute(ctx->state);
      p = 0;
    }
    out.data[i] = (uint8_t)(ctx->state[p / 8] >> (8 * (p % 8)));
    ++p;
  }
  TC_KMAC256_ctx_clear(ctx);
  return TC_OK;
}

TC_status TC_KMAC256_final(struct TC_KMAC256_ctx* ctx, TC_buffer out)
{
  return tc_kmac_final(ctx, out, 0);
}

TC_status TC_KMAC256_final_short_tag(struct TC_KMAC256_ctx* ctx, TC_buffer out)
{
  return tc_kmac_final(ctx, out, 1);
}

void TC_KMAC256_ctx_clear(struct TC_KMAC256_ctx* ctx)
{
  if (ctx)
    TC_secure_zero(ctx, sizeof(*ctx));
}

static TC_status tc_kmac_digest(TC_bytes key, TC_bytes data, TC_bytes custom, TC_buffer out,
                                int short_tag)
{
  struct TC_KMAC256_ctx ctx;
  TC_status status;
  if (!tc_kmac_span(key.data, key.length) || !tc_internal_span_valid(data.data, data.length) ||
      !tc_kmac_span(custom.data, custom.length) || !tc_kmac_output(out) ||
      !tc_internal_tag_length_allowed(out.capacity, SIZE_MAX, short_tag))
    return TC_ERROR;
  /* The local context holds the keyed sponge. It is wiped on every path. */
  status = TC_KMAC256_init(&ctx, key, custom);
  if (status == TC_OK)
    status = TC_KMAC256_update(&ctx, data);
  if (status == TC_OK)
    status = tc_kmac_final(&ctx, out, short_tag);
  TC_KMAC256_ctx_clear(&ctx);
  return status;
}

TC_status TC_KMAC256_digest(TC_bytes key, TC_bytes data, TC_bytes custom, TC_buffer out)
{
  return tc_kmac_digest(key, data, custom, out, 0);
}

TC_status TC_KMAC256_digest_short_tag(TC_bytes key, TC_bytes data, TC_bytes custom, TC_buffer out)
{
  return tc_kmac_digest(key, data, custom, out, 1);
}

static TC_status tc_kmac_verify(TC_bytes key, TC_bytes data, TC_bytes custom, TC_bytes tag,
                                int short_tag)
{
  uint8_t computed[TC_MIN_TAG_LEN > 1 ? TC_MIN_TAG_LEN - 1 : 1];
  uint8_t* candidate = computed;
  TC_status status;

  if (!tc_kmac_span(tag.data, tag.length) ||
      !tc_internal_tag_length_allowed(tag.length, SIZE_MAX, short_tag))
    return TC_ERROR;
  /* Default tags can be arbitrarily long. Reuse the caller's comparison
   * storage only for the bounded short-tag case; default verification uses
   * a streaming context below to avoid a variable-length stack object. */
  if (!short_tag) {
    struct TC_KMAC256_ctx ctx;
    size_t i;
    unsigned p = 0;
    status = TC_KMAC256_init(&ctx, key, custom);
    if (status == TC_OK)
      status = TC_KMAC256_update(&ctx, data);
    if (status != TC_OK) {
      TC_KMAC256_ctx_clear(&ctx);
      return status;
    }
    tc_kmac_encode(&ctx, (uint64_t)tag.length * 8, 1);
    ctx.state[ctx.position / 8] ^= UINT64_C(0x04) << (8 * (ctx.position % 8));
    ctx.state[(TC_KMAC_RATE - 1) / 8] ^= UINT64_C(0x80) << 56;
    tc_kmac_permute(ctx.state);
    uint8_t different = 0;
    for (i = 0; i < tag.length; ++i) {
      if (p == TC_KMAC_RATE) {
        tc_kmac_permute(ctx.state);
        p = 0;
      }
      different |= (uint8_t)((ctx.state[p / 8] >> (8 * (p % 8))) ^ tag.data[i]);
      ++p;
    }
    TC_KMAC256_ctx_clear(&ctx);
    return different == 0 ? TC_OK : TC_MISMATCH;
  }
  status = tc_kmac_digest(key, data, custom, (TC_buffer){candidate, tag.length}, 1);
  return tc_internal_verify_tag(status, candidate, sizeof computed, tag.data, tag.length);
}

TC_status TC_KMAC256_verify(TC_bytes key, TC_bytes data, TC_bytes custom, TC_bytes tag)
{
  return tc_kmac_verify(key, data, custom, tag, 0);
}

TC_status TC_KMAC256_verify_short_tag(TC_bytes key, TC_bytes data, TC_bytes custom, TC_bytes tag)
{
  return tc_kmac_verify(key, data, custom, tag, 1);
}
#endif
