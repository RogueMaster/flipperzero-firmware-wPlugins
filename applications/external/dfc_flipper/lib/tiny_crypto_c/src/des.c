/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Portable DES and Triple-DES (3DES / TDES) block cipher and key schedule for
 * small embedded devices and microcontrollers. Modes live in des_modes.c.
 *
 * The compact implementation style follows kokke's tiny-AES-c:
 * https://github.com/kokke/tiny-AES-c
 *
 */

#include <string.h>
#include <tiny_crypto/des.h>
#include "internal.h"
#include "des_internal.h"

/* AVR uses separate program and data address spaces. Plain const arrays are
 * copied into scarce SRAM at startup, so tables need both PROGMEM placement
 * and explicit reads. Other targets use normal memory accesses. */
#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define TC_DES_TABLE_STORAGE PROGMEM
#define TC_DES_READ_U32(table, index) pgm_read_dword(&(table)[(index)])
#define TC_DES_READ_U8(table, index) pgm_read_byte(&(table)[(index)])
#else
#define TC_DES_TABLE_STORAGE
#define TC_DES_READ_U32(table, index) ((table)[(index)])
#define TC_DES_READ_U8(table, index) ((table)[(index)])
#endif

/*****************************************************************************/
/* Private Lookup Tables (ROM/Flash)                                         */
/*****************************************************************************/

/* Combined S-Box & P-Permutation Tables (8 x 64 uint32_t = 2KB ROM) */
static const uint32_t SP1[64] TC_DES_TABLE_STORAGE = {
    0x00808200U, 0x00000000U, 0x00008000U, 0x00808202U, 0x00808002U, 0x00008202U, 0x00000002U,
    0x00008000U, 0x00000200U, 0x00808200U, 0x00808202U, 0x00000200U, 0x00800202U, 0x00808002U,
    0x00800000U, 0x00000002U, 0x00000202U, 0x00800200U, 0x00800200U, 0x00008200U, 0x00008200U,
    0x00808000U, 0x00808000U, 0x00800202U, 0x00008002U, 0x00800002U, 0x00800002U, 0x00008002U,
    0x00000000U, 0x00000202U, 0x00008202U, 0x00800000U, 0x00008000U, 0x00808202U, 0x00000002U,
    0x00808000U, 0x00808200U, 0x00800000U, 0x00800000U, 0x00000200U, 0x00808002U, 0x00008000U,
    0x00008200U, 0x00800002U, 0x00000200U, 0x00000002U, 0x00800202U, 0x00008202U, 0x00808202U,
    0x00008002U, 0x00808000U, 0x00800202U, 0x00800002U, 0x00000202U, 0x00008202U, 0x00808200U,
    0x00000202U, 0x00800200U, 0x00800200U, 0x00000000U, 0x00008002U, 0x00008200U, 0x00000000U,
    0x00808002U};

static const uint32_t SP2[64] TC_DES_TABLE_STORAGE = {
    0x40084010U, 0x40004000U, 0x00004000U, 0x00084010U, 0x00080000U, 0x00000010U, 0x40080010U,
    0x40004010U, 0x40000010U, 0x40084010U, 0x40084000U, 0x40000000U, 0x40004000U, 0x00080000U,
    0x00000010U, 0x40080010U, 0x00084000U, 0x00080010U, 0x40004010U, 0x00000000U, 0x40000000U,
    0x00004000U, 0x00084010U, 0x40080000U, 0x00080010U, 0x40000010U, 0x00000000U, 0x00084000U,
    0x00004010U, 0x40084000U, 0x40080000U, 0x00004010U, 0x00000000U, 0x00084010U, 0x40080010U,
    0x00080000U, 0x40004010U, 0x40080000U, 0x40084000U, 0x00004000U, 0x40080000U, 0x40004000U,
    0x00000010U, 0x40084010U, 0x00084010U, 0x00000010U, 0x00004000U, 0x40000000U, 0x00004010U,
    0x40084000U, 0x00080000U, 0x40000010U, 0x00080010U, 0x40004010U, 0x40000010U, 0x00080010U,
    0x00084000U, 0x00000000U, 0x40004000U, 0x00004010U, 0x40000000U, 0x40080010U, 0x40084010U,
    0x00084000U};

static const uint32_t SP3[64] TC_DES_TABLE_STORAGE = {
    0x00000104U, 0x04010100U, 0x00000000U, 0x04010004U, 0x04000100U, 0x00000000U, 0x00010104U,
    0x04000100U, 0x00010004U, 0x04000004U, 0x04000004U, 0x00010000U, 0x04010104U, 0x00010004U,
    0x04010000U, 0x00000104U, 0x04000000U, 0x00000004U, 0x04010100U, 0x00000100U, 0x00010100U,
    0x04010000U, 0x04010004U, 0x00010104U, 0x04000104U, 0x00010100U, 0x00010000U, 0x04000104U,
    0x00000004U, 0x04010104U, 0x00000100U, 0x04000000U, 0x04010100U, 0x04000000U, 0x00010004U,
    0x00000104U, 0x00010000U, 0x04010100U, 0x04000100U, 0x00000000U, 0x00000100U, 0x00010004U,
    0x04010104U, 0x04000100U, 0x04000004U, 0x00000100U, 0x00000000U, 0x04010004U, 0x04000104U,
    0x00010000U, 0x04000000U, 0x04010104U, 0x00000004U, 0x00010104U, 0x00010100U, 0x04000004U,
    0x04010000U, 0x04000104U, 0x00000104U, 0x04010000U, 0x00010104U, 0x00000004U, 0x04010004U,
    0x00010100U};

static const uint32_t SP4[64] TC_DES_TABLE_STORAGE = {
    0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U, 0x00401040U, 0x80400040U, 0x80400000U,
    0x80001000U, 0x00000000U, 0x00401000U, 0x00401000U, 0x80401040U, 0x80000040U, 0x00000000U,
    0x00400040U, 0x80400000U, 0x80000000U, 0x00001000U, 0x00400000U, 0x80401000U, 0x00000040U,
    0x00400000U, 0x80001000U, 0x00001040U, 0x80400040U, 0x80000000U, 0x00001040U, 0x00400040U,
    0x00001000U, 0x00401040U, 0x80401040U, 0x80000040U, 0x00400040U, 0x80400000U, 0x00401000U,
    0x80401040U, 0x80000040U, 0x00000000U, 0x00000000U, 0x00401000U, 0x00001040U, 0x00400040U,
    0x80400040U, 0x80000000U, 0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U, 0x80401040U,
    0x80000040U, 0x80000000U, 0x00001000U, 0x80400000U, 0x80001000U, 0x00401040U, 0x80400040U,
    0x80001000U, 0x00001040U, 0x00400000U, 0x80401000U, 0x00000040U, 0x00400000U, 0x00001000U,
    0x00401040U};

static const uint32_t SP5[64] TC_DES_TABLE_STORAGE = {
    0x00000080U, 0x01040080U, 0x01040000U, 0x21000080U, 0x00040000U, 0x00000080U, 0x20000000U,
    0x01040000U, 0x20040080U, 0x00040000U, 0x01000080U, 0x20040080U, 0x21000080U, 0x21040000U,
    0x00040080U, 0x20000000U, 0x01000000U, 0x20040000U, 0x20040000U, 0x00000000U, 0x20000080U,
    0x21040080U, 0x21040080U, 0x01000080U, 0x21040000U, 0x20000080U, 0x00000000U, 0x21000000U,
    0x01040080U, 0x01000000U, 0x21000000U, 0x00040080U, 0x00040000U, 0x21000080U, 0x00000080U,
    0x01000000U, 0x20000000U, 0x01040000U, 0x21000080U, 0x20040080U, 0x01000080U, 0x20000000U,
    0x21040000U, 0x01040080U, 0x20040080U, 0x00000080U, 0x01000000U, 0x21040000U, 0x21040080U,
    0x00040080U, 0x21000000U, 0x21040080U, 0x01040000U, 0x00000000U, 0x20040000U, 0x21000000U,
    0x00040080U, 0x01000080U, 0x20000080U, 0x00040000U, 0x00000000U, 0x20040000U, 0x01040080U,
    0x20000080U};

static const uint32_t SP6[64] TC_DES_TABLE_STORAGE = {
    0x10000008U, 0x10200000U, 0x00002000U, 0x10202008U, 0x10200000U, 0x00000008U, 0x10202008U,
    0x00200000U, 0x10002000U, 0x00202008U, 0x00200000U, 0x10000008U, 0x00200008U, 0x10002000U,
    0x10000000U, 0x00002008U, 0x00000000U, 0x00200008U, 0x10002008U, 0x00002000U, 0x00202000U,
    0x10002008U, 0x00000008U, 0x10200008U, 0x10200008U, 0x00000000U, 0x00202008U, 0x10202000U,
    0x00002008U, 0x00202000U, 0x10202000U, 0x10000000U, 0x10002000U, 0x00000008U, 0x10200008U,
    0x00202000U, 0x10202008U, 0x00200000U, 0x00002008U, 0x10000008U, 0x00200000U, 0x10002000U,
    0x10000000U, 0x00002008U, 0x10000008U, 0x10202008U, 0x00202000U, 0x10200000U, 0x00202008U,
    0x10202000U, 0x00000000U, 0x10200008U, 0x00000008U, 0x00002000U, 0x10200000U, 0x00202008U,
    0x00002000U, 0x00200008U, 0x10002008U, 0x00000000U, 0x10202000U, 0x10000000U, 0x00200008U,
    0x10002008U};

static const uint32_t SP7[64] TC_DES_TABLE_STORAGE = {
    0x00100000U, 0x02100001U, 0x02000401U, 0x00000000U, 0x00000400U, 0x02000401U, 0x00100401U,
    0x02100400U, 0x02100401U, 0x00100000U, 0x00000000U, 0x02000001U, 0x00000001U, 0x02000000U,
    0x02100001U, 0x00000401U, 0x02000400U, 0x00100401U, 0x00100001U, 0x02000400U, 0x02000001U,
    0x02100000U, 0x02100400U, 0x00100001U, 0x02100000U, 0x00000400U, 0x00000401U, 0x02100401U,
    0x00100400U, 0x00000001U, 0x02000000U, 0x00100400U, 0x02000000U, 0x00100400U, 0x00100000U,
    0x02000401U, 0x02000401U, 0x02100001U, 0x02100001U, 0x00000001U, 0x00100001U, 0x02000000U,
    0x02000400U, 0x00100000U, 0x02100400U, 0x00000401U, 0x00100401U, 0x02100400U, 0x00000401U,
    0x02000001U, 0x02100401U, 0x02100000U, 0x00100400U, 0x00000000U, 0x00000001U, 0x02100401U,
    0x00000000U, 0x00100401U, 0x02100000U, 0x00000400U, 0x02000001U, 0x02000400U, 0x00000400U,
    0x00100001U};

static const uint32_t SP8[64] TC_DES_TABLE_STORAGE = {
    0x08000820U, 0x00000800U, 0x00020000U, 0x08020820U, 0x08000000U, 0x08000820U, 0x00000020U,
    0x08000000U, 0x00020020U, 0x08020000U, 0x08020820U, 0x00020800U, 0x08020800U, 0x00020820U,
    0x00000800U, 0x00000020U, 0x08020000U, 0x08000020U, 0x08000800U, 0x00000820U, 0x00020800U,
    0x00020020U, 0x08020020U, 0x08020800U, 0x00000820U, 0x00000000U, 0x00000000U, 0x08020020U,
    0x08000020U, 0x08000800U, 0x00020820U, 0x00020000U, 0x00020820U, 0x00020000U, 0x08020800U,
    0x00000800U, 0x00000020U, 0x08020020U, 0x00000800U, 0x00020820U, 0x08000800U, 0x00000020U,
    0x08000020U, 0x08020000U, 0x08020020U, 0x08000000U, 0x00020000U, 0x08000820U, 0x00000000U,
    0x08020820U, 0x00020020U, 0x08000020U, 0x08020000U, 0x08000800U, 0x08000820U, 0x00000000U,
    0x08020820U, 0x00020800U, 0x00020800U, 0x00000820U, 0x00000820U, 0x00020020U, 0x08000000U,
    0x08020800U};

/* Permuted Choice 1 (PC-1) matrices */
static const uint8_t PC1_C[28] TC_DES_TABLE_STORAGE = {57, 49, 41, 33, 25, 17, 9,  1,  58, 50,
                                                       42, 34, 26, 18, 10, 2,  59, 51, 43, 35,
                                                       27, 19, 11, 3,  60, 52, 44, 36};

static const uint8_t PC1_D[28] TC_DES_TABLE_STORAGE = {63, 55, 47, 39, 31, 23, 15, 7,  62, 54,
                                                       46, 38, 30, 22, 14, 6,  61, 53, 45, 37,
                                                       29, 21, 13, 5,  28, 20, 12, 4};

/* Permuted Choice 2 (PC-2) matrix */
/* DES_ prefix avoids AVR register macros such as PC2. */
static const uint8_t DES_PC2[48] TC_DES_TABLE_STORAGE = {
    14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10, 23, 19, 12, 4,  26, 8,  16, 7,  27, 20, 13, 2,
    41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};

/* Subkey Left Shift Schedule */
static const uint8_t SHIFTS[16] TC_DES_TABLE_STORAGE = {1, 1, 2, 2, 2, 2, 2, 2,
                                                        1, 2, 2, 2, 2, 2, 2, 1};

/*****************************************************************************/
/* Private Helper Functions                                                  */
/*****************************************************************************/

int tc_des_keys_equal(const uint8_t* left, const uint8_t* right)
{
  uint8_t diff = 0;
  size_t i;

  for (i = 0; i < TC_DES_KEYLEN; ++i)
    diff |= (uint8_t)((left[i] ^ right[i]) & 0xfeu);
  return diff == 0;
}

#if TC_DES_REJECT_WEAK_KEYS
/* The four weak and twelve semi-weak DES keys. Parity bits are ignored when
 * comparing, so equivalent encodings are rejected too. */
static const uint8_t des_weak_keys[16][TC_DES_KEYLEN] TC_DES_TABLE_STORAGE = {
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    {0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe},
    {0xe0, 0xe0, 0xe0, 0xe0, 0xf1, 0xf1, 0xf1, 0xf1},
    {0x1f, 0x1f, 0x1f, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e},
    {0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe},
    {0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01},
    {0x1f, 0xe0, 0x1f, 0xe0, 0x0e, 0xf1, 0x0e, 0xf1},
    {0xe0, 0x1f, 0xe0, 0x1f, 0xf1, 0x0e, 0xf1, 0x0e},
    {0x01, 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1},
    {0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1, 0x01},
    {0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe},
    {0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e},
    {0x01, 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e},
    {0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e, 0x01},
    {0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1, 0xfe},
    {0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1}};

static int tc_des_key_is_weak(const uint8_t* key)
{
  uint8_t weak = 0;
  size_t candidate;
  size_t i;

  for (candidate = 0; candidate < 16; ++candidate) {
    uint8_t diff = 0;
    for (i = 0; i < TC_DES_KEYLEN; ++i)
      diff |= (uint8_t)((key[i] ^ TC_DES_READ_U8(des_weak_keys[candidate], i)) & 0xfeu);
    weak |= (uint8_t)(diff == 0);
  }
  return weak != 0;
}

int tc_des_bundle_is_rejected(const uint8_t* key, size_t keylen)
{
  if (tc_des_key_is_weak(key))
    return 1;
  if (keylen >= TC_DES_KEYLEN_2KEY &&
      (tc_des_key_is_weak(key + TC_DES_KEYLEN) || tc_des_keys_equal(key, key + TC_DES_KEYLEN)))
    return 1;
  return keylen == TC_DES_KEYLEN_3KEY &&
         (tc_des_key_is_weak(key + (2u * TC_DES_KEYLEN)) ||
          tc_des_keys_equal(key + TC_DES_KEYLEN, key + (2u * TC_DES_KEYLEN)));
}
#endif

static inline uint8_t tc_des_get_key_bit(const uint8_t* key, uint8_t bit_1based)
{
  uint8_t byte_idx = (uint8_t)((bit_1based - 1) / 8);
  uint8_t bit_idx = (uint8_t)(7 - ((bit_1based - 1) % 8));
  return (key[byte_idx] >> bit_idx) & 1U;
}

/* Pack each 48-bit round key into six bytes. The round function expands these
 * into eight 6-bit S-box inputs, saving 25% versus byte-per-input storage. */
void tc_des_key_schedule(uint8_t (*sk)[6], const uint8_t* key)
{
  uint32_t C = 0;
  uint32_t D = 0;

  for (uint8_t i = 0; i < 28; ++i) {
    C = (C << 1) | tc_des_get_key_bit(key, TC_DES_READ_U8(PC1_C, i));
    D = (D << 1) | tc_des_get_key_bit(key, TC_DES_READ_U8(PC1_D, i));
  }

  for (uint8_t r = 0; r < 16; ++r) {
    uint8_t shift = TC_DES_READ_U8(SHIFTS, r);
    C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFFU;
    D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFFU;

    memset(sk[r], 0, 6);

    for (uint8_t i = 0; i < 48; ++i) {
      uint8_t bit_pos = TC_DES_READ_U8(DES_PC2, i);
      uint8_t bit_val;
      if (bit_pos <= 28)
        bit_val = (uint8_t)((C >> (28u - bit_pos)) & 1u);
      else
        bit_val = (uint8_t)((D >> (56u - bit_pos)) & 1u);
      sk[r][i / 8u] |= (uint8_t)(bit_val << (7u - (i % 8u)));
    }
  }
  TC_secure_zero(&C, sizeof C);
  TC_secure_zero(&D, sizeof D);
}

/* A single key fills sk[0..15]. A TDEA bundle fills sk[0..47] with K1, K2
 * and K3, where two-key TDEA repeats K1 as K3. */
void tc_des_schedule_key(uint8_t (*sk)[6], const uint8_t* key, size_t keylen)
{
  tc_des_key_schedule(&sk[0], key);
  if (keylen == TC_DES_KEYLEN)
    return;
  tc_des_key_schedule(&sk[16], key + TC_DES_KEYLEN);
  if (keylen == TC_DES_KEYLEN_2KEY)
    memcpy(&sk[32], &sk[0], 16u * 6u);
  else
    tc_des_key_schedule(&sk[32], key + TC_DES_KEYLEN_2KEY);
}

/* Fast 32-bit Outerbridge Initial Permutation (IP) */
static inline void tc_des_initial_permutation(uint32_t* pL, uint32_t* pR)
{
  uint32_t L = *pL;
  uint32_t R = *pR;
  uint32_t t;

  t = ((L >> 4) ^ R) & 0x0F0F0F0FU;
  R ^= t;
  L ^= (t << 4);

  t = ((L >> 16) ^ R) & 0x0000FFFFU;
  R ^= t;
  L ^= (t << 16);

  t = ((R >> 2) ^ L) & 0x33333333U;
  L ^= t;
  R ^= (t << 2);

  t = ((R >> 8) ^ L) & 0x00FF00FFU;
  L ^= t;
  R ^= (t << 8);

  t = ((L >> 1) ^ R) & 0x55555555U;
  R ^= t;
  L ^= (t << 1);

  *pL = L;
  *pR = R;
}

/* Fast 32-bit Outerbridge Final Permutation (FP = IP^-1) */
static inline void tc_des_final_permutation(uint32_t* pL, uint32_t* pR)
{
  uint32_t L = *pL;
  uint32_t R = *pR;
  uint32_t t;

  t = ((R >> 1) ^ L) & 0x55555555U;
  L ^= t;
  R ^= (t << 1);

  t = ((L >> 8) ^ R) & 0x00FF00FFU;
  R ^= t;
  L ^= (t << 8);

  t = ((L >> 2) ^ R) & 0x33333333U;
  R ^= t;
  L ^= (t << 2);

  t = ((R >> 16) ^ L) & 0x0000FFFFU;
  L ^= t;
  R ^= (t << 16);

  t = ((R >> 4) ^ L) & 0x0F0F0F0FU;
  L ^= t;
  R ^= (t << 4);

  *pL = L;
  *pR = R;
}

/* Single DES block cipher core */
void tc_des_cipher_block(const uint8_t (*sk)[6], uint8_t* buf, int decrypt)
{
  uint32_t L = tc_internal_load_be32(buf);
  uint32_t R = tc_internal_load_be32(buf + 4);

  tc_des_initial_permutation(&L, &R);

  for (int i = 0; i < 16; ++i) {
    int r = decrypt ? (15 - i) : i;

    /* Split the packed 48 bits at 6-bit boundaries without 64-bit arithmetic. */
    const uint8_t* packed = sk[r];
    uint8_t sk8_0 = (uint8_t)(packed[0] >> 2);
    uint8_t sk8_1 = (uint8_t)(((packed[0] & 0x03u) << 4) | (packed[1] >> 4));
    uint8_t sk8_2 = (uint8_t)(((packed[1] & 0x0fu) << 2) | (packed[2] >> 6));
    uint8_t sk8_3 = (uint8_t)(packed[2] & 0x3fu);
    uint8_t sk8_4 = (uint8_t)(packed[3] >> 2);
    uint8_t sk8_5 = (uint8_t)(((packed[3] & 0x03u) << 4) | (packed[4] >> 4));
    uint8_t sk8_6 = (uint8_t)(((packed[4] & 0x0fu) << 2) | (packed[5] >> 6));
    uint8_t sk8_7 = (uint8_t)(packed[5] & 0x3fu);

    uint8_t b1 = (uint8_t)((((R & 1U) << 5) | ((R >> 27) & 0x1FU)) ^ sk8_0);
    uint8_t b2 = (uint8_t)(((R >> 23) & 0x3FU) ^ sk8_1);
    uint8_t b3 = (uint8_t)(((R >> 19) & 0x3FU) ^ sk8_2);
    uint8_t b4 = (uint8_t)(((R >> 15) & 0x3FU) ^ sk8_3);
    uint8_t b5 = (uint8_t)(((R >> 11) & 0x3FU) ^ sk8_4);
    uint8_t b6 = (uint8_t)(((R >> 7) & 0x3FU) ^ sk8_5);
    uint8_t b7 = (uint8_t)(((R >> 3) & 0x3FU) ^ sk8_6);
    uint8_t b8 = (uint8_t)((((R & 0x1FU) << 1) | ((R >> 31) & 1U)) ^ sk8_7);

    uint32_t f_res = TC_DES_READ_U32(SP1, b1) ^ TC_DES_READ_U32(SP2, b2) ^
                     TC_DES_READ_U32(SP3, b3) ^ TC_DES_READ_U32(SP4, b4) ^
                     TC_DES_READ_U32(SP5, b5) ^ TC_DES_READ_U32(SP6, b6) ^
                     TC_DES_READ_U32(SP7, b7) ^ TC_DES_READ_U32(SP8, b8);

    uint32_t L_next = R;
    uint32_t R_next = L ^ f_res;
    L = L_next;
    R = R_next;
  }

  tc_des_final_permutation(&L, &R);

  buf[0] = (uint8_t)(R >> 24);
  buf[1] = (uint8_t)(R >> 16);
  buf[2] = (uint8_t)(R >> 8);
  buf[3] = (uint8_t)(R);
  buf[4] = (uint8_t)(L >> 24);
  buf[5] = (uint8_t)(L >> 16);
  buf[6] = (uint8_t)(L >> 8);
  buf[7] = (uint8_t)(L);
}

/* Run one DES stage or the EDE bundle selected by the key length. */
void tc_des_encrypt_scheduled(const void* schedule, uint8_t block[TC_DES_BLOCKLEN], int triple)
{
  const uint8_t (*sk)[6] = (const uint8_t (*)[6])schedule;
  tc_des_cipher_block(&sk[0], block, 0);
  if (triple) {
    tc_des_cipher_block(&sk[16], block, 1);
    tc_des_cipher_block(&sk[32], block, 0);
  }
}

void tc_des_decrypt_scheduled(const void* schedule, uint8_t block[TC_DES_BLOCKLEN], int triple)
{
  const uint8_t (*sk)[6] = (const uint8_t (*)[6])schedule;
  if (!triple) {
    tc_des_cipher_block(&sk[0], block, 1);
    return;
  }
  tc_des_cipher_block(&sk[32], block, 1);
  tc_des_cipher_block(&sk[16], block, 0);
  tc_des_cipher_block(&sk[0], block, 1);
}
