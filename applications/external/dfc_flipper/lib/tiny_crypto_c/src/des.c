/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * DES and Triple-DES implementation for tiny_crypto_c.
 * Portable C implementation of DES and Triple-DES (3DES / TDES)
 * optimized for small embedded devices and microcontrollers.
 *
 * Inspired by and created in the design style of kokke's tiny-AES-c:
 * https://github.com/kokke/tiny-AES-c
 *
 */

#include <string.h>
#include <tiny_crypto/des.h>
#include "internal.h"

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
  0x00808200U, 0x00000000U, 0x00008000U, 0x00808202U, 0x00808002U, 0x00008202U, 0x00000002U, 0x00008000U,
  0x00000200U, 0x00808200U, 0x00808202U, 0x00000200U, 0x00800202U, 0x00808002U, 0x00800000U, 0x00000002U,
  0x00000202U, 0x00800200U, 0x00800200U, 0x00008200U, 0x00008200U, 0x00808000U, 0x00808000U, 0x00800202U,
  0x00008002U, 0x00800002U, 0x00800002U, 0x00008002U, 0x00000000U, 0x00000202U, 0x00008202U, 0x00800000U,
  0x00008000U, 0x00808202U, 0x00000002U, 0x00808000U, 0x00808200U, 0x00800000U, 0x00800000U, 0x00000200U,
  0x00808002U, 0x00008000U, 0x00008200U, 0x00800002U, 0x00000200U, 0x00000002U, 0x00800202U, 0x00008202U,
  0x00808202U, 0x00008002U, 0x00808000U, 0x00800202U, 0x00800002U, 0x00000202U, 0x00008202U, 0x00808200U,
  0x00000202U, 0x00800200U, 0x00800200U, 0x00000000U, 0x00008002U, 0x00008200U, 0x00000000U, 0x00808002U
};

static const uint32_t SP2[64] TC_DES_TABLE_STORAGE = {
  0x40084010U, 0x40004000U, 0x00004000U, 0x00084010U, 0x00080000U, 0x00000010U, 0x40080010U, 0x40004010U,
  0x40000010U, 0x40084010U, 0x40084000U, 0x40000000U, 0x40004000U, 0x00080000U, 0x00000010U, 0x40080010U,
  0x00084000U, 0x00080010U, 0x40004010U, 0x00000000U, 0x40000000U, 0x00004000U, 0x00084010U, 0x40080000U,
  0x00080010U, 0x40000010U, 0x00000000U, 0x00084000U, 0x00004010U, 0x40084000U, 0x40080000U, 0x00004010U,
  0x00000000U, 0x00084010U, 0x40080010U, 0x00080000U, 0x40004010U, 0x40080000U, 0x40084000U, 0x00004000U,
  0x40080000U, 0x40004000U, 0x00000010U, 0x40084010U, 0x00084010U, 0x00000010U, 0x00004000U, 0x40000000U,
  0x00004010U, 0x40084000U, 0x00080000U, 0x40000010U, 0x00080010U, 0x40004010U, 0x40000010U, 0x00080010U,
  0x00084000U, 0x00000000U, 0x40004000U, 0x00004010U, 0x40000000U, 0x40080010U, 0x40084010U, 0x00084000U
};

static const uint32_t SP3[64] TC_DES_TABLE_STORAGE = {
  0x00000104U, 0x04010100U, 0x00000000U, 0x04010004U, 0x04000100U, 0x00000000U, 0x00010104U, 0x04000100U,
  0x00010004U, 0x04000004U, 0x04000004U, 0x00010000U, 0x04010104U, 0x00010004U, 0x04010000U, 0x00000104U,
  0x04000000U, 0x00000004U, 0x04010100U, 0x00000100U, 0x00010100U, 0x04010000U, 0x04010004U, 0x00010104U,
  0x04000104U, 0x00010100U, 0x00010000U, 0x04000104U, 0x00000004U, 0x04010104U, 0x00000100U, 0x04000000U,
  0x04010100U, 0x04000000U, 0x00010004U, 0x00000104U, 0x00010000U, 0x04010100U, 0x04000100U, 0x00000000U,
  0x00000100U, 0x00010004U, 0x04010104U, 0x04000100U, 0x04000004U, 0x00000100U, 0x00000000U, 0x04010004U,
  0x04000104U, 0x00010000U, 0x04000000U, 0x04010104U, 0x00000004U, 0x00010104U, 0x00010100U, 0x04000004U,
  0x04010000U, 0x04000104U, 0x00000104U, 0x04010000U, 0x00010104U, 0x00000004U, 0x04010004U, 0x00010100U
};

static const uint32_t SP4[64] TC_DES_TABLE_STORAGE = {
  0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U, 0x00401040U, 0x80400040U, 0x80400000U, 0x80001000U,
  0x00000000U, 0x00401000U, 0x00401000U, 0x80401040U, 0x80000040U, 0x00000000U, 0x00400040U, 0x80400000U,
  0x80000000U, 0x00001000U, 0x00400000U, 0x80401000U, 0x00000040U, 0x00400000U, 0x80001000U, 0x00001040U,
  0x80400040U, 0x80000000U, 0x00001040U, 0x00400040U, 0x00001000U, 0x00401040U, 0x80401040U, 0x80000040U,
  0x00400040U, 0x80400000U, 0x00401000U, 0x80401040U, 0x80000040U, 0x00000000U, 0x00000000U, 0x00401000U,
  0x00001040U, 0x00400040U, 0x80400040U, 0x80000000U, 0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U,
  0x80401040U, 0x80000040U, 0x80000000U, 0x00001000U, 0x80400000U, 0x80001000U, 0x00401040U, 0x80400040U,
  0x80001000U, 0x00001040U, 0x00400000U, 0x80401000U, 0x00000040U, 0x00400000U, 0x00001000U, 0x00401040U
};

static const uint32_t SP5[64] TC_DES_TABLE_STORAGE = {
  0x00000080U, 0x01040080U, 0x01040000U, 0x21000080U, 0x00040000U, 0x00000080U, 0x20000000U, 0x01040000U,
  0x20040080U, 0x00040000U, 0x01000080U, 0x20040080U, 0x21000080U, 0x21040000U, 0x00040080U, 0x20000000U,
  0x01000000U, 0x20040000U, 0x20040000U, 0x00000000U, 0x20000080U, 0x21040080U, 0x21040080U, 0x01000080U,
  0x21040000U, 0x20000080U, 0x00000000U, 0x21000000U, 0x01040080U, 0x01000000U, 0x21000000U, 0x00040080U,
  0x00040000U, 0x21000080U, 0x00000080U, 0x01000000U, 0x20000000U, 0x01040000U, 0x21000080U, 0x20040080U,
  0x01000080U, 0x20000000U, 0x21040000U, 0x01040080U, 0x20040080U, 0x00000080U, 0x01000000U, 0x21040000U,
  0x21040080U, 0x00040080U, 0x21000000U, 0x21040080U, 0x01040000U, 0x00000000U, 0x20040000U, 0x21000000U,
  0x00040080U, 0x01000080U, 0x20000080U, 0x00040000U, 0x00000000U, 0x20040000U, 0x01040080U, 0x20000080U
};

static const uint32_t SP6[64] TC_DES_TABLE_STORAGE = {
  0x10000008U, 0x10200000U, 0x00002000U, 0x10202008U, 0x10200000U, 0x00000008U, 0x10202008U, 0x00200000U,
  0x10002000U, 0x00202008U, 0x00200000U, 0x10000008U, 0x00200008U, 0x10002000U, 0x10000000U, 0x00002008U,
  0x00000000U, 0x00200008U, 0x10002008U, 0x00002000U, 0x00202000U, 0x10002008U, 0x00000008U, 0x10200008U,
  0x10200008U, 0x00000000U, 0x00202008U, 0x10202000U, 0x00002008U, 0x00202000U, 0x10202000U, 0x10000000U,
  0x10002000U, 0x00000008U, 0x10200008U, 0x00202000U, 0x10202008U, 0x00200000U, 0x00002008U, 0x10000008U,
  0x00200000U, 0x10002000U, 0x10000000U, 0x00002008U, 0x10000008U, 0x10202008U, 0x00202000U, 0x10200000U,
  0x00202008U, 0x10202000U, 0x00000000U, 0x10200008U, 0x00000008U, 0x00002000U, 0x10200000U, 0x00202008U,
  0x00002000U, 0x00200008U, 0x10002008U, 0x00000000U, 0x10202000U, 0x10000000U, 0x00200008U, 0x10002008U
};

static const uint32_t SP7[64] TC_DES_TABLE_STORAGE = {
  0x00100000U, 0x02100001U, 0x02000401U, 0x00000000U, 0x00000400U, 0x02000401U, 0x00100401U, 0x02100400U,
  0x02100401U, 0x00100000U, 0x00000000U, 0x02000001U, 0x00000001U, 0x02000000U, 0x02100001U, 0x00000401U,
  0x02000400U, 0x00100401U, 0x00100001U, 0x02000400U, 0x02000001U, 0x02100000U, 0x02100400U, 0x00100001U,
  0x02100000U, 0x00000400U, 0x00000401U, 0x02100401U, 0x00100400U, 0x00000001U, 0x02000000U, 0x00100400U,
  0x02000000U, 0x00100400U, 0x00100000U, 0x02000401U, 0x02000401U, 0x02100001U, 0x02100001U, 0x00000001U,
  0x00100001U, 0x02000000U, 0x02000400U, 0x00100000U, 0x02100400U, 0x00000401U, 0x00100401U, 0x02100400U,
  0x00000401U, 0x02000001U, 0x02100401U, 0x02100000U, 0x00100400U, 0x00000000U, 0x00000001U, 0x02100401U,
  0x00000000U, 0x00100401U, 0x02100000U, 0x00000400U, 0x02000001U, 0x02000400U, 0x00000400U, 0x00100001U
};

static const uint32_t SP8[64] TC_DES_TABLE_STORAGE = {
  0x08000820U, 0x00000800U, 0x00020000U, 0x08020820U, 0x08000000U, 0x08000820U, 0x00000020U, 0x08000000U,
  0x00020020U, 0x08020000U, 0x08020820U, 0x00020800U, 0x08020800U, 0x00020820U, 0x00000800U, 0x00000020U,
  0x08020000U, 0x08000020U, 0x08000800U, 0x00000820U, 0x00020800U, 0x00020020U, 0x08020020U, 0x08020800U,
  0x00000820U, 0x00000000U, 0x00000000U, 0x08020020U, 0x08000020U, 0x08000800U, 0x00020820U, 0x00020000U,
  0x00020820U, 0x00020000U, 0x08020800U, 0x00000800U, 0x00000020U, 0x08020020U, 0x00000800U, 0x00020820U,
  0x08000800U, 0x00000020U, 0x08000020U, 0x08020000U, 0x08020020U, 0x08000000U, 0x00020000U, 0x08000820U,
  0x00000000U, 0x08020820U, 0x00020020U, 0x08000020U, 0x08020000U, 0x08000800U, 0x08000820U, 0x00000000U,
  0x08020820U, 0x00020800U, 0x00020800U, 0x00000820U, 0x00000820U, 0x00020020U, 0x08000000U, 0x08020800U
};

/* Permuted Choice 1 (PC-1) matrices */
static const uint8_t PC1_C[28] TC_DES_TABLE_STORAGE = {
  57, 49, 41, 33, 25, 17, 9,
   1, 58, 50, 42, 34, 26, 18,
  10,  2, 59, 51, 43, 35, 27,
  19, 11,  3, 60, 52, 44, 36
};

static const uint8_t PC1_D[28] TC_DES_TABLE_STORAGE = {
  63, 55, 47, 39, 31, 23, 15,
   7, 62, 54, 46, 38, 30, 22,
  14,  6, 61, 53, 45, 37, 29,
  21, 13,  5, 28, 20, 12,  4
};

/* Permuted Choice 2 (PC-2) matrix */
/* DES_ prefix avoids AVR register macros such as PC2. */
static const uint8_t DES_PC2[48] TC_DES_TABLE_STORAGE = {
  14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
  23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
  41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
  44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

/* Subkey Left Shift Schedule */
static const uint8_t SHIFTS[16] TC_DES_TABLE_STORAGE = {
  1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

/*****************************************************************************/
/* Private Helper Functions                                                  */
/*****************************************************************************/

#if TC_DES_REJECT_WEAK_KEYS
/* The four weak and twelve semi-weak DES keys. Parity bits are ignored when
 * comparing, so equivalent encodings are rejected too. */
static const uint8_t des_weak_keys[16][TC_DES_KEYLEN] TC_DES_TABLE_STORAGE = {
  { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
  { 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe },
  { 0xe0, 0xe0, 0xe0, 0xe0, 0xf1, 0xf1, 0xf1, 0xf1 },
  { 0x1f, 0x1f, 0x1f, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e },
  { 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe },
  { 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01 },
  { 0x1f, 0xe0, 0x1f, 0xe0, 0x0e, 0xf1, 0x0e, 0xf1 },
  { 0xe0, 0x1f, 0xe0, 0x1f, 0xf1, 0x0e, 0xf1, 0x0e },
  { 0x01, 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1 },
  { 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1, 0x01 },
  { 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe },
  { 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e },
  { 0x01, 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e },
  { 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e, 0x01 },
  { 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1, 0xfe },
  { 0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1 }
};

static int tc_des_key_is_weak(const uint8_t* key)
{
  uint8_t weak = 0;
  size_t candidate;
  size_t i;

  for (candidate = 0; candidate < 16; ++candidate)
  {
    uint8_t diff = 0;
    for (i = 0; i < TC_DES_KEYLEN; ++i)
      diff |= (uint8_t)((key[i] ^
                         TC_DES_READ_U8(des_weak_keys[candidate], i)) & 0xfeu);
    weak |= (uint8_t)(diff == 0);
  }
  return weak != 0;
}

static int tc_des_keys_equal(const uint8_t* left, const uint8_t* right)
{
  uint8_t diff = 0;
  size_t i;

  for (i = 0; i < TC_DES_KEYLEN; ++i)
    diff |= (uint8_t)((left[i] ^ right[i]) & 0xfeu);
  return diff == 0;
}

static int tc_des_bundle_is_rejected(const uint8_t* key, size_t keylen)
{
  if (tc_des_key_is_weak(key))
    return 1;
  if (keylen >= TC_DES3_KEYLEN_2KEY &&
      (tc_des_key_is_weak(key + TC_DES_KEYLEN) ||
       tc_des_keys_equal(key, key + TC_DES_KEYLEN)))
    return 1;
  return keylen == TC_DES3_KEYLEN_3KEY &&
         (tc_des_key_is_weak(key + (2u * TC_DES_KEYLEN)) ||
          tc_des_keys_equal(key + TC_DES_KEYLEN,
                            key + (2u * TC_DES_KEYLEN)));
}
#endif

static inline uint8_t tc_des_get_key_bit(const uint8_t* key, uint8_t bit_1based)
{
  uint8_t byte_idx = (uint8_t)((bit_1based - 1) / 8);
  uint8_t bit_idx  = (uint8_t)(7 - ((bit_1based - 1) % 8));
  return (key[byte_idx] >> bit_idx) & 1U;
}

/* Pack each 48-bit round key into six bytes. The round function expands these
 * into eight 6-bit S-box inputs, saving 25% versus byte-per-input storage. */
static void tc_des_key_schedule(uint8_t (*sk)[6], const uint8_t* key)
{
  uint32_t C = 0;
  uint32_t D = 0;

  for (uint8_t i = 0; i < 28; ++i)
  {
    C = (C << 1) | tc_des_get_key_bit(key, TC_DES_READ_U8(PC1_C, i));
    D = (D << 1) | tc_des_get_key_bit(key, TC_DES_READ_U8(PC1_D, i));
  }

  for (uint8_t r = 0; r < 16; ++r)
  {
    uint8_t shift = TC_DES_READ_U8(SHIFTS, r);
    C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFFU;
    D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFFU;

    memset(sk[r], 0, 6);

    for (uint8_t i = 0; i < 48; ++i)
    {
      uint8_t bit_pos = TC_DES_READ_U8(DES_PC2, i);
      uint8_t bit_val;
      if (bit_pos <= 28)
        bit_val = (uint8_t)((C >> (28u - bit_pos)) & 1u);
      else
        bit_val = (uint8_t)((D >> (56u - bit_pos)) & 1u);
      sk[r][i / 8u] |= (uint8_t)(bit_val << (7u - (i % 8u)));
    }
  }
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
static void tc_des_cipher_block(const uint8_t (*sk)[6], uint8_t* buf, int decrypt)
{
  uint32_t L = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
  uint32_t R = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];

  tc_des_initial_permutation(&L, &R);

  for (int i = 0; i < 16; ++i)
  {
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
    uint8_t b6 = (uint8_t)(((R >> 7)  & 0x3FU) ^ sk8_5);
    uint8_t b7 = (uint8_t)(((R >> 3)  & 0x3FU) ^ sk8_6);
    uint8_t b8 = (uint8_t)((((R & 0x1FU) << 1) | ((R >> 31) & 1U)) ^ sk8_7);

    uint32_t f_res = TC_DES_READ_U32(SP1, b1) ^
                     TC_DES_READ_U32(SP2, b2) ^
                     TC_DES_READ_U32(SP3, b3) ^
                     TC_DES_READ_U32(SP4, b4) ^
                     TC_DES_READ_U32(SP5, b5) ^
                     TC_DES_READ_U32(SP6, b6) ^
                     TC_DES_READ_U32(SP7, b7) ^
                     TC_DES_READ_U32(SP8, b8);

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

typedef void (*tc_des_mode_block_fn)(const void* cipher, uint8_t* block);

/* Public wrappers must check before taking addresses of context members. */
#if TC_STRICT
  #define DES_MODE_REQUIRE_CTX(ctx) do { if ((ctx) == NULL) return TC_ERROR; } while (0)
#else
  #define DES_MODE_REQUIRE_CTX(ctx) do { (void)(ctx); } while (0)
#endif

#if TC_DES_ENABLE_CBC
static TC_status tc_des_mode_cbc(const void* cipher,
                                 uint8_t iv[TC_DES_BLOCKLEN], uint8_t* buf,
                                 size_t length,
                                 tc_des_mode_block_fn crypt_block, int decrypt)
{
  uint8_t saved[TC_DES_BLOCKLEN];
  size_t i;
  uint8_t j;

#if TC_STRICT
  if (cipher == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  if ((length % TC_DES_BLOCKLEN) != 0)
    return TC_ERROR;

  for (i = 0; i < length; i += TC_DES_BLOCKLEN)
  {
    if (decrypt)
      memcpy(saved, buf + i, TC_DES_BLOCKLEN);
    else
      for (j = 0; j < TC_DES_BLOCKLEN; ++j)
        buf[i + j] ^= iv[j];

    crypt_block(cipher, buf + i);

    if (decrypt)
    {
      for (j = 0; j < TC_DES_BLOCKLEN; ++j)
        buf[i + j] ^= iv[j];
      memcpy(iv, saved, TC_DES_BLOCKLEN);
    }
    else
    {
      memcpy(iv, buf + i, TC_DES_BLOCKLEN);
    }
  }
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CTR
static TC_status tc_des_mode_ctr(const void* cipher,
                                 uint8_t iv[TC_DES_BLOCKLEN],
                                 uint8_t stream[TC_DES_BLOCKLEN], uint8_t* pos,
                                 uint8_t* buf, size_t length,
                                 tc_des_mode_block_fn encrypt_block)
{
  size_t available;
  size_t uncached_length;
  size_t blocks_needed;
  size_t i;

#if TC_STRICT
  if (cipher == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  if (length == 0)
    return TC_OK;
  if (*pos > TC_DES_BLOCKLEN)
    return TC_ERROR;

  available = *pos < TC_DES_BLOCKLEN ? TC_DES_BLOCKLEN - *pos : 0;
  uncached_length = length > available ? length - available : 0;
  blocks_needed = uncached_length / TC_DES_BLOCKLEN +
                  ((uncached_length % TC_DES_BLOCKLEN) != 0 ? 1u : 0u);
  if (!tc_internal_counter_has_blocks(iv, TC_DES_BLOCKLEN, blocks_needed))
    return TC_ERROR;

  for (i = 0; i < length; ++i)
  {
    if (*pos == TC_DES_BLOCKLEN)
    {
      memcpy(stream, iv, TC_DES_BLOCKLEN);
      encrypt_block(cipher, stream);
      tc_internal_increment_be(iv, TC_DES_BLOCKLEN);
      *pos = 0;
    }
    buf[i] ^= stream[(*pos)++];
  }
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CFB64
/* A short final CFB-64 segment shifts only its ciphertext bytes into the
 * feedback register, as specified by SP 800-38A. */
static void tc_des_cfb64_shift_iv(uint8_t* iv, const uint8_t* ct,
                                  size_t length)
{
  if (length >= TC_DES_BLOCKLEN)
  {
    memcpy(iv, ct, TC_DES_BLOCKLEN);
    return;
  }
  memmove(iv, iv + length, TC_DES_BLOCKLEN - length);
  memcpy(iv + TC_DES_BLOCKLEN - length, ct, length);
}

static TC_status tc_des_mode_cfb64(const void* cipher,
                                   uint8_t iv[TC_DES_BLOCKLEN], uint8_t* buf,
                                   size_t length,
                                   tc_des_mode_block_fn encrypt_block, int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  uint8_t saved[TC_DES_BLOCKLEN];
  size_t offset = 0;

#if TC_STRICT
  if (cipher == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  while (offset < length)
  {
    const size_t segment = length - offset < TC_DES_BLOCKLEN ?
                               length - offset : TC_DES_BLOCKLEN;
    size_t j;

    if (decrypt)
      memcpy(saved, buf + offset, segment);
    memcpy(keystream, iv, TC_DES_BLOCKLEN);
    encrypt_block(cipher, keystream);
    for (j = 0; j < segment; ++j)
      buf[offset + j] ^= keystream[j];
    tc_des_cfb64_shift_iv(iv, decrypt ? saved : buf + offset, segment);
    offset += segment;
  }
#if TC_ZEROIZE
  TC_secure_zero(keystream, sizeof(keystream));
  TC_secure_zero(saved, sizeof(saved));
#endif
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CFB8
static TC_status tc_des_mode_cfb8(const void* cipher,
                                  uint8_t iv[TC_DES_BLOCKLEN], uint8_t* buf,
                                  size_t length,
                                  tc_des_mode_block_fn encrypt_block, int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  size_t i;

#if TC_STRICT
  if (cipher == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  for (i = 0; i < length; ++i)
  {
    const uint8_t ciphertext = buf[i];
    memcpy(keystream, iv, TC_DES_BLOCKLEN);
    encrypt_block(cipher, keystream);
    buf[i] ^= keystream[0];
    memmove(iv, iv + 1, TC_DES_BLOCKLEN - 1);
    iv[TC_DES_BLOCKLEN - 1] = decrypt ? ciphertext : buf[i];
  }
#if TC_ZEROIZE
  TC_secure_zero(keystream, sizeof(keystream));
#endif
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CFB1
static void tc_des_cfb1_shift_iv(uint8_t* iv, uint8_t ciphertext_bit)
{
  uint8_t j;
  for (j = 0; j < TC_DES_BLOCKLEN - 1; ++j)
    iv[j] = (uint8_t)((iv[j] << 1) | (iv[j + 1] >> 7));
  iv[TC_DES_BLOCKLEN - 1] =
      (uint8_t)((iv[TC_DES_BLOCKLEN - 1] << 1) | ciphertext_bit);
}

static TC_status tc_des_mode_cfb1(const void* cipher,
                                  uint8_t iv[TC_DES_BLOCKLEN], uint8_t* buf,
                                  size_t bit_length,
                                  tc_des_mode_block_fn encrypt_block, int decrypt)
{
  uint8_t keystream[TC_DES_BLOCKLEN];
  size_t i;

#if TC_STRICT
  if (cipher == NULL || (bit_length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  for (i = 0; i < bit_length; ++i)
  {
    const size_t byte_index = i / 8;
    const uint8_t shift = (uint8_t)(7 - (i % 8));
    const uint8_t input_bit = (uint8_t)((buf[byte_index] >> shift) & 1u);
    uint8_t output_bit;
    uint8_t ciphertext_bit;

    memcpy(keystream, iv, TC_DES_BLOCKLEN);
    encrypt_block(cipher, keystream);
    output_bit = (uint8_t)((input_bit ^ (keystream[0] >> 7)) & 1u);
    ciphertext_bit = decrypt ? input_bit : output_bit;
    buf[byte_index] = (uint8_t)((buf[byte_index] & ~(1u << shift)) |
                                ((unsigned)output_bit << shift));
    tc_des_cfb1_shift_iv(iv, ciphertext_bit);
  }
#if TC_ZEROIZE
  TC_secure_zero(keystream, sizeof(keystream));
#endif
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_OFB
static TC_status tc_des_mode_ofb(const void* cipher,
                                 uint8_t iv[TC_DES_BLOCKLEN], uint8_t* pos,
                                 uint8_t* buf, size_t length,
                                 tc_des_mode_block_fn encrypt_block)
{
  size_t i;
#if TC_STRICT
  if (cipher == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  for (i = 0; i < length; ++i)
  {
    if (*pos == TC_DES_BLOCKLEN)
    {
      encrypt_block(cipher, iv);
      *pos = 0;
    }
    buf[i] ^= iv[(*pos)++];
  }
  return TC_OK;
}
#endif

/*****************************************************************************/
/* Public Functions: secure wipe                                             */
/*****************************************************************************/

void TC_DES_ctx_clear(struct TC_DES_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

#if TC_DES_ENABLE_TDES
void TC_DES3_ctx_clear(struct TC_DES3_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}
#endif

/*****************************************************************************/
/* Public Functions: Single DES                                              */
/*****************************************************************************/

#if TC_DES_ENABLE_CBC || TC_DES_ENABLE_CTR || TC_DES_ENABLE_OFB || \
    TC_DES_ENABLE_CFB1 || TC_DES_ENABLE_CFB8 || TC_DES_ENABLE_CFB64
static void tc_des_encrypt_mode_block(const void* cipher, uint8_t* block)
{
  const struct TC_DES_ctx* ctx = (const struct TC_DES_ctx*)cipher;
  tc_des_cipher_block(ctx->Sk, block, 0);
}
#endif

#if TC_DES_ENABLE_CBC
static void tc_des_decrypt_mode_block(const void* cipher, uint8_t* block)
{
  const struct TC_DES_ctx* ctx = (const struct TC_DES_ctx*)cipher;
  tc_des_cipher_block(ctx->Sk, block, 1);
}
#endif

TC_status TC_DES_init_ctx(struct TC_DES_ctx* ctx, const uint8_t* key)
{
  if (ctx == NULL || key == NULL)
    return TC_ERROR;
#if TC_DES_REJECT_WEAK_KEYS
  if (tc_des_bundle_is_rejected(key, TC_DES_KEYLEN))
    return TC_ERROR;
#endif
  tc_des_key_schedule(ctx->Sk, key);
#if TC_DES_NEEDS_IV
  memset(ctx->Iv, 0, TC_DES_BLOCKLEN);
#endif
#if TC_DES_ENABLE_CTR
  memset(ctx->ctr_stream, 0, TC_DES_BLOCKLEN);
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}

#if TC_DES_NEEDS_IV
TC_status TC_DES_init_ctx_iv(struct TC_DES_ctx* ctx, const uint8_t* key,
                            const uint8_t* iv)
{
  if (iv == NULL || TC_DES_init_ctx(ctx, key) != TC_OK)
    return TC_ERROR;
  memcpy(ctx->Iv, iv, TC_DES_BLOCKLEN);
#if TC_DES_ENABLE_CTR
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}

TC_status TC_DES_ctx_set_iv(struct TC_DES_ctx* ctx, const uint8_t* iv)
{
  if (ctx == NULL || iv == NULL)
    return TC_ERROR;
  memcpy(ctx->Iv, iv, TC_DES_BLOCKLEN);
#if TC_DES_ENABLE_CTR
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_ECB
TC_status TC_DES_ECB_encrypt(const struct TC_DES_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  tc_des_cipher_block(ctx->Sk, buf, 0);
  return TC_OK;
}

TC_status TC_DES_ECB_decrypt(const struct TC_DES_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  tc_des_cipher_block(ctx->Sk, buf, 1);
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CBC
TC_status TC_DES_CBC_encrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cbc(ctx, ctx->Iv, buf, length,
                         tc_des_encrypt_mode_block, 0);
}

TC_status TC_DES_CBC_decrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cbc(ctx, ctx->Iv, buf, length,
                         tc_des_decrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CTR
TC_status TC_DES_CTR_crypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_ctr(ctx, ctx->Iv, ctx->ctr_stream, &ctx->ctr_pos, buf,
                         length, tc_des_encrypt_mode_block);
}
#endif

#if TC_DES_ENABLE_CFB64
TC_status TC_DES_CFB64_encrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb64(ctx, ctx->Iv, buf, length,
                           tc_des_encrypt_mode_block, 0);
}

TC_status TC_DES_CFB64_decrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  /* CFB decryption still uses the cipher's forward direction. */
  return tc_des_mode_cfb64(ctx, ctx->Iv, buf, length,
                           tc_des_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CFB8
TC_status TC_DES_CFB8_encrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb8(ctx, ctx->Iv, buf, length,
                          tc_des_encrypt_mode_block, 0);
}

TC_status TC_DES_CFB8_decrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb8(ctx, ctx->Iv, buf, length,
                          tc_des_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CFB1
TC_status TC_DES_CFB1_encrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb1(ctx, ctx->Iv, buf, bit_length,
                          tc_des_encrypt_mode_block, 0);
}

TC_status TC_DES_CFB1_decrypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb1(ctx, ctx->Iv, buf, bit_length,
                          tc_des_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_OFB
TC_status TC_DES_OFB_crypt(struct TC_DES_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_ofb(ctx, ctx->Iv, &ctx->ofb_pos, buf, length,
                         tc_des_encrypt_mode_block);
}
#endif

/*****************************************************************************/
/* Public Functions: Triple DES (3DES / TDES)                                */
/*****************************************************************************/

#if TC_DES_ENABLE_TDES

TC_status TC_DES3_init_ctx(struct TC_DES3_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || key == NULL)
    return TC_ERROR;
  if (keylen != 16 && keylen != 24)
    return TC_ERROR;
#if TC_DES_REJECT_WEAK_KEYS
  if (tc_des_bundle_is_rejected(key, keylen))
    return TC_ERROR;
#endif

  tc_des_key_schedule(&ctx->Sk[0], key);
  if (keylen == 16)
  {
    /* 2-Key 3DES: K1, K2, K1 */
    tc_des_key_schedule(&ctx->Sk[16], key + 8);
    tc_des_key_schedule(&ctx->Sk[32], key);
  }
  else
  {
    /* 3-Key 3DES: K1, K2, K3 */
    tc_des_key_schedule(&ctx->Sk[16], key + 8);
    tc_des_key_schedule(&ctx->Sk[32], key + 16);
  }
#if TC_DES_NEEDS_IV
  memset(ctx->Iv, 0, TC_DES_BLOCKLEN);
#endif
#if TC_DES_ENABLE_CTR
  memset(ctx->ctr_stream, 0, TC_DES_BLOCKLEN);
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}

#if TC_DES_NEEDS_IV
TC_status TC_DES3_init_ctx_iv(struct TC_DES3_ctx* ctx, const uint8_t* key, size_t keylen, const uint8_t* iv)
{
  if (iv == NULL)
    return TC_ERROR;
  if (TC_DES3_init_ctx(ctx, key, keylen) != TC_OK)
    return TC_ERROR;
  memcpy(ctx->Iv, iv, TC_DES_BLOCKLEN);
#if TC_DES_ENABLE_CTR
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}

TC_status TC_DES3_ctx_set_iv(struct TC_DES3_ctx* ctx, const uint8_t* iv)
{
  if (ctx == NULL || iv == NULL)
    return TC_ERROR;
  memcpy(ctx->Iv, iv, TC_DES_BLOCKLEN);
#if TC_DES_ENABLE_CTR
  ctx->ctr_pos = TC_DES_BLOCKLEN;
#endif
#if TC_DES_ENABLE_OFB
  ctx->ofb_pos = TC_DES_BLOCKLEN;
#endif
  return TC_OK;
}
#endif

/* 3DES Core Encryption: E(K1) -> D(K2) -> E(K3) */
#if TC_DES_ENABLE_ECB || TC_DES_ENABLE_CBC || TC_DES_ENABLE_CTR || \
    TC_DES_ENABLE_OFB || TC_DES_ENABLE_CFB1 || TC_DES_ENABLE_CFB8 || \
    TC_DES_ENABLE_CFB64
static void tc_des3_encrypt_block(const struct TC_DES3_ctx* ctx, uint8_t* buf)
{
  tc_des_cipher_block(&ctx->Sk[0],  buf, 0); /* Encrypt K1 */
  tc_des_cipher_block(&ctx->Sk[16], buf, 1); /* Decrypt K2 */
  tc_des_cipher_block(&ctx->Sk[32], buf, 0); /* Encrypt K3 */
}
#endif

#if (TC_DES_ENABLE_ECB == 1) || (TC_DES_ENABLE_CBC == 1)
/* 3DES Core Decryption: D(K1) -> E(K2) -> D(K3) */
static void tc_des3_decrypt_block(const struct TC_DES3_ctx* ctx, uint8_t* buf)
{
  tc_des_cipher_block(&ctx->Sk[32], buf, 1); /* Decrypt K3 */
  tc_des_cipher_block(&ctx->Sk[16], buf, 0); /* Encrypt K2 */
  tc_des_cipher_block(&ctx->Sk[0],  buf, 1); /* Decrypt K1 */
}
#endif

#if TC_DES_ENABLE_CBC || TC_DES_ENABLE_CTR || TC_DES_ENABLE_OFB || \
    TC_DES_ENABLE_CFB1 || TC_DES_ENABLE_CFB8 || TC_DES_ENABLE_CFB64
static void tc_des3_encrypt_mode_block(const void* cipher, uint8_t* block)
{
  tc_des3_encrypt_block((const struct TC_DES3_ctx*)cipher, block);
}
#endif

#if TC_DES_ENABLE_CBC
static void tc_des3_decrypt_mode_block(const void* cipher, uint8_t* block)
{
  tc_des3_decrypt_block((const struct TC_DES3_ctx*)cipher, block);
}
#endif

#if TC_DES_ENABLE_ECB
TC_status TC_DES3_ECB_encrypt(const struct TC_DES3_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  tc_des3_encrypt_block(ctx, buf);
  return TC_OK;
}

TC_status TC_DES3_ECB_decrypt(const struct TC_DES3_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  tc_des3_decrypt_block(ctx, buf);
  return TC_OK;
}
#endif

#if TC_DES_ENABLE_CBC
TC_status TC_DES3_CBC_encrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cbc(ctx, ctx->Iv, buf, length,
                         tc_des3_encrypt_mode_block, 0);
}

TC_status TC_DES3_CBC_decrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cbc(ctx, ctx->Iv, buf, length,
                         tc_des3_decrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CTR
TC_status TC_DES3_CTR_crypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_ctr(ctx, ctx->Iv, ctx->ctr_stream, &ctx->ctr_pos, buf,
                         length, tc_des3_encrypt_mode_block);
}
#endif

#if TC_DES_ENABLE_CFB64
TC_status TC_DES3_CFB64_encrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb64(ctx, ctx->Iv, buf, length,
                           tc_des3_encrypt_mode_block, 0);
}

TC_status TC_DES3_CFB64_decrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb64(ctx, ctx->Iv, buf, length,
                           tc_des3_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CFB8
TC_status TC_DES3_CFB8_encrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb8(ctx, ctx->Iv, buf, length,
                          tc_des3_encrypt_mode_block, 0);
}

TC_status TC_DES3_CFB8_decrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb8(ctx, ctx->Iv, buf, length,
                          tc_des3_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_CFB1
TC_status TC_DES3_CFB1_encrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb1(ctx, ctx->Iv, buf, bit_length,
                          tc_des3_encrypt_mode_block, 0);
}

TC_status TC_DES3_CFB1_decrypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_cfb1(ctx, ctx->Iv, buf, bit_length,
                          tc_des3_encrypt_mode_block, 1);
}
#endif

#if TC_DES_ENABLE_OFB
TC_status TC_DES3_OFB_crypt(struct TC_DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  DES_MODE_REQUIRE_CTX(ctx);
  return tc_des_mode_ofb(ctx, ctx->Iv, &ctx->ofb_pos, buf, length,
                         tc_des3_encrypt_mode_block);
}
#endif

#endif /* #if TC_DES_ENABLE_TDES */

/*****************************************************************************/
/* Public Functions: DES / 3DES CMAC (NIST SP 800-38B)                      */
/*****************************************************************************/
#if TC_DES_ENABLE_CMAC

static void tc_des_cmac_shift_left(const uint8_t* input, uint8_t* output)
{
  uint8_t overflow = 0;
  for (int i = 7; i >= 0; --i)
  {
    output[i] = (uint8_t)((input[i] << 1) | overflow);
    overflow = (input[i] & 0x80U) ? 1U : 0U;
  }
}

/* CMAC needs only the raw block cipher, so the context schedules keys and
   chains blocks itself; it must keep working with the optional ECB/CBC/TDES
   mode gates compiled out. */
static void tc_des_cmac_encrypt_block(const struct TC_DES_CMAC_ctx* ctx,
                                      uint8_t* buf)
{
  tc_des_cipher_block(&ctx->sk[0], buf, 0);
  if (ctx->triple)
  {
    tc_des_cipher_block(&ctx->sk[16], buf, 1);
    tc_des_cipher_block(&ctx->sk[32], buf, 0);
  }
}

static void tc_des_cmac_generate_subkeys(const struct TC_DES_CMAC_ctx* ctx,
                                         uint8_t* k1, uint8_t* k2)
{
  static const uint8_t const_Rb = 0x1BU;
  uint8_t L[TC_DES_BLOCKLEN] = {0};

  tc_des_cmac_encrypt_block(ctx, L);

  tc_des_cmac_shift_left(L, k1);
  k1[7] ^= (uint8_t)(const_Rb & (uint8_t)(0U - (uint8_t)(L[0] >> 7)));

  tc_des_cmac_shift_left(k1, k2);
  k2[7] ^= (uint8_t)(const_Rb & (uint8_t)(0U - (uint8_t)(k1[0] >> 7)));

#if TC_ZEROIZE
  TC_secure_zero(L, sizeof(L));
#endif
}

/* Absorb one full block into the CBC-MAC chain (zero IV, SP 800-38B). */
static void tc_des_cmac_absorb(struct TC_DES_CMAC_ctx* ctx, const uint8_t* block)
{
  size_t i;
  for (i = 0; i < TC_DES_BLOCKLEN; ++i)
    ctx->mac[i] ^= block[i];
  tc_des_cmac_encrypt_block(ctx, ctx->mac);
}

TC_status TC_DES_CMAC_init(struct TC_DES_CMAC_ctx* ctx, const uint8_t* key, size_t keylen)
{
  if (ctx == NULL || key == NULL)
    return TC_ERROR;
  if (keylen != 8 && keylen != 16 && keylen != 24)
    return TC_ERROR;
#if TC_DES_REJECT_WEAK_KEYS
  if (tc_des_bundle_is_rejected(key, keylen))
    return TC_ERROR;
#endif

  ctx->triple = (uint8_t)(keylen != 8);
  tc_des_key_schedule(&ctx->sk[0], key);
  if (keylen == 16)
  {
    /* 2-Key 3DES: K1, K2, K1 */
    tc_des_key_schedule(&ctx->sk[16], key + 8);
    tc_des_key_schedule(&ctx->sk[32], key);
  }
  else if (keylen == 24)
  {
    /* 3-Key 3DES: K1, K2, K3 */
    tc_des_key_schedule(&ctx->sk[16], key + 8);
    tc_des_key_schedule(&ctx->sk[32], key + 16);
  }
  tc_des_cmac_generate_subkeys(ctx, ctx->k1, ctx->k2);
  memset(ctx->mac, 0, TC_DES_BLOCKLEN);
  memset(ctx->buf, 0, TC_DES_BLOCKLEN);
  ctx->buf_len = 0;
  return TC_OK;
}

TC_status TC_DES_CMAC_update(struct TC_DES_CMAC_ctx* ctx, const uint8_t* data, size_t len)
{
#if TC_STRICT
  if (ctx == NULL || (len != 0 && data == NULL))
    return TC_ERROR;
#endif
  if (len == 0)
    return TC_OK;

  /* A full pending block is only chained once more data arrives, so the
     last block of the message stays available to *_final. */
  if (ctx->buf_len == TC_DES_BLOCKLEN)
  {
    tc_des_cmac_absorb(ctx, ctx->buf);
    ctx->buf_len = 0;
  }
  if (ctx->buf_len != 0)
  {
    const size_t space = (size_t)TC_DES_BLOCKLEN - ctx->buf_len;
    const size_t take = len < space ? len : space;
    memcpy(ctx->buf + ctx->buf_len, data, take);
    ctx->buf_len = (uint8_t)(ctx->buf_len + take);
    data += take;
    len -= take;
    if (len == 0)
      return TC_OK;
    tc_des_cmac_absorb(ctx, ctx->buf);
    ctx->buf_len = 0;
  }
  while (len > TC_DES_BLOCKLEN)
  {
    tc_des_cmac_absorb(ctx, data);
    data += TC_DES_BLOCKLEN;
    len -= TC_DES_BLOCKLEN;
  }
  memcpy(ctx->buf, data, len);
  ctx->buf_len = (uint8_t)len;
  return TC_OK;
}

TC_status TC_DES_CMAC_final(struct TC_DES_CMAC_ctx* ctx, uint8_t tag[TC_DES_CMAC_TAG_MAX])
{
  size_t i;

#if TC_STRICT
  if (ctx == NULL || tag == NULL)
    return TC_ERROR;
#endif
  if (ctx->buf_len == TC_DES_BLOCKLEN)
  {
    for (i = 0; i < TC_DES_BLOCKLEN; ++i)
      ctx->buf[i] ^= ctx->k1[i];
  }
  else
  {
    memset(ctx->buf + ctx->buf_len, 0, (size_t)TC_DES_BLOCKLEN - ctx->buf_len);
    ctx->buf[ctx->buf_len] = 0x80U;
    for (i = 0; i < TC_DES_BLOCKLEN; ++i)
      ctx->buf[i] ^= ctx->k2[i];
  }
  tc_des_cmac_absorb(ctx, ctx->buf);
  memcpy(tag, ctx->mac, TC_DES_BLOCKLEN);

#if TC_ZEROIZE
  TC_secure_zero(ctx, sizeof(*ctx));
#endif
  return TC_OK;
}

void TC_DES_CMAC_ctx_clear(struct TC_DES_CMAC_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

TC_status TC_DES_CMAC(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len)
{
  struct TC_DES_CMAC_ctx ctx;
  uint8_t full[TC_DES_CMAC_TAG_MAX];

  if (key == NULL || tag == NULL ||
      tag_len < TC_DES_CMAC_MIN_TAG_LEN || tag_len > TC_DES_CMAC_TAG_MAX ||
      (msg_len != 0 && msg == NULL))
  {
    return TC_ERROR;
  }
  if (TC_DES_CMAC_init(&ctx, key, keylen) != TC_OK)
    return TC_ERROR;
  /* Empty message: msg may be NULL; update only reads when msg_len > 0. */
  if (TC_DES_CMAC_update(&ctx, msg, msg_len) != TC_OK ||
      TC_DES_CMAC_final(&ctx, full) != TC_OK)
  {
    TC_DES_CMAC_ctx_clear(&ctx);
    return TC_ERROR;
  }
  memcpy(tag, full, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(full, sizeof(full));
#endif
  return TC_OK;
}

TC_status TC_DES_CMAC_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[TC_DES_CMAC_TAG_MAX];
  TC_status status;

  if (tag == NULL ||
      tag_len < TC_DES_CMAC_MIN_TAG_LEN || tag_len > TC_DES_CMAC_TAG_MAX)
  {
    return TC_ERROR;
  }
  if (TC_DES_CMAC(key, keylen, msg, msg_len, computed, tag_len) != TC_OK)
  {
    return TC_ERROR;
  }

  /* Keep tag verification independent of the first mismatching byte. */
  status = TC_ct_equal(computed, tag, tag_len);

#if TC_ZEROIZE
  TC_secure_zero(computed, sizeof(computed));
#endif
  return status;
}

#endif /* TC_DES_ENABLE_CMAC */
