/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/md5.h>
#if TC_ENABLE_MD5
#include "hash_core_internal.h"

#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define MD5_TABLE_STORAGE PROGMEM
#define MD5_TABLE_READ(i) pgm_read_dword(&constants[(i)])
#define MD5_SHIFT_READ(i) pgm_read_byte(&shifts[(i) / 16][(i) % 4])
#else
#define MD5_TABLE_STORAGE
#define MD5_TABLE_READ(i) constants[(i)]
#define MD5_SHIFT_READ(i) shifts[(i) / 16][(i) % 4]
#endif

/* RFC 1321 section 3.4: floor(2^32 * abs(sin(i + 1))). */
static const uint32_t constants[64] MD5_TABLE_STORAGE = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u,
    0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u,
    0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du,
    0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
    0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
    0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u,
    0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu,
    0xeb86d391u};

static uint32_t load_word(const uint8_t* data)
{
  return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
         (uint32_t)data[3] << 24;
}

static void compress(void* chaining, const uint8_t* block)
{
  uint32_t* state = (uint32_t*)chaining;
  static const uint8_t shifts[4][4] MD5_TABLE_STORAGE = {
      {7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21}};
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  for (unsigned round = 0; round < 64; ++round) {
    uint32_t function, sum;
    unsigned word, shift = MD5_SHIFT_READ(round);
    if (round < 16) {
      function = (b & c) | (~b & d);
      word = round;
    } else if (round < 32) {
      function = (b & d) | (c & ~d);
      word = (5 * round + 1) % 16;
    } else if (round < 48) {
      function = b ^ c ^ d;
      word = (3 * round + 5) % 16;
    } else {
      function = c ^ (b | ~d);
      word = (7 * round) % 16;
    }
    /* Read words from the shared block instead of expanding a schedule. */
    sum = a + function + MD5_TABLE_READ(round) + load_word(block + 4 * word);
    a = d;
    d = c;
    c = b;
    b += (sum << shift) | (sum >> (32 - shift));
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

static void state_init(void* chaining)
{
  uint32_t* state = (uint32_t*)chaining;
  state[0] = 0x67452301u;
  state[1] = 0xefcdab89u;
  state[2] = 0x98badcfeu;
  state[3] = 0x10325476u;
}

static void digest_out(const void* state, uint8_t* digest)
{
  tc_hash_store_le32_words(digest, state, 4);
}

static tc_hash_view view(void* context)
{
  struct TC_MD5_ctx* ctx = (struct TC_MD5_ctx*)context;
  return TC_HASH_VIEW(ctx);
}

/* RFC 1321 encodes the low 64 bits of the bit count, so the 64-bit byte
 * count sets the message limit. MD5 has no HMAC API. */
static const tc_hash_algorithm_info tc_md5_info TC_HASH_INFO_STORAGE = {
    TC_MD5_BLOCKLEN,
    8u,
    TC_MD5_DIGESTLEN,
    TC_HASH_LENGTH_LITTLE_ENDIAN,
    UINT64_MAX,
    state_init,
    compress,
    digest_out,
    view,
    NULL};

TC_HASH_DEFINE(MD5, md5)
#endif
