/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 *
 * tiny-DES-c
 * Portable C implementation of DES and Triple-DES (3DES / TDES)
 * optimized for small embedded devices and microcontrollers.
 *
 * Inspired by and created in the design style of kokke's tiny-AES-c:
 * https://github.com/kokke/tiny-AES-c
 *
 * All material in this repository is in the public domain.
 */

#include <string.h>
#include "des.h"

/*****************************************************************************/
/* Private Lookup Tables (ROM/Flash)                                         */
/*****************************************************************************/

/* Combined S-Box & P-Permutation Tables (8 x 64 uint32_t = 2KB ROM) */
static const uint32_t SP1[64] = {
  0x00808200U, 0x00000000U, 0x00008000U, 0x00808202U, 0x00808002U, 0x00008202U, 0x00000002U, 0x00008000U,
  0x00000200U, 0x00808200U, 0x00808202U, 0x00000200U, 0x00800202U, 0x00808002U, 0x00800000U, 0x00000002U,
  0x00000202U, 0x00800200U, 0x00800200U, 0x00008200U, 0x00008200U, 0x00808000U, 0x00808000U, 0x00800202U,
  0x00008002U, 0x00800002U, 0x00800002U, 0x00008002U, 0x00000000U, 0x00000202U, 0x00008202U, 0x00800000U,
  0x00008000U, 0x00808202U, 0x00000002U, 0x00808000U, 0x00808200U, 0x00800000U, 0x00800000U, 0x00000200U,
  0x00808002U, 0x00008000U, 0x00008200U, 0x00800002U, 0x00000200U, 0x00000002U, 0x00800202U, 0x00008202U,
  0x00808202U, 0x00008002U, 0x00808000U, 0x00800202U, 0x00800002U, 0x00000202U, 0x00008202U, 0x00808200U,
  0x00000202U, 0x00800200U, 0x00800200U, 0x00000000U, 0x00008002U, 0x00008200U, 0x00000000U, 0x00808002U
};

static const uint32_t SP2[64] = {
  0x40084010U, 0x40004000U, 0x00004000U, 0x00084010U, 0x00080000U, 0x00000010U, 0x40080010U, 0x40004010U,
  0x40000010U, 0x40084010U, 0x40084000U, 0x40000000U, 0x40004000U, 0x00080000U, 0x00000010U, 0x40080010U,
  0x00084000U, 0x00080010U, 0x40004010U, 0x00000000U, 0x40000000U, 0x00004000U, 0x00084010U, 0x40080000U,
  0x00080010U, 0x40000010U, 0x00000000U, 0x00084000U, 0x00004010U, 0x40084000U, 0x40080000U, 0x00004010U,
  0x00000000U, 0x00084010U, 0x40080010U, 0x00080000U, 0x40004010U, 0x40080000U, 0x40084000U, 0x00004000U,
  0x40080000U, 0x40004000U, 0x00000010U, 0x40084010U, 0x00084010U, 0x00000010U, 0x00004000U, 0x40000000U,
  0x00004010U, 0x40084000U, 0x00080000U, 0x40000010U, 0x00080010U, 0x40004010U, 0x40000010U, 0x00080010U,
  0x00084000U, 0x00000000U, 0x40004000U, 0x00004010U, 0x40000000U, 0x40080010U, 0x40084010U, 0x00084000U
};

static const uint32_t SP3[64] = {
  0x00000104U, 0x04010100U, 0x00000000U, 0x04010004U, 0x04000100U, 0x00000000U, 0x00010104U, 0x04000100U,
  0x00010004U, 0x04000004U, 0x04000004U, 0x00010000U, 0x04010104U, 0x00010004U, 0x04010000U, 0x00000104U,
  0x04000000U, 0x00000004U, 0x04010100U, 0x00000100U, 0x00010100U, 0x04010000U, 0x04010004U, 0x00010104U,
  0x04000104U, 0x00010100U, 0x00010000U, 0x04000104U, 0x00000004U, 0x04010104U, 0x00000100U, 0x04000000U,
  0x04010100U, 0x04000000U, 0x00010004U, 0x00000104U, 0x00010000U, 0x04010100U, 0x04000100U, 0x00000000U,
  0x00000100U, 0x00010004U, 0x04010104U, 0x04000100U, 0x04000004U, 0x00000100U, 0x00000000U, 0x04010004U,
  0x04000104U, 0x00010000U, 0x04000000U, 0x04010104U, 0x00000004U, 0x00010104U, 0x00010100U, 0x04000004U,
  0x04010000U, 0x04000104U, 0x00000104U, 0x04010000U, 0x00010104U, 0x00000004U, 0x04010004U, 0x00010100U
};

static const uint32_t SP4[64] = {
  0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U, 0x00401040U, 0x80400040U, 0x80400000U, 0x80001000U,
  0x00000000U, 0x00401000U, 0x00401000U, 0x80401040U, 0x80000040U, 0x00000000U, 0x00400040U, 0x80400000U,
  0x80000000U, 0x00001000U, 0x00400000U, 0x80401000U, 0x00000040U, 0x00400000U, 0x80001000U, 0x00001040U,
  0x80400040U, 0x80000000U, 0x00001040U, 0x00400040U, 0x00001000U, 0x00401040U, 0x80401040U, 0x80000040U,
  0x00400040U, 0x80400000U, 0x00401000U, 0x80401040U, 0x80000040U, 0x00000000U, 0x00000000U, 0x00401000U,
  0x00001040U, 0x00400040U, 0x80400040U, 0x80000000U, 0x80401000U, 0x80001040U, 0x80001040U, 0x00000040U,
  0x80401040U, 0x80000040U, 0x80000000U, 0x00001000U, 0x80400000U, 0x80001000U, 0x00401040U, 0x80400040U,
  0x80001000U, 0x00001040U, 0x00400000U, 0x80401000U, 0x00000040U, 0x00400000U, 0x00001000U, 0x00401040U
};

static const uint32_t SP5[64] = {
  0x00000080U, 0x01040080U, 0x01040000U, 0x21000080U, 0x00040000U, 0x00000080U, 0x20000000U, 0x01040000U,
  0x20040080U, 0x00040000U, 0x01000080U, 0x20040080U, 0x21000080U, 0x21040000U, 0x00040080U, 0x20000000U,
  0x01000000U, 0x20040000U, 0x20040000U, 0x00000000U, 0x20000080U, 0x21040080U, 0x21040080U, 0x01000080U,
  0x21040000U, 0x20000080U, 0x00000000U, 0x21000000U, 0x01040080U, 0x01000000U, 0x21000000U, 0x00040080U,
  0x00040000U, 0x21000080U, 0x00000080U, 0x01000000U, 0x20000000U, 0x01040000U, 0x21000080U, 0x20040080U,
  0x01000080U, 0x20000000U, 0x21040000U, 0x01040080U, 0x20040080U, 0x00000080U, 0x01000000U, 0x21040000U,
  0x21040080U, 0x0040080U, 0x21000000U, 0x21040080U, 0x01040000U, 0x00000000U, 0x20040000U, 0x21000000U,
  0x00040080U, 0x01000080U, 0x20000080U, 0x00040000U, 0x00000000U, 0x20040000U, 0x01040080U, 0x20000080U
};

static const uint32_t SP6[64] = {
  0x10000008U, 0x10200000U, 0x00002000U, 0x10202008U, 0x10200000U, 0x00000008U, 0x10202008U, 0x00200000U,
  0x10002000U, 0x00202008U, 0x00200000U, 0x10000008U, 0x00200008U, 0x10002000U, 0x10000000U, 0x00002008U,
  0x00000000U, 0x00200008U, 0x10002008U, 0x00002000U, 0x00202000U, 0x10002008U, 0x00000008U, 0x10200008U,
  0x10200008U, 0x00000000U, 0x00202008U, 0x10202000U, 0x00002008U, 0x00202000U, 0x10202000U, 0x10000000U,
  0x10002000U, 0x00000008U, 0x10200008U, 0x00202000U, 0x10202008U, 0x00200000U, 0x00002008U, 0x10000008U,
  0x00200000U, 0x10002000U, 0x10000000U, 0x00002008U, 0x10000008U, 0x10202008U, 0x00202000U, 0x10200000U,
  0x00202008U, 0x10202000U, 0x00000000U, 0x10200008U, 0x00000008U, 0x00002000U, 0x10200000U, 0x00202008U,
  0x00002000U, 0x00200008U, 0x10002008U, 0x00000000U, 0x10202000U, 0x10000000U, 0x00200008U, 0x10002008U
};

static const uint32_t SP7[64] = {
  0x00100000U, 0x02100001U, 0x02000401U, 0x00000000U, 0x00000400U, 0x02000401U, 0x00100401U, 0x02100400U,
  0x02100401U, 0x00100000U, 0x00000000U, 0x02000001U, 0x00000001U, 0x02000000U, 0x02100001U, 0x00000401U,
  0x02000400U, 0x00100401U, 0x00100001U, 0x02000400U, 0x02000001U, 0x02100000U, 0x02100400U, 0x00100001U,
  0x02100000U, 0x00000400U, 0x00000401U, 0x02100401U, 0x00100400U, 0x00000001U, 0x02000000U, 0x00100400U,
  0x02000000U, 0x00100400U, 0x00100000U, 0x02000401U, 0x02000401U, 0x02100001U, 0x02100001U, 0x00000001U,
  0x00100001U, 0x02000000U, 0x02000400U, 0x00100000U, 0x02100400U, 0x00000401U, 0x00100401U, 0x02100400U,
  0x00000401U, 0x02000001U, 0x02100401U, 0x02100000U, 0x00100400U, 0x00000000U, 0x00000001U, 0x02100401U,
  0x00000000U, 0x00100401U, 0x02100000U, 0x00000400U, 0x02000001U, 0x02000400U, 0x00000400U, 0x00100001U
};

static const uint32_t SP8[64] = {
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
static const uint8_t PC1_C[28] = {
  57, 49, 41, 33, 25, 17, 9,
   1, 58, 50, 42, 34, 26, 18,
  10,  2, 59, 51, 43, 35, 27,
  19, 11,  3, 60, 52, 44, 36
};

static const uint8_t PC1_D[28] = {
  63, 55, 47, 39, 31, 23, 15,
   7, 62, 54, 46, 38, 30, 22,
  14,  6, 61, 53, 45, 37, 29,
  21, 13,  5, 28, 20, 12,  4
};

/* Permuted Choice 2 (PC-2) matrix */
static const uint8_t PC2[48] = {
  14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
  23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
  41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
  44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

/* Subkey Left Shift Schedule */
static const uint8_t SHIFTS[16] = {
  1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

/*****************************************************************************/
/* Private Helper Functions                                                  */
/*****************************************************************************/

static inline uint8_t get_key_bit(const uint8_t* key, uint8_t bit_1based)
{
  uint8_t byte_idx = (uint8_t)((bit_1based - 1) / 8);
  uint8_t bit_idx  = (uint8_t)(7 - ((bit_1based - 1) % 8));
  return (key[byte_idx] >> bit_idx) & 1U;
}

/* Single DES key schedule calculation */
static void des_key_schedule(uint32_t (*sk)[2], const uint8_t* key)
{
  uint32_t C = 0;
  uint32_t D = 0;

  for (uint8_t i = 0; i < 28; ++i)
  {
    C = (C << 1) | get_key_bit(key, PC1_C[i]);
    D = (D << 1) | get_key_bit(key, PC1_D[i]);
  }

  for (uint8_t r = 0; r < 16; ++r)
  {
    uint8_t shift = SHIFTS[r];
    C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFFU;
    D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFFU;

    uint64_t CD = ((uint64_t)C << 28) | D;
    uint64_t key48 = 0;

    for (uint8_t i = 0; i < 48; ++i)
    {
      uint8_t bit_pos = PC2[i];
      uint8_t bit_val = (uint8_t)((CD >> (56 - bit_pos)) & 1U);
      key48 = (key48 << 1) | bit_val;
    }

    uint8_t sk8[8];
    sk8[0] = (uint8_t)((key48 >> 42) & 0x3FU);
    sk8[1] = (uint8_t)((key48 >> 36) & 0x3FU);
    sk8[2] = (uint8_t)((key48 >> 30) & 0x3FU);
    sk8[3] = (uint8_t)((key48 >> 24) & 0x3FU);
    sk8[4] = (uint8_t)((key48 >> 18) & 0x3FU);
    sk8[5] = (uint8_t)((key48 >> 12) & 0x3FU);
    sk8[6] = (uint8_t)((key48 >> 6)  & 0x3FU);
    sk8[7] = (uint8_t)( key48        & 0x3FU);

    sk[r][0] = ((uint32_t)sk8[0] << 24) | ((uint32_t)sk8[1] << 16) | ((uint32_t)sk8[2] << 8) | sk8[3];
    sk[r][1] = ((uint32_t)sk8[4] << 24) | ((uint32_t)sk8[5] << 16) | ((uint32_t)sk8[6] << 8) | sk8[7];
  }
}

/* Fast 32-bit Outerbridge Initial Permutation (IP) */
static inline void des_IP(uint32_t* pL, uint32_t* pR)
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
static inline void des_FP(uint32_t* pL, uint32_t* pR)
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
static void des_cipher_block(const uint32_t (*sk)[2], uint8_t* buf, int decrypt)
{
  uint32_t L = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
  uint32_t R = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];

  des_IP(&L, &R);

  for (int i = 0; i < 16; ++i)
  {
    int r = decrypt ? (15 - i) : i;

    uint32_t sk_w0 = sk[r][0];
    uint32_t sk_w1 = sk[r][1];

    uint8_t sk8_0 = (uint8_t)(sk_w0 >> 24);
    uint8_t sk8_1 = (uint8_t)(sk_w0 >> 16);
    uint8_t sk8_2 = (uint8_t)(sk_w0 >> 8);
    uint8_t sk8_3 = (uint8_t)(sk_w0);

    uint8_t sk8_4 = (uint8_t)(sk_w1 >> 24);
    uint8_t sk8_5 = (uint8_t)(sk_w1 >> 16);
    uint8_t sk8_6 = (uint8_t)(sk_w1 >> 8);
    uint8_t sk8_7 = (uint8_t)(sk_w1);

    uint8_t b1 = (uint8_t)((((R & 1U) << 5) | ((R >> 27) & 0x1FU)) ^ sk8_0);
    uint8_t b2 = (uint8_t)(((R >> 23) & 0x3FU) ^ sk8_1);
    uint8_t b3 = (uint8_t)(((R >> 19) & 0x3FU) ^ sk8_2);
    uint8_t b4 = (uint8_t)(((R >> 15) & 0x3FU) ^ sk8_3);
    uint8_t b5 = (uint8_t)(((R >> 11) & 0x3FU) ^ sk8_4);
    uint8_t b6 = (uint8_t)(((R >> 7)  & 0x3FU) ^ sk8_5);
    uint8_t b7 = (uint8_t)(((R >> 3)  & 0x3FU) ^ sk8_6);
    uint8_t b8 = (uint8_t)((((R & 0x1FU) << 1) | ((R >> 31) & 1U)) ^ sk8_7);

    uint32_t f_res = SP1[b1] ^ SP2[b2] ^ SP3[b3] ^ SP4[b4] ^ SP5[b5] ^ SP6[b6] ^ SP7[b7] ^ SP8[b8];

    uint32_t L_next = R;
    uint32_t R_next = L ^ f_res;
    L = L_next;
    R = R_next;
  }

  des_FP(&L, &R);

  buf[0] = (uint8_t)(R >> 24);
  buf[1] = (uint8_t)(R >> 16);
  buf[2] = (uint8_t)(R >> 8);
  buf[3] = (uint8_t)(R);
  buf[4] = (uint8_t)(L >> 24);
  buf[5] = (uint8_t)(L >> 16);
  buf[6] = (uint8_t)(L >> 8);
  buf[7] = (uint8_t)(L);
}

#if DES_ENABLE_CTR
static void increment_iv(uint8_t* iv)
{
  for (int i = DES_BLOCKLEN - 1; i >= 0; --i)
  {
    if (++iv[i] != 0)
      break;
  }
}

/*
 * Return 1 if at least `needed` big-endian counter blocks can be consumed
 * without wrapping the 64-bit counter mid-request.
 */
static int ctr_blocks_until_wrap(const uint8_t iv[DES_BLOCKLEN], size_t needed)
{
  uint8_t remaining[DES_BLOCKLEN];
  uint8_t bi;
  size_t blocks;
  uint8_t carry;
  uint8_t all_zero = 1;

  carry = 0;
  for (bi = DES_BLOCKLEN; bi > 0; --bi)
  {
    const uint8_t idx = (uint8_t)(bi - 1u);
    const unsigned diff = (unsigned)(0u - (unsigned)iv[idx] - (unsigned)carry);
    remaining[idx] = (uint8_t)diff;
    carry = (uint8_t)(iv[idx] != 0 || carry != 0 ? 1u : 0u);
  }

  for (bi = 0; bi < DES_BLOCKLEN; ++bi)
  {
    if (iv[bi] != 0)
    {
      all_zero = 0;
      break;
    }
  }
  if (all_zero)
    return 1;

  /* If any high byte of remaining is non-zero, remaining exceeds size_t range. */
  for (bi = 0; bi + sizeof(size_t) < DES_BLOCKLEN; ++bi)
  {
    if (remaining[bi] != 0)
      return 1;
  }

  blocks = 0;
  for (bi = 0; bi < sizeof(size_t) && bi < DES_BLOCKLEN; ++bi)
    blocks = (blocks << 8) | remaining[DES_BLOCKLEN - sizeof(size_t) + bi];

  return blocks >= needed;
}
#endif

/*****************************************************************************/
/* Public Functions: secure wipe                                             */
/*****************************************************************************/

void DES_secure_zero(void* memory, size_t length)
{
  volatile uint8_t* bytes = (volatile uint8_t*)memory;
  size_t i;

  for (i = 0; i < length; ++i)
    bytes[i] = 0;
#if defined(__GNUC__) || defined(__clang__)
  __asm__ __volatile__("" ::: "memory");
#endif
}

void DES_ctx_clear(struct DES_ctx* ctx)
{
  if (ctx == NULL)
    return;
  DES_secure_zero(ctx, sizeof(*ctx));
}

#if DES_ENABLE_TDES
void DES3_ctx_clear(struct DES3_ctx* ctx)
{
  if (ctx == NULL)
    return;
  DES_secure_zero(ctx, sizeof(*ctx));
}
#endif

/*****************************************************************************/
/* Public Functions: Single DES                                              */
/*****************************************************************************/

void DES_init_ctx(struct DES_ctx* ctx, const uint8_t* key)
{
  des_key_schedule(ctx->Sk, key);
}

#if DES_NEEDS_IV
void DES_init_ctx_iv(struct DES_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
  des_key_schedule(ctx->Sk, key);
  memcpy(ctx->Iv, iv, DES_BLOCKLEN);
}

void DES_ctx_set_iv(struct DES_ctx* ctx, const uint8_t* iv)
{
  memcpy(ctx->Iv, iv, DES_BLOCKLEN);
}
#endif

#if DES_ENABLE_ECB
void DES_ECB_encrypt(const struct DES_ctx* ctx, uint8_t* buf)
{
#if DES_STRICT
  if (ctx == NULL || buf == NULL)
    return;
#endif
  des_cipher_block(ctx->Sk, buf, 0);
}

void DES_ECB_decrypt(const struct DES_ctx* ctx, uint8_t* buf)
{
#if DES_STRICT
  if (ctx == NULL || buf == NULL)
    return;
#endif
  des_cipher_block(ctx->Sk, buf, 1);
}
#endif

#if DES_ENABLE_CBC
int DES_CBC_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if ((length % DES_BLOCKLEN) != 0)
    return DES_ERR;

  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= ctx->Iv[j];
    }
    des_cipher_block(ctx->Sk, buf + i, 0);
    memcpy(ctx->Iv, buf + i, DES_BLOCKLEN);
  }
  return DES_OK;
}

int DES_CBC_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t store[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if ((length % DES_BLOCKLEN) != 0)
    return DES_ERR;

  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    memcpy(store, buf + i, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, buf + i, 1);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= ctx->Iv[j];
    }
    memcpy(ctx->Iv, store, DES_BLOCKLEN);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CTR
int DES_CTR_crypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t buffer[DES_BLOCKLEN];
  size_t i = 0;
  size_t blocks_needed;

#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if (length == 0)
    return DES_OK;

  blocks_needed = length / DES_BLOCKLEN + ((length % DES_BLOCKLEN) != 0 ? 1u : 0u);
  if (!ctr_blocks_until_wrap(ctx->Iv, blocks_needed))
    return DES_ERR;

  for (; i < length; i += DES_BLOCKLEN)
  {
    memcpy(buffer, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, buffer, 0);

    size_t block_bytes = (length - i < DES_BLOCKLEN) ? (length - i) : DES_BLOCKLEN;
    for (size_t bi = 0; bi < block_bytes; ++bi)
    {
      buf[i + bi] ^= buffer[bi];
    }
    increment_iv(ctx->Iv);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB64
/* CFB-64 allows a shorter final segment (SP 800-38A); shift rem ciphertext bytes into Iv. */
static void cfb64_shift_iv(uint8_t* iv, const uint8_t* ct, size_t rem)
{
  if (rem >= DES_BLOCKLEN)
  {
    memcpy(iv, ct, DES_BLOCKLEN);
    return;
  }
  memmove(iv, iv + rem, DES_BLOCKLEN - rem);
  memcpy(iv + (DES_BLOCKLEN - rem), ct, rem);
}

int DES_CFB64_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
  size_t i;
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif

  for (i = 0; i + DES_BLOCKLEN <= length; i += DES_BLOCKLEN)
  {
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    memcpy(ctx->Iv, buf + i, DES_BLOCKLEN);
  }
  if (i < length)
  {
    size_t rem = length - i;
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    for (size_t j = 0; j < rem; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    cfb64_shift_iv(ctx->Iv, buf + i, rem);
  }
  return DES_OK;
}

int DES_CFB64_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
  uint8_t store[DES_BLOCKLEN];
  size_t i;
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif

  for (i = 0; i + DES_BLOCKLEN <= length; i += DES_BLOCKLEN)
  {
    memcpy(store, buf + i, DES_BLOCKLEN);
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    /* CFB decryption uses the forward (encrypt) direction of the cipher */
    des_cipher_block(ctx->Sk, keystream, 0);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    memcpy(ctx->Iv, store, DES_BLOCKLEN);
  }
  if (i < length)
  {
    size_t rem = length - i;
    memcpy(store, buf + i, rem);
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    for (size_t j = 0; j < rem; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    cfb64_shift_iv(ctx->Iv, store, rem);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB8
int DES_CFB8_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; ++i)
  {
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    buf[i] ^= keystream[0];
    memmove(ctx->Iv, ctx->Iv + 1, DES_BLOCKLEN - 1);
    ctx->Iv[DES_BLOCKLEN - 1] = buf[i];
  }
  return DES_OK;
}

int DES_CFB8_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; ++i)
  {
    uint8_t c = buf[i];
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    buf[i] ^= keystream[0];
    memmove(ctx->Iv, ctx->Iv + 1, DES_BLOCKLEN - 1);
    ctx->Iv[DES_BLOCKLEN - 1] = c;
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB1
/* Shift the 64-bit feedback register left one bit, inserting the ciphertext bit */
static void cfb1_shift_iv(uint8_t* iv, uint8_t ct_bit)
{
  for (uint8_t j = 0; j < DES_BLOCKLEN - 1; ++j)
  {
    iv[j] = (uint8_t)((iv[j] << 1) | (iv[j + 1] >> 7));
  }
  iv[DES_BLOCKLEN - 1] = (uint8_t)((iv[DES_BLOCKLEN - 1] << 1) | ct_bit);
}

int DES_CFB1_encrypt(struct DES_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (bit_length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < bit_length; ++i)
  {
    size_t byte_idx = i / 8;
    uint8_t shift = (uint8_t)(7 - (i % 8));
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    uint8_t ct_bit = (uint8_t)(((buf[byte_idx] >> shift) ^ (keystream[0] >> 7)) & 1U);
    buf[byte_idx] = (uint8_t)((buf[byte_idx] & ~(1U << shift)) | (ct_bit << shift));
    cfb1_shift_iv(ctx->Iv, ct_bit);
  }
  return DES_OK;
}

int DES_CFB1_decrypt(struct DES_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (bit_length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < bit_length; ++i)
  {
    size_t byte_idx = i / 8;
    uint8_t shift = (uint8_t)(7 - (i % 8));
    uint8_t ct_bit = (buf[byte_idx] >> shift) & 1U;
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des_cipher_block(ctx->Sk, keystream, 0);
    uint8_t pt_bit = (uint8_t)((ct_bit ^ (keystream[0] >> 7)) & 1U);
    buf[byte_idx] = (uint8_t)((buf[byte_idx] & ~(1U << shift)) | (pt_bit << shift));
    cfb1_shift_iv(ctx->Iv, ct_bit);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_OFB
int DES_OFB_crypt(struct DES_ctx* ctx, uint8_t* buf, size_t length)
{
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    /* Feedback is the keystream itself, independent of the data */
    des_cipher_block(ctx->Sk, ctx->Iv, 0);
    size_t block_bytes = (length - i < DES_BLOCKLEN) ? (length - i) : DES_BLOCKLEN;
    for (size_t bi = 0; bi < block_bytes; ++bi)
    {
      buf[i + bi] ^= ctx->Iv[bi];
    }
  }
  return DES_OK;
}
#endif

/*****************************************************************************/
/* Public Functions: Triple DES (3DES / TDES)                                */
/*****************************************************************************/

#if DES_ENABLE_TDES

int DES3_init_ctx(struct DES3_ctx* ctx, const uint8_t* key, size_t keylen)
{
#if DES_STRICT
  if (ctx == NULL || key == NULL)
    return DES_ERR;
#endif
  if (keylen != 16 && keylen != 24)
    return DES_ERR;

  des_key_schedule(&ctx->Sk[0], key);
  if (keylen == 16)
  {
    /* 2-Key 3DES: K1, K2, K1 */
    des_key_schedule(&ctx->Sk[16], key + 8);
    des_key_schedule(&ctx->Sk[32], key);
  }
  else
  {
    /* 3-Key 3DES: K1, K2, K3 */
    des_key_schedule(&ctx->Sk[16], key + 8);
    des_key_schedule(&ctx->Sk[32], key + 16);
  }
  return DES_OK;
}

#if DES_NEEDS_IV
int DES3_init_ctx_iv(struct DES3_ctx* ctx, const uint8_t* key, size_t keylen, const uint8_t* iv)
{
#if DES_STRICT
  if (iv == NULL)
    return DES_ERR;
#endif
  if (DES3_init_ctx(ctx, key, keylen) != DES_OK)
    return DES_ERR;
  memcpy(ctx->Iv, iv, DES_BLOCKLEN);
  return DES_OK;
}

void DES3_ctx_set_iv(struct DES3_ctx* ctx, const uint8_t* iv)
{
#if DES_STRICT
  if (ctx == NULL || iv == NULL)
    return;
#endif
  memcpy(ctx->Iv, iv, DES_BLOCKLEN);
}
#endif

/* 3DES Core Encryption: E(K3) -> D(K2) -> E(K1) */
static void des3_encrypt_block(const struct DES3_ctx* ctx, uint8_t* buf)
{
  des_cipher_block(&ctx->Sk[0],  buf, 0); /* Encrypt K1 */
  des_cipher_block(&ctx->Sk[16], buf, 1); /* Decrypt K2 */
  des_cipher_block(&ctx->Sk[32], buf, 0); /* Encrypt K3 */
}

#if (DES_ENABLE_ECB == 1) || (DES_ENABLE_CBC == 1)
/* 3DES Core Decryption: D(K1) -> E(K2) -> D(K3) */
static void des3_decrypt_block(const struct DES3_ctx* ctx, uint8_t* buf)
{
  des_cipher_block(&ctx->Sk[32], buf, 1); /* Decrypt K3 */
  des_cipher_block(&ctx->Sk[16], buf, 0); /* Encrypt K2 */
  des_cipher_block(&ctx->Sk[0],  buf, 1); /* Decrypt K1 */
}
#endif

#if DES_ENABLE_ECB
void DES3_ECB_encrypt(const struct DES3_ctx* ctx, uint8_t* buf)
{
#if DES_STRICT
  if (ctx == NULL || buf == NULL)
    return;
#endif
  des3_encrypt_block(ctx, buf);
}

void DES3_ECB_decrypt(const struct DES3_ctx* ctx, uint8_t* buf)
{
#if DES_STRICT
  if (ctx == NULL || buf == NULL)
    return;
#endif
  des3_decrypt_block(ctx, buf);
}
#endif

#if DES_ENABLE_CBC
int DES3_CBC_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if ((length % DES_BLOCKLEN) != 0)
    return DES_ERR;

  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= ctx->Iv[j];
    }
    des3_encrypt_block(ctx, buf + i);
    memcpy(ctx->Iv, buf + i, DES_BLOCKLEN);
  }
  return DES_OK;
}

int DES3_CBC_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t store[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if ((length % DES_BLOCKLEN) != 0)
    return DES_ERR;

  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    memcpy(store, buf + i, DES_BLOCKLEN);
    des3_decrypt_block(ctx, buf + i);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= ctx->Iv[j];
    }
    memcpy(ctx->Iv, store, DES_BLOCKLEN);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CTR
int DES3_CTR_crypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t buffer[DES_BLOCKLEN];
  size_t i = 0;
  size_t blocks_needed;

#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  if (length == 0)
    return DES_OK;

  blocks_needed = length / DES_BLOCKLEN + ((length % DES_BLOCKLEN) != 0 ? 1u : 0u);
  if (!ctr_blocks_until_wrap(ctx->Iv, blocks_needed))
    return DES_ERR;

  for (; i < length; i += DES_BLOCKLEN)
  {
    memcpy(buffer, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, buffer);

    size_t block_bytes = (length - i < DES_BLOCKLEN) ? (length - i) : DES_BLOCKLEN;
    for (size_t bi = 0; bi < block_bytes; ++bi)
    {
      buf[i + bi] ^= buffer[bi];
    }
    increment_iv(ctx->Iv);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB64
int DES3_CFB64_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
  size_t i;
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif

  for (i = 0; i + DES_BLOCKLEN <= length; i += DES_BLOCKLEN)
  {
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    memcpy(ctx->Iv, buf + i, DES_BLOCKLEN);
  }
  if (i < length)
  {
    size_t rem = length - i;
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    for (size_t j = 0; j < rem; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    cfb64_shift_iv(ctx->Iv, buf + i, rem);
  }
  return DES_OK;
}

int DES3_CFB64_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
  uint8_t store[DES_BLOCKLEN];
  size_t i;
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif

  for (i = 0; i + DES_BLOCKLEN <= length; i += DES_BLOCKLEN)
  {
    memcpy(store, buf + i, DES_BLOCKLEN);
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    /* CFB decryption uses the forward (encrypt) direction of the cipher */
    des3_encrypt_block(ctx, keystream);
    for (uint8_t j = 0; j < DES_BLOCKLEN; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    memcpy(ctx->Iv, store, DES_BLOCKLEN);
  }
  if (i < length)
  {
    size_t rem = length - i;
    memcpy(store, buf + i, rem);
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    for (size_t j = 0; j < rem; ++j)
    {
      buf[i + j] ^= keystream[j];
    }
    cfb64_shift_iv(ctx->Iv, store, rem);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB8
int DES3_CFB8_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; ++i)
  {
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    buf[i] ^= keystream[0];
    memmove(ctx->Iv, ctx->Iv + 1, DES_BLOCKLEN - 1);
    ctx->Iv[DES_BLOCKLEN - 1] = buf[i];
  }
  return DES_OK;
}

int DES3_CFB8_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; ++i)
  {
    uint8_t c = buf[i];
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    buf[i] ^= keystream[0];
    memmove(ctx->Iv, ctx->Iv + 1, DES_BLOCKLEN - 1);
    ctx->Iv[DES_BLOCKLEN - 1] = c;
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_CFB1
int DES3_CFB1_encrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (bit_length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < bit_length; ++i)
  {
    size_t byte_idx = i / 8;
    uint8_t shift = (uint8_t)(7 - (i % 8));
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    uint8_t ct_bit = (uint8_t)(((buf[byte_idx] >> shift) ^ (keystream[0] >> 7)) & 1U);
    buf[byte_idx] = (uint8_t)((buf[byte_idx] & ~(1U << shift)) | (ct_bit << shift));
    cfb1_shift_iv(ctx->Iv, ct_bit);
  }
  return DES_OK;
}

int DES3_CFB1_decrypt(struct DES3_ctx* ctx, uint8_t* buf, size_t bit_length)
{
  uint8_t keystream[DES_BLOCKLEN];
#if DES_STRICT
  if (ctx == NULL || (bit_length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < bit_length; ++i)
  {
    size_t byte_idx = i / 8;
    uint8_t shift = (uint8_t)(7 - (i % 8));
    uint8_t ct_bit = (buf[byte_idx] >> shift) & 1U;
    memcpy(keystream, ctx->Iv, DES_BLOCKLEN);
    des3_encrypt_block(ctx, keystream);
    uint8_t pt_bit = (uint8_t)((ct_bit ^ (keystream[0] >> 7)) & 1U);
    buf[byte_idx] = (uint8_t)((buf[byte_idx] & ~(1U << shift)) | (pt_bit << shift));
    cfb1_shift_iv(ctx->Iv, ct_bit);
  }
  return DES_OK;
}
#endif

#if DES_ENABLE_OFB
int DES3_OFB_crypt(struct DES3_ctx* ctx, uint8_t* buf, size_t length)
{
#if DES_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return DES_ERR;
#endif
  for (size_t i = 0; i < length; i += DES_BLOCKLEN)
  {
    /* Feedback is the keystream itself, independent of the data */
    des3_encrypt_block(ctx, ctx->Iv);
    size_t block_bytes = (length - i < DES_BLOCKLEN) ? (length - i) : DES_BLOCKLEN;
    for (size_t bi = 0; bi < block_bytes; ++bi)
    {
      buf[i + bi] ^= ctx->Iv[bi];
    }
  }
  return DES_OK;
}
#endif

#endif /* #if DES_ENABLE_TDES */

/*****************************************************************************/
/* Public Functions: DES / 3DES CMAC (NIST SP 800-38B)                      */
/*****************************************************************************/
#if DES_ENABLE_CMAC

static void cmac_shift_left(const uint8_t* input, uint8_t* output)
{
  uint8_t overflow = 0;
  for (int i = 7; i >= 0; --i)
  {
    output[i] = (input[i] << 1) | overflow;
    overflow = (input[i] & 0x80U) ? 1U : 0U;
  }
}

/* CMAC needs only the raw block cipher, so it schedules keys and chains
   blocks itself; it must keep working with the optional ECB/CBC/TDES mode
   gates compiled out. */
struct cmac_cipher
{
  uint32_t Sk[48][2];
  int triple;
};

static int cmac_cipher_init(struct cmac_cipher* c, const uint8_t* key, size_t keylen)
{
  if (keylen != 8 && keylen != 16 && keylen != 24)
  {
    return -1;
  }
  c->triple = (keylen != 8);
  des_key_schedule(&c->Sk[0], key);
  if (keylen == 16)
  {
    /* 2-Key 3DES: K1, K2, K1 */
    des_key_schedule(&c->Sk[16], key + 8);
    des_key_schedule(&c->Sk[32], key);
  }
  else if (keylen == 24)
  {
    /* 3-Key 3DES: K1, K2, K3 */
    des_key_schedule(&c->Sk[16], key + 8);
    des_key_schedule(&c->Sk[32], key + 16);
  }
  return 0;
}

static void cmac_encrypt_block(const struct cmac_cipher* c, uint8_t* buf)
{
  des_cipher_block(&c->Sk[0], buf, 0);
  if (c->triple)
  {
    des_cipher_block(&c->Sk[16], buf, 1);
    des_cipher_block(&c->Sk[32], buf, 0);
  }
}

static void cmac_generate_subkeys(const struct cmac_cipher* c, uint8_t* k1, uint8_t* k2)
{
  static const uint8_t const_Rb = 0x1BU;
  uint8_t L[DES_BLOCKLEN] = {0};

  cmac_encrypt_block(c, L);

  cmac_shift_left(L, k1);
  if (L[0] & 0x80U)
  {
    k1[7] ^= const_Rb;
  }

  cmac_shift_left(k1, k2);
  if (k1[0] & 0x80U)
  {
    k2[7] ^= const_Rb;
  }

#if DES_ZEROIZE
  DES_secure_zero(L, sizeof(L));
#endif
}

int DES_CMAC(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
             uint8_t* tag, size_t tag_len)
{
  struct cmac_cipher cipher;
  uint8_t k1[DES_BLOCKLEN];
  uint8_t k2[DES_BLOCKLEN];
  uint8_t last_block[DES_BLOCKLEN];
  uint8_t mac[DES_BLOCKLEN];
  size_t n_blocks;
  size_t last_idx;
  int is_complete;
  size_t i;

  if (key == NULL || tag == NULL ||
      tag_len < DES_CMAC_MIN_TAG_LEN || tag_len > DES_CMAC_TAG_MAX ||
      (msg_len != 0 && msg == NULL))
  {
    return DES_ERR;
  }
  if (cmac_cipher_init(&cipher, key, keylen) != 0)
  {
    return DES_ERR;
  }

  cmac_generate_subkeys(&cipher, k1, k2);

  n_blocks = (msg_len + DES_BLOCKLEN - 1) / DES_BLOCKLEN;
  is_complete = 1;
  if (n_blocks == 0)
  {
    n_blocks = 1;
    is_complete = 0;
  }
  else
  {
    is_complete = (msg_len % DES_BLOCKLEN == 0);
  }

  memset(last_block, 0, DES_BLOCKLEN);
  last_idx = n_blocks - 1;

  if (is_complete)
  {
    memcpy(last_block, msg + (last_idx * DES_BLOCKLEN), DES_BLOCKLEN);
    for (i = 0; i < DES_BLOCKLEN; ++i)
    {
      last_block[i] ^= k1[i];
    }
  }
  else
  {
    size_t rem = msg_len % DES_BLOCKLEN;
    if (rem > 0)
    {
      memcpy(last_block, msg + (last_idx * DES_BLOCKLEN), rem);
    }
    last_block[rem] = 0x80U;
    for (i = 0; i < DES_BLOCKLEN; ++i)
    {
      last_block[i] ^= k2[i];
    }
  }

  /* CBC-MAC chain from zero IV (SP 800-38B). */
  memset(mac, 0, DES_BLOCKLEN);
  for (i = 0; i < last_idx; ++i)
  {
    size_t j;
    for (j = 0; j < DES_BLOCKLEN; ++j)
    {
      mac[j] ^= msg[i * DES_BLOCKLEN + j];
    }
    cmac_encrypt_block(&cipher, mac);
  }
  for (i = 0; i < DES_BLOCKLEN; ++i)
  {
    mac[i] ^= last_block[i];
  }
  cmac_encrypt_block(&cipher, mac);

  memcpy(tag, mac, tag_len);

#if DES_ZEROIZE
  DES_secure_zero(&cipher, sizeof(cipher));
  DES_secure_zero(k1, sizeof(k1));
  DES_secure_zero(k2, sizeof(k2));
  DES_secure_zero(last_block, sizeof(last_block));
  DES_secure_zero(mac, sizeof(mac));
#endif

  return DES_OK;
}

int DES_CMAC_verify(const uint8_t* key, size_t keylen, const uint8_t* msg, size_t msg_len,
                    const uint8_t* tag, size_t tag_len)
{
  uint8_t computed[DES_CMAC_TAG_MAX];
  uint8_t difference = 0;
  size_t i;

  if (tag == NULL ||
      tag_len < DES_CMAC_MIN_TAG_LEN || tag_len > DES_CMAC_TAG_MAX)
  {
    return DES_ERR;
  }
  if (DES_CMAC(key, keylen, msg, msg_len, computed, tag_len) != DES_OK)
  {
    return DES_ERR;
  }

  for (i = 0; i < tag_len; ++i)
  {
    difference |= (uint8_t)(computed[i] ^ tag[i]);
  }

#if DES_ZEROIZE
  DES_secure_zero(computed, sizeof(computed));
#endif
  return difference == 0 ? DES_OK : DES_ERR;
}

#endif /* DES_ENABLE_CMAC */

