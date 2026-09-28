/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/config.h>
#include "unicode_internal.h"
#if TC_ENABLE_X509
#include "internal.h"
#include "string_internal.h"

#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define TC_UNICODE_STORAGE PROGMEM
#if defined(__AVR_HAVE_RAMPZ__)
#define U32(t, i) pgm_read_dword_far(pgm_get_far_address(t) + (uint32_t)(i) * 4u)
#define U16(t, i) pgm_read_word_far(pgm_get_far_address(t) + (uint32_t)(i) * 2u)
#define U8(t, i) pgm_read_byte_far(pgm_get_far_address(t) + (uint32_t)(i))
#else
#define U32(t, i) pgm_read_dword(&(t)[i])
#define U16(t, i) pgm_read_word(&(t)[i])
#define U8(t, i) pgm_read_byte(&(t)[i])
#endif
#else
#define TC_UNICODE_STORAGE
#define U32(t, i) ((t)[i])
#define U16(t, i) ((t)[i])
#define U8(t, i) ((t)[i])
#endif
#include "unicode_data.inc"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static TC_TLV_result storage_arguments(const void* input, size_t bytes,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work);

/* Separate searches keep flash addresses visible to AVR's far-read macros. */
#define SEARCH(name, table) \
static size_t name(uint32_t point) \
{ \
  size_t low = 0, high = COUNT(table); \
  while (low < high) { \
    const size_t middle = low + (high - low) / 2; \
    if (U32(table, middle) < point) low = middle + 1; \
    else high = middle; \
  } \
  return low; \
}
SEARCH(class_index, tc_unicode_class_keys)
SEARCH(decomposition_index, tc_unicode_decomposition_keys)
SEARCH(folding_index, tc_unicode_folding_keys)
SEARCH(allowed_index, tc_unicode_allowed_ends)
SEARCH(mark_index, tc_unicode_marks_ends)

int tc_unicode_allowed(uint32_t point)
{
  const size_t index = allowed_index(point);
  return index < COUNT(tc_unicode_allowed_starts) && U32(tc_unicode_allowed_starts, index) <= point;
}

int tc_unicode_mark(uint32_t point)
{
  const size_t index = mark_index(point);
  return index < COUNT(tc_unicode_marks_starts) && U32(tc_unicode_marks_starts, index) <= point;
}

size_t tc_unicode_map(uint32_t point, uint32_t mapped[4])
{
  size_t index, i, count, offset;
  if ((point >= 9 && point <= 13) || point == 0x85 || point == 0x20
      || point == 0xa0 || point == 0x1680 || (point >= 0x2000 && point <= 0x200a)
      || point == 0x2028 || point == 0x2029 || point == 0x202f || point == 0x205f || point == 0x3000) {
    mapped[0] = 0x20;
    return 1;
  }
  /* RFC 4518 erratum 860 corrects the variation-selector range to FE00-FE0F. */
  if (point < 0x20 || (point >= 0x7f && point <= 0x9f) || point == 0xad
      || point == 0x34f || point == 0x6dd || point == 0x70f || point == 0x1806
      || (point >= 0x180b && point <= 0x180e) || (point >= 0x200b && point <= 0x200f)
      || (point >= 0x202a && point <= 0x202e) || (point >= 0x2060 && point <= 0x2063)
      || (point >= 0x206a && point <= 0x206f) || (point >= 0xfe00 && point <= 0xfe0f)
      || point == 0xfeff || (point >= 0xfff9 && point <= 0xfffc)
      || (point >= 0x1d173 && point <= 0x1d17a) || point == 0xe0001
      || (point >= 0xe0020 && point <= 0xe007f)) return 0;
  index = folding_index(point);
  if (index == COUNT(tc_unicode_folding_keys) || U32(tc_unicode_folding_keys, index) != point) {
    mapped[0] = point;
    return 1;
  }
  count = U8(tc_unicode_folding_lengths, index);
  offset = U16(tc_unicode_folding_offsets, index);
  for (i = 0; i < count; ++i) mapped[i] = U32(tc_unicode_folding_pool, offset + i);
  return count;
}

TC_TLV_result tc_unicode_finish_name(uint32_t* buffer, size_t length,
    size_t capacity, size_t* written, size_t* work)
{
  size_t i, compacted = 0, spaces = 0, required, offset;
  uint32_t next = 0;
  int pending_space = 0;
  TC_TLV_result result = storage_arguments(NULL, 0, buffer, capacity, written, work);
  if (result != TC_TLV_OK) return result;
  if (length > capacity) return TC_TLV_ARGUMENT;
  for (i = 0; i < length; ++i) {
    const uint32_t point = buffer[i];
    if (!*work) return TC_TLV_LIMIT;
    --*work;
    if (!tc_unicode_allowed(point)) return TC_TLV_INVALID;
    /* A space carrying a combining mark is part of the name. */
    if (point == 0x20 && (i + 1 == length || !tc_unicode_mark(buffer[i + 1]))) {
      pending_space = 1;
      continue;
    }
    if (pending_space && compacted) {
      buffer[compacted++] = 0x20;
      ++spaces;
    }
    pending_space = 0;
    buffer[compacted++] = point;
  }
  if (capacity < 2 || compacted > capacity - 2 || spaces > capacity - 2 - compacted)
    return TC_TLV_LIMIT;
  required = compacted + spaces + 2;
  /* Expand backwards so the added boundary spaces cannot overwrite input. */
  offset = required;
  if (!*work) return TC_TLV_LIMIT;
  --*work;
  buffer[--offset] = 0x20;
  for (i = compacted; i; --i) {
    const uint32_t point = buffer[i - 1];
    const size_t copies = point == 0x20 && !tc_unicode_mark(next) ? 2 : 1;
    size_t j;
    if (*work < copies) { *work = 0; return TC_TLV_LIMIT; }
    *work -= copies;
    for (j = 0; j < copies; ++j) buffer[--offset] = point;
    next = point;
  }
  if (!*work) return TC_TLV_LIMIT;
  --*work;
  buffer[0] = 0x20;
  *written = required;
  return TC_TLV_OK;
}

static unsigned combining_class(uint32_t point)
{
  const size_t index = class_index(point);
  return index < COUNT(tc_unicode_class_keys) && U32(tc_unicode_class_keys, index) == point
      ? U8(tc_unicode_class_values, index) : 0;
}

static uint32_t composition(uint32_t first, uint32_t second)
{
  size_t low = 0, high = COUNT(tc_unicode_composition_first);
  if (first >= 0x1100 && first < 0x1113 && second >= 0x1161 && second < 0x1176)
    return 0xac00 + (first - 0x1100) * 588 + (second - 0x1161) * 28;
  if (first >= 0xac00 && first < 0xd7a4 && (first - 0xac00) % 28 == 0
      && second > 0x11a7 && second < 0x11c3)
    return first + second - 0x11a7;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const uint32_t a = U32(tc_unicode_composition_first, middle);
    const uint32_t b = U32(tc_unicode_composition_second, middle);
    if (a < first || (a == first && b < second)) low = middle + 1;
    else high = middle;
  }
  if (low < COUNT(tc_unicode_composition_first)
      && U32(tc_unicode_composition_first, low) == first
      && U32(tc_unicode_composition_second, low) == second)
    return U32(tc_unicode_composition_values, low);
  return 0;
}

static TC_TLV_result append(uint32_t point, uint32_t* storage, size_t capacity,
                            size_t* length, size_t* work)
{
  size_t offset = *length;
  const unsigned combining = combining_class(point);
  if (!*work || offset == capacity) return TC_TLV_LIMIT;
  --*work;
  if (combining) {
    while (offset && combining_class(storage[offset - 1]) > combining) {
      if (!*work) return TC_TLV_LIMIT;
      --*work;
      storage[offset] = storage[offset - 1];
      --offset;
    }
  }
  storage[offset] = point;
  ++*length;
  return TC_TLV_OK;
}

static TC_TLV_result storage_arguments(const void* input, size_t bytes,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work)
{
  if ((!input && bytes) || (!storage && capacity) || !written || !work
      || capacity > SIZE_MAX / sizeof(*storage))
    return TC_TLV_ARGUMENT;
  if (!tc_internal_ranges_disjoint(input, bytes, storage, capacity * sizeof(*storage))
      || !tc_internal_ranges_disjoint(input, bytes, written, sizeof(*written))
      || !tc_internal_ranges_disjoint(input, bytes, work, sizeof(*work))
      || !tc_internal_ranges_disjoint(storage, capacity * sizeof(*storage), written, sizeof(*written))
      || !tc_internal_ranges_disjoint(storage, capacity * sizeof(*storage), work, sizeof(*work))
      || !tc_internal_ranges_disjoint(written, sizeof(*written), work, sizeof(*work)))
    return TC_TLV_ARGUMENT;
  return TC_TLV_OK;
}

static TC_TLV_result decompose(uint32_t point, uint32_t* storage,
    size_t capacity, size_t* used, size_t* work)
{
  uint32_t hangul[3];
  size_t index, count = 1, offset = 0, j;
  int is_hangul, mapped;
  TC_TLV_result result;
  if (!*work) return TC_TLV_LIMIT;
  --*work;
  if (point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return TC_TLV_INVALID;
  is_hangul = point >= 0xac00 && point < 0xd7a4;
  index = decomposition_index(point);
  mapped = index < COUNT(tc_unicode_decomposition_keys)
      && U32(tc_unicode_decomposition_keys, index) == point;
  if (is_hangul) {
    const uint32_t syllable = point - 0xac00;
    hangul[0] = 0x1100 + syllable / 588;
    hangul[1] = 0x1161 + (syllable % 588) / 28;
    hangul[2] = 0x11a7 + syllable % 28;
    count = syllable % 28 ? 3 : 2;
  } else if (mapped) {
    count = U8(tc_unicode_decomposition_lengths, index);
    offset = U16(tc_unicode_decomposition_offsets, index);
  }
  for (j = 0; j < count; ++j) {
    const uint32_t child = is_hangul ? hangul[j] : mapped
        ? U32(tc_unicode_decomposition_pool, offset + j) : point;
    result = append(child, storage, capacity, used, work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

static TC_TLV_result recompose(uint32_t* storage, size_t used, size_t* written, size_t* work)
{
  size_t i, output = 0, starter = SIZE_MAX;
  unsigned previous_class = 0;
  for (i = 0; i < used; ++i) {
    const uint32_t point = storage[i];
    const unsigned combining = combining_class(point);
    uint32_t composite = 0;
    if (!*work) return TC_TLV_LIMIT;
    --*work;
    if (starter != SIZE_MAX && (!previous_class || previous_class < combining))
      composite = composition(storage[starter], point);
    if (composite) storage[starter] = composite;
    else {
      if (!combining) starter = output;
      storage[output++] = point;
      previous_class = combining;
    }
  }
  *written = output;
  return TC_TLV_OK;
}

TC_TLV_result tc_unicode_nfkc(const uint32_t* input, size_t length,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work)
{
  size_t i, used = 0;
  TC_TLV_result result;
  if (length > SIZE_MAX / sizeof(*input)) return TC_TLV_ARGUMENT;
  result = storage_arguments(input, length * sizeof(*input), storage, capacity, written, work);
  if (result != TC_TLV_OK) return result;
  for (i = 0; i < length; ++i) {
    result = decompose(input[i], storage, capacity, &used, work);
    if (result != TC_TLV_OK) return result;
  }
  return recompose(storage, used, written, work);
}

TC_TLV_result tc_unicode_prepare(unsigned tag, TC_bytes input,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work)
{
  size_t offset = 0, used = 0;
  TC_TLV_result result = storage_arguments(input.data, input.length, storage, capacity, written, work);
  if (result != TC_TLV_OK) return result;
  for (;;) {
    uint32_t point;
    if (offset < input.length) {
      if (!*work) return TC_TLV_LIMIT;
      --*work;
    }
    result = tc_asn1_string_next(tag, input, &offset, &point);
    if (result == TC_TLV_END) break;
    if (result != TC_TLV_OK) return result;
    result = tc_unicode_prepare_point(point,storage,capacity,&used,work);
    if (result != TC_TLV_OK) return result;
  }
  return tc_unicode_prepare_finish(storage,used,capacity,written,work);
}

TC_TLV_result tc_unicode_prepare_point(uint32_t point, uint32_t* storage,
    size_t capacity, size_t* used, size_t* work)
{
  uint32_t mapped[4];
  const size_t count = tc_unicode_map(point,mapped);
  size_t i;
  for (i = 0; i < count; ++i) {
    TC_TLV_result result = decompose(mapped[i],storage,capacity,used,work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_unicode_prepare_finish(uint32_t* storage, size_t used,
    size_t capacity, size_t* written, size_t* work)
{
  size_t normalized, prepared;
  TC_TLV_result result = recompose(storage,used,&normalized,work);
  if (result != TC_TLV_OK) return result;
  result = tc_unicode_finish_name(storage, normalized, capacity, &prepared, work);
  if (result == TC_TLV_OK) *written = prepared;
  return result;
}
#endif
