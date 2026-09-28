/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SHA-384 / SHA-512 and their HMACs for tiny_crypto_c (FIPS 180-4,
 * FIPS 198-1 / RFC 2104). This is the 64-bit Merkle-Damgard core: 128-byte
 * blocks, eight 64-bit state words, a 128-bit length field. The 32-bit
 * SHA-1 / SHA-224 / SHA-256 core in hash.c is untouched by this file so that
 * SHA-256-only firmware images do not change size.
 */

#include <string.h>
#include <tiny_crypto/common.h>
#if TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.h>
#endif
#include "internal.h"

/* SHA-384 reuses the SHA-512 compression function; either digest pulls in
 * the shared core, and each public API is gated on its own switch. */
#define TC_HASH_SHA512_CORE (TC_ENABLE_SHA384 || TC_ENABLE_SHA512)

#if TC_HASH_SHA512_CORE

#if defined(__AVR__) && TC_AVR_PROGMEM
  #include <avr/pgmspace.h>
  #define HASH_K512_STORAGE PROGMEM
  #ifdef pgm_read_qword
    #define HASH_K512_READ(i) pgm_read_qword(&K512[(i)])
  #else
    /* avr-libc < 2.0 has no 64-bit flash read; assemble two little-endian
       dwords the way the compiler laid the constant out. */
    #define HASH_K512_READ(i) \
      (((uint64_t)pgm_read_dword((const uint32_t*)&K512[(i)] + 1) << 32) | \
       (uint64_t)pgm_read_dword((const uint32_t*)&K512[(i)]))
  #endif
#else
  #define HASH_K512_STORAGE
  #define HASH_K512_READ(i) K512[(i)]
#endif

/*****************************************************************************/
/* Private Helpers                                                           */
/*****************************************************************************/

/* Rotation with compile-time constant counts in 1..63 only. */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/*****************************************************************************/
/* Shared 128-byte block streaming                                           */
/*****************************************************************************/

/* FIPS 180-4 encodes the SHA-512 message length as 128 bits, so any 64-bit
   byte count is representable: (count >> 61, count << 3). The only limit is
   the 64-bit Count field itself (2^64 - 1 bytes). */
typedef void (*tc_sha512_compress_fn)(uint64_t* state, const uint8_t* block);

static TC_status tc_sha512_stream_update(uint64_t* state, uint64_t* count,
                                         uint8_t* buf_len, uint8_t* buf,
                                         const uint8_t* data, size_t len,
                                         tc_sha512_compress_fn compress)
{
  if (len == 0)
    return TC_OK;
  if ((uint64_t)len > UINT64_MAX - *count)
    return TC_ERROR;

  *count += (uint64_t)len;
  if (*buf_len != 0)
  {
    const size_t space = (size_t)TC_SHA512_BLOCKLEN - *buf_len;
    const size_t take = len < space ? len : space;
    memcpy(buf + *buf_len, data, take);
    *buf_len = (uint8_t)(*buf_len + take);
    data += take;
    len -= take;
    if (*buf_len == TC_SHA512_BLOCKLEN)
    {
      compress(state, buf);
      *buf_len = 0;
    }
  }

  while (len >= TC_SHA512_BLOCKLEN)
  {
    compress(state, data);
    data += TC_SHA512_BLOCKLEN;
    len -= TC_SHA512_BLOCKLEN;
  }
  if (len != 0)
  {
    memcpy(buf, data, len);
    *buf_len = (uint8_t)len;
  }
  return TC_OK;
}

static void tc_sha512_stream_final(uint64_t* state, size_t state_words,
                                   uint64_t count, uint8_t* buf_len, uint8_t* buf,
                                   uint8_t* digest, tc_sha512_compress_fn compress)
{
  size_t i;

  buf[(*buf_len)++] = 0x80u;
  if (*buf_len > 112u)
  {
    memset(buf + *buf_len, 0, (size_t)TC_SHA512_BLOCKLEN - *buf_len);
    compress(state, buf);
    *buf_len = 0;
  }
  memset(buf + *buf_len, 0, 112u - *buf_len);
  tc_internal_store_be64(buf + 112u, count >> 61);
  tc_internal_store_be64(buf + 120u, count << 3);
  compress(state, buf);

  for (i = 0; i < state_words; ++i)
    tc_internal_store_be64(digest + (8u * i), state[i]);
}

/*****************************************************************************/
/* SHA-512 compression                                                       */
/*****************************************************************************/

/* Round constants (ROM/Flash, 640 bytes) */
static const uint64_t K512[80] HASH_K512_STORAGE = {
  0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
  0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
  0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
  0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
  0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
  0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
  0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
  0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
  0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
  0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
  0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
  0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
  0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
  0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
  0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
  0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
  0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
  0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
  0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
  0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL
};

#define TC_SHA512_CH(x, y, z)  ((z) ^ ((x) & ((y) ^ (z))))
#define TC_SHA512_MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) ^ (y))))
#define TC_SHA512_BSIG0(x)     (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define TC_SHA512_BSIG1(x)     (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define TC_SHA512_SSIG0(x)     (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define TC_SHA512_SSIG1(x)     (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

#define TC_SHA512_STEP(a, b, c, d, e, f, g, h, k, w) do { \
  uint64_t t1 = (h) + TC_SHA512_BSIG1(e) + TC_SHA512_CH((e), (f), (g)) + (k) + (w); \
  uint64_t t2 = TC_SHA512_BSIG0(a) + TC_SHA512_MAJ((a), (b), (c)); \
  h = g; \
  g = f; \
  f = e; \
  e = d + t1; \
  d = c; \
  c = b; \
  b = a; \
  a = t1 + t2; \
} while (0)

/* One 128-byte block. Rolling 16-word schedule keeps the stack small. */
static void tc_sha512_compress_impl(uint64_t state[8],
                                    const uint8_t block[TC_SHA512_BLOCKLEN],
                                    int wipe_schedule)
{
  uint64_t W[16];
  uint64_t a, b, c, d, e, f, g, h;
  unsigned t;

  for (t = 0; t < 16; ++t)
    W[t] = tc_internal_load_be64(block + 8U * t);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  /* Rounds 0..15 */
  for (t = 0; t < 16; ++t)
  {
    TC_SHA512_STEP(a, b, c, d, e, f, g, h, HASH_K512_READ(t), W[t]);
  }

  /* Rounds 16..79 */
  for (t = 16; t < 80; ++t)
  {
    uint64_t w = TC_SHA512_SSIG1(W[(t - 2U) & 15U]) + W[(t - 7U) & 15U] +
                 TC_SHA512_SSIG0(W[(t - 15U) & 15U]) + W[t & 15U];
    W[t & 15U] = w;
    TC_SHA512_STEP(a, b, c, d, e, f, g, h, HASH_K512_READ(t), w);
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;

#if TC_ZEROIZE
  if (wipe_schedule)
    TC_secure_zero(W, sizeof(W));
#else
  (void)wipe_schedule;
#endif
}

static void tc_sha512_compress(uint64_t state[8],
                               const uint8_t block[TC_SHA512_BLOCKLEN])
{
  tc_sha512_compress_impl(state, block, 0);
}

/* HMAC's pad blocks contain key-derived material. Wipe their expanded
 * schedules without paying that cost for ordinary, public-data hashing. */
#if TC_ENABLE_HMAC
static void tc_sha512_compress_secret(uint64_t state[8],
                                      const uint8_t block[TC_SHA512_BLOCKLEN])
{
  tc_sha512_compress_impl(state, block, 1);
}
#endif

/*****************************************************************************/
/* SHA-512                                                                   */
/*****************************************************************************/

#if TC_ENABLE_SHA512

static void tc_sha512_state_init(uint64_t state[8])
{
  state[0] = 0x6A09E667F3BCC908ULL;
  state[1] = 0xBB67AE8584CAA73BULL;
  state[2] = 0x3C6EF372FE94F82BULL;
  state[3] = 0xA54FF53A5F1D36F1ULL;
  state[4] = 0x510E527FADE682D1ULL;
  state[5] = 0x9B05688C2B3E6C1FULL;
  state[6] = 0x1F83D9ABFB41BD6BULL;
  state[7] = 0x5BE0CD19137E2179ULL;
}

TC_status TC_SHA512_init(struct TC_SHA512_ctx* ctx)
{
  if (ctx == NULL)
    return TC_ERROR;
  /* Wipe Buf as well as State: re-init without *_ctx_clear would otherwise
     leave the previous partial block (HMAC pad residue or message) in RAM. */
  TC_secure_zero(ctx, sizeof(*ctx));
  tc_sha512_state_init(ctx->State);
  return TC_OK;
}

TC_status TC_SHA512_update(struct TC_SHA512_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  return tc_sha512_stream_update(ctx->State, &ctx->Count, &ctx->BufLen, ctx->Buf,
                                 data, len, tc_sha512_compress);
}

TC_status TC_SHA512_final(struct TC_SHA512_ctx* ctx, uint8_t* digest)
{
#if TC_STRICT
  if (ctx == NULL || digest == NULL)
    return TC_ERROR;
#endif
  tc_sha512_stream_final(ctx->State, 8, ctx->Count, &ctx->BufLen, ctx->Buf,
                         digest, tc_sha512_compress);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_SHA512_ctx_clear(struct TC_SHA512_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_SHA512_digest(const uint8_t* data, size_t len, uint8_t* digest)
{
  struct TC_SHA512_ctx ctx;

  if (digest == NULL || (len != 0 && data == NULL))
    return TC_ERROR;

  TC_SHA512_init(&ctx);
  if (TC_SHA512_update(&ctx, data, len) != TC_OK)
  {
    TC_SHA512_ctx_clear(&ctx);
    return TC_ERROR;
  }
  return TC_SHA512_final(&ctx, digest);
}

#endif /* TC_ENABLE_SHA512 */

/*****************************************************************************/
/* SHA-384                                                                   */
/*****************************************************************************/

#if TC_ENABLE_SHA384

/* FIPS 180-4 section 5.3.4: fractional parts of the square roots of the
 * 9th through 16th primes. */
static void tc_sha384_state_init(uint64_t state[8])
{
  state[0] = 0xCBBB9D5DC1059ED8ULL;
  state[1] = 0x629A292A367CD507ULL;
  state[2] = 0x9159015A3070DD17ULL;
  state[3] = 0x152FECD8F70E5939ULL;
  state[4] = 0x67332667FFC00B31ULL;
  state[5] = 0x8EB44A8768581511ULL;
  state[6] = 0xDB0C2E0D64F98FA7ULL;
  state[7] = 0x47B5481DBEFA4FA4ULL;
}

TC_status TC_SHA384_init(struct TC_SHA384_ctx* ctx)
{
  if (ctx == NULL)
    return TC_ERROR;
  TC_secure_zero(ctx, sizeof(*ctx));
  tc_sha384_state_init(ctx->State);
  return TC_OK;
}

TC_status TC_SHA384_update(struct TC_SHA384_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  return tc_sha512_stream_update(ctx->State, &ctx->Count, &ctx->BufLen, ctx->Buf,
                                 data, len, tc_sha512_compress);
}

TC_status TC_SHA384_final(struct TC_SHA384_ctx* ctx, uint8_t* digest)
{
#if TC_STRICT
  if (ctx == NULL || digest == NULL)
    return TC_ERROR;
#endif
  /* Emit the leftmost six state words (384 bits). */
  tc_sha512_stream_final(ctx->State, 6, ctx->Count, &ctx->BufLen, ctx->Buf,
                         digest, tc_sha512_compress);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_SHA384_ctx_clear(struct TC_SHA384_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_SHA384_digest(const uint8_t* data, size_t len, uint8_t* digest)
{
  struct TC_SHA384_ctx ctx;

  if (digest == NULL || (len != 0 && data == NULL))
    return TC_ERROR;

  TC_SHA384_init(&ctx);
  if (TC_SHA384_update(&ctx, data, len) != TC_OK)
  {
    TC_SHA384_ctx_clear(&ctx);
    return TC_ERROR;
  }
  return TC_SHA384_final(&ctx, digest);
}

#endif /* TC_ENABLE_SHA384 */

/*****************************************************************************/
/* HMAC (FIPS 198-1 / RFC 2104) over the 128-byte block                     */
/*****************************************************************************/

#if TC_ENABLE_HMAC

#define HMAC_IPAD 0x36U
#define HMAC_OPAD 0x5CU

typedef void (*tc_sha512_state_init_fn)(uint64_t* state);

static TC_status tc_sha512_hmac_init_common(uint64_t* inner_state, uint64_t* inner_count,
                                            uint8_t* inner_buf_len, uint8_t* inner_buf,
                                            uint64_t* outer_state, size_t state_words,
                                            const uint8_t* key, size_t keylen,
                                            tc_sha512_state_init_fn state_init,
                                            tc_sha512_compress_fn compress_secret)
{
  uint8_t block[TC_SHA512_BLOCKLEN];
  size_t i;

  memset(block, 0, sizeof(block));
  if (keylen > sizeof(block))
  {
    /* Reuse the caller's inner state for H(K). This avoids stacking a second
     * full hash context when an HMAC key is longer than one block. */
    state_init(inner_state);
    *inner_count = 0;
    *inner_buf_len = 0;
    memset(inner_buf, 0, sizeof(block));
    if (tc_sha512_stream_update(inner_state, inner_count, inner_buf_len, inner_buf,
                                key, keylen, compress_secret) != TC_OK)
      return TC_ERROR;
    tc_sha512_stream_final(inner_state, state_words, *inner_count, inner_buf_len,
                           inner_buf, block, compress_secret);
  }
  else if (keylen != 0)
  {
    memcpy(block, key, keylen);
  }

  for (i = 0; i < sizeof(block); ++i)
    block[i] ^= HMAC_IPAD;
  state_init(inner_state);
  compress_secret(inner_state, block);
  *inner_count = sizeof(block);
  *inner_buf_len = 0;
  memset(inner_buf, 0, sizeof(block));

  /* ipad ^ opad converts K'^ipad into K'^opad in place. */
  for (i = 0; i < sizeof(block); ++i)
    block[i] ^= (uint8_t)(HMAC_IPAD ^ HMAC_OPAD);
  state_init(outer_state);
  compress_secret(outer_state, block);

#if TC_ZEROIZE
  TC_secure_zero(block, sizeof(block));
#endif
  return TC_OK;
}

/* Outer hash of (K' ^ opad) || inner: exactly one padded block since the
 * inner digest (<= 64 bytes) plus 0x80 and the 16-byte length fit in 128. */
static void tc_sha512_hmac_outer_final(uint64_t* state, size_t state_words,
                                       size_t digest_len, const uint8_t* inner,
                                       uint8_t* tag, tc_sha512_compress_fn compress_secret)
{
  uint8_t block[TC_SHA512_BLOCKLEN];
  size_t i;

  memcpy(block, inner, digest_len);
  block[digest_len] = 0x80U;
  memset(block + digest_len + 1, 0, sizeof(block) - digest_len - 1 - 16);
  tc_internal_store_be64(block + sizeof(block) - 16, 0);
  tc_internal_store_be64(block + sizeof(block) - 8,
                       ((uint64_t)sizeof(block) + digest_len) << 3);
  compress_secret(state, block);
  for (i = 0; i < state_words; ++i)
    tc_internal_store_be64(tag + 8U * i, state[i]);
#if TC_ZEROIZE
  TC_secure_zero(block, sizeof(block));
#endif
}

#if TC_ENABLE_SHA384

TC_status TC_HMAC_SHA384_init(struct TC_HMAC_SHA384_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || (keylen != 0 && key == NULL))
    return TC_ERROR;
  return tc_sha512_hmac_init_common(ctx->Inner.State, &ctx->Inner.Count,
                                    &ctx->Inner.BufLen, ctx->Inner.Buf, ctx->OuterState,
                                    6, key, keylen,
                                    tc_sha384_state_init, tc_sha512_compress_secret);
}

TC_status TC_HMAC_SHA384_update(struct TC_HMAC_SHA384_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL)
    return TC_ERROR;
#endif
  return TC_SHA384_update(&ctx->Inner, data, len);
}

TC_status TC_HMAC_SHA384_final(struct TC_HMAC_SHA384_ctx* ctx, uint8_t* tag)
{
  uint8_t inner[TC_SHA384_DIGESTLEN];

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif

  /* One-shot: TC_ZEROIZE wipes the context; call TC_HMAC_SHA384_init to reuse. */
  if (TC_SHA384_final(&ctx->Inner, inner) != TC_OK)
  {
    TC_HMAC_SHA384_ctx_clear(ctx);
    return TC_ERROR;
  }
  tc_sha512_hmac_outer_final(ctx->OuterState, 6, TC_SHA384_DIGESTLEN, inner, tag,
                             tc_sha512_compress_secret);

#if TC_ZEROIZE
  TC_secure_zero(inner, sizeof(inner));
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_HMAC_SHA384_ctx_clear(struct TC_HMAC_SHA384_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_HMAC_SHA384_digest(const uint8_t* key, size_t keylen,
                                const uint8_t* msg, size_t msg_len,
                                uint8_t* tag, size_t tag_len)
{
  struct TC_HMAC_SHA384_ctx ctx;
  uint8_t full[TC_SHA384_DIGESTLEN];

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA384_DIGESTLEN ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA384_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;

  if (TC_HMAC_SHA384_update(&ctx, msg, msg_len) != TC_OK ||
      TC_HMAC_SHA384_final(&ctx, full) != TC_OK)
  {
    TC_HMAC_SHA384_ctx_clear(&ctx);
    TC_secure_zero(full, sizeof(full));
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_HMAC_SHA384_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                                const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_SHA384_DIGESTLEN];
  int rc;

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA384_DIGESTLEN)
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA384_digest(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  /* TC_MISMATCH vs TC_ERROR: a well-formed miss is not a programming error. */
  rc = TC_ct_equal(computed, tag, tag_len);
#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return rc;
}

#endif /* TC_ENABLE_SHA384 */

#if TC_ENABLE_SHA512

TC_status TC_HMAC_SHA512_init(struct TC_HMAC_SHA512_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || (keylen != 0 && key == NULL))
    return TC_ERROR;
  return tc_sha512_hmac_init_common(ctx->Inner.State, &ctx->Inner.Count,
                                    &ctx->Inner.BufLen, ctx->Inner.Buf, ctx->OuterState,
                                    8, key, keylen,
                                    tc_sha512_state_init, tc_sha512_compress_secret);
}

TC_status TC_HMAC_SHA512_update(struct TC_HMAC_SHA512_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL)
    return TC_ERROR;
#endif
  return TC_SHA512_update(&ctx->Inner, data, len);
}

TC_status TC_HMAC_SHA512_final(struct TC_HMAC_SHA512_ctx* ctx, uint8_t* tag)
{
  uint8_t inner[TC_SHA512_DIGESTLEN];

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif

  /* One-shot: TC_ZEROIZE wipes the context; call TC_HMAC_SHA512_init to reuse. */
  if (TC_SHA512_final(&ctx->Inner, inner) != TC_OK)
  {
    TC_HMAC_SHA512_ctx_clear(ctx);
    return TC_ERROR;
  }
  tc_sha512_hmac_outer_final(ctx->OuterState, 8, TC_SHA512_DIGESTLEN, inner, tag,
                             tc_sha512_compress_secret);

#if TC_ZEROIZE
  TC_secure_zero(inner, sizeof(inner));
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_HMAC_SHA512_ctx_clear(struct TC_HMAC_SHA512_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_HMAC_SHA512_digest(const uint8_t* key, size_t keylen,
                                const uint8_t* msg, size_t msg_len,
                                uint8_t* tag, size_t tag_len)
{
  struct TC_HMAC_SHA512_ctx ctx;
  uint8_t full[TC_SHA512_DIGESTLEN];

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA512_DIGESTLEN ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA512_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;

  if (TC_HMAC_SHA512_update(&ctx, msg, msg_len) != TC_OK ||
      TC_HMAC_SHA512_final(&ctx, full) != TC_OK)
  {
    TC_HMAC_SHA512_ctx_clear(&ctx);
    TC_secure_zero(full, sizeof(full));
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_HMAC_SHA512_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                                const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_SHA512_DIGESTLEN];
  int rc;

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA512_DIGESTLEN)
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA512_digest(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  /* TC_MISMATCH vs TC_ERROR: a well-formed miss is not a programming error. */
  rc = TC_ct_equal(computed, tag, tag_len);
#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return rc;
}

#endif /* TC_ENABLE_SHA512 */

#endif /* TC_ENABLE_HMAC */

#endif /* TC_HASH_SHA512_CORE */
