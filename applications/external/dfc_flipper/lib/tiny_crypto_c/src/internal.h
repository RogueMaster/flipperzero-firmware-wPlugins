/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Private cross-algorithm helpers. Not installed as part of the public API. */
#ifndef TINY_CRYPTO_INTERNAL_H_
#define TINY_CRYPTO_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
  #include <stdlib.h>
#endif

/* Keep mask selection as arithmetic. Otherwise an optimizer may replace it
 * with a conditional pointer and load only the selected secret operand. */
static inline uint32_t tc_internal_mask_barrier(uint32_t mask)
{
#if defined(__GNUC__) || defined(__clang__)
  __asm__ __volatile__("" : "+r"(mask));
#else
  volatile uint32_t value = mask;
  mask = value;
#endif
  return mask;
}

/* Test whether two byte ranges are disjoint without forming end pointers.
 * Subtraction avoids wrap when a range begins near UINTPTR_MAX. */
static inline int tc_internal_ranges_disjoint(const void* a, size_t a_len,
                                              const void* b, size_t b_len)
{
  const uintptr_t pa = (uintptr_t)a;
  const uintptr_t pb = (uintptr_t)b;

  if (a_len == 0 || b_len == 0)
    return 1;
  if (pa < pb)
    return a_len <= (size_t)(pb - pa);
  return b_len <= (size_t)(pa - pb);
}

static inline void tc_internal_store_be32(uint8_t* dst, uint32_t value)
{
  dst[0] = (uint8_t)(value >> 24);
  dst[1] = (uint8_t)(value >> 16);
  dst[2] = (uint8_t)(value >> 8);
  dst[3] = (uint8_t)value;
}

static inline uint64_t tc_internal_load_be64(const uint8_t* src)
{
#if defined(__GNUC__) || defined(__clang__)
  #if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  uint64_t value;
  memcpy(&value, src, sizeof(value));
  return __builtin_bswap64(value);
  #elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  uint64_t value;
  memcpy(&value, src, sizeof(value));
  return value;
  #else
  return ((uint64_t)src[0] << 56) | ((uint64_t)src[1] << 48) |
         ((uint64_t)src[2] << 40) | ((uint64_t)src[3] << 32) |
         ((uint64_t)src[4] << 24) | ((uint64_t)src[5] << 16) |
         ((uint64_t)src[6] << 8) | (uint64_t)src[7];
  #endif
#elif defined(_MSC_VER)
  uint64_t value;
  memcpy(&value, src, sizeof(value));
  return _byteswap_uint64(value);
#else
  uint64_t value = 0;
  unsigned i;
  for (i = 0; i < 8; ++i)
    value = (value << 8) | src[i];
  return value;
#endif
}

static inline void tc_internal_store_be64(uint8_t* dst, uint64_t value)
{
#if defined(__GNUC__) || defined(__clang__)
  #if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  const uint64_t swapped = __builtin_bswap64(value);
  memcpy(dst, &swapped, sizeof(swapped));
  #elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  memcpy(dst, &value, sizeof(value));
  #else
  unsigned i;
  for (i = 0; i < 8; ++i)
    dst[i] = (uint8_t)(value >> (56u - 8u * i));
  #endif
#elif defined(_MSC_VER)
  const uint64_t swapped = _byteswap_uint64(value);
  memcpy(dst, &swapped, sizeof(swapped));
#else
  unsigned i;
  for (i = 0; i < 8; ++i)
    dst[i] = (uint8_t)(value >> (56u - 8u * i));
#endif
}

static inline void tc_internal_increment_be(uint8_t* counter, size_t length)
{
  while (length != 0)
  {
    --length;
    if (++counter[length] != 0)
      break;
  }
}

/* A zero counter has the full 2^(8*length) block space remaining, which may
 * exceed size_t. Other counters use (2^n - counter) as the available count. */
static inline int tc_internal_counter_has_blocks(const uint8_t* counter,
                                                 size_t length, size_t needed)
{
  uint8_t remaining[16];
  size_t i;
  size_t first_size_byte;
  size_t blocks = 0;
  uint8_t carry = 0;
  uint8_t all_zero = 1;

  if (length > sizeof(remaining))
    return 0;
  for (i = length; i > 0; --i)
  {
    const size_t index = i - 1u;
    const unsigned diff =
        (unsigned)(0u - (unsigned)counter[index] - (unsigned)carry);
    remaining[index] = (uint8_t)diff;
    carry = (uint8_t)(counter[index] != 0 || carry != 0);
    all_zero &= (uint8_t)(counter[index] == 0);
  }
  if (all_zero)
    return 1;

  first_size_byte = length > sizeof(size_t) ? length - sizeof(size_t) : 0;
  for (i = 0; i < first_size_byte; ++i)
    if (remaining[i] != 0)
      return 1;
  for (i = first_size_byte; i < length; ++i)
    blocks = (blocks << 8) | remaining[i];
  return blocks >= needed;
}

static inline void tc_internal_xor(uint8_t* dst, const uint8_t* src,
                                   size_t length)
{
  size_t i;
  for (i = 0; i < length; ++i)
    dst[i] ^= src[i];
}

#endif /* TINY_CRYPTO_INTERNAL_H_ */
