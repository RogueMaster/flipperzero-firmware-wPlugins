/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * AES key wrap KW and KWP (NIST SP 800-38F section 6, RFC 3394, RFC 5649). */
#include <tiny_crypto/aes_kw.h>
#include "aes_internal.h"

#if TC_AES_ENABLE_KW
/* Semiblock size in bytes: half of the 128-bit AES block (section 5.1). */
enum { TC_AES_KW_SEMIBLOCK = 8 };

/* Expanded KEK. The build policy of TC_AES_KW_KEK_LENGTH_SUPPORTED selects
 * the fixed or the length-aware schedule. */
#if TC_AES_ENABLE_DYNAMIC
typedef TC_AES_dynamic_key tc_aes_kw_kek;
#else
typedef struct TC_AES_key_ctx tc_aes_kw_kek;
#endif

/* Expand kek into schedule and describe it for the block cipher descriptor.
 * Fails only in the runtime S-box profile before TC_AES_init_sbox. */
static TC_status tc_aes_kw_kek_init(tc_aes_kw_kek* schedule, TC_bytes kek,
                                    tc_aes_block_key* block_key)
{
#if TC_AES_ENABLE_DYNAMIC
  if (TC_AES_dynamic_key_init(schedule, (TC_bytes){kek.data, kek.length}) != TC_OK)
    return TC_ERROR;
  block_key->rounds = schedule->rounds;
#else
  if (TC_AES_key_init(schedule, (TC_bytes){kek.data, TC_AES_KEYLEN}) != TC_OK)
    return TC_ERROR;
  block_key->rounds = TC_AES_FIXED_ROUNDS;
#endif
  block_key->round_key = schedule->round_key;
  return TC_OK;
}

/* A = A xor [t]64 (section 6.1 Algorithm 1 step 2). t <= 6n, and an accepted
 * wrapped length of 8(n + 1) bytes fits size_t, so t fits size_t and only the
 * low sizeof(size_t) bytes of A change. */
static void tc_aes_kw_xor_step(uint8_t a[TC_AES_KW_SEMIBLOCK], size_t t)
{
  unsigned k;
  for (k = 0; k < sizeof(size_t) && k < TC_AES_KW_SEMIBLOCK; ++k)
    a[TC_AES_KW_SEMIBLOCK - 1u - k] ^= (uint8_t)(t >> (8u * k));
}

/* Encrypt the semiblock string S in place: W (section 6.1 Algorithm 1) in
 * the indexed form of RFC 3394 section 2.2.1, or one block encryption for
 * two semiblocks (section 6.3 Algorithm 5 step 5). Stops at the first failed
 * block. */
static TC_status tc_aes_kw_seal(const tc_block_cipher* cipher, uint8_t* s, size_t length)
{
  const size_t n = length / TC_AES_KW_SEMIBLOCK - 1u;
  uint8_t block[TC_AES_BLOCKLEN];
  TC_status status = TC_OK;
  size_t t = 1, i;
  unsigned j;
  if (length == TC_AES_BLOCKLEN)
    return cipher->encrypt(cipher->key, s);
  /* s = 6(n - 1) steps. A is s[0..8] and R[i] is s[8i..8i + 8]. */
  for (j = 0; j < 6u && status == TC_OK; ++j) {
    for (i = 1; i <= n; ++i, ++t) {
      uint8_t* r = s + TC_AES_KW_SEMIBLOCK * i;
      memcpy(block, s, TC_AES_KW_SEMIBLOCK);
      memcpy(block + TC_AES_KW_SEMIBLOCK, r, TC_AES_KW_SEMIBLOCK);
      status = cipher->encrypt(cipher->key, block);
      if (status != TC_OK)
        break;
      memcpy(s, block, TC_AES_KW_SEMIBLOCK);
      tc_aes_kw_xor_step(s, t);
      memcpy(r, block + TC_AES_KW_SEMIBLOCK, TC_AES_KW_SEMIBLOCK);
    }
  }
  TC_secure_zero(block, sizeof block);
  return status;
}

/* Decrypt c into the header a and the working area r: W^-1 (section 6.1
 * Algorithm 2) in the indexed form of RFC 3394 section 2.2.2, or one block
 * decryption for two semiblocks (section 6.3 Algorithm 6 step 3). The header
 * and then the remaining semiblocks are copied out of c before the first
 * block operation, so c and r may overlap. Stops at the first failed block. */
static TC_status tc_aes_kw_open(const tc_block_cipher* cipher, const uint8_t* c, size_t length,
                                uint8_t a[TC_AES_KW_SEMIBLOCK], uint8_t* r)
{
  const size_t n = length / TC_AES_KW_SEMIBLOCK - 1u;
  uint8_t block[TC_AES_BLOCKLEN];
  TC_status status = TC_OK;
  size_t t = 6u * n, i;
  unsigned j;
  memcpy(a, c, TC_AES_KW_SEMIBLOCK);
  memmove(r, c + TC_AES_KW_SEMIBLOCK, length - TC_AES_KW_SEMIBLOCK);
  if (length == TC_AES_BLOCKLEN) {
    memcpy(block, a, TC_AES_KW_SEMIBLOCK);
    memcpy(block + TC_AES_KW_SEMIBLOCK, r, TC_AES_KW_SEMIBLOCK);
    status = cipher->decrypt(cipher->key, block);
    if (status == TC_OK) {
      memcpy(a, block, TC_AES_KW_SEMIBLOCK);
      memcpy(r, block + TC_AES_KW_SEMIBLOCK, TC_AES_KW_SEMIBLOCK);
    }
    TC_secure_zero(block, sizeof block);
    return status;
  }
  /* t runs from 6n down to 1. R[i] is r[8(i - 1)..8i]. */
  for (j = 0; j < 6u && status == TC_OK; ++j) {
    for (i = n; i >= 1u; --i, --t) {
      uint8_t* ri = r + TC_AES_KW_SEMIBLOCK * (i - 1u);
      tc_aes_kw_xor_step(a, t);
      memcpy(block, a, TC_AES_KW_SEMIBLOCK);
      memcpy(block + TC_AES_KW_SEMIBLOCK, ri, TC_AES_KW_SEMIBLOCK);
      status = cipher->decrypt(cipher->key, block);
      if (status != TC_OK)
        break;
      memcpy(a, block, TC_AES_KW_SEMIBLOCK);
      memcpy(ri, block + TC_AES_KW_SEMIBLOCK, TC_AES_KW_SEMIBLOCK);
    }
  }
  TC_secure_zero(block, sizeof block);
  return status;
}

/* KWP-AD checks (section 6.3 Algorithm 6 steps 4 to 8, RFC 5649 section 3)
 * in constant time: MSB32(A) = ICV2, 8(n-2) < MLI <= 8(n-1) and zero
 * padding. padded_length = 8(n-1) <= 2^32 - 8. The three checks feed one
 * accumulator and one final branch. */
static TC_status tc_aes_kwp_check(const uint8_t a[TC_AES_KW_SEMIBLOCK], const uint8_t* padded,
                                  uint32_t padded_length, size_t* length)
{
  const uint32_t mli = tc_internal_load_be32(a + 4);
  /* 0..7 exactly when MLI is in range, computed mod 2^32. */
  const uint32_t pad = padded_length - mli;
  const uint8_t* last = padded + padded_length - TC_AES_KW_SEMIBLOCK;
  uint32_t bad = (uint32_t)((a[0] ^ 0xa6u) | (a[1] ^ 0x59u) | (a[2] ^ 0x59u) | (a[3] ^ 0xa6u));
  unsigned k;
  bad |= pad >> 3; /* MLI out of range, including 0 and above padded_length. */
  for (k = 0; k < TC_AES_KW_SEMIBLOCK; ++k) {
    /* Byte k of the last semiblock is padding when k + pad >= 8. */
    const uint32_t is_pad = 0u - (((uint32_t)k + (pad & 7u)) >> 3);
    bad |= last[k] & is_pad;
  }
  if (tc_internal_mask_barrier(bad) != 0)
    return TC_MISMATCH;
  *length = mli;
  return TC_OK;
}

/* KW-AD check (section 6.2 Algorithm 4 step 3): MSB64(S) = ICV1. */
static TC_status tc_aes_kw_check(const uint8_t a[TC_AES_KW_SEMIBLOCK])
{
  uint8_t icv1[TC_AES_KW_SEMIBLOCK];
  memset(icv1, 0xa6, sizeof icv1);
  return TC_ct_equal((TC_bytes){a, sizeof icv1}, (TC_bytes){icv1, sizeof icv1});
}

/* length is a multiple of alignment in [min, max]. Passing max as a
 * uint64_t argument keeps 16 and 32-bit size_t builds free of constant
 * comparisons. */
static int tc_aes_kw_length_ok(size_t length, size_t min, uint64_t max, size_t alignment)
{
  return length >= min && length % alignment == 0 && (uint64_t)length <= max;
}

static int tc_aes_kw_kek_ok(TC_bytes kek)
{
  return kek.data != NULL && TC_AES_KW_KEK_LENGTH_SUPPORTED(kek.length);
}

/* Wrap arguments. KW takes 16.. semiblock-aligned key data (Table 1) and
 * KWP any nonempty length up to the section 5.3.2 restriction. The wrapped
 * size must also fit size_t. */
static int tc_aes_kw_wrap_args_ok(TC_bytes kek, TC_bytes key_data, TC_buffer wrapped, int padded)
{
  size_t wrapped_length;
  if (!tc_aes_kw_kek_ok(kek) || key_data.data == NULL || wrapped.data == NULL)
    return 0;
  if (padded) {
    if (!tc_aes_kw_length_ok(key_data.length, 1u, TC_AES_KWP_MAX_KEY_DATA_BYTES, 1u) ||
        key_data.length > SIZE_MAX - 15u)
      return 0;
    wrapped_length = TC_AES_KWP_WRAPPED_BYTES(key_data.length);
  } else {
    if (!tc_aes_kw_length_ok(key_data.length, 2u * TC_AES_KW_SEMIBLOCK,
                             TC_AES_KW_MAX_KEY_DATA_BYTES, TC_AES_KW_SEMIBLOCK) ||
        key_data.length > SIZE_MAX - TC_AES_KW_SEMIBLOCK)
      return 0;
    wrapped_length = TC_AES_KW_WRAPPED_BYTES(key_data.length);
  }
  return wrapped.capacity >= wrapped_length;
}

/* Unwrap arguments. KW-AD takes 3.. semiblocks and KWP-AD 2.. semiblocks
 * (sections 5.2 and 5.3.1). */
static int tc_aes_kw_unwrap_args_ok(TC_bytes kek, TC_bytes wrapped, TC_buffer key_data, int padded)
{
  const size_t min = (padded ? 2u : 3u) * TC_AES_KW_SEMIBLOCK;
  const uint64_t max = padded ? TC_AES_KWP_MAX_WRAPPED_BYTES : TC_AES_KW_MAX_WRAPPED_BYTES;
  return tc_aes_kw_kek_ok(kek) && wrapped.data != NULL && key_data.data != NULL &&
         tc_aes_kw_length_ok(wrapped.length, min, max, TC_AES_KW_SEMIBLOCK) &&
         key_data.capacity >= wrapped.length - TC_AES_KW_SEMIBLOCK;
}

/* KW-AE (Algorithm 3) and KWP-AE (Algorithm 5) on validated arguments. The
 * key data moves into place first, so any overlap with out is safe. A cipher
 * failure wipes out. */
static TC_status tc_aes_kw_wrap(TC_bytes kek, TC_bytes key_data, uint8_t* out, int padded)
{
  static const uint8_t icv2[4] = {0xa6, 0x59, 0x59, 0xa6};
  const size_t length = key_data.length;
  const size_t wrapped_length =
      padded ? TC_AES_KWP_WRAPPED_BYTES(length) : TC_AES_KW_WRAPPED_BYTES(length);
  tc_aes_kw_kek schedule;
  tc_aes_block_key block_key;
  TC_status status = tc_aes_kw_kek_init(&schedule, kek, &block_key);
  if (status == TC_OK) {
    const tc_block_cipher cipher = tc_aes_block_cipher(&block_key);
    memmove(out + TC_AES_KW_SEMIBLOCK, key_data.data, length);
    if (padded) {
      /* S = ICV2 || [len(P)]32 || P || 0^padlen. */
      memset(out + TC_AES_KW_SEMIBLOCK + length, 0, wrapped_length - TC_AES_KW_SEMIBLOCK - length);
      memcpy(out, icv2, sizeof icv2);
      tc_internal_store_be32(out + 4, (uint32_t)length);
    } else {
      /* S = ICV1 || P. */
      memset(out, 0xa6, TC_AES_KW_SEMIBLOCK);
    }
    status = tc_aes_kw_seal(&cipher, out, wrapped_length);
    if (status != TC_OK)
      TC_secure_zero(out, wrapped_length);
  }
  TC_secure_zero(&schedule, sizeof schedule);
  return status;
}

/* KW-AD (Algorithm 4) and KWP-AD (Algorithm 6) on validated arguments. The
 * integrity check runs only after W^-1 succeeds, so a cipher failure stays
 * TC_ERROR. Every failure wipes the working area. key_data_length is written
 * last, for KWP on success only. */
static TC_status tc_aes_kw_unwrap(TC_bytes kek, TC_bytes wrapped, uint8_t* key_data,
                                  size_t* key_data_length)
{
  const size_t area = wrapped.length - TC_AES_KW_SEMIBLOCK;
  uint8_t a[TC_AES_KW_SEMIBLOCK];
  tc_aes_kw_kek schedule;
  tc_aes_block_key block_key;
  size_t length = area;
  TC_status status = tc_aes_kw_kek_init(&schedule, kek, &block_key);
  if (status == TC_OK) {
    const tc_block_cipher cipher = tc_aes_block_cipher_inverse(&block_key);
    status = tc_aes_kw_open(&cipher, wrapped.data, wrapped.length, a, key_data);
    if (status == TC_OK)
      status = key_data_length ? tc_aes_kwp_check(a, key_data, (uint32_t)area, &length)
                               : tc_aes_kw_check(a);
    if (status != TC_OK)
      TC_secure_zero(key_data, area);
    else if (key_data_length)
      *key_data_length = length;
  }
  TC_secure_zero(a, sizeof a);
  TC_secure_zero(&schedule, sizeof schedule);
  return status;
}

TC_status TC_AES_KW_wrap(TC_bytes kek, TC_bytes key_data, TC_buffer wrapped)
{
  if (!tc_aes_kw_wrap_args_ok(kek, key_data, wrapped, 0))
    return TC_ERROR;
  return tc_aes_kw_wrap(kek, key_data, wrapped.data, 0);
}

TC_status TC_AES_KW_unwrap(TC_bytes kek, TC_bytes wrapped, TC_buffer key_data)
{
  if (!tc_aes_kw_unwrap_args_ok(kek, wrapped, key_data, 0))
    return TC_ERROR;
  return tc_aes_kw_unwrap(kek, wrapped, key_data.data, NULL);
}

TC_status TC_AES_KWP_wrap(TC_bytes kek, TC_bytes key_data, TC_buffer wrapped)
{
  if (!tc_aes_kw_wrap_args_ok(kek, key_data, wrapped, 1))
    return TC_ERROR;
  return tc_aes_kw_wrap(kek, key_data, wrapped.data, 1);
}

TC_status TC_AES_KWP_unwrap(TC_bytes kek, TC_bytes wrapped, TC_buffer key_data,
                            size_t* key_data_length)
{
  if (!tc_aes_kw_unwrap_args_ok(kek, wrapped, key_data, 1) || key_data_length == NULL ||
      !tc_internal_ranges_disjoint(key_data_length, sizeof *key_data_length, key_data.data,
                                   wrapped.length - TC_AES_KW_SEMIBLOCK))
    return TC_ERROR;
  return tc_aes_kw_unwrap(kek, wrapped, key_data.data, key_data_length);
}
#endif
