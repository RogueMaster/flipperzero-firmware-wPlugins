/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SHA-1, SHA-224 and SHA-256 compression functions and their public hash and
 * HMAC APIs (FIPS 180-4, FIPS 198-1 / RFC 2104). Buffering, padding, keyed
 * state and HMAC construction live in hash_core.c. SHA-384 and SHA-512 live
 * in sha512.c.
 *
 * The compact implementation style follows kokke's tiny-AES-c:
 * https://github.com/kokke/tiny-AES-c
 */

#include <string.h>
#include <tiny_crypto/common.h>
#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256
#include <tiny_crypto/hash.h>
#include "hash_core_internal.h"
#include "internal.h"
#if defined(_MSC_VER)
#include <stdlib.h>
#endif

/* SHA-224 reuses the SHA-256 compression function. */
#define TC_HASH_SHA256_CORE (TC_ENABLE_SHA224 || TC_ENABLE_SHA256)

/* FIPS 180-4 encodes a 64-bit bit count, so the largest message is
 * floor((2^64 - 1) / 8) bytes. */
#define SHA_MAX_MESSAGE_BYTES (UINT64_MAX >> 3)
#define SHA_LENGTH_BYTES 8u

/* Rotations with compile-time constant counts in 1..31 only. */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/*****************************************************************************/
/* SHA-1                                                                     */
/*****************************************************************************/

#if TC_ENABLE_SHA1

#define TC_SHA1_STEP(a, b, c, d, e, f, k, w)                                                       \
  do {                                                                                             \
    uint32_t temp = ROTL32(a, 5) + (f) + (e) + (k) + (w);                                          \
    e = d;                                                                                         \
    d = c;                                                                                         \
    c = ROTL32(b, 30);                                                                             \
    b = a;                                                                                         \
    a = temp;                                                                                      \
  } while (0)

/* One 64-byte block. Rolling 16-word schedule keeps the stack small. The
 * schedule can hold secret input such as HMAC key blocks, so it is wiped
 * after every block. */
static void sha1_compress(void* chaining, const uint8_t* block)
{
  uint32_t* state = (uint32_t*)chaining;
  uint32_t W[16];
  uint32_t a, b, c, d, e;
  unsigned t;

  for (t = 0; t < 16; ++t)
    W[t] = tc_internal_load_be32(block + 4U * t);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];

  /* Rounds 0..15 */
  for (t = 0; t < 16; ++t) {
    uint32_t f = d ^ (b & (c ^ d));
    TC_SHA1_STEP(a, b, c, d, e, f, 0x5A827999U, W[t]);
  }

  /* Rounds 16..19 */
  for (t = 16; t < 20; ++t) {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = d ^ (b & (c ^ d));
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x5A827999U, w);
  }

  /* Rounds 20..39 */
  for (t = 20; t < 40; ++t) {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = b ^ c ^ d;
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x6ED9EBA1U, w);
  }

  /* Rounds 40..59 */
  for (t = 40; t < 60; ++t) {
    uint32_t w = ROTL32(W[(t - 3U) & 15U] ^ W[(t - 8U) & 15U] ^ W[(t - 14U) & 15U] ^ W[t & 15U], 1);
    uint32_t f = (b & c) | (d & (b ^ c));
    W[t & 15U] = w;
    TC_SHA1_STEP(a, b, c, d, e, f, 0x8F1BBCDCU, w);
  }

  /* Rounds 60..79 */
  for (t = 60; t < 80; ++t) {
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

  TC_secure_zero(W, sizeof(W));
}

static void sha1_state_init(void* chaining)
{
  uint32_t* state = (uint32_t*)chaining;
  state[0] = 0x67452301U;
  state[1] = 0xEFCDAB89U;
  state[2] = 0x98BADCFEU;
  state[3] = 0x10325476U;
  state[4] = 0xC3D2E1F0U;
}

static void sha1_digest(const void* state, uint8_t* digest)
{
  tc_hash_store_be32_words(digest, state, 5);
}

static tc_hash_view sha1_view(void* context)
{
  struct TC_SHA1_ctx* ctx = (struct TC_SHA1_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

#if TC_ENABLE_HMAC
static tc_hmac_view hmac_sha1_view(void* context)
{
  struct TC_HMAC_SHA1_ctx* ctx = (struct TC_HMAC_SHA1_ctx*)context;
  return TC_HMAC_VIEW(ctx);
}
#endif

const tc_hash_algorithm_info tc_sha1_info TC_HASH_INFO_STORAGE = {
    TC_SHA1_BLOCKLEN,
    SHA_LENGTH_BYTES,
    TC_SHA1_DIGESTLEN,
    TC_HASH_LENGTH_BIG_ENDIAN,
    SHA_MAX_MESSAGE_BYTES,
    sha1_state_init,
    sha1_compress,
    sha1_digest,
    sha1_view,
    TC_HASH_HMAC_VIEW(hmac_sha1_view)};

#endif /* TC_ENABLE_SHA1 */

/*****************************************************************************/
/* SHA-256 / SHA-224                                                         */
/*****************************************************************************/

#if TC_HASH_SHA256_CORE

#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define HASH_K256_STORAGE PROGMEM
#define HASH_K256_READ(i) pgm_read_dword(&K256[(i)])
#else
#define HASH_K256_STORAGE
#define HASH_K256_READ(i) K256[(i)]
#endif

/* Round constants (ROM/Flash, 256 bytes) */
static const uint32_t K256[64] HASH_K256_STORAGE = {
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U,
    0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU,
    0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU,
    0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU,
    0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
    0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U,
    0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U,
    0xC67178F2U};

#define TC_SHA256_CH(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define TC_SHA256_MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) ^ (y))))
#define TC_SHA256_BSIG0(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define TC_SHA256_BSIG1(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define TC_SHA256_SSIG0(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define TC_SHA256_SSIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

#define TC_SHA256_STEP(a, b, c, d, e, f, g, h, k, w)                                               \
  do {                                                                                             \
    uint32_t t1 = (h) + TC_SHA256_BSIG1(e) + TC_SHA256_CH((e), (f), (g)) + (k) + (w);              \
    uint32_t t2 = TC_SHA256_BSIG0(a) + TC_SHA256_MAJ((a), (b), (c));                               \
    h = g;                                                                                         \
    g = f;                                                                                         \
    f = e;                                                                                         \
    e = d + t1;                                                                                    \
    d = c;                                                                                         \
    c = b;                                                                                         \
    b = a;                                                                                         \
    a = t1 + t2;                                                                                   \
  } while (0)

/* One 64-byte block. Rolling 16-word schedule keeps the stack small. The
 * schedule can hold secret input such as HMAC key blocks, so it is wiped
 * after every block. */
static void sha256_compress(void* chaining, const uint8_t* block)
{
  uint32_t* state = (uint32_t*)chaining;
  uint32_t W[16];
  uint32_t a, b, c, d, e, f, g, h;
  unsigned t;

  for (t = 0; t < 16; ++t)
    W[t] = tc_internal_load_be32(block + 4U * t);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  /* Rounds 0..15 */
  for (t = 0; t < 16; ++t) {
    TC_SHA256_STEP(a, b, c, d, e, f, g, h, HASH_K256_READ(t), W[t]);
  }

  /* Rounds 16..63 */
  for (t = 16; t < 64; ++t) {
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

  TC_secure_zero(W, sizeof(W));
}

#endif /* TC_HASH_SHA256_CORE */

#if TC_ENABLE_SHA256

static void sha256_state_init(void* chaining)
{
  uint32_t* state = (uint32_t*)chaining;
  state[0] = 0x6A09E667U;
  state[1] = 0xBB67AE85U;
  state[2] = 0x3C6EF372U;
  state[3] = 0xA54FF53AU;
  state[4] = 0x510E527FU;
  state[5] = 0x9B05688CU;
  state[6] = 0x1F83D9ABU;
  state[7] = 0x5BE0CD19U;
}

static void sha256_digest(const void* state, uint8_t* digest)
{
  tc_hash_store_be32_words(digest, state, 8);
}

static tc_hash_view sha256_view(void* context)
{
  struct TC_SHA256_ctx* ctx = (struct TC_SHA256_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

#if TC_ENABLE_HMAC
static tc_hmac_view hmac_sha256_view(void* context)
{
  struct TC_HMAC_SHA256_ctx* ctx = (struct TC_HMAC_SHA256_ctx*)context;
  return TC_HMAC_VIEW(ctx);
}
#endif

const tc_hash_algorithm_info tc_sha256_info TC_HASH_INFO_STORAGE = {
    TC_SHA256_BLOCKLEN,    SHA_LENGTH_BYTES,
    TC_SHA256_DIGESTLEN,   TC_HASH_LENGTH_BIG_ENDIAN,
    SHA_MAX_MESSAGE_BYTES, sha256_state_init,
    sha256_compress,       sha256_digest,
    sha256_view,           TC_HASH_HMAC_VIEW(hmac_sha256_view)};

#endif /* TC_ENABLE_SHA256 */

#if TC_ENABLE_SHA224

/* FIPS 180-4 section 5.3.2: the SHA-224 IV is the second 32 bits of the
 * fractional parts of the square roots of the 9th through 16th primes. */
static void sha224_state_init(void* chaining)
{
  uint32_t* state = (uint32_t*)chaining;
  state[0] = 0xC1059ED8U;
  state[1] = 0x367CD507U;
  state[2] = 0x3070DD17U;
  state[3] = 0xF70E5939U;
  state[4] = 0xFFC00B31U;
  state[5] = 0x68581511U;
  state[6] = 0x64F98FA7U;
  state[7] = 0xBEFA4FA4U;
}

/* SHA-224 emits the leftmost seven state words. */
static void sha224_digest(const void* state, uint8_t* digest)
{
  tc_hash_store_be32_words(digest, state, 7);
}

static tc_hash_view sha224_view(void* context)
{
  struct TC_SHA224_ctx* ctx = (struct TC_SHA224_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

#if TC_ENABLE_HMAC
static tc_hmac_view hmac_sha224_view(void* context)
{
  struct TC_HMAC_SHA224_ctx* ctx = (struct TC_HMAC_SHA224_ctx*)context;
  return TC_HMAC_VIEW(ctx);
}
#endif

const tc_hash_algorithm_info tc_sha224_info TC_HASH_INFO_STORAGE = {
    TC_SHA224_BLOCKLEN,    SHA_LENGTH_BYTES,
    TC_SHA224_DIGESTLEN,   TC_HASH_LENGTH_BIG_ENDIAN,
    SHA_MAX_MESSAGE_BYTES, sha224_state_init,
    sha256_compress,       sha224_digest,
    sha224_view,           TC_HASH_HMAC_VIEW(hmac_sha224_view)};

#endif /* TC_ENABLE_SHA224 */

/*****************************************************************************/
/* Public hash APIs                                                          */
/*****************************************************************************/

#if TC_ENABLE_SHA1
TC_HASH_DEFINE(SHA1, sha1)
#endif

#if TC_ENABLE_SHA224
TC_HASH_DEFINE(SHA224, sha224)
#endif

#if TC_ENABLE_SHA256
TC_HASH_DEFINE(SHA256, sha256)
#endif

/*****************************************************************************/
/* Public HMAC APIs                                                          */
/*****************************************************************************/

#if TC_ENABLE_HMAC

#if TC_ENABLE_SHA1
TC_HMAC_DEFINE(SHA1, sha1)
#endif

#if TC_ENABLE_SHA224
TC_HMAC_DEFINE(SHA224, sha224)
#endif

#if TC_ENABLE_SHA256
TC_HMAC_DEFINE(SHA256, sha256)
#endif

#endif /* TC_ENABLE_HMAC */

#endif /* TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 */
