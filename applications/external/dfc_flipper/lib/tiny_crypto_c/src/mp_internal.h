/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_MP_INTERNAL_H_
#define TC_MP_INTERNAL_H_
#include "internal.h"
#include <string.h>

/* Select the limb width before including this private header. */
#if TC_MP_WORD_BITS == 8
typedef uint8_t tc_mp_word;
typedef uint16_t tc_mp_wide;
#elif TC_MP_WORD_BITS == 32
typedef uint32_t tc_mp_word;
typedef uint64_t tc_mp_wide;
#else
#error "TC_MP_WORD_BITS must be 8 or 32"
#endif

static inline void tc_mp_select(tc_mp_word* out, const tc_mp_word* a,
    const tc_mp_word* b, tc_mp_word mask, size_t n)
{
  mask = (tc_mp_word)tc_internal_mask_barrier(mask);
  for (size_t i = 0; i < n; ++i)
    out[i] = (tc_mp_word)((a[i] & mask) | (b[i] & (tc_mp_word)~mask));
}

static inline int tc_mp_equal(const tc_mp_word* a, const tc_mp_word* b, size_t n)
{
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i) difference |= a[i] ^ b[i];
  return difference == 0;
}

static inline void tc_mp_shift_right(tc_mp_word* value, size_t n, tc_mp_word high)
{
  for (size_t i = n; i; --i) {
    tc_mp_word low = value[i - 1];
    value[i - 1] = (tc_mp_word)((low >> 1) | ((tc_mp_wide)high << (TC_MP_WORD_BITS - 1)));
    high = low & 1u;
  }
}

/* Fixed-width unsigned big-endian bytes; length is a multiple of limb width. */
/* Input fits width bytes; width is a whole number of limbs. Buffers are disjoint. */
static inline void tc_mp_from_be_padded(tc_mp_word* out, const uint8_t* bytes,
    size_t length, size_t width)
{
  memset(out,0,width);
  for (size_t i = 0; i < length; ++i)
    out[i / sizeof *out] |= (tc_mp_word)((tc_mp_wide)bytes[length - 1 - i] <<
        (8 * (i % sizeof *out)));
}

static inline void tc_mp_from_be(tc_mp_word* out, const uint8_t* bytes, size_t length)
{
  tc_mp_from_be_padded(out,bytes,length,length);
}

static inline void tc_mp_to_be(uint8_t* out, const tc_mp_word* words, size_t length)
{
  for (size_t i = 0; i < length; ++i)
    out[length - 1 - i] = (uint8_t)(words[i / sizeof *words] >> (8 * (i % sizeof *words)));
}

static inline tc_mp_word tc_mp_subtract(tc_mp_word* out, const tc_mp_word* a,
    const tc_mp_word* b, size_t n)
{
  tc_mp_wide borrow = 0;
  for (size_t i = 0; i < n; ++i) {
    tc_mp_wide value = (tc_mp_wide)a[i] - b[i] - borrow;
    out[i] = (tc_mp_word)value;
    borrow = (value >> TC_MP_WORD_BITS) & 1u;
  }
  return (tc_mp_word)borrow;
}

/* Reduce a value below 2p. scratch has n limbs and is disjoint from inputs/out. */
static inline void tc_mp_reduce(tc_mp_word* out, const tc_mp_word* low,
    tc_mp_word high, const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  tc_mp_word borrow = tc_mp_subtract(scratch,low,p,n);
  tc_mp_word mask = (tc_mp_word)(0u - (unsigned)((high != 0) | (borrow == 0)));
  tc_mp_select(out,scratch,low,mask,n);
}

/* a,b < p. out may equal either input; scratch is separate and has n limbs. */
static inline void tc_mp_add_mod(tc_mp_word* out, const tc_mp_word* a,
    const tc_mp_word* b, const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)a[i] + b[i];
    out[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  tc_mp_reduce(out,out,(tc_mp_word)carry,p,n,scratch);
}

/* Newton iteration doubles the correct inverse bits each round. low is odd. */
static inline tc_mp_word tc_mp_montgomery_factor(tc_mp_word low)
{
  tc_mp_word inverse = 1;
  for (unsigned bits = 1; bits < TC_MP_WORD_BITS; bits *= 2)
    inverse = (tc_mp_word)((tc_mp_wide)inverse *
        (tc_mp_word)(2u - (tc_mp_wide)low * inverse));
  return (tc_mp_word)(0u - inverse);
}

/* Compute R^2 mod p for p > 1. out, p and scratch are disjoint n-limb arrays.
 * n > 0 and n*2*word_bits must fit size_t. Inputs need not fill the top limb. */
static inline void tc_mp_montgomery_r2(tc_mp_word* out, const tc_mp_word* p,
    size_t n, tc_mp_word* scratch)
{
  memset(out,0,n * sizeof *out);
  out[0] = 1;
  for (size_t bit = 0; bit < n * 2 * TC_MP_WORD_BITS; ++bit)
    tc_mp_add_mod(out,out,out,p,n,scratch);
}

/* Multiply two n-limb values into a separate 2n-limb output. */
static inline void tc_mp_multiply(tc_mp_word* out, const tc_mp_word* a,
    const tc_mp_word* b, size_t n)
{
  memset(out,0,2 * n * sizeof *out);
  for (size_t i = 0; i < n; ++i) {
    tc_mp_wide carry = 0;
    for (size_t j = 0; j < n; ++j) {
      tc_mp_wide value = (tc_mp_wide)a[i] * b[j] + out[i + j] + carry;
      out[i + j] = (tc_mp_word)value;
      carry = value >> TC_MP_WORD_BITS;
    }
    out[i + n] = (tc_mp_word)carry;
  }
}

/* Reduce a little-endian limb array modulo p > 1, including even p. out and
 * scratch have n limbs; all arrays are disjoint. Loop bounds use input_words/n. */
static inline void tc_mp_reduce_words(tc_mp_word* out, const tc_mp_word* input,
    size_t input_words, const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  memset(out,0,n * sizeof *out);
  for (size_t i = input_words; i; --i) {
    for (unsigned bit = TC_MP_WORD_BITS; bit; --bit) {
      tc_mp_wide carry = (input[i - 1] >> (bit - 1)) & 1u;
      for (size_t j = 0; j < n; ++j) {
        carry += (tc_mp_wide)out[j] * 2;
        out[j] = (tc_mp_word)carry;
        carry >>= TC_MP_WORD_BITS;
      }
      tc_mp_reduce(out,out,(tc_mp_word)carry,p,n,scratch);
    }
  }
}

/* Odd p, n > 0, a,b < p. n0 = -p[0]^-1 modulo the limb radix.
 * product has 2n+2 limbs; reduced has n. Scratch arrays are mutually disjoint
 * and separate from inputs/out. out may equal a or b. Loop bounds depend on n.
 * Returns a*b/R mod p, where R is the radix raised to n. */
static inline void tc_mp_montgomery(tc_mp_word* out, const tc_mp_word* a,
    const tc_mp_word* b, const tc_mp_word* p, size_t n, tc_mp_word n0,
    tc_mp_word* product, tc_mp_word* reduced)
{
  tc_mp_word* t = product;
  tc_mp_multiply(t,a,b,n);
  t[2 * n] = 0; t[2 * n + 1] = 0;
  for (size_t i = 0; i < n; ++i) {
    tc_mp_word m = (tc_mp_word)((tc_mp_wide)t[i] * n0);
    tc_mp_wide carry = 0;
    for (size_t j = 0; j < n; ++j) {
      tc_mp_wide value = (tc_mp_wide)m * p[j] + t[i + j] + carry;
      t[i + j] = (tc_mp_word)value;
      carry = value >> TC_MP_WORD_BITS;
    }
    for (size_t j = i + n; j <= 2 * n; ++j) {
      carry += t[j];
      t[j] = (tc_mp_word)carry;
      carry >>= TC_MP_WORD_BITS;
    }
  }
  tc_mp_reduce(out,t + n,t[2 * n],p,n,reduced);
}

/* Base and one are Montgomery residues below p. Exponent bytes are big-endian;
 * leading zero bytes are processed too. out, temporary (n limbs), product and
 * reduced are mutually disjoint and separate from every input. Two multiplies
 * per exponent bit; selection does not index memory with exponent bits.
 * RSA callers must supply blinding, fault checks and secret cleanup. */
/* exponent_length <= width; leading zero bytes are supplied without a copy. */
static inline void tc_mp_power_padded(tc_mp_word* out, const tc_mp_word* base,
    const uint8_t* exponent, size_t exponent_length, size_t width, const tc_mp_word* one,
    const tc_mp_word* p, size_t n, tc_mp_word n0, tc_mp_word* temporary,
    tc_mp_word* product, tc_mp_word* reduced)
{
  memcpy(out,one,n * sizeof *out);
  const size_t padding = width - exponent_length;
  for (size_t i = 0; i < width; ++i) {
    const uint8_t value = i < padding ? 0 : exponent[i - padding];
    for (unsigned bit = 8; bit; --bit) {
      tc_mp_word mask = (tc_mp_word)(0u - ((value >> (bit - 1)) & 1u));
      tc_mp_montgomery(out,out,out,p,n,n0,product,reduced);
      tc_mp_montgomery(temporary,out,base,p,n,n0,product,reduced);
      tc_mp_select(out,temporary,out,mask,n);
    }
  }
}

static inline void tc_mp_power(tc_mp_word* out, const tc_mp_word* base,
    const uint8_t* exponent, size_t exponent_length, const tc_mp_word* one,
    const tc_mp_word* p, size_t n, tc_mp_word n0, tc_mp_word* temporary,
    tc_mp_word* product, tc_mp_word* reduced)
{
  tc_mp_power_padded(out,base,exponent,exponent_length,exponent_length,
      one,p,n,n0,temporary,product,reduced);
}
#endif
