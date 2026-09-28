/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 * AES has a fixed 16-byte block size. This file implements the enabled block,
 * and streaming modes. AEAD and MAC implementations live in separate files.
 */


/*****************************************************************************/
/* Includes:                                                                 */
/*****************************************************************************/
#include <string.h> /* memcpy, memset */
#include <tiny_crypto/aes.h>
#include "aes_internal.h"
#include "aes_platform_internal.h"

/* Keep fixed S-boxes in AVR flash. Runtime S-box mode remains writable SRAM
 * by design and therefore bypasses these accessors. */
#if defined(__AVR__) && TC_AVR_PROGMEM
  #include <avr/pgmspace.h>
  #define TC_AES_TABLE_STORAGE PROGMEM
  #define TC_AES_READ_TABLE(table, index) pgm_read_byte(&(table)[(index)])
#else
  #define TC_AES_TABLE_STORAGE
  #define TC_AES_READ_TABLE(table, index) ((table)[(index)])
#endif

/*****************************************************************************/
/* Integer width note (MCU / AVR-oriented):                                  */
/*   Use size_t only for API buffer lengths that may exceed 255.             */
/*   Prefer uint8_t for block offsets (0..15), rounds, and similar counters. */
/*****************************************************************************/

/*****************************************************************************/
/* Defines:                                                                  */
/*****************************************************************************/
/* Number of columns in the AES state (constant for AES: 4). */
#define Nb 4

#if TC_AES_KEY_BITS == 256
    #define Nk 8
    #define Nr 14
#elif TC_AES_KEY_BITS == 192
    #define Nk 6
    #define Nr 12
#else
    #define Nk 4
    #define Nr 10
#endif

/*****************************************************************************/
/* Private variables:                                                        */
/*****************************************************************************/
/* Runtime S-box mode trades read-only table storage for a fixed RAM table. */
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
static uint8_t sbox[256];
static uint8_t sbox_ready;
#elif TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_FAST
static const uint8_t sbox[256] TC_AES_TABLE_STORAGE = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };
#endif

#if (defined(TC_AES_ENABLE_CBC) && TC_AES_ENABLE_CBC == 1) || (defined(TC_AES_ENABLE_ECB) && TC_AES_ENABLE_ECB == 1) || \
    (defined(TC_AES_CAVP) && TC_AES_CAVP == 1) || TC_AES_ENABLE_DYNAMIC
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
static uint8_t rsbox[256];
#elif TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_FAST
static const uint8_t rsbox[256] TC_AES_TABLE_STORAGE = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };
#endif
#endif

/*****************************************************************************/
/* Private functions:                                                        */
/*****************************************************************************/
static uint8_t tc_aes_xtime(uint8_t x);
#if TC_AES_SBOX_MODE != TC_AES_SBOX_MODE_FAST
static uint8_t tc_aes_sbox_multiply(uint8_t a, uint8_t b)
{
  uint8_t result = 0;
  unsigned i;

  for (i = 0; i < 8; ++i)
  {
    result ^= (uint8_t)(0u - (uint8_t)(b & 1u)) & a;
    a = (uint8_t)((uint8_t)(a << 1) ^
                  (0x1bu & (uint8_t)(0u - (uint8_t)(a >> 7))));
    b >>= 1;
  }

  return result;
}

static uint8_t tc_aes_sbox_inverse(uint8_t value)
{
  uint8_t result = 1;
  uint8_t factor = value;
  unsigned exponent = 254;

  /* The exponent is public and fixed. This also maps zero to zero. */
  while (exponent != 0)
  {
    if (exponent & 1u)
      result = tc_aes_sbox_multiply(result, factor);
    factor = tc_aes_sbox_multiply(factor, factor);
    exponent >>= 1;
  }

  return result;
}

static uint8_t tc_aes_sbox_rotate_left(uint8_t value, unsigned count)
{
  return (uint8_t)((value << count) | (value >> (8u - count)));
}

#endif

#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
void TC_AES_init_sbox(void)
{
  unsigned i;

  for (i = 0; i < 256; ++i)
  {
    const uint8_t inverse = tc_aes_sbox_inverse((uint8_t)i);
    sbox[i] = (uint8_t)(inverse ^
                        tc_aes_sbox_rotate_left(inverse, 1) ^
                        tc_aes_sbox_rotate_left(inverse, 2) ^
                        tc_aes_sbox_rotate_left(inverse, 3) ^
                        tc_aes_sbox_rotate_left(inverse, 4) ^ 0x63);
  }

#if (defined(TC_AES_ENABLE_CBC) && TC_AES_ENABLE_CBC == 1) || (defined(TC_AES_ENABLE_ECB) && TC_AES_ENABLE_ECB == 1) || \
    (defined(TC_AES_CAVP) && TC_AES_CAVP == 1) || TC_AES_ENABLE_DYNAMIC
  for (i = 0; i < 256; ++i)
    rsbox[sbox[i]] = (uint8_t)i;
#endif
  sbox_ready = 1;
}
#endif

#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
static uint8_t tc_aes_ct_eq_mask_u8(uint8_t a, uint8_t b)
{
  const uint16_t difference = (uint16_t)(a ^ b);
  return (uint8_t)((difference - 1U) >> 8);
}
#endif

void TC_AES_ctx_clear(struct TC_AES_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}

void TC_AES_key_ctx_clear(struct TC_AES_key_ctx* ctx)
{
  if (ctx == NULL)
    return;
  TC_secure_zero(ctx, sizeof(*ctx));
}


static uint8_t tc_aes_xtime(uint8_t x)
{
  return (uint8_t)((uint8_t)(x << 1) ^
                   (((x >> 7) & 1u) * 0x1bu));
}

/* Constant-time mode computes the S-box. Runtime mode scans its generated
 * RAM table; fast mode opts into a secret-indexed lookup. */
static uint8_t tc_aes_sbox_value(uint8_t num)
{
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_FAST
  return TC_AES_READ_TABLE(sbox, num);
#elif TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_CONSTANT_TIME
  const uint8_t inverse = tc_aes_sbox_inverse(num);
  return (uint8_t)(inverse ^ tc_aes_sbox_rotate_left(inverse, 1) ^
                   tc_aes_sbox_rotate_left(inverse, 2) ^
                   tc_aes_sbox_rotate_left(inverse, 3) ^
                   tc_aes_sbox_rotate_left(inverse, 4) ^ 0x63u);
#else
#if defined(__AVR__) && TC_AVR_PROGMEM
  uint8_t value = 0;
  unsigned i;

  for (i = 0; i < 256; ++i)
  {
    const uint8_t mask = tc_aes_ct_eq_mask_u8((uint8_t)i, num);
    value |= TC_AES_READ_TABLE(sbox, i) & mask;
  }
#else
  const volatile uint8_t *table = sbox;
  uint8_t value = 0;
  unsigned i;

  for (i = 0; i < 256; ++i)
  {
    const uint8_t mask = tc_aes_ct_eq_mask_u8((uint8_t)i, num);
    value |= table[i] & mask;
  }
#endif

  return value;
#endif
}

static void tc_aes_key_expansion(uint8_t* round_key, const uint8_t* key, unsigned words)
{
  unsigned i, j, k;
  uint8_t tempa[4];
  uint8_t rcon = 0x01;

  tc_aes_copy_bytes(round_key, key, words * 4);

  for (i = words; i < Nb * ((words + 6) + 1); ++i)
  {
    {
      k = (i - 1) * 4;
      tempa[0]=round_key[k + 0];
      tempa[1]=round_key[k + 1];
      tempa[2]=round_key[k + 2];
      tempa[3]=round_key[k + 3];

    }

    if (i % words == 0)
    {
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1];
        tempa[1] = tempa[2];
        tempa[2] = tempa[3];
        tempa[3] = u8tmp;
      }

      {
        tempa[0] = tc_aes_sbox_value(tempa[0]);
        tempa[1] = tc_aes_sbox_value(tempa[1]);
        tempa[2] = tc_aes_sbox_value(tempa[2]);
        tempa[3] = tc_aes_sbox_value(tempa[3]);
      }

      tempa[0] = tempa[0] ^ rcon;
      rcon = tc_aes_xtime(rcon);
    }
#if TC_AES_KEY_BITS == 256 || TC_AES_ENABLE_DYNAMIC
    if (words == 8 && i % words == 4)
    {
      {
        tempa[0] = tc_aes_sbox_value(tempa[0]);
        tempa[1] = tc_aes_sbox_value(tempa[1]);
        tempa[2] = tc_aes_sbox_value(tempa[2]);
        tempa[3] = tc_aes_sbox_value(tempa[3]);
      }
    }
#endif
    j = i * 4; k=(i - words) * 4;
    round_key[j + 0] = round_key[k + 0] ^ tempa[0];
    round_key[j + 1] = round_key[k + 1] ^ tempa[1];
    round_key[j + 2] = round_key[k + 2] ^ tempa[2];
    round_key[j + 3] = round_key[k + 3] ^ tempa[3];
  }
#if TC_ZEROIZE
  TC_secure_zero(tempa, sizeof(tempa));
#endif
}

TC_status TC_AES_key_init(struct TC_AES_key_ctx* ctx, const uint8_t* key)
{
  if (ctx == NULL || key == NULL)
    return TC_ERROR;
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
  if (!sbox_ready)
    return TC_ERROR;
#endif
  tc_aes_key_expansion(ctx->round_key, key, Nk);
  return TC_OK;
}

TC_status TC_AES_init_ctx(struct TC_AES_ctx* ctx, const uint8_t* key)
{
  if (ctx == NULL || TC_AES_key_init(&ctx->key, key) != TC_OK)
    return TC_ERROR;
#if (defined(TC_AES_ENABLE_CBC) && (TC_AES_ENABLE_CBC == 1)) || \
    (defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)) || \
    (defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1))
  memset(ctx->iv, 0, TC_AES_BLOCKLEN);
#endif
#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)
  memset(ctx->ctr_stream, 0, TC_AES_BLOCKLEN);
  ctx->ctr_pos = TC_AES_BLOCKLEN;
#endif
#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)
  ctx->ofb_pos = TC_AES_BLOCKLEN;
#endif
  return TC_OK;
}
#if (defined(TC_AES_ENABLE_CBC) && (TC_AES_ENABLE_CBC == 1)) || (defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)) || \
    (defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1))
TC_status TC_AES_init_ctx_iv(struct TC_AES_ctx* ctx, const uint8_t* key,
                            const uint8_t* iv)
{
  if (iv == NULL || TC_AES_init_ctx(ctx, key) != TC_OK)
    return TC_ERROR;
  tc_aes_copy_bytes(ctx->iv, iv, TC_AES_BLOCKLEN);
#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)
  ctx->ctr_pos = TC_AES_BLOCKLEN;
#endif
#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)
  ctx->ofb_pos = TC_AES_BLOCKLEN;
#endif
  return TC_OK;
}
TC_status TC_AES_ctx_set_iv(struct TC_AES_ctx* ctx, const uint8_t* iv)
{
  if (ctx == NULL || iv == NULL)
    return TC_ERROR;
  tc_aes_copy_bytes(ctx->iv, iv, TC_AES_BLOCKLEN);
#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)
  ctx->ctr_pos = TC_AES_BLOCKLEN;
#endif
#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)
  ctx->ofb_pos = TC_AES_BLOCKLEN;
#endif
  return TC_OK;
}
#endif

#if (defined(TC_AES_ENABLE_CBC) && TC_AES_ENABLE_CBC == 1) || (defined(TC_AES_ENABLE_ECB) && TC_AES_ENABLE_ECB == 1) || \
    (defined(TC_AES_ENABLE_CTR) && TC_AES_ENABLE_CTR == 1) || (defined(TC_AES_ENABLE_OFB) && TC_AES_ENABLE_OFB == 1) || \
    (defined(TC_AES_ENABLE_GCM) && TC_AES_ENABLE_GCM == 1) || (defined(TC_AES_ENABLE_CCM) && TC_AES_ENABLE_CCM == 1) || \
    (defined(TC_AES_ENABLE_EAX) && TC_AES_ENABLE_EAX == 1) || \
    (defined(TC_AES_ENABLE_EAX_PRIME) && TC_AES_ENABLE_EAX_PRIME == 1) || \
    (defined(TC_AES_ENABLE_SIV) && TC_AES_ENABLE_SIV == 1) || \
    (defined(TC_AES_ENABLE_CMAC) && TC_AES_ENABLE_CMAC == 1) || \
    (defined(TC_AES_CAVP) && TC_AES_CAVP == 1) || TC_AES_ENABLE_DYNAMIC

/* Add the selected round key to the state. */
static void tc_aes_add_round_key(uint8_t round, state_t* state, const uint8_t* round_key)
{
  uint8_t i,j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[i][j] ^= round_key[(round * Nb * 4) + (i * Nb) + j];
    }
  }
}

static void tc_aes_sub_bytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = tc_aes_sbox_value((*state)[j][i]);
    }
  }
}

static void tc_aes_shift_rows(state_t* state)
{
  uint8_t temp;

  /* Rotate rows left by their row index. */
  temp           = (*state)[0][1];
  (*state)[0][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[3][1];
  (*state)[3][1] = temp;

  temp           = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp           = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  temp           = (*state)[0][3];
  (*state)[0][3] = (*state)[3][3];
  (*state)[3][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[1][3];
  (*state)[1][3] = temp;
}

static void tc_aes_mix_columns(state_t* state)
{
  uint8_t i;
  uint8_t Tmp, Tm, t;
  for (i = 0; i < 4; ++i)
  {  
    t   = (*state)[i][0];
    Tmp = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3] ;
    Tm  = (*state)[i][0] ^ (*state)[i][1] ; Tm = tc_aes_xtime(Tm);  (*state)[i][0] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][1] ^ (*state)[i][2] ; Tm = tc_aes_xtime(Tm);  (*state)[i][1] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][2] ^ (*state)[i][3] ; Tm = tc_aes_xtime(Tm);  (*state)[i][2] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][3] ^ t ;              Tm = tc_aes_xtime(Tm);  (*state)[i][3] ^= Tm ^ Tmp ;
  }
}

#if (defined(TC_AES_ENABLE_CBC) && TC_AES_ENABLE_CBC == 1) || (defined(TC_AES_ENABLE_ECB) && TC_AES_ENABLE_ECB == 1) || \
    (defined(TC_AES_CAVP) && TC_AES_CAVP == 1) || TC_AES_ENABLE_DYNAMIC
static uint8_t tc_aes_inverse_sbox_value(uint8_t num)
{
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_FAST
  return TC_AES_READ_TABLE(rsbox, num);
#elif TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_CONSTANT_TIME
  const uint8_t affine_inverse =
    (uint8_t)(tc_aes_sbox_rotate_left(num, 1) ^
              tc_aes_sbox_rotate_left(num, 3) ^
              tc_aes_sbox_rotate_left(num, 6) ^ 0x05u);
  return tc_aes_sbox_inverse(affine_inverse);
#else
#if defined(__AVR__) && TC_AVR_PROGMEM
  uint8_t value = 0;
  unsigned i;

  for (i = 0; i < 256; ++i)
  {
    const uint8_t mask = tc_aes_ct_eq_mask_u8((uint8_t)i, num);
    value |= TC_AES_READ_TABLE(rsbox, i) & mask;
  }
#else
  const volatile uint8_t *table = rsbox;
  uint8_t value = 0;
  unsigned i;

  for (i = 0; i < 256; ++i)
  {
    const uint8_t mask = tc_aes_ct_eq_mask_u8((uint8_t)i, num);
    value |= table[i] & mask;
  }
#endif

  return value;
#endif
}

static void tc_aes_inverse_mix_columns(state_t* state)
{
  uint8_t i;
  for (i = 0; i < 4; ++i)
  {
    uint8_t a = (*state)[i][0];
    uint8_t b = (*state)[i][1];
    uint8_t c = (*state)[i][2];
    uint8_t d = (*state)[i][3];
    const uint8_t ac4 = tc_aes_xtime(tc_aes_xtime((uint8_t)(a ^ c)));
    const uint8_t bd4 = tc_aes_xtime(tc_aes_xtime((uint8_t)(b ^ d)));
    uint8_t all;
    uint8_t adjacent;

    /* Pre-whitening by 4*(a^c) and 4*(b^d), then applying the forward
     * transform, is algebraically equal to InvMixColumns. */
    a ^= ac4;
    c ^= ac4;
    b ^= bd4;
    d ^= bd4;
    all = (uint8_t)(a ^ b ^ c ^ d);
    adjacent = tc_aes_xtime((uint8_t)(a ^ b));
    (*state)[i][0] = (uint8_t)(a ^ adjacent ^ all);
    adjacent = tc_aes_xtime((uint8_t)(b ^ c));
    (*state)[i][1] = (uint8_t)(b ^ adjacent ^ all);
    adjacent = tc_aes_xtime((uint8_t)(c ^ d));
    (*state)[i][2] = (uint8_t)(c ^ adjacent ^ all);
    adjacent = tc_aes_xtime((uint8_t)(d ^ a));
    (*state)[i][3] = (uint8_t)(d ^ adjacent ^ all);
  }
}


static void tc_aes_inverse_sub_bytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = tc_aes_inverse_sbox_value((*state)[j][i]);
    }
  }
}

static void tc_aes_inverse_shift_rows(state_t* state)
{
  uint8_t temp;

  /* Rotate rows right by their row index. */
  temp = (*state)[3][1];
  (*state)[3][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[0][1];
  (*state)[0][1] = temp;

  temp = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  temp = (*state)[0][3];
  (*state)[0][3] = (*state)[1][3];
  (*state)[1][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[3][3];
  (*state)[3][3] = temp;
}
#endif

TC_status tc_aes_cipher_rounds(state_t* state, const uint8_t* round_key, uint8_t rounds)
{
  uint8_t round = 0;
#if TC_AES_PLATFORM
  tc_aes_platform_result result = tc_aes_platform_block(round_key, rounds, (uint8_t*)state, 0);
  if (result != TC_AES_PLATFORM_UNSUPPORTED)
    return result == TC_AES_PLATFORM_OK ? TC_OK : TC_ERROR;
#endif

  tc_aes_add_round_key(0, state, round_key);

  for (round = 1; ; ++round)
  {
    tc_aes_sub_bytes(state);
    tc_aes_shift_rows(state);
    if (round == rounds) {
      break;
    }
    tc_aes_mix_columns(state);
    tc_aes_add_round_key(round, state, round_key);
  }
  tc_aes_add_round_key(rounds, state, round_key);
  return TC_OK;
}

TC_status tc_aes_cipher(state_t* state, const uint8_t* round_key)
{ return tc_aes_cipher_rounds(state, round_key, Nr); }
#endif

#if defined(TC_AES_CAVP) && (TC_AES_CAVP == 1)
void TC_AES_CAVP_encrypt_block(const uint8_t* key, uint8_t block[TC_AES_BLOCKLEN])
{
  struct TC_AES_ctx ctx;

  if (TC_AES_init_ctx(&ctx, key) != TC_OK)
    return;
  tc_aes_cipher((state_t*)block, ctx.key.round_key);
#if TC_ZEROIZE
  TC_AES_ctx_clear(&ctx);
#endif
}

#endif

#if (defined(TC_AES_ENABLE_CBC) && TC_AES_ENABLE_CBC == 1) || (defined(TC_AES_ENABLE_ECB) && TC_AES_ENABLE_ECB == 1) || \
    (defined(TC_AES_CAVP) && TC_AES_CAVP == 1) || TC_AES_ENABLE_DYNAMIC
static TC_status tc_aes_inverse_rounds(state_t* state, const uint8_t* round_key, uint8_t rounds)
{
  uint8_t round = 0;
#if TC_AES_PLATFORM
  tc_aes_platform_result result = tc_aes_platform_block(round_key, rounds, (uint8_t*)state, 1);
  if (result != TC_AES_PLATFORM_UNSUPPORTED)
    return result == TC_AES_PLATFORM_OK ? TC_OK : TC_ERROR;
#endif

  tc_aes_add_round_key(rounds, state, round_key);

  for (round = (rounds - 1); ; --round)
  {
    tc_aes_inverse_shift_rows(state);
    tc_aes_inverse_sub_bytes(state);
    tc_aes_add_round_key(round, state, round_key);
    if (round == 0) {
      break;
    }
    tc_aes_inverse_mix_columns(state);
  }
  return TC_OK;
}

#if TC_AES_ENABLE_CBC || TC_AES_ENABLE_ECB || (defined(TC_AES_CAVP) && TC_AES_CAVP == 1)
static TC_status tc_aes_inverse_cipher(state_t* state, const uint8_t* round_key)
{ return tc_aes_inverse_rounds(state, round_key, Nr); }
#endif
#endif

#if defined(TC_AES_CAVP) && (TC_AES_CAVP == 1)
void TC_AES_CAVP_decrypt_block(const uint8_t* key, uint8_t block[TC_AES_BLOCKLEN])
{
  struct TC_AES_ctx ctx;

  if (TC_AES_init_ctx(&ctx, key) != TC_OK)
    return;
  tc_aes_inverse_cipher((state_t*)block, ctx.key.round_key);
#if TC_ZEROIZE
  TC_AES_ctx_clear(&ctx);
#endif
}
#endif

/*****************************************************************************/
/* Public functions:                                                         */
/*****************************************************************************/
#if defined(TC_AES_ENABLE_ECB) && (TC_AES_ENABLE_ECB == 1)


TC_status TC_AES_ECB_encrypt(const struct TC_AES_key_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  return tc_aes_cipher((state_t*)buf, ctx->round_key);
}

TC_status TC_AES_ECB_decrypt(const struct TC_AES_key_ctx* ctx, uint8_t* buf)
{
  if (ctx == NULL || buf == NULL)
    return TC_ERROR;
  return tc_aes_inverse_cipher((state_t*)buf, ctx->round_key);
}


#endif





#if TC_AES_ENABLE_CBC || TC_AES_ENABLE_DYNAMIC
static TC_status tc_aes_cbc_encrypt(const uint8_t* key, uint8_t rounds, uint8_t iv[16],
                               uint8_t* buffer, size_t length)
{
  size_t offset;
  for (offset = 0; offset < length; offset += 16) {
    tc_internal_xor(buffer + offset, iv, 16);
    if (tc_aes_cipher_rounds((state_t*)(buffer + offset), key, rounds) != TC_OK)
      return TC_ERROR;
    memcpy(iv, buffer + offset, 16);
  }
  return TC_OK;
}

static TC_status tc_aes_cbc_decrypt(const uint8_t* key, uint8_t rounds, uint8_t iv[16],
                               uint8_t* buffer, size_t length)
{
  uint8_t previous[16];
  size_t offset;
  TC_status status = TC_OK;
  for (offset = 0; offset < length; offset += 16) {
    memcpy(previous, buffer + offset, 16);
    status = tc_aes_inverse_rounds((state_t*)(buffer + offset), key, rounds);
    if (status != TC_OK) break;
    tc_internal_xor(buffer + offset, iv, 16);
    memcpy(iv, previous, 16);
  }
#if TC_ZEROIZE
  TC_secure_zero(previous, sizeof previous);
#endif
  return status;
}
#endif

#if TC_AES_ENABLE_CBC
TC_status TC_AES_CBC_encrypt(struct TC_AES_ctx* ctx, uint8_t* buffer, size_t length)
{
#if TC_STRICT
  if (!ctx || (!buffer && length)) return TC_ERROR;
#endif
  if (length % 16) return TC_ERROR;
  return tc_aes_cbc_encrypt(ctx->key.round_key, Nr, ctx->iv, buffer, length);
}

TC_status TC_AES_CBC_decrypt(struct TC_AES_ctx* ctx, uint8_t* buffer, size_t length)
{
#if TC_STRICT
  if (!ctx || (!buffer && length)) return TC_ERROR;
#endif
  if (length % 16) return TC_ERROR;
  return tc_aes_cbc_decrypt(ctx->key.round_key, Nr, ctx->iv, buffer, length);
}
#endif /* CBC */



#if defined(TC_AES_ENABLE_CTR) && (TC_AES_ENABLE_CTR == 1)

TC_status TC_AES_CTR_crypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length)
{
  size_t offset = 0;
  size_t available;
  size_t uncached_length;
  size_t blocks_needed;

#if TC_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif
  if (length == 0)
    return TC_OK;

  available = ctx->ctr_pos < TC_AES_BLOCKLEN ?
                  TC_AES_BLOCKLEN - ctx->ctr_pos : 0;
  uncached_length = length > available ? length - available : 0;
  blocks_needed = uncached_length / TC_AES_BLOCKLEN +
                  ((uncached_length % TC_AES_BLOCKLEN) != 0 ? 1u : 0u);
  if (!tc_internal_counter_has_blocks(ctx->iv, TC_AES_BLOCKLEN,
                                      blocks_needed))
    return TC_ERROR;

  while (offset < length && ctx->ctr_pos < TC_AES_BLOCKLEN)
    buf[offset++] ^= ctx->ctr_stream[ctx->ctr_pos++];

  while (length - offset >= TC_AES_BLOCKLEN)
  {
    tc_aes_copy_bytes(ctx->ctr_stream, ctx->iv, TC_AES_BLOCKLEN);
    if (tc_aes_cipher((state_t*)ctx->ctr_stream, ctx->key.round_key) != TC_OK)
      return TC_ERROR;
    tc_internal_increment_be(ctx->iv, TC_AES_BLOCKLEN);
    tc_internal_xor(buf + offset, ctx->ctr_stream, TC_AES_BLOCKLEN);
    offset += TC_AES_BLOCKLEN;
    ctx->ctr_pos = TC_AES_BLOCKLEN;
  }

  if (offset < length)
  {
    tc_aes_copy_bytes(ctx->ctr_stream, ctx->iv, TC_AES_BLOCKLEN);
    if (tc_aes_cipher((state_t*)ctx->ctr_stream, ctx->key.round_key) != TC_OK)
      return TC_ERROR;
    tc_internal_increment_be(ctx->iv, TC_AES_BLOCKLEN);
    ctx->ctr_pos = 0;
    while (offset < length)
      buf[offset++] ^= ctx->ctr_stream[ctx->ctr_pos++];
  }
  return TC_OK;
}

#endif /* CTR */


#if defined(TC_AES_ENABLE_OFB) && (TC_AES_ENABLE_OFB == 1)

TC_status TC_AES_OFB_crypt(struct TC_AES_ctx* ctx, uint8_t* buf, size_t length)
{
  uint8_t pos;

#if TC_STRICT
  if (ctx == NULL || (length != 0 && buf == NULL))
    return TC_ERROR;
#endif

  pos = ctx->ofb_pos;
  while (length-- != 0)
  {
    if (pos == TC_AES_BLOCKLEN)
    {
      if (tc_aes_cipher((state_t*)ctx->iv, ctx->key.round_key) != TC_OK) {
        ctx->ofb_pos = pos;
        return TC_ERROR;
      }
      pos = 0;
    }
    *buf++ ^= ctx->iv[pos++];
  }
  ctx->ofb_pos = pos;
  return TC_OK;
}

#endif /* OFB */

#if TC_AES_ENABLE_DYNAMIC
TC_status TC_AES_dynamic_key_init(TC_AES_dynamic_key* ctx, const uint8_t* key, size_t length)
{
  if (!ctx || !key || (length != 16 && length != 24 && length != 32) ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, key, length)) return TC_ERROR;
#if TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
  if (!sbox_ready) return TC_ERROR;
#endif
  tc_aes_key_expansion(ctx->round_key, key, (unsigned)(length / 4));
  ctx->rounds = (uint8_t)(length / 4 + 6);
  memset(ctx->round_key + 16 * (ctx->rounds + 1), 0,
         sizeof ctx->round_key - 16 * (ctx->rounds + 1));
  return TC_OK;
}

void TC_AES_dynamic_key_clear(TC_AES_dynamic_key* ctx)
{ if (ctx) TC_secure_zero(ctx, sizeof *ctx); }

static int tc_aes_dynamic_key_valid(const TC_AES_dynamic_key* ctx)
{ return ctx && (ctx->rounds == 10 || ctx->rounds == 12 || ctx->rounds == 14); }

TC_status TC_AES_dynamic_encrypt(const TC_AES_dynamic_key* ctx, uint8_t block[16])
{
  if (!tc_aes_dynamic_key_valid(ctx) || !block ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, block, 16)) return TC_ERROR;
  return tc_aes_cipher_rounds((state_t*)block, ctx->round_key, ctx->rounds);
}

TC_status TC_AES_dynamic_decrypt(const TC_AES_dynamic_key* ctx, uint8_t block[16])
{
  if (!tc_aes_dynamic_key_valid(ctx) || !block ||
      !tc_internal_ranges_disjoint(ctx, sizeof *ctx, block, 16)) return TC_ERROR;
  return tc_aes_inverse_rounds((state_t*)block, ctx->round_key, ctx->rounds);
}

static int tc_aes_dynamic_cbc_valid(const TC_AES_dynamic_key* ctx,
    const uint8_t* iv, const uint8_t* buffer, size_t length)
{
  return tc_aes_dynamic_key_valid(ctx) && iv && (buffer || !length) && !(length % 16) &&
    tc_internal_ranges_disjoint(ctx, sizeof *ctx, iv, 16) &&
    tc_internal_ranges_disjoint(ctx, sizeof *ctx, buffer, length) &&
    tc_internal_ranges_disjoint(iv, 16, buffer, length);
}

TC_status TC_AES_dynamic_CBC_encrypt(const TC_AES_dynamic_key* ctx, uint8_t iv[16],
    uint8_t* buffer, size_t length)
{
  if (!tc_aes_dynamic_cbc_valid(ctx, iv, buffer, length)) return TC_ERROR;
  return tc_aes_cbc_encrypt(ctx->round_key, ctx->rounds, iv, buffer, length);
}

TC_status TC_AES_dynamic_CBC_decrypt(const TC_AES_dynamic_key* ctx, uint8_t iv[16],
    uint8_t* buffer, size_t length)
{
  if (!tc_aes_dynamic_cbc_valid(ctx, iv, buffer, length)) return TC_ERROR;
  return tc_aes_cbc_decrypt(ctx->round_key, ctx->rounds, iv, buffer, length);
}
#endif
