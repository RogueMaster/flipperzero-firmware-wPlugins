/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Private cross-algorithm helpers for library sources. */
#ifndef TINY_CRYPTO_INTERNAL_H_
#define TINY_CRYPTO_INTERNAL_H_

#include <tiny_crypto/common.h>
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
static inline int tc_internal_ranges_disjoint(const void* a, size_t a_len, const void* b,
                                              size_t b_len)
{
  const uintptr_t pa = (uintptr_t)a;
  const uintptr_t pb = (uintptr_t)b;

  if (a_len == 0 || b_len == 0)
    return 1;
  if (pa < pb)
    return a_len <= (size_t)(pb - pa);
  return b_len <= (size_t)(pa - pb);
}

/* Compare a computed MAC with a received tag in constant time and wipe the
 * computed value. A failed computation reports TC_ERROR; a mismatch reports
 * TC_MISMATCH. */
static inline TC_status tc_internal_verify_tag(TC_status computed_status, uint8_t* computed,
                                               size_t computed_size, const uint8_t* tag,
                                               size_t tag_length)
{
  TC_status status = computed_status == TC_OK ? TC_ct_equal((TC_bytes){computed, tag_length},
                                                            (TC_bytes){tag, tag_length})
                                              : TC_ERROR;
  TC_secure_zero(computed, computed_size);
  return status;
}

/* Tag length policy for CCM, EAX, AES-CMAC and DES-CMAC. The default entry
 * points take TC_MIN_TAG_LEN..max_length bytes and the _short_tag entry points
 * take 1..TC_MIN_TAG_LEN - 1, so each valid length has exactly one entry
 * point. A zero-length tag is never accepted because it would compare equal
 * to any MAC. */
static inline int tc_internal_tag_length_allowed(size_t length, size_t max_length, int short_tag)
{
  if (length == 0 || length > max_length)
    return 0;
  return short_tag ? length < TC_MIN_TAG_LEN : length >= TC_MIN_TAG_LEN;
}

/* A span is valid when it has storage or is empty and its address range
 * stays below UINTPTR_MAX. */
static inline int tc_internal_span_valid(const void* data, size_t length)
{
  return (data != NULL || length == 0) && length <= UINTPTR_MAX - (uintptr_t)data;
}

/* Storage plan check for an operation that writes several regions. Every span
 * must be valid, the writable spans pairwise disjoint, and each writable span
 * disjoint from every input. Inputs may overlap one another. Pass the same
 * array as writable with no inputs to require every span to be disjoint. */
static inline int tc_internal_writes_disjoint(const TC_bytes* writable, size_t count,
                                              const TC_bytes* input, size_t inputs)
{
  size_t i, j;
  if ((count && !writable) || (inputs && !input))
    return 0;
  for (j = 0; j < inputs; ++j)
    if (!tc_internal_span_valid(input[j].data, input[j].length))
      return 0;
  for (i = 0; i < count; ++i) {
    if (!tc_internal_span_valid(writable[i].data, writable[i].length))
      return 0;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writable[i].data, writable[i].length, writable[j].data,
                                       writable[j].length))
        return 0;
    for (j = 0; j < inputs; ++j)
      if (!tc_internal_ranges_disjoint(writable[i].data, writable[i].length, input[j].data,
                                       input[j].length))
        return 0;
  }
  return 1;
}

/* An array of count input spans is usable when the array is present or
 * empty, its byte size fits size_t, every span is valid, and the array and
 * every span are disjoint from output. An empty output skips the overlap
 * checks. Callers that write output before reading every part need this, so
 * that writing output cannot change a part or its descriptor. */
static inline int tc_internal_parts_valid(const TC_bytes* parts, size_t count, const void* output,
                                          size_t output_length)
{
  size_t i;
  if ((count != 0 && parts == NULL) || count > SIZE_MAX / sizeof *parts ||
      !tc_internal_ranges_disjoint(parts, count * sizeof *parts, output, output_length))
    return 0;
  for (i = 0; i < count; ++i)
    if (!tc_internal_span_valid(parts[i].data, parts[i].length) ||
        !tc_internal_ranges_disjoint(parts[i].data, parts[i].length, output, output_length))
      return 0;
  return 1;
}

static inline uint32_t tc_internal_load_be32(const uint8_t* src)
{
#if (defined(__GNUC__) || defined(__clang__)) && defined(__BYTE_ORDER__) &&                        \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  uint32_t value;
  memcpy(&value, src, sizeof(value));
  return __builtin_bswap32(value);
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__BYTE_ORDER__) &&                      \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  uint32_t value;
  memcpy(&value, src, sizeof(value));
  return value;
#elif defined(_MSC_VER)
  uint32_t value;
  memcpy(&value, src, sizeof(value));
  return _byteswap_ulong(value);
#else
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 8) |
         (uint32_t)src[3];
#endif
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
  return ((uint64_t)src[0] << 56) | ((uint64_t)src[1] << 48) | ((uint64_t)src[2] << 40) |
         ((uint64_t)src[3] << 32) | ((uint64_t)src[4] << 24) | ((uint64_t)src[5] << 16) |
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

/* Increment a big-endian counter. Returns 1 when it wraps to zero. */
static inline uint8_t tc_internal_increment_be(uint8_t* counter, size_t length)
{
  while (length != 0) {
    --length;
    if (++counter[length] != 0)
      return 0;
  }
  return 1;
}

/* Callers validate position <= block_length before counting new blocks. */
static inline size_t tc_internal_counter_blocks_needed(size_t length, size_t block_length,
                                                       size_t position)
{
  const size_t available = position < block_length ? block_length - position : 0;
  const size_t uncached = length > available ? length - available : 0;
  return uncached / block_length + (uncached % block_length != 0);
}

/* A fresh zero counter has the full 2^(8*length) block space remaining.
 * That count fits size_t only when length < sizeof(size_t). Other counters use
 * (2^n - counter) as the available count. exhausted is set once the counter
 * has wrapped, so a zero counter left by a wrap has no blocks remaining. */
static inline int tc_internal_counter_has_blocks(const uint8_t* counter, size_t length,
                                                 size_t needed, uint8_t exhausted)
{
  if (exhausted)
    return needed == 0;
  uint8_t remaining[16];
  size_t i;
  size_t first_size_byte;
  size_t blocks = 0;
  uint8_t carry = 0;
  uint8_t all_zero = 1;

  if (length > sizeof(remaining))
    return 0;
  for (i = length; i > 0; --i) {
    const size_t index = i - 1u;
    const unsigned diff = (unsigned)(0u - (unsigned)counter[index] - (unsigned)carry);
    remaining[index] = (uint8_t)diff;
    carry = (uint8_t)(counter[index] != 0 || carry != 0);
    all_zero = (uint8_t)(all_zero & (counter[index] == 0));
  }
  if (all_zero)
    return length >= sizeof(size_t) || needed <= ((size_t)1 << (8u * length));

  first_size_byte = length > sizeof(size_t) ? length - sizeof(size_t) : 0;
  for (i = 0; i < first_size_byte; ++i)
    if (remaining[i] != 0)
      return 1;
  for (i = first_size_byte; i < length; ++i)
    blocks = (blocks << 8) | remaining[i];
  return blocks >= needed;
}

static inline void tc_internal_xor(uint8_t* dst, const uint8_t* src, size_t length)
{
  size_t i;
  for (i = 0; i < length; ++i)
    dst[i] ^= src[i];
}

/* 1 when year, month and day name a Gregorian calendar date with a nonzero
 * year. */
static inline int tc_internal_calendar_date(unsigned year, unsigned month, unsigned day)
{
  unsigned days;
  if (!year || month < 1 || month > 12 || !day)
    return 0;
  if (month == 2)
    days = 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  else
    days = (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
  return day <= days;
}

#endif /* TINY_CRYPTO_INTERNAL_H_ */
