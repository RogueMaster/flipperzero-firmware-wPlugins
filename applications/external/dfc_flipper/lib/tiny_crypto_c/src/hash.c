/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hash and HMAC implementation for tiny_crypto_c.
 * Portable C implementation of SHA-1, SHA-224, SHA-256 and their HMACs
 * (FIPS 180-4, FIPS 198-1 / RFC 2104) optimized for small embedded devices
 * and microcontrollers. The 64-bit SHA-384 / SHA-512 family lives in
 * sha512.c so this file stays the 32-bit Merkle-Damgard core.
 *
 * The compact implementation style follows kokke's tiny-AES-c:
 * https://github.com/kokke/tiny-AES-c
 */

#include <string.h>
#include <tiny_crypto/common.h>
#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256
#include <tiny_crypto/hash.h>
#endif
#include "internal.h"
#include "hash64_internal.h"

/* SHA-224 reuses the SHA-256 compression function; either digest pulls in
 * the shared core, and each public API is gated on its own switch. */
#define TC_HASH_SHA256_CORE (TC_ENABLE_SHA224 || TC_ENABLE_SHA256)
#define TC_HASH_CORE32      (TC_ENABLE_SHA1 || TC_HASH_SHA256_CORE)

#if TC_HASH_CORE32

#if defined(__AVR__) && TC_AVR_PROGMEM
  #include <avr/pgmspace.h>
  #define HASH_K256_STORAGE PROGMEM
  #define HASH_K256_READ(i) pgm_read_dword(&K256[(i)])
#else
  #define HASH_K256_STORAGE
  #define HASH_K256_READ(i) K256[(i)]
#endif

/*****************************************************************************/
/* Private Helpers                                                           */
/*****************************************************************************/

/* Rotations with compile-time constant counts in 1..31 only. */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* Byte-wise big-endian access with compiler builtin fast-paths. */
#if defined(__GNUC__) || defined(__clang__)
  #if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    static inline uint32_t tc_hash_load_be32(const uint8_t* p)
    {
      uint32_t v;
      memcpy(&v, p, sizeof(v));
      return __builtin_bswap32(v);
    }
    static inline void tc_hash_store_be32(uint8_t* p, uint32_t v)
    {
      uint32_t swapped = __builtin_bswap32(v);
      memcpy(p, &swapped, sizeof(swapped));
    }
  #elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    static inline uint32_t tc_hash_load_be32(const uint8_t* p)
    {
      uint32_t v;
      memcpy(&v, p, sizeof(v));
      return v;
    }
    static inline void tc_hash_store_be32(uint8_t* p, uint32_t v)
    {
      memcpy(p, &v, sizeof(v));
    }
  #else
    static inline uint32_t tc_hash_load_be32(const uint8_t* p)
    {
      return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
             ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    static inline void tc_hash_store_be32(uint8_t* p, uint32_t v)
    {
      p[0] = (uint8_t)(v >> 24);
      p[1] = (uint8_t)(v >> 16);
      p[2] = (uint8_t)(v >> 8);
      p[3] = (uint8_t)v;
    }
  #endif
#elif defined(_MSC_VER)
  #include <stdlib.h>
  static inline uint32_t tc_hash_load_be32(const uint8_t* p)
  {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return _byteswap_ulong(v);
  }
  static inline void tc_hash_store_be32(uint8_t* p, uint32_t v)
  {
    uint32_t swapped = _byteswap_ulong(v);
    memcpy(p, &swapped, sizeof(swapped));
  }
#else
  static inline uint32_t tc_hash_load_be32(const uint8_t* p)
  {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
  }

  static inline void tc_hash_store_be32(uint8_t* p, uint32_t v)
  {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
  }
#endif

/*****************************************************************************/
/* Public Functions: Wiping                                                  */
/*****************************************************************************/

/* FIPS 180-4 encodes length as a 64-bit bit count, so the largest valid
   message is floor((2^64 − 1) / 8) = 2^61 − 1 bytes. Beyond that the
   padding length field wraps and the digest is spec-invalid. */
#define HASH_MAX_BYTES (UINT64_MAX >> 3)

static int tc_hash_length_overflow(uint64_t count, size_t len)
{
  if (count > HASH_MAX_BYTES)
    return 1;
  return (uint64_t)len > (HASH_MAX_BYTES - count);
}

/* SHA-1 and SHA-256 share the same 64-byte Merkle-Damgard buffering rules.
 * Keep that state machine here; only the compression function differs. */
static TC_status tc_hash_stream_update(uint32_t* state, uint64_t* count,
                                    uint8_t* buf_len, uint8_t* buf,
                                    const uint8_t* data, size_t len,
                                    tc_hash_compress_fn compress)
{
  if (len == 0)
    return TC_OK;
  if (tc_hash_length_overflow(*count, len))
    return TC_ERROR;

  *count += (uint64_t)len;
  tc_hash64_absorb(state,buf_len,buf,data,len,compress);
  return TC_OK;
}

static void tc_hash_stream_final(uint32_t* state, size_t state_words,
                              uint64_t count, uint8_t* buf_len, uint8_t* buf,
                              uint8_t* digest, tc_hash_compress_fn compress)
{
  size_t i;
  tc_hash64_finish(state,count,buf_len,buf,compress,TC_HASH_LENGTH_BIG_ENDIAN);

  for (i = 0; i < state_words; ++i)
    tc_hash_store_be32(digest + (4u * i), state[i]);
}

/*****************************************************************************/
/* SHA-1                                                                     */
/*****************************************************************************/

#if TC_ENABLE_SHA1

#define TC_SHA1_STEP(a, b, c, d, e, f, k, w) do { \
  uint32_t temp = ROTL32(a, 5) + (f) + (e) + (k) + (w); \
  e = d; \
  d = c; \
  c = ROTL32(b, 30); \
  b = a; \
  a = temp; \
} while (0)

/* One 64-byte block. Rolling 16-word schedule keeps the stack small. */
static void tc_hash_sha1_compress_impl(uint32_t state[5],
                               const uint8_t block[TC_SHA1_BLOCKLEN],
                               int wipe_schedule)
{
  uint32_t W[16];
  uint32_t a, b, c, d, e;
  unsigned t;

  for (t = 0; t < 16; ++t)
    W[t] = tc_hash_load_be32(block + 4U * t);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];

  /* Rounds 0..15 */
  for (t = 0; t < 16; ++t)
  {
    uint32_t f = d ^ (b & (c ^ d));
    TC_SHA1_STEP(a, b, c, d, e, f, 0x5A827999U, W[t]);
  }

  /* Rounds 16..19 */
  for (t = 16; t < 20; ++t)
  {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = d ^ (b & (c ^ d));
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x5A827999U, w);
  }

  /* Rounds 20..39 */
  for (t = 20; t < 40; ++t)
  {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = b ^ c ^ d;
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x6ED9EBA1U, w);
  }

  /* Rounds 40..59 */
  for (t = 40; t < 60; ++t)
  {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = (b & c) | (d & (b ^ c));
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x8F1BBCDCU, w);
  }

  /* Rounds 60..79 */
  for (t = 60; t < 80; ++t)
  {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = b ^ c ^ d;
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0xCA62C1D6U, w);
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;

#if TC_ZEROIZE
  if (wipe_schedule)
    TC_secure_zero(W, sizeof(W));
#else
  (void)wipe_schedule;
#endif
}

static void tc_hash_sha1_compress(uint32_t state[5],
                          const uint8_t block[TC_SHA1_BLOCKLEN])
{
  tc_hash_sha1_compress_impl(state, block, 0);
}

/* HMAC's pad blocks contain key-derived material. Wipe their expanded
 * schedules without paying that cost for ordinary, public-data hashing. */
#if TC_ENABLE_HMAC
static void tc_hash_sha1_compress_secret(uint32_t state[5],
                                 const uint8_t block[TC_SHA1_BLOCKLEN])
{
  tc_hash_sha1_compress_impl(state, block, 1);
}
#endif

static void tc_hash_sha1_state_init(uint32_t state[5])
{
  state[0] = 0x67452301U;
  state[1] = 0xEFCDAB89U;
  state[2] = 0x98BADCFEU;
  state[3] = 0x10325476U;
  state[4] = 0xC3D2E1F0U;
}

TC_status TC_SHA1_init(struct TC_SHA1_ctx* ctx)
{
  if (ctx == NULL)
    return TC_ERROR;
  /* Wipe Buf as well as State: re-init without *_ctx_clear would otherwise
     leave the previous partial block (HMAC pad residue or message) in RAM. */
  TC_secure_zero(ctx, sizeof(*ctx));
  tc_hash_sha1_state_init(ctx->State);
  return TC_OK;
}

TC_status TC_SHA1_update(struct TC_SHA1_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  return tc_hash_stream_update(ctx->State, &ctx->Count, &ctx->BufLen, ctx->Buf,
                            data, len, tc_hash_sha1_compress);
}

TC_status TC_SHA1_final(struct TC_SHA1_ctx* ctx, uint8_t* digest)
{
#if TC_STRICT
  if (ctx == NULL || digest == NULL)
    return TC_ERROR;
#endif
  tc_hash_stream_final(ctx->State, 5, ctx->Count, &ctx->BufLen, ctx->Buf,
                    digest, tc_hash_sha1_compress);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_SHA1_ctx_clear(struct TC_SHA1_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_SHA1_digest(const uint8_t* data, size_t len, uint8_t* digest)
{
  struct TC_SHA1_ctx ctx;

  if (digest == NULL || (len != 0 && data == NULL))
    return TC_ERROR;

  TC_SHA1_init(&ctx);
  if (TC_SHA1_update(&ctx, data, len) != TC_OK)
  {
    TC_SHA1_ctx_clear(&ctx);
    return TC_ERROR;
  }
  return TC_SHA1_final(&ctx, digest);
}

#endif /* TC_ENABLE_SHA1 */

/*****************************************************************************/
/* SHA-256 / SHA-224                                                         */
/*****************************************************************************/

#if TC_HASH_SHA256_CORE

/* Round constants (ROM/Flash, 256 bytes) */
static const uint32_t K256[64] HASH_K256_STORAGE = {
  0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
  0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
  0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
  0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
  0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
  0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
  0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
  0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U
};

#define TC_SHA256_CH(x, y, z)  ((z) ^ ((x) & ((y) ^ (z))))
#define TC_SHA256_MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) ^ (y))))
#define TC_SHA256_BSIG0(x)     (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define TC_SHA256_BSIG1(x)     (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define TC_SHA256_SSIG0(x)     (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define TC_SHA256_SSIG1(x)     (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

#define TC_SHA256_STEP(a, b, c, d, e, f, g, h, k, w) do { \
  uint32_t t1 = (h) + TC_SHA256_BSIG1(e) + TC_SHA256_CH((e), (f), (g)) + (k) + (w); \
  uint32_t t2 = TC_SHA256_BSIG0(a) + TC_SHA256_MAJ((a), (b), (c)); \
  h = g; \
  g = f; \
  f = e; \
  e = d + t1; \
  d = c; \
  c = b; \
  b = a; \
  a = t1 + t2; \
} while (0)

/* One 64-byte block. Rolling 16-word schedule keeps the stack small. */
static void tc_hash_sha256_compress_impl(uint32_t state[8],
                                 const uint8_t block[TC_SHA256_BLOCKLEN],
                                 int wipe_schedule)
{
  uint32_t W[16];
  uint32_t a, b, c, d, e, f, g, h;
  unsigned t;

  for (t = 0; t < 16; ++t)
    W[t] = tc_hash_load_be32(block + 4U * t);

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
    TC_SHA256_STEP(a, b, c, d, e, f, g, h, HASH_K256_READ(t), W[t]);
  }

  /* Rounds 16..63 */
  for (t = 16; t < 64; ++t)
  {
    uint32_t w = TC_SHA256_SSIG1(W[(t - 2U) & 15U]) + W[(t - 7U) & 15U] +
                 TC_SHA256_SSIG0(W[(t - 15U) & 15U]) + W[t & 15U];
    W[t & 15U] = w;
    TC_SHA256_STEP(a, b, c, d, e, f, g, h, HASH_K256_READ(t), w);
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

static void tc_hash_sha256_compress(uint32_t state[8],
                            const uint8_t block[TC_SHA256_BLOCKLEN])
{
  tc_hash_sha256_compress_impl(state, block, 0);
}

#if TC_ENABLE_HMAC
static void tc_hash_sha256_compress_secret(uint32_t state[8],
                                           const uint8_t block[TC_SHA256_BLOCKLEN])
{
  tc_hash_sha256_compress_impl(state, block, 1);
}
#endif

#if TC_ENABLE_SHA256

static void tc_hash_sha256_state_init(uint32_t state[8])
{
  state[0] = 0x6A09E667U;
  state[1] = 0xBB67AE85U;
  state[2] = 0x3C6EF372U;
  state[3] = 0xA54FF53AU;
  state[4] = 0x510E527FU;
  state[5] = 0x9B05688CU;
  state[6] = 0x1F83D9ABU;
  state[7] = 0x5BE0CD19U;
}

TC_status TC_SHA256_init(struct TC_SHA256_ctx* ctx)
{
  if (ctx == NULL)
    return TC_ERROR;
  /* Wipe Buf as well as State: re-init without *_ctx_clear would otherwise
     leave the previous partial block (HMAC pad residue or message) in RAM. */
  TC_secure_zero(ctx, sizeof(*ctx));
  tc_hash_sha256_state_init(ctx->State);
  return TC_OK;
}

TC_status TC_SHA256_update(struct TC_SHA256_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  return tc_hash_stream_update(ctx->State, &ctx->Count, &ctx->BufLen, ctx->Buf,
                            data, len, tc_hash_sha256_compress);
}

TC_status TC_SHA256_final(struct TC_SHA256_ctx* ctx, uint8_t* digest)
{
#if TC_STRICT
  if (ctx == NULL || digest == NULL)
    return TC_ERROR;
#endif
  tc_hash_stream_final(ctx->State, 8, ctx->Count, &ctx->BufLen, ctx->Buf,
                    digest, tc_hash_sha256_compress);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_SHA256_ctx_clear(struct TC_SHA256_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_SHA256_digest(const uint8_t* data, size_t len, uint8_t* digest)
{
  struct TC_SHA256_ctx ctx;

  if (digest == NULL || (len != 0 && data == NULL))
    return TC_ERROR;

  TC_SHA256_init(&ctx);
  if (TC_SHA256_update(&ctx, data, len) != TC_OK)
  {
    TC_SHA256_ctx_clear(&ctx);
    return TC_ERROR;
  }
  return TC_SHA256_final(&ctx, digest);
}

#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA224

/* FIPS 180-4 section 5.3.2: the SHA-224 IV is the second 32 bits of the
 * fractional parts of the square roots of the 9th through 16th primes. */
static void tc_hash_sha224_state_init(uint32_t state[8])
{
  state[0] = 0xC1059ED8U;
  state[1] = 0x367CD507U;
  state[2] = 0x3070DD17U;
  state[3] = 0xF70E5939U;
  state[4] = 0xFFC00B31U;
  state[5] = 0x68581511U;
  state[6] = 0x64F98FA7U;
  state[7] = 0xBEFA4FA4U;
}

TC_status TC_SHA224_init(struct TC_SHA224_ctx* ctx)
{
  if (ctx == NULL)
    return TC_ERROR;
  TC_secure_zero(ctx, sizeof(*ctx));
  tc_hash_sha224_state_init(ctx->State);
  return TC_OK;
}

TC_status TC_SHA224_update(struct TC_SHA224_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  return tc_hash_stream_update(ctx->State, &ctx->Count, &ctx->BufLen, ctx->Buf,
                            data, len, tc_hash_sha256_compress);
}

TC_status TC_SHA224_final(struct TC_SHA224_ctx* ctx, uint8_t* digest)
{
#if TC_STRICT
  if (ctx == NULL || digest == NULL)
    return TC_ERROR;
#endif
  /* Emit the leftmost seven state words (224 bits). */
  tc_hash_stream_final(ctx->State, 7, ctx->Count, &ctx->BufLen, ctx->Buf,
                    digest, tc_hash_sha256_compress);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_SHA224_ctx_clear(struct TC_SHA224_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_SHA224_digest(const uint8_t* data, size_t len, uint8_t* digest)
{
  struct TC_SHA224_ctx ctx;

  if (digest == NULL || (len != 0 && data == NULL))
    return TC_ERROR;

  TC_SHA224_init(&ctx);
  if (TC_SHA224_update(&ctx, data, len) != TC_OK)
  {
    TC_SHA224_ctx_clear(&ctx);
    return TC_ERROR;
  }
  return TC_SHA224_final(&ctx, digest);
}

#endif /* TC_ENABLE_SHA224 */

#endif /* TC_HASH_SHA256_CORE */

/*****************************************************************************/
/* HMAC (FIPS 198-1 / RFC 2104)                                              */
/*****************************************************************************/

#if TC_ENABLE_HMAC

#define HMAC_IPAD 0x36U
#define HMAC_OPAD 0x5CU

typedef void (*tc_hash_state_init_fn)(uint32_t* state);

static TC_status tc_hash_hmac_init_common(uint32_t* inner_state, uint64_t* inner_count,
                                  uint8_t* inner_buf_len, uint8_t* inner_buf,
                                  uint32_t* outer_state, size_t state_words,
                                  const uint8_t* key, size_t keylen,
                                  tc_hash_state_init_fn state_init,
                                  tc_hash_compress_fn compress_secret)
{
  uint8_t block[64];
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
    if (tc_hash_stream_update(inner_state, inner_count, inner_buf_len, inner_buf,
                           key, keylen, compress_secret) != TC_OK)
      return TC_ERROR;
    tc_hash_stream_final(inner_state, state_words, *inner_count, inner_buf_len,
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

static void tc_hash_hmac_outer_final(uint32_t* state, size_t state_words,
                             size_t digest_len, const uint8_t* inner,
                             uint8_t* tag, tc_hash_compress_fn compress_secret)
{
  uint8_t block[64];
  size_t i;

  memcpy(block, inner, digest_len);
  block[digest_len] = 0x80U;
  memset(block + digest_len + 1, 0, sizeof(block) - digest_len - 1 - 8);
  tc_internal_store_be64(block + sizeof(block) - 8,
                         ((uint64_t)sizeof(block) + digest_len) << 3);
  compress_secret(state, block);
  for (i = 0; i < state_words; ++i)
    tc_hash_store_be32(tag + 4U * i, state[i]);
#if TC_ZEROIZE
  TC_secure_zero(block, sizeof(block));
#endif
}

#if TC_ENABLE_SHA1

TC_status TC_HMAC_SHA1_init(struct TC_HMAC_SHA1_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || (keylen != 0 && key == NULL))
    return TC_ERROR;
  return tc_hash_hmac_init_common(ctx->Inner.State, &ctx->Inner.Count,
                          &ctx->Inner.BufLen, ctx->Inner.Buf, ctx->OuterState,
                          5, key, keylen, tc_hash_sha1_state_init,
                          tc_hash_sha1_compress_secret);
}

TC_status TC_HMAC_SHA1_update(struct TC_HMAC_SHA1_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL)
    return TC_ERROR;
#endif
  return TC_SHA1_update(&ctx->Inner, data, len);
}

TC_status TC_HMAC_SHA1_final(struct TC_HMAC_SHA1_ctx* ctx, uint8_t* tag)
{
  uint8_t inner[TC_SHA1_DIGESTLEN];

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif

  /* One-shot: TC_ZEROIZE wipes the context; call TC_HMAC_SHA1_init to reuse. */
  if (TC_SHA1_final(&ctx->Inner, inner) != TC_OK)
  {
    TC_HMAC_SHA1_ctx_clear(ctx);
    return TC_ERROR;
  }
  tc_hash_hmac_outer_final(ctx->OuterState, 5, TC_SHA1_DIGESTLEN, inner, tag,
                   tc_hash_sha1_compress_secret);

#if TC_ZEROIZE
  TC_secure_zero(inner, sizeof(inner));
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_HMAC_SHA1_ctx_clear(struct TC_HMAC_SHA1_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_HMAC_SHA1_digest(const uint8_t* key, size_t keylen,
                  const uint8_t* msg, size_t msg_len,
              uint8_t* tag, size_t tag_len)
{
  struct TC_HMAC_SHA1_ctx ctx;
  uint8_t full[TC_SHA1_DIGESTLEN];

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA1_DIGESTLEN ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA1_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;

  if (TC_HMAC_SHA1_update(&ctx, msg, msg_len) != TC_OK ||
      TC_HMAC_SHA1_final(&ctx, full) != TC_OK)
  {
    TC_HMAC_SHA1_ctx_clear(&ctx);
    TC_secure_zero(full, sizeof(full));
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_HMAC_SHA1_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                     const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_SHA1_DIGESTLEN];
  int rc;

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA1_DIGESTLEN)
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA1_digest(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  /* TC_MISMATCH vs TC_ERROR: a well-formed miss is not a programming error. */
  rc = TC_ct_equal(computed, tag, tag_len);
#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return rc;
}

#endif /* TC_ENABLE_SHA1 */

#if TC_ENABLE_SHA224

TC_status TC_HMAC_SHA224_init(struct TC_HMAC_SHA224_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || (keylen != 0 && key == NULL))
    return TC_ERROR;
  /* state_words only sizes H(K) for over-long keys: a 28-byte SHA-224 digest. */
  return tc_hash_hmac_init_common(ctx->Inner.State, &ctx->Inner.Count,
                          &ctx->Inner.BufLen, ctx->Inner.Buf, ctx->OuterState,
                          7, key, keylen,
                          tc_hash_sha224_state_init, tc_hash_sha256_compress_secret);
}

TC_status TC_HMAC_SHA224_update(struct TC_HMAC_SHA224_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL)
    return TC_ERROR;
#endif
  return TC_SHA224_update(&ctx->Inner, data, len);
}

TC_status TC_HMAC_SHA224_final(struct TC_HMAC_SHA224_ctx* ctx, uint8_t* tag)
{
  uint8_t inner[TC_SHA224_DIGESTLEN];

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif

  if (TC_SHA224_final(&ctx->Inner, inner) != TC_OK)
  {
    TC_HMAC_SHA224_ctx_clear(ctx);
    return TC_ERROR;
  }
  tc_hash_hmac_outer_final(ctx->OuterState, 7, TC_SHA224_DIGESTLEN, inner, tag,
                   tc_hash_sha256_compress_secret);

#if TC_ZEROIZE
  TC_secure_zero(inner, sizeof(inner));
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_HMAC_SHA224_ctx_clear(struct TC_HMAC_SHA224_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_HMAC_SHA224_digest(const uint8_t* key, size_t keylen,
                    const uint8_t* msg, size_t msg_len,
                uint8_t* tag, size_t tag_len)
{
  struct TC_HMAC_SHA224_ctx ctx;
  uint8_t full[TC_SHA224_DIGESTLEN];

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA224_DIGESTLEN ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA224_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;

  if (TC_HMAC_SHA224_update(&ctx, msg, msg_len) != TC_OK ||
      TC_HMAC_SHA224_final(&ctx, full) != TC_OK)
  {
    TC_HMAC_SHA224_ctx_clear(&ctx);
    TC_secure_zero(full, sizeof(full));
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_HMAC_SHA224_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_SHA224_DIGESTLEN];
  int rc;

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA224_DIGESTLEN)
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA224_digest(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  rc = TC_ct_equal(computed, tag, tag_len);
#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return rc;
}

#endif /* TC_ENABLE_SHA224 */

#if TC_ENABLE_SHA256

TC_status TC_HMAC_SHA256_init(struct TC_HMAC_SHA256_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || (keylen != 0 && key == NULL))
    return TC_ERROR;
  return tc_hash_hmac_init_common(ctx->Inner.State, &ctx->Inner.Count,
                          &ctx->Inner.BufLen, ctx->Inner.Buf, ctx->OuterState,
                          8, key, keylen,
                          tc_hash_sha256_state_init, tc_hash_sha256_compress_secret);
}

TC_status TC_HMAC_SHA256_update(struct TC_HMAC_SHA256_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL)
    return TC_ERROR;
#endif
  return TC_SHA256_update(&ctx->Inner, data, len);
}

TC_status TC_HMAC_SHA256_final(struct TC_HMAC_SHA256_ctx* ctx, uint8_t* tag)
{
  uint8_t inner[TC_SHA256_DIGESTLEN];

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif

  /* One-shot: TC_ZEROIZE wipes the context; call TC_HMAC_SHA256_init to reuse. */
  if (TC_SHA256_final(&ctx->Inner, inner) != TC_OK)
  {
    TC_HMAC_SHA256_ctx_clear(ctx);
    return TC_ERROR;
  }
  tc_hash_hmac_outer_final(ctx->OuterState, 8, TC_SHA256_DIGESTLEN, inner, tag,
                   tc_hash_sha256_compress_secret);

#if TC_ZEROIZE
  TC_secure_zero(inner, sizeof(inner));
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_HMAC_SHA256_ctx_clear(struct TC_HMAC_SHA256_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_HMAC_SHA256_digest(const uint8_t* key, size_t keylen,
                    const uint8_t* msg, size_t msg_len,
                uint8_t* tag, size_t tag_len)
{
  struct TC_HMAC_SHA256_ctx ctx;
  uint8_t full[TC_SHA256_DIGESTLEN];

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA256_DIGESTLEN ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA256_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;

  if (TC_HMAC_SHA256_update(&ctx, msg, msg_len) != TC_OK ||
      TC_HMAC_SHA256_final(&ctx, full) != TC_OK)
  {
    TC_HMAC_SHA256_ctx_clear(&ctx);
    TC_secure_zero(full, sizeof(full));
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_HMAC_SHA256_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                       const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_SHA256_DIGESTLEN];
  int rc;

  if (tag == NULL ||
      tag_len < TC_HMAC_MIN_TAG_LEN || tag_len > TC_SHA256_DIGESTLEN)
  {
    return TC_ERROR;
  }
  if (TC_HMAC_SHA256_digest(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
    return TC_ERROR;

  /* TC_MISMATCH vs TC_ERROR: a well-formed miss is not a programming error. */
  rc = TC_ct_equal(computed, tag, tag_len);
#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return rc;
}

#endif /* TC_ENABLE_SHA256 */

#endif /* TC_ENABLE_HMAC */

#endif /* TC_HASH_CORE32 */
