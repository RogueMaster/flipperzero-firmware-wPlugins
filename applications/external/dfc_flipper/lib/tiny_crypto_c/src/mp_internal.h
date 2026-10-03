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

static inline void tc_mp_select(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                tc_mp_word mask, size_t n)
{
  mask = (tc_mp_word)tc_internal_mask_barrier(mask);
  for (size_t i = 0; i < n; ++i)
    out[i] = (tc_mp_word)((a[i] & mask) | (b[i] & (tc_mp_word)~mask));
}

static inline int tc_mp_equal(const tc_mp_word* a, const tc_mp_word* b, size_t n)
{
  tc_mp_word difference = 0;
  for (size_t i = 0; i < n; ++i)
    difference |= a[i] ^ b[i];
  return difference == 0;
}

/* All-ones when every limb is zero, otherwise zero. Runs in time that depends
 * only on n. */
static inline tc_mp_word tc_mp_zero_mask(const tc_mp_word* value, size_t n)
{
  tc_mp_word any = 0;
  for (size_t i = 0; i < n; ++i)
    any |= value[i];
  /* (any | -any) has its top bit set exactly when any is nonzero. */
  const tc_mp_word nonzero =
      (tc_mp_word)(((tc_mp_wide)(any | (tc_mp_word)(0u - any)) >> (TC_MP_WORD_BITS - 1)) & 1u);
  return (tc_mp_word)(nonzero - 1u);
}

static inline void tc_mp_shift_right(tc_mp_word* value, size_t n, tc_mp_word high)
{
  for (size_t i = n; i; --i) {
    tc_mp_word low = value[i - 1];
    value[i - 1] = (tc_mp_word)((low >> 1) | ((tc_mp_wide)high << (TC_MP_WORD_BITS - 1)));
    high = low & 1u;
  }
}

/* Fixed-width unsigned big-endian bytes. length is a multiple of limb width. */
/* Input fits width bytes. width is a whole number of limbs. Buffers are disjoint. */
static inline void tc_mp_from_be_padded(tc_mp_word* out, const uint8_t* bytes, size_t length,
                                        size_t width)
{
  memset(out, 0, width);
  for (size_t i = 0; i < length; ++i)
    out[i / sizeof *out] |=
        (tc_mp_word)((tc_mp_wide)bytes[length - 1 - i] << (8 * (i % sizeof *out)));
}

static inline void tc_mp_from_be(tc_mp_word* out, const uint8_t* bytes, size_t length)
{
  tc_mp_from_be_padded(out, bytes, length, length);
}

static inline void tc_mp_to_be(uint8_t* out, const tc_mp_word* words, size_t length)
{
  for (size_t i = 0; i < length; ++i)
    out[length - 1 - i] = (uint8_t)(words[i / sizeof *words] >> (8 * (i % sizeof *words)));
}

static inline tc_mp_word tc_mp_subtract(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                        size_t n)
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
static inline void tc_mp_reduce(tc_mp_word* out, const tc_mp_word* low, tc_mp_word high,
                                const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  tc_mp_word borrow = tc_mp_subtract(scratch, low, p, n);
  tc_mp_word mask = (tc_mp_word)(0u - (unsigned)((high != 0) | (borrow == 0)));
  tc_mp_select(out, scratch, low, mask, n);
}

/* a,b < p. out may equal either input. scratch is separate and has n limbs. */
static inline void tc_mp_add_mod(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                 const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  tc_mp_wide carry = 0;
  for (size_t i = 0; i < n; ++i) {
    carry += (tc_mp_wide)a[i] + b[i];
    out[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  tc_mp_reduce(out, out, (tc_mp_word)carry, p, n, scratch);
}

/* Newton iteration doubles the correct inverse bits each round. low is odd. */
static inline tc_mp_word tc_mp_montgomery_factor(tc_mp_word low)
{
  tc_mp_word inverse = 1;
  for (unsigned bits = 1; bits < TC_MP_WORD_BITS; bits *= 2)
    inverse = (tc_mp_word)((tc_mp_wide)inverse * (tc_mp_word)(2u - (tc_mp_wide)low * inverse));
  return (tc_mp_word)(0u - inverse);
}

/* Compute R^2 mod p for p > 1. out, p and scratch are disjoint n-limb arrays.
 * n > 0 and n*2*word_bits must fit size_t. Inputs need not fill the top limb. */
static inline void tc_mp_montgomery_r2(tc_mp_word* out, const tc_mp_word* p, size_t n,
                                       tc_mp_word* scratch)
{
  memset(out, 0, n * sizeof *out);
  out[0] = 1;
  for (size_t bit = 0; bit < n * 2 * TC_MP_WORD_BITS; ++bit)
    tc_mp_add_mod(out, out, out, p, n, scratch);
}

/* Multiply two n-limb values into a separate 2n-limb output. */
static inline void tc_mp_multiply(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                  size_t n)
{
  memset(out, 0, 2 * n * sizeof *out);
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

/* Divide a little-endian limb array by p > 1, including even p, with one
 * restoring step per input bit. remainder and scratch have n limbs. A non-NULL
 * quotient has input_words limbs. All arrays are disjoint. Loop bounds and
 * memory access depend only on input_words and n. */
static inline void tc_mp_divide_words(tc_mp_word* quotient, tc_mp_word* remainder,
                                      const tc_mp_word* input, size_t input_words,
                                      const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  memset(remainder, 0, n * sizeof *remainder);
  if (quotient)
    memset(quotient, 0, input_words * sizeof *quotient);
  for (size_t i = input_words; i; --i) {
    for (unsigned bit = TC_MP_WORD_BITS; bit; --bit) {
      tc_mp_wide carry = (input[i - 1] >> (bit - 1)) & 1u;
      for (size_t j = 0; j < n; ++j) {
        carry += (tc_mp_wide)remainder[j] * 2;
        remainder[j] = (tc_mp_word)carry;
        carry >>= TC_MP_WORD_BITS;
      }
      /* remainder < 2p here. Subtract p when the shifted value reaches it. */
      const tc_mp_word borrow = tc_mp_subtract(scratch, remainder, p, n);
      const tc_mp_word take = (tc_mp_word)((tc_mp_word)carry | (tc_mp_word)(borrow ^ 1u)) & 1u;
      tc_mp_select(remainder, scratch, remainder, (tc_mp_word)(0u - take), n);
      if (quotient)
        quotient[i - 1] |= (tc_mp_word)(take << (bit - 1));
    }
  }
}

/* Reduce a little-endian limb array modulo p > 1, including even p. out and
 * scratch have n limbs. All arrays are disjoint. Loop bounds use input_words/n. */
static inline void tc_mp_reduce_words(tc_mp_word* out, const tc_mp_word* input, size_t input_words,
                                      const tc_mp_word* p, size_t n, tc_mp_word* scratch)
{
  tc_mp_divide_words(NULL, out, input, input_words, p, n, scratch);
}

/* One step of bit-serial reduction by a public divisor 1 < d < 2^31: shift
 * bit into remainder < d and subtract d when the result reaches it. Returns
 * the quotient bit. Every step runs the same instructions, so a secret
 * dividend does not reach variable-time division. */
static inline uint32_t tc_mp_mod_u32_step(uint32_t* remainder, unsigned bit, uint32_t divisor)
{
  const uint32_t shifted = (*remainder << 1) | (uint32_t)(bit & 1u);
  /* The top bit of shifted - divisor is clear exactly when no borrow occurs. */
  const uint32_t take = ((shifted - divisor) >> 31) ^ 1u;
  *remainder = shifted - (divisor & (0u - take));
  return take;
}

/* Divide a little-endian limb array by a public divisor 1 < d < 2^31 and
 * return the remainder. quotient may equal input. The running time depends
 * only on n. */
static inline uint32_t tc_mp_divide_u32(tc_mp_word* quotient, const tc_mp_word* input, size_t n,
                                        uint32_t divisor)
{
  uint32_t remainder = 0;
  for (size_t i = n; i; --i) {
    const tc_mp_word word = input[i - 1];
    tc_mp_word q = 0;
    for (unsigned bit = TC_MP_WORD_BITS; bit; --bit) {
      const uint32_t take =
          tc_mp_mod_u32_step(&remainder, (unsigned)((word >> (bit - 1)) & 1u), divisor);
      q = (tc_mp_word)(q | (tc_mp_word)(take << (bit - 1)));
    }
    if (quotient)
      quotient[i - 1] = q;
  }
  return remainder;
}

/* Remainder of a big-endian magnitude modulo a public 1 < d < 2^31, with
 * running time that depends only on length. */
static inline uint32_t tc_mp_mod_u32_be(const uint8_t* value, size_t length, uint32_t divisor)
{
  uint32_t remainder = 0;
  for (size_t i = 0; i < length; ++i)
    for (unsigned bit = 8; bit; --bit)
      /* Only the remainder is needed. The quotient bit is discarded. */
      (void)tc_mp_mod_u32_step(&remainder, (unsigned)(value[i] >> (bit - 1)), divisor);
  return remainder;
}

/* Caller-owned limb scratch with its capacity in limbs. */
typedef struct {
  tc_mp_word* words;
  size_t capacity;
} tc_mp_scratch;

/* An odd modulus p of n limbs for Montgomery arithmetic. n0 = -p[0]^-1
 * modulo the limb radix. product (2n+2 limbs) and reduced (n limbs) are
 * scratch, mutually disjoint and separate from every operand. */
typedef struct {
  const tc_mp_word* p;
  size_t n;
  tc_mp_word n0;
  tc_mp_word* product;
  tc_mp_word* reduced;
} tc_mp_modulus;

/* a*b/R mod p for a, b < p, where R is the radix raised to n. out may equal a
 * or b. Loop bounds depend on n only. */
static inline void tc_mp_montgomery(tc_mp_word* out, const tc_mp_word* a, const tc_mp_word* b,
                                    const tc_mp_modulus* m)
{
  const size_t n = m->n;
  tc_mp_word* t = m->product;
  tc_mp_multiply(t, a, b, n);
  t[2 * n] = 0;
  t[2 * n + 1] = 0;
  for (size_t i = 0; i < n; ++i) {
    tc_mp_word factor = (tc_mp_word)((tc_mp_wide)t[i] * m->n0);
    tc_mp_wide carry = 0;
    for (size_t j = 0; j < n; ++j) {
      tc_mp_wide value = (tc_mp_wide)factor * m->p[j] + t[i + j] + carry;
      t[i + j] = (tc_mp_word)value;
      carry = value >> TC_MP_WORD_BITS;
    }
    for (size_t j = i + n; j <= 2 * n; ++j) {
      carry += t[j];
      t[j] = (tc_mp_word)carry;
      carry >>= TC_MP_WORD_BITS;
    }
  }
  tc_mp_reduce(out, t + n, t[2 * n], m->p, n, m->reduced);
}

/* base^exponent for Montgomery residues base and one below p. exponent is
 * big-endian and at most width bytes; it is read as width bytes with leading
 * zeros, so the work depends only on width. Each bit costs two multiplies,
 * and selection never indexes memory by exponent bits. out and temporary (n
 * limbs) are disjoint from each other, the inputs and the modulus scratch.
 * RSA callers supply blinding, fault checks and secret cleanup. */
static inline void tc_mp_power_padded(tc_mp_word* out, const tc_mp_word* base, TC_bytes exponent,
                                      size_t width, const tc_mp_word* one, const tc_mp_modulus* m,
                                      tc_mp_word* temporary)
{
  memcpy(out, one, m->n * sizeof *out);
  const size_t padding = width - exponent.length;
  for (size_t i = 0; i < width; ++i) {
    const uint8_t value = i < padding ? 0 : exponent.data[i - padding];
    for (unsigned bit = 8; bit; --bit) {
      tc_mp_word mask = (tc_mp_word)(0u - ((value >> (bit - 1)) & 1u));
      tc_mp_montgomery(out, out, out, m);
      tc_mp_montgomery(temporary, out, base, m);
      tc_mp_select(out, temporary, out, mask, m->n);
    }
  }
}

static inline void tc_mp_power(tc_mp_word* out, const tc_mp_word* base, TC_bytes exponent,
                               const tc_mp_word* one, const tc_mp_modulus* m, tc_mp_word* temporary)
{
  tc_mp_power_padded(out, base, exponent, exponent.length, one, m, temporary);
}

/* Public exponents may select multiplies by bit. Secret exponents use
 * tc_mp_power_padded so the work is independent of their bit pattern. */
static inline void tc_mp_power_public(tc_mp_word* out, const tc_mp_word* base, TC_bytes exponent,
                                      const tc_mp_word* one, const tc_mp_modulus* m)
{
  int started = 0;
  memcpy(out, one, m->n * sizeof *out);
  for (size_t i = 0; i < exponent.length; ++i) {
    for (unsigned bit = 8; bit; --bit) {
      const unsigned set = (exponent.data[i] >> (bit - 1)) & 1u;
      if (started)
        tc_mp_montgomery(out, out, out, m);
      if (set) {
        if (started)
          tc_mp_montgomery(out, out, base, m);
        else {
          memcpy(out, base, m->n * sizeof *out);
          started = 1;
        }
      }
    }
  }
}
#endif
