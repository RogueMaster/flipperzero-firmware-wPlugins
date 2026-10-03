/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SHA-384 and SHA-512 compression and their public hash and HMAC APIs
 * (FIPS 180-4, FIPS 198-1 / RFC 2104): 128-byte blocks, eight 64-bit state
 * words and a 128-bit length field. Buffering, padding, keyed state and HMAC
 * construction live in hash_core.c.
 */

#include <string.h>
#include <tiny_crypto/common.h>
#if TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.h>
#include "hash_core_internal.h"
#include "internal.h"

/* The 128-bit length field represents any 64-bit byte count, so the only
 * limit is the 64-bit count field. */
#define SHA512_MAX_MESSAGE_BYTES UINT64_MAX
#define SHA512_LENGTH_BYTES 16u

#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define HASH_K512_STORAGE PROGMEM
#ifdef pgm_read_qword
#define HASH_K512_READ(i) pgm_read_qword(&K512[(i)])
#else
/* avr-libc < 2.0 has no 64-bit flash read. Assemble two little-endian
       dwords the way the compiler laid the constant out. */
#define HASH_K512_READ(i)                                                                          \
  (((uint64_t)pgm_read_dword((const uint32_t*)&K512[(i)] + 1) << 32) |                             \
   (uint64_t)pgm_read_dword((const uint32_t*)&K512[(i)]))
#endif
#else
#define HASH_K512_STORAGE
#define HASH_K512_READ(i) K512[(i)]
#endif

/* Rotation with compile-time constant counts in 1..63 only. */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

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
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL};

#define TC_SHA512_CH(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define TC_SHA512_MAJ(x, y, z) (((x) & (y)) | ((z) & ((x) ^ (y))))
#define TC_SHA512_BSIG0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define TC_SHA512_BSIG1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define TC_SHA512_SSIG0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define TC_SHA512_SSIG1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

#define TC_SHA512_STEP(a, b, c, d, e, f, g, h, k, w)                                               \
  do {                                                                                             \
    uint64_t t1 = (h) + TC_SHA512_BSIG1(e) + TC_SHA512_CH((e), (f), (g)) + (k) + (w);              \
    uint64_t t2 = TC_SHA512_BSIG0(a) + TC_SHA512_MAJ((a), (b), (c));                               \
    h = g;                                                                                         \
    g = f;                                                                                         \
    f = e;                                                                                         \
    e = d + t1;                                                                                    \
    d = c;                                                                                         \
    c = b;                                                                                         \
    b = a;                                                                                         \
    a = t1 + t2;                                                                                   \
  } while (0)

/* One 128-byte block. Rolling 16-word schedule keeps the stack small. The
 * schedule can hold secret input such as HMAC key blocks, so it is wiped
 * after every block. */
static void sha512_compress(void* chaining, const uint8_t* block)
{
  uint64_t* state = (uint64_t*)chaining;
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
  for (t = 0; t < 16; ++t) {
    TC_SHA512_STEP(a, b, c, d, e, f, g, h, HASH_K512_READ(t), W[t]);
  }

  /* Rounds 16..79 */
  for (t = 16; t < 80; ++t) {
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

  TC_secure_zero(W, sizeof(W));
}

/*****************************************************************************/
/* SHA-512                                                                   */
/*****************************************************************************/

#if TC_ENABLE_SHA512

static void sha512_state_init(void* chaining)
{
  uint64_t* state = (uint64_t*)chaining;
  state[0] = 0x6A09E667F3BCC908ULL;
  state[1] = 0xBB67AE8584CAA73BULL;
  state[2] = 0x3C6EF372FE94F82BULL;
  state[3] = 0xA54FF53A5F1D36F1ULL;
  state[4] = 0x510E527FADE682D1ULL;
  state[5] = 0x9B05688C2B3E6C1FULL;
  state[6] = 0x1F83D9ABFB41BD6BULL;
  state[7] = 0x5BE0CD19137E2179ULL;
}

static void sha512_digest(const void* state, uint8_t* digest)
{
  tc_hash_store_be64_words(digest, state, 8);
}

static tc_hash_view sha512_view(void* context)
{
  struct TC_SHA512_ctx* ctx = (struct TC_SHA512_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

#if TC_ENABLE_HMAC
static tc_hmac_view hmac_sha512_view(void* context)
{
  struct TC_HMAC_SHA512_ctx* ctx = (struct TC_HMAC_SHA512_ctx*)context;
  return TC_HMAC_VIEW(ctx);
}
#endif

const tc_hash_algorithm_info tc_sha512_info TC_HASH_INFO_STORAGE = {
    TC_SHA512_BLOCKLEN,
    SHA512_LENGTH_BYTES,
    TC_SHA512_DIGESTLEN,
    TC_HASH_LENGTH_BIG_ENDIAN,
    SHA512_MAX_MESSAGE_BYTES,
    sha512_state_init,
    sha512_compress,
    sha512_digest,
    sha512_view,
    TC_HASH_HMAC_VIEW(hmac_sha512_view)};

#endif /* TC_ENABLE_SHA512 */

/*****************************************************************************/
/* SHA-384                                                                   */
/*****************************************************************************/

#if TC_ENABLE_SHA384

/* FIPS 180-4 section 5.3.4: fractional parts of the square roots of the
 * 9th through 16th primes. */
static void sha384_state_init(void* chaining)
{
  uint64_t* state = (uint64_t*)chaining;
  state[0] = 0xCBBB9D5DC1059ED8ULL;
  state[1] = 0x629A292A367CD507ULL;
  state[2] = 0x9159015A3070DD17ULL;
  state[3] = 0x152FECD8F70E5939ULL;
  state[4] = 0x67332667FFC00B31ULL;
  state[5] = 0x8EB44A8768581511ULL;
  state[6] = 0xDB0C2E0D64F98FA7ULL;
  state[7] = 0x47B5481DBEFA4FA4ULL;
}

/* SHA-384 emits the leftmost six state words. */
static void sha384_digest(const void* state, uint8_t* digest)
{
  tc_hash_store_be64_words(digest, state, 6);
}

static tc_hash_view sha384_view(void* context)
{
  struct TC_SHA384_ctx* ctx = (struct TC_SHA384_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

#if TC_ENABLE_HMAC
static tc_hmac_view hmac_sha384_view(void* context)
{
  struct TC_HMAC_SHA384_ctx* ctx = (struct TC_HMAC_SHA384_ctx*)context;
  return TC_HMAC_VIEW(ctx);
}
#endif

const tc_hash_algorithm_info tc_sha384_info TC_HASH_INFO_STORAGE = {
    TC_SHA384_BLOCKLEN,
    SHA512_LENGTH_BYTES,
    TC_SHA384_DIGESTLEN,
    TC_HASH_LENGTH_BIG_ENDIAN,
    SHA512_MAX_MESSAGE_BYTES,
    sha384_state_init,
    sha512_compress,
    sha384_digest,
    sha384_view,
    TC_HASH_HMAC_VIEW(hmac_sha384_view)};

#endif /* TC_ENABLE_SHA384 */

/*****************************************************************************/
/* Public hash APIs                                                          */
/*****************************************************************************/

#if TC_ENABLE_SHA384
TC_HASH_DEFINE(SHA384, sha384)
#endif

#if TC_ENABLE_SHA512
TC_HASH_DEFINE(SHA512, sha512)
#endif

/*****************************************************************************/
/* Public HMAC APIs                                                          */
/*****************************************************************************/

#if TC_ENABLE_HMAC

#if TC_ENABLE_SHA384
TC_HMAC_DEFINE(SHA384, sha384)
#endif

#if TC_ENABLE_SHA512
TC_HMAC_DEFINE(SHA512, sha512)
#endif

#endif /* TC_ENABLE_HMAC */

#endif /* TC_ENABLE_SHA384 || TC_ENABLE_SHA512 */
