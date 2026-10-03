/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/ec.h>
#include "internal.h"
#if TC_ENABLE_EC
#include <tiny_crypto/hash.h>
#include "hash_core_internal.h"
#define TC_MP_WORD_BITS TC_EC_WORD_BITS
#include "mp_internal.h"
#include "mp_inverse_internal.h"
#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define EC_STORAGE PROGMEM
#define EC_BYTE(p) pgm_read_byte(p)
#else
#define EC_STORAGE
#define EC_BYTE(p) (*(p))
#endif
typedef TC_EC_word word;
typedef struct {
  TC_EC_workspace* w;
  size_t words, bytes;
  word order_factor;
} ec_state;

/* SEC 2 v2.0 parameters. Rows are p, n, b, R^2 mod p, GxR mod p, GyR mod p,
 * with R = 2^(8 * coordinate_bytes). These curves have a = -3, h = 1. */
#if TC_EC_ENABLE_P192
static const uint8_t params_192[6][24] EC_STORAGE = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0x99, 0xde, 0xf8, 0x36, 0x14, 0x6b, 0xc9, 0xb1, 0xb4, 0xd2, 0x28, 0x31},
    {0x64, 0x21, 0x05, 0x19, 0xe5, 0x9c, 0x80, 0xe7, 0x0f, 0xa7, 0xe9, 0xab,
     0x72, 0x24, 0x30, 0x49, 0xfe, 0xb8, 0xde, 0xec, 0xc1, 0x46, 0xb9, 0xb1},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
    {0x95, 0x4c, 0xc8, 0xf9, 0xf3, 0xd2, 0x18, 0xf7, 0x8a, 0x4b, 0xd3, 0xf7,
     0x76, 0xd1, 0x29, 0x09, 0x0d, 0x8c, 0xb3, 0x0c, 0x33, 0x2f, 0xa1, 0x08},
    {0x6a, 0x29, 0x3d, 0x83, 0x6a, 0xed, 0xa8, 0x4d, 0xde, 0x22, 0xb5, 0x24,
     0x89, 0x66, 0xf0, 0x5e, 0x7b, 0x12, 0xa3, 0x37, 0x1e, 0x42, 0x22, 0x89}};
/* R² modulo the group order, with R = 2^192. */
static const uint8_t order_r2_192[24] EC_STORAGE = {0x28, 0xbe, 0x56, 0x77, 0xea, 0x05, 0x81, 0xa2,
                                                    0x46, 0x96, 0xea, 0x5b, 0xbb, 0x3a, 0x6b, 0xee,
                                                    0xce, 0x66, 0xba, 0xcc, 0xde, 0xb3, 0x59, 0x61};
#endif
#if TC_EC_ENABLE_P256
static const uint8_t params_256[6][32] EC_STORAGE = {
    {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
     0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51},
    {0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd,
     0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53,
     0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b},
    {0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff,
     0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03},
    {0x18, 0x90, 0x5f, 0x76, 0xa5, 0x37, 0x55, 0xc6, 0x79, 0xfb, 0x73,
     0x2b, 0x77, 0x62, 0x25, 0x10, 0x75, 0xba, 0x95, 0xfc, 0x5f, 0xed,
     0xb6, 0x01, 0x79, 0xe7, 0x30, 0xd4, 0x18, 0xa9, 0x14, 0x3c},
    {0x85, 0x71, 0xff, 0x18, 0x25, 0x88, 0x5d, 0x85, 0xd2, 0xe8, 0x86,
     0x88, 0xdd, 0x21, 0xf3, 0x25, 0x8b, 0x4a, 0xb8, 0xe4, 0xba, 0x19,
     0xe4, 0x5c, 0xdd, 0xf2, 0x53, 0x57, 0xce, 0x95, 0x56, 0x0a},
};
static const uint8_t order_r2_256[32] EC_STORAGE = {
    0x66, 0xe1, 0x2d, 0x94, 0xf3, 0xd9, 0x56, 0x20, 0x28, 0x45, 0xb2, 0x39, 0x2b, 0x6b, 0xec, 0x59,
    0x46, 0x99, 0x79, 0x9c, 0x49, 0xbd, 0x6f, 0xa6, 0x83, 0x24, 0x4c, 0x95, 0xbe, 0x79, 0xee, 0xa2};
#endif
#if TC_EC_ENABLE_P384
static const uint8_t params_384[6][48] EC_STORAGE = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xc7, 0x63, 0x4d, 0x81, 0xf4, 0x37, 0x2d, 0xdf, 0x58, 0x1a, 0x0d, 0xb2,
     0x48, 0xb0, 0xa7, 0x7a, 0xec, 0xec, 0x19, 0x6a, 0xcc, 0xc5, 0x29, 0x73},
    {0xb3, 0x31, 0x2f, 0xa7, 0xe2, 0x3e, 0xe7, 0xe4, 0x98, 0x8e, 0x05, 0x6b,
     0xe3, 0xf8, 0x2d, 0x19, 0x18, 0x1d, 0x9c, 0x6e, 0xfe, 0x81, 0x41, 0x12,
     0x03, 0x14, 0x08, 0x8f, 0x50, 0x13, 0x87, 0x5a, 0xc6, 0x56, 0x39, 0x8d,
     0x8a, 0x2e, 0xd1, 0x9d, 0x2a, 0x85, 0xc8, 0xed, 0xd3, 0xec, 0x2a, 0xef},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
     0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
     0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x01},
    {0x4d, 0x3a, 0xad, 0xc2, 0x29, 0x9e, 0x15, 0x13, 0x81, 0x2f, 0xf7, 0x23,
     0x61, 0x4e, 0xde, 0x2b, 0x64, 0x54, 0x86, 0x84, 0x59, 0xa3, 0x0e, 0xff,
     0x87, 0x9c, 0x3a, 0xfc, 0x54, 0x1b, 0x4d, 0x6e, 0x20, 0xe3, 0x78, 0xe2,
     0xa0, 0xd6, 0xce, 0x38, 0x3d, 0xd0, 0x75, 0x66, 0x49, 0xc0, 0xb5, 0x28},
    {0x2b, 0x78, 0xab, 0xc2, 0x5a, 0x15, 0xc5, 0xe9, 0xdd, 0x80, 0x02, 0x26,
     0x39, 0x69, 0xa8, 0x40, 0xc6, 0xc3, 0x52, 0x19, 0x68, 0xf4, 0xff, 0xd9,
     0x8b, 0xad, 0xe7, 0x56, 0x2e, 0x83, 0xb0, 0x50, 0xa1, 0xbf, 0xa8, 0xbf,
     0x7b, 0xb4, 0xa9, 0xac, 0x23, 0x04, 0x3d, 0xad, 0x4b, 0x03, 0xa4, 0xfe},
};
static const uint8_t order_r2_384[48] EC_STORAGE = {
    0x0c, 0x84, 0xee, 0x01, 0x2b, 0x39, 0xbf, 0x21, 0x3f, 0xb0, 0x5b, 0x7a, 0x28, 0x26, 0x68, 0x95,
    0xd4, 0x0d, 0x49, 0x17, 0x4a, 0xab, 0x1c, 0xc5, 0xbc, 0x3e, 0x48, 0x3a, 0xfc, 0xb8, 0x29, 0x47,
    0xff, 0x3d, 0x81, 0xe5, 0xdf, 0x1a, 0xa4, 0x19, 0x2d, 0x31, 0x9b, 0x24, 0x19, 0xb4, 0x09, 0xa9};
#endif

enum { EC_P = 24, EC_N, EC_B, EC_R2, EC_ONE, EC_SCALAR };
#define F(s, i) ((s)->w->fields[(i)])
#define T(s, i) F(s, 12 + (i))

static void add(ec_state* s, word* out, const word* a, const word* b)
{
  tc_mp_add_mod(out, a, b, F(s, EC_P), s->words, s->w->reduced);
}

static void sub(ec_state* s, word* out, const word* a, const word* b)
{
  tc_mp_sub_mod(out, a, b, F(s, EC_P), s->words);
}

/* Montgomery multiplication. The low word of either prime is -1, so
 * -p^-1 mod 2^word_bits is 1. Carry propagation has a fixed loop bound. */
static void mul(ec_state* s, word* out, const word* a, const word* b)
{
  const tc_mp_modulus field = {F(s, EC_P), s->words, 1, s->w->product, s->w->reduced};
  tc_mp_montgomery(out, a, b, &field);
}

static void copy(ec_state* s, word* out, const word* in)
{
  if (out != in)
    memcpy(out, in, s->bytes);
}

/* Jacobian coordinates: affine x = X/Z^2, y = Y/Z^3. Z = 0 is infinity.
 * a = -3 lets the doubling slope use 3(X-Z^2)(X+Z^2). */
static void point_double(ec_state* s, unsigned out, unsigned in)
{
  word *x = F(s, in), *y = F(s, in + 1), *z = F(s, in + 2);
  mul(s, T(s, 0), x, x);
  mul(s, T(s, 1), y, y);
  mul(s, T(s, 2), T(s, 1), T(s, 1));
  add(s, T(s, 3), x, T(s, 1));
  mul(s, T(s, 3), T(s, 3), T(s, 3));
  sub(s, T(s, 3), T(s, 3), T(s, 0));
  sub(s, T(s, 3), T(s, 3), T(s, 2));
  add(s, T(s, 3), T(s, 3), T(s, 3));
  mul(s, T(s, 4), z, z);
  sub(s, T(s, 5), x, T(s, 4));
  add(s, T(s, 4), x, T(s, 4));
  mul(s, T(s, 5), T(s, 5), T(s, 4));
  add(s, T(s, 4), T(s, 5), T(s, 5));
  add(s, T(s, 4), T(s, 4), T(s, 5));
  mul(s, F(s, out), T(s, 4), T(s, 4));
  sub(s, F(s, out), F(s, out), T(s, 3));
  sub(s, F(s, out), F(s, out), T(s, 3));
  sub(s, T(s, 3), T(s, 3), F(s, out));
  mul(s, F(s, out + 1), T(s, 4), T(s, 3));
  add(s, T(s, 2), T(s, 2), T(s, 2));
  add(s, T(s, 2), T(s, 2), T(s, 2));
  add(s, T(s, 2), T(s, 2), T(s, 2));
  sub(s, F(s, out + 1), F(s, out + 1), T(s, 2));
  mul(s, F(s, out + 2), y, z);
  add(s, F(s, out + 2), F(s, out + 2), F(s, out + 2));
}

/* Compute R0 + R1 in slots 6..8. Secret-scalar callers precompute doubling
 * in slots 9..11. Public-scalar callers compute it only for equal points. */
static void point_add(ec_state* s, int public_inputs)
{
  word equal, first_infinity, second_infinity;
  unsigned i;
  mul(s, T(s, 0), F(s, 2), F(s, 2));
  mul(s, T(s, 1), F(s, 5), F(s, 5));
  mul(s, T(s, 2), F(s, 0), T(s, 1));
  mul(s, T(s, 3), F(s, 3), T(s, 0));
  mul(s, T(s, 1), T(s, 1), F(s, 5));
  mul(s, T(s, 0), T(s, 0), F(s, 2));
  mul(s, T(s, 4), F(s, 1), T(s, 1));
  mul(s, T(s, 5), F(s, 4), T(s, 0));
  sub(s, T(s, 3), T(s, 3), T(s, 2));
  sub(s, T(s, 5), T(s, 5), T(s, 4));
  equal = (word)(tc_mp_zero_mask(T(s, 3), s->words) & tc_mp_zero_mask(T(s, 5), s->words));
  first_infinity = tc_mp_zero_mask(F(s, 2), s->words);
  second_infinity = tc_mp_zero_mask(F(s, 5), s->words);
  mul(s, T(s, 6), T(s, 3), T(s, 3));
  mul(s, T(s, 7), T(s, 6), T(s, 3));
  mul(s, T(s, 2), T(s, 2), T(s, 6));
  mul(s, F(s, 6), T(s, 5), T(s, 5));
  sub(s, F(s, 6), F(s, 6), T(s, 7));
  sub(s, F(s, 6), F(s, 6), T(s, 2));
  sub(s, F(s, 6), F(s, 6), T(s, 2));
  sub(s, T(s, 2), T(s, 2), F(s, 6));
  mul(s, F(s, 7), T(s, 5), T(s, 2));
  mul(s, T(s, 4), T(s, 4), T(s, 7));
  sub(s, F(s, 7), F(s, 7), T(s, 4));
  mul(s, F(s, 8), F(s, 2), F(s, 5));
  mul(s, F(s, 8), F(s, 8), T(s, 3));
  if (public_inputs && equal)
    point_double(s, 9, 0);
  for (i = 0; i < 3; ++i) {
    tc_mp_select(F(s, 6 + i), F(s, 9 + i), F(s, 6 + i), equal, s->words);
    tc_mp_select(F(s, 6 + i), F(s, 3 + i), F(s, 6 + i), first_infinity, s->words);
    tc_mp_select(F(s, 6 + i), F(s, i), F(s, 6 + i), second_infinity, s->words);
  }
}

/* Constant-time swap of the ladder points (fields 0-2 and 3-5) when bit is 1. */
static void swap_points(ec_state* s, unsigned bit)
{
  const word mask = (word)(0u - bit);
  for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
    tc_mp_swap(F(s, coordinate), F(s, 3 + coordinate), mask, s->words);
}

static void multiply_point(ec_state* s)
{
  size_t i;
  unsigned coordinate;
  for (i = s->bytes * 8; i > 0; --i) {
    unsigned bit =
        (unsigned)((F(s, EC_SCALAR)[(i - 1) / TC_EC_WORD_BITS] >> ((i - 1) % TC_EC_WORD_BITS)) &
                   1u);
    swap_points(s, bit);
    point_double(s, 9, 0);
    point_add(s, 0);
    for (coordinate = 0; coordinate < 3; ++coordinate) {
      copy(s, F(s, coordinate), F(s, 9 + coordinate));
      copy(s, F(s, 3 + coordinate), F(s, 6 + coordinate));
    }
    swap_points(s, bit);
  }
}

/* Verification scalars are public. Skip point additions for zero bits while
 * retaining the same complete addition formulas for exceptional points. */
static void multiply_point_public(ec_state* s)
{
  for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
    memset(F(s, coordinate), 0, s->bytes);
  for (size_t i = s->bytes * 8; i > 0; --i) {
    const unsigned bit =
        (unsigned)((F(s, EC_SCALAR)[(i - 1) / TC_EC_WORD_BITS] >> ((i - 1) % TC_EC_WORD_BITS)) &
                   1u);
    point_double(s, 9, 0);
    for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
      copy(s, F(s, coordinate), F(s, 9 + coordinate));
    if (bit) {
      point_add(s, 1);
      for (unsigned coordinate = 0; coordinate < 3; ++coordinate)
        copy(s, F(s, coordinate), F(s, 6 + coordinate));
    }
  }
}

/* Fermat inversion over either curve prime. The exponent and loop shape are
 * public; base may contain a secret nonce or point coordinate. */
static void invert_prime(ec_state* s, word* out, const word* base, const word* one,
                         const word* prime, word factor)
{
  copy(s, out, one);
  for (size_t i = s->bytes * 8; i > 0; --i) {
    const size_t index = (i - 1) / TC_EC_WORD_BITS;
    word exponent = prime[index];
    if (index == 0)
      exponent = (word)(exponent - 2u);
    const tc_mp_modulus field = {prime, s->words, factor, s->w->product, s->w->reduced};
    tc_mp_montgomery(out, out, out, &field);
    if ((exponent >> ((i - 1) % TC_EC_WORD_BITS)) & 1u)
      tc_mp_montgomery(out, out, base, &field);
  }
}

static void point_to_affine(ec_state* s)
{
  invert_prime(s, F(s, 3), F(s, 2), F(s, EC_ONE), F(s, EC_P), 1);
  mul(s, F(s, 4), F(s, 3), F(s, 3));
  mul(s, F(s, 0), F(s, 0), F(s, 4));
  mul(s, F(s, 4), F(s, 4), F(s, 3));
  mul(s, F(s, 1), F(s, 1), F(s, 4));
}

size_t TC_EC_coordinate_bytes(TC_EC_curve curve)
{
  switch (curve) {
#if TC_EC_ENABLE_P192
  case TC_EC_P192:
    return 24;
#endif
#if TC_EC_ENABLE_P256
  case TC_EC_P256:
    return 32;
#endif
#if TC_EC_ENABLE_P384
  case TC_EC_P384:
    return 48;
#endif
  default:
    return 0;
  }
}

/* Load one big-endian curve parameter row, which may live in AVR flash.
 * Caller inputs in RAM use tc_mp_from_be. */
static void import_parameter(ec_state* s, word* out, const uint8_t* row)
{
  memset(out, 0, s->bytes);
  for (size_t i = 0; i < s->bytes; ++i)
    out[i / sizeof(word)] |=
        (word)((word)EC_BYTE(row + s->bytes - 1 - i) << (8 * (i % sizeof(word))));
}

static void initialize(ec_state* s, TC_EC_workspace* workspace, size_t bytes)
{
  const uint8_t* params = NULL;
  const uint8_t* order_r2 = NULL;
  unsigned i;
  s->w = workspace;
  s->bytes = bytes;
  s->words = bytes / sizeof(word);
  memset(workspace, 0, sizeof *workspace);
#if TC_EC_ENABLE_P192
  if (bytes == 24) {
    params = &params_192[0][0];
    order_r2 = order_r2_192;
  }
#endif
#if TC_EC_ENABLE_P256
  if (bytes == 32) {
    params = &params_256[0][0];
    order_r2 = order_r2_256;
  }
#endif
#if TC_EC_ENABLE_P384
  if (bytes == 48) {
    params = &params_384[0][0];
    order_r2 = order_r2_384;
  }
#endif
  for (i = 0; i < 4; ++i)
    import_parameter(s, F(s, EC_P + i), params + i * bytes);
  import_parameter(s, F(s, EC_SCALAR), order_r2);
  s->order_factor = tc_mp_montgomery_factor(F(s, EC_N)[0]);
  import_parameter(s, F(s, 3), params + 4 * bytes);
  import_parameter(s, F(s, 4), params + 5 * bytes);
  F(s, EC_ONE)[0] = 1;
  mul(s, F(s, EC_ONE), F(s, EC_ONE), F(s, EC_R2));
  mul(s, F(s, EC_B), F(s, EC_B), F(s, EC_R2));
}

static int validate_point(ec_state* s)
{
  if (!tc_mp_subtract(s->w->reduced, F(s, 3), F(s, EC_P), s->words) ||
      !tc_mp_subtract(s->w->reduced, F(s, 4), F(s, EC_P), s->words))
    return 0;
  mul(s, F(s, 3), F(s, 3), F(s, EC_R2));
  mul(s, F(s, 4), F(s, 4), F(s, EC_R2));
  copy(s, F(s, 5), F(s, EC_ONE));
  mul(s, T(s, 0), F(s, 4), F(s, 4));
  mul(s, T(s, 1), F(s, 3), F(s, 3));
  mul(s, T(s, 1), T(s, 1), F(s, 3));
  sub(s, T(s, 1), T(s, 1), F(s, 3));
  sub(s, T(s, 1), T(s, 1), F(s, 3));
  sub(s, T(s, 1), T(s, 1), F(s, 3));
  add(s, T(s, 1), T(s, 1), F(s, EC_B));
  sub(s, T(s, 0), T(s, 0), T(s, 1));
  return tc_mp_zero_mask(T(s, 0), s->words) != 0;
}

/* The built-in generator coordinates are already Montgomery residues. */
static void prepare_generator(ec_state* s)
{
  copy(s, F(s, 5), F(s, EC_ONE));
}

/* Leave the Montgomery domain and write one affine coordinate. */
static void export_coordinate(ec_state* s, uint8_t* output, unsigned coordinate)
{
  memset(T(s, 11), 0, s->bytes);
  T(s, 11)[0] = 1;
  mul(s, T(s, 10), F(s, coordinate), T(s, 11));
  tc_mp_to_be(output, T(s, 10), s->bytes);
}

/* Work units per operation. A scalar multiplication or an inversion costs one
 * unit per curve bit. */
uint32_t TC_EC_operation_work(TC_EC_curve curve, TC_EC_operation operation)
{
  const uint32_t bits = (uint32_t)TC_EC_coordinate_bytes(curve) * 8u;
  if (!bits)
    return 0;
  switch (operation) {
  case TC_EC_OPERATION_PUBLIC_KEY:
    return 2 * bits; /* multiplication and affine conversion */
  case TC_EC_OPERATION_VALIDATE:
    return 1;
  case TC_EC_OPERATION_ECDH:
    return 2 * bits + 1;
  case TC_EC_OPERATION_VERIFY:
    return 4 * bits + 1; /* order inversion, two multiplications, affine */
  case TC_EC_OPERATION_SIGN:
    return 3 * bits + 1 + (TC_ECDSA_SIGN_VERIFY ? 4 * bits + 1 : 0);
  case TC_EC_OPERATION_GENERATE:
    return 2 * bits + 1;
  }
  return 0;
}

static TC_EC_result charge(TC_EC_curve curve, TC_EC_operation operation, TC_work_budget* work)
{
  const uint32_t cost = TC_EC_operation_work(curve, operation);
  if (work->remaining < cost)
    return TC_EC_LIMIT;
  work->remaining -= cost;
  return TC_EC_OK;
}

/* A private scalar is in [1, n - 1]. Expects an initialized state. */
static int scalar_valid(ec_state* s, const word* scalar)
{
  return !tc_mp_zero_mask(scalar, s->words) &&
         tc_mp_subtract(s->w->reduced, scalar, F(s, EC_N), s->words);
}

static int spans_disjoint(const TC_bytes* spans, size_t count)
{
  for (size_t i = 0; i < count; ++i) {
    if (!tc_internal_span_valid(spans[i].data, spans[i].length))
      return 0;
    for (size_t j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(spans[i].data, spans[i].length, spans[j].data,
                                       spans[j].length))
        return 0;
  }
  return 1;
}

/* Public key or ECDH on validated arguments. output holds the full encoding.
 * The workspace is wiped. */
static TC_EC_result key_operation(size_t bytes, const uint8_t* scalar, const uint8_t* peer,
                                  uint8_t* output, TC_EC_workspace* workspace)
{
  ec_state s;
  TC_EC_result status = TC_EC_INVALID;
  initialize(&s, workspace, bytes);
  tc_mp_from_be(F(&s, EC_SCALAR), scalar, s.bytes);
  if (!scalar_valid(&s, F(&s, EC_SCALAR)))
    goto done;
  if (peer) {
    tc_mp_from_be(F(&s, 3), peer + 1, s.bytes);
    tc_mp_from_be(F(&s, 4), peer + 1 + bytes, s.bytes);
    if (!validate_point(&s))
      goto done;
  } else
    prepare_generator(&s);
  multiply_point(&s);
  if (tc_mp_zero_mask(F(&s, 2), s.words))
    goto done;
  point_to_affine(&s);
  if (peer)
    export_coordinate(&s, output, 0);
  else {
    output[0] = 4;
    export_coordinate(&s, output + 1, 0);
    export_coordinate(&s, output + 1 + bytes, 1);
  }
  status = TC_EC_OK;
done:
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

TC_EC_result TC_EC_public_key(TC_EC_curve curve, TC_bytes private_key, TC_buffer public_key,
                              TC_EC_workspace* workspace, TC_work_budget* work)
{
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  uint8_t point[1 + 2 * TC_EC_MAX_BYTES];
  const TC_bytes spans[] = {private_key,
                            {public_key.data, public_key.capacity},
                            {(const uint8_t*)workspace, sizeof *workspace},
                            {(const uint8_t*)work, sizeof *work}};
  if (!workspace || !work || !private_key.data || !public_key.data || !spans_disjoint(spans, 4))
    return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (private_key.length != bytes)
    return TC_EC_INVALID;
  if (public_key.capacity < 1 + 2 * bytes)
    return TC_EC_LIMIT;
  TC_EC_result status = charge(curve, TC_EC_OPERATION_PUBLIC_KEY, work);
  if (status != TC_EC_OK)
    return status;
  status = key_operation(bytes, private_key.data, NULL, point, workspace);
  if (status == TC_EC_OK)
    memcpy(public_key.data, point, 1 + 2 * bytes);
  TC_secure_zero(point, sizeof point);
  return status;
}

TC_EC_result TC_EC_generate_key_pair(TC_EC_curve curve, TC_buffer private_key, TC_buffer public_key,
                                     TC_EC_workspace* workspace, TC_EC_execution* execution)
{
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  uint8_t candidate[TC_EC_MAX_BYTES];
  uint8_t point[1 + 2 * TC_EC_MAX_BYTES];
  const TC_bytes spans[] = {{private_key.data, private_key.capacity},
                            {public_key.data, public_key.capacity},
                            {(const uint8_t*)workspace, sizeof *workspace},
                            {(const uint8_t*)execution, sizeof *execution}};
  if (!workspace || !execution || !execution->random.fill || !private_key.data ||
      !public_key.data || !spans_disjoint(spans, 4))
    return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (private_key.capacity < bytes || public_key.capacity < 1 + 2 * bytes)
    return TC_EC_LIMIT;
  TC_EC_result status = TC_EC_LIMIT;
  for (size_t attempt = 0; attempt < execution->random_attempts; ++attempt) {
    status = charge(curve, TC_EC_OPERATION_GENERATE, &execution->work);
    if (status != TC_EC_OK)
      break;
    if (execution->random.fill(execution->random.context, candidate, bytes) != TC_OK) {
      status = TC_EC_ERROR;
      break;
    }
    /* SEC 1 3.2.1: an out-of-range draw is discarded. */
    status = key_operation(bytes, candidate, NULL, point, workspace);
    if (status == TC_EC_OK) {
      memcpy(private_key.data, candidate, bytes);
      memcpy(public_key.data, point, 1 + 2 * bytes);
      break;
    }
    status = TC_EC_LIMIT;
  }
  TC_secure_zero(candidate, sizeof candidate);
  TC_secure_zero(point, sizeof point);
  return status;
}

TC_EC_result TC_ECDH(TC_EC_curve curve, TC_bytes private_key, TC_bytes peer_public_key,
                     TC_buffer shared_secret, TC_EC_workspace* workspace, TC_work_budget* work)
{
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  uint8_t secret[TC_EC_MAX_BYTES];
  const TC_bytes spans[] = {private_key,
                            {shared_secret.data, shared_secret.capacity},
                            {(const uint8_t*)workspace, sizeof *workspace},
                            {(const uint8_t*)work, sizeof *work}};
  if (!workspace || !work || !private_key.data || !peer_public_key.data || !shared_secret.data ||
      !spans_disjoint(spans, 4) ||
      !tc_internal_ranges_disjoint(peer_public_key.data, peer_public_key.length, shared_secret.data,
                                   shared_secret.capacity) ||
      !tc_internal_ranges_disjoint(peer_public_key.data, peer_public_key.length, workspace,
                                   sizeof *workspace) ||
      !tc_internal_ranges_disjoint(peer_public_key.data, peer_public_key.length, work,
                                   sizeof *work))
    return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (private_key.length != bytes || peer_public_key.length != 1 + 2 * bytes ||
      peer_public_key.data[0] != 4)
    return TC_EC_INVALID;
  if (shared_secret.capacity < bytes)
    return TC_EC_LIMIT;
  TC_EC_result status = charge(curve, TC_EC_OPERATION_ECDH, work);
  if (status != TC_EC_OK)
    return status;
  status = key_operation(bytes, private_key.data, peer_public_key.data, secret, workspace);
  if (status == TC_EC_OK)
    memcpy(shared_secret.data, secret, bytes);
  TC_secure_zero(secret, sizeof secret);
  return status;
}

TC_EC_result TC_EC_validate_public_key(TC_EC_curve curve, TC_bytes public_key,
                                       TC_EC_workspace* workspace, TC_work_budget* work)
{
  ec_state s;
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  if (!workspace || !work || !public_key.data ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, public_key.data,
                                   public_key.length) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, work, sizeof *work))
    return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (public_key.length != 1 + 2 * bytes)
    return TC_EC_INVALID;
  TC_EC_result status = charge(curve, TC_EC_OPERATION_VALIDATE, work);
  if (status != TC_EC_OK)
    return status;
  if (public_key.data[0] != 4)
    return TC_EC_INVALID;
  initialize(&s, workspace, bytes);
  tc_mp_from_be(F(&s, 3), public_key.data + 1, s.bytes);
  tc_mp_from_be(F(&s, 4), public_key.data + 1 + bytes, s.bytes);
  status = validate_point(&s) ? TC_EC_OK : TC_EC_INVALID;
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

/* Order arithmetic uses a different Montgomery factor than field arithmetic. */
static void order_mul(ec_state* s, word* out, const word* a, const word* b)
{
  const tc_mp_modulus field = {F(s, EC_N), s->words, s->order_factor, s->w->product, s->w->reduced};
  tc_mp_montgomery(out, a, b, &field);
}

/* FIPS 186-5 6.4.1 step 2 and 6.4.2 step 3: keep the leftmost len(n) bits of
 * the digest. Supported orders fill their byte width, so this keeps whole
 * bytes. The integer is reduced modulo n for the order arithmetic. */
static void digest_scalar(ec_state* s, word* out, const uint8_t* digest, size_t digest_len)
{
  if (digest_len > s->bytes)
    digest_len = s->bytes;
  tc_mp_from_be_padded(out, digest, digest_len, s->bytes);
  tc_mp_reduce(out, out, 0, F(s, EC_N), s->words, s->w->reduced);
}

static int verification_scalars(ec_state* s, const uint8_t* digest, size_t digest_len,
                                const uint8_t* signature, TC_ECDSA_workspace* workspace)
{
  tc_mp_from_be(F(s, 6), signature, s->bytes);
  tc_mp_from_be(F(s, 7), signature + s->bytes, s->bytes);
  if (tc_mp_zero_mask(F(s, 6), s->words) || tc_mp_zero_mask(F(s, 7), s->words) ||
      !tc_mp_subtract(s->w->reduced, F(s, 6), F(s, EC_N), s->words) ||
      !tc_mp_subtract(s->w->reduced, F(s, 7), F(s, EC_N), s->words))
    return 0;

  /* Supported orders fill their byte width. Short hashes are zero-extended. */
  digest_scalar(s, F(s, 8), digest, digest_len);
  copy(s, F(s, 0), F(s, EC_SCALAR));
  memset(F(s, 1), 0, s->bytes);
  F(s, 1)[0] = 1;
  order_mul(s, F(s, 2), F(s, 1), F(s, 0));
  order_mul(s, F(s, 7), F(s, 7), F(s, 0));
  invert_prime(s, F(s, 9), F(s, 7), F(s, 2), F(s, EC_N), s->order_factor);
  order_mul(s, F(s, 8), F(s, 8), F(s, 0));
  order_mul(s, F(s, 6), F(s, 6), F(s, 0));
  order_mul(s, F(s, 8), F(s, 8), F(s, 9));
  order_mul(s, F(s, 6), F(s, 6), F(s, 9));
  order_mul(s, workspace->scalars[0], F(s, 8), F(s, 1));
  order_mul(s, workspace->scalars[1], F(s, 6), F(s, 1));
  return 1;
}

/* ECDSA verification (SEC 1 4.1.4) on validated arguments. The workspace is
 * wiped. */
static TC_EC_result verify(size_t bytes, const uint8_t* public_key, const uint8_t* digest,
                           size_t digest_len, const uint8_t* signature,
                           TC_ECDSA_workspace* workspace)
{
  ec_state s;
  unsigned i;
  TC_EC_result status = TC_EC_INVALID;
  initialize(&s, &workspace->ec, bytes);
  if (public_key[0] != 4 || !verification_scalars(&s, digest, digest_len, signature, workspace))
    goto done;

  initialize(&s, &workspace->ec, bytes);
  tc_mp_from_be(F(&s, 3), public_key + 1, s.bytes);
  tc_mp_from_be(F(&s, 4), public_key + 1 + bytes, s.bytes);
  if (!validate_point(&s))
    goto done;
  copy(&s, F(&s, EC_SCALAR), workspace->scalars[1]);
  multiply_point_public(&s);
  for (i = 0; i < 3; ++i)
    copy(&s, workspace->point[i], F(&s, i));

  initialize(&s, &workspace->ec, bytes);
  prepare_generator(&s);
  copy(&s, F(&s, EC_SCALAR), workspace->scalars[0]);
  multiply_point_public(&s);
  for (i = 0; i < 3; ++i)
    copy(&s, F(&s, 3 + i), workspace->point[i]);
  point_add(&s, 1);
  for (i = 0; i < 3; ++i)
    copy(&s, F(&s, i), F(&s, 6 + i));
  if (tc_mp_zero_mask(F(&s, 2), s.words))
    goto done;
  point_to_affine(&s);
  memset(F(&s, 3), 0, bytes);
  F(&s, 3)[0] = 1;
  mul(&s, F(&s, 0), F(&s, 0), F(&s, 3));
  tc_mp_reduce(F(&s, 0), F(&s, 0), 0, F(&s, EC_N), s.words, s.w->reduced);
  tc_mp_from_be(F(&s, 1), signature, s.bytes);
  if (tc_mp_equal(F(&s, 0), F(&s, 1), s.words))
    status = TC_EC_OK;
done:
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

TC_EC_result TC_ECDSA_verify_digest(TC_EC_curve curve, TC_bytes public_key, TC_bytes digest,
                                    TC_bytes signature, TC_ECDSA_workspace* workspace,
                                    TC_work_budget* work)
{
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  if (!workspace || !work || !public_key.data || !digest.data || !digest.length || !signature.data)
    return TC_EC_ARGUMENT;
  const TC_bytes writes[] = {{(const uint8_t*)workspace, sizeof *workspace},
                             {(const uint8_t*)work, sizeof *work}};
  const TC_bytes inputs[] = {public_key, digest, signature};
  if (!spans_disjoint(writes, 2))
    return TC_EC_ARGUMENT;
  for (size_t i = 0; i < 3; ++i)
    for (size_t j = 0; j < 2; ++j)
      if (!tc_internal_ranges_disjoint(inputs[i].data, inputs[i].length, writes[j].data,
                                       writes[j].length))
        return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (public_key.length != 1 + 2 * bytes || signature.length != 2 * bytes)
    return TC_EC_INVALID;
  TC_EC_result status = charge(curve, TC_EC_OPERATION_VERIFY, work);
  if (status != TC_EC_OK)
    return status;
  return verify(bytes, public_key.data, digest.data, digest.length, signature.data, workspace);
}

#if defined(TC_TEST_ECDSA_FAULT)
/* Test seam: corrupts a signature between signing and self-verification. */
void (*tc_test_ecdsa_fault)(uint8_t* signature, size_t length);
#endif

#if TC_HASH_CORE_ENABLED
typedef struct {
  const tc_hash_algorithm_info* hash;
  size_t digest_length;
  size_t block_length;
  uint8_t key[TC_HASH_CORE_MAX_DIGEST];
  uint8_t value[TC_HASH_CORE_MAX_DIGEST];
  uint8_t generated;
} tc_rfc6979;

static TC_status tc_ecdsa_hmac(const tc_hash_algorithm_info* hash, size_t block_length,
                               const uint8_t* key, size_t key_length, const TC_bytes* parts,
                               size_t part_count, uint8_t* tag)
{
  TC_hash_context context;
  uint8_t block[TC_HASH_CORE_MAX_BLOCK], inner[TC_HASH_CORE_MAX_DIGEST];
  const size_t digest_length = tc_hash_core_digest_bytes(hash);
  if (key_length > block_length)
    return TC_ERROR;
  memset(block, 0x36, block_length);
  for (size_t i = 0; i < key_length; ++i)
    block[i] ^= key[i];
  TC_status status = tc_hash_core_init(hash, &context);
  if (status == TC_OK)
    status = tc_hash_core_update(hash, &context, block, block_length);
  for (size_t i = 0; status == TC_OK && i < part_count; ++i)
    status = tc_hash_core_update(hash, &context, parts[i].data, parts[i].length);
  if (status == TC_OK)
    status = tc_hash_core_final(hash, &context, inner);
  memset(block, 0x5c, block_length);
  for (size_t i = 0; i < key_length; ++i)
    block[i] ^= key[i];
  if (status == TC_OK)
    status = tc_hash_core_init(hash, &context);
  if (status == TC_OK)
    status = tc_hash_core_update(hash, &context, block, block_length);
  if (status == TC_OK)
    status = tc_hash_core_update(hash, &context, inner, digest_length);
  if (status == TC_OK)
    status = tc_hash_core_final(hash, &context, tag);
  tc_hash_core_clear(hash, &context);
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(inner, sizeof inner);
  return status;
}

static const uint8_t* tc_ecdsa_order(size_t bytes)
{
#if TC_EC_ENABLE_P192
  if (bytes == 24)
    return &params_192[1][0];
#endif
#if TC_EC_ENABLE_P256
  if (bytes == 32)
    return &params_256[1][0];
#endif
#if TC_EC_ENABLE_P384
  if (bytes == 48)
    return &params_384[1][0];
#endif
  return NULL;
}

static void tc_ecdsa_bits2octets(uint8_t* output, size_t width, TC_bytes digest)
{
  const uint8_t* order = tc_ecdsa_order(width);
  memset(output, 0, width);
  if (digest.length >= width)
    memcpy(output, digest.data, width);
  else
    memcpy(output + width - digest.length, digest.data, digest.length);
  uint8_t reduced[TC_EC_MAX_BYTES];
  unsigned borrow = 0;
  for (size_t i = width; i > 0; --i) {
    const unsigned a = output[i - 1], b = EC_BYTE(order + i - 1) + borrow;
    reduced[i - 1] = (uint8_t)(a - b);
    borrow = a < b;
  }
  const uint8_t use_reduced = (uint8_t)(borrow - 1u);
  for (size_t i = 0; i < width; ++i)
    output[i] = (uint8_t)((reduced[i] & use_reduced) | (output[i] & (uint8_t)~use_reduced));
  TC_secure_zero(reduced, sizeof reduced);
}

static TC_status tc_rfc6979_reseed(tc_rfc6979* state, uint8_t separator, TC_bytes private_key,
                                   TC_bytes reduced_digest)
{
  const TC_bytes parts[] = {
      {state->value, state->digest_length}, {&separator, 1}, private_key, reduced_digest};
  uint8_t next[TC_HASH_CORE_MAX_DIGEST];
  TC_status status = tc_ecdsa_hmac(state->hash, state->block_length, state->key,
                                   state->digest_length, parts, private_key.data ? 4 : 2, next);
  if (status == TC_OK)
    memcpy(state->key, next, state->digest_length);
  if (status == TC_OK)
    status = tc_ecdsa_hmac(state->hash, state->block_length, state->key, state->digest_length,
                           &(TC_bytes){state->value, state->digest_length}, 1, next);
  if (status == TC_OK)
    memcpy(state->value, next, state->digest_length);
  TC_secure_zero(next, sizeof next);
  return status;
}

static TC_status tc_rfc6979_fill(void* context, uint8_t* output, size_t length)
{
  tc_rfc6979* state = (tc_rfc6979*)context;
  if (state->generated &&
      tc_rfc6979_reseed(state, 0, (TC_bytes){NULL, 0}, (TC_bytes){NULL, 0}) != TC_OK)
    return TC_ERROR;
  size_t written = 0;
  uint8_t next[TC_HASH_CORE_MAX_DIGEST];
  while (written < length) {
    if (tc_ecdsa_hmac(state->hash, state->block_length, state->key, state->digest_length,
                      &(TC_bytes){state->value, state->digest_length}, 1, next) != TC_OK) {
      TC_secure_zero(next, sizeof next);
      return TC_ERROR;
    }
    memcpy(state->value, next, state->digest_length);
    const size_t take =
        length - written < state->digest_length ? length - written : state->digest_length;
    memcpy(output + written, state->value, take);
    written += take;
  }
  state->generated = 1;
  TC_secure_zero(next, sizeof next);
  return TC_OK;
}

static TC_status tc_rfc6979_init(tc_rfc6979* state, TC_hash_algorithm algorithm,
                                 TC_bytes private_key, TC_bytes digest, size_t width)
{
  const tc_hash_algorithm_info* hash = tc_hash_core_lookup(algorithm);
  if (!hash || tc_hash_core_digest_bytes(hash) != digest.length)
    return TC_ERROR;
  memset(state, 0, sizeof *state);
  state->hash = hash;
  state->digest_length = digest.length;
  state->block_length = algorithm == TC_HASH_SHA384 || algorithm == TC_HASH_SHA512 ? 128u : 64u;
  memset(state->value, 1, state->digest_length);
  uint8_t reduced[TC_EC_MAX_BYTES];
  tc_ecdsa_bits2octets(reduced, width, digest);
  TC_status status = tc_rfc6979_reseed(state, 0, private_key, (TC_bytes){reduced, width});
  if (status == TC_OK)
    status = tc_rfc6979_reseed(state, 1, private_key, (TC_bytes){reduced, width});
  TC_secure_zero(reduced, sizeof reduced);
  return status;
}
#endif

TC_EC_result TC_ECDSA_sign_digest(TC_EC_curve curve, const TC_ECDSA_sign_options* options,
                                  TC_bytes private_key, TC_bytes public_key, TC_bytes digest,
                                  TC_buffer signature, TC_ECDSA_workspace* workspace,
                                  TC_work_budget* work)
{
  if (!options || !work)
    return TC_EC_ARGUMENT;
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  if (!bytes)
    return TC_EC_UNSUPPORTED;
#if TC_HASH_CORE_ENABLED
  const tc_hash_algorithm_info* hash = tc_hash_core_lookup(options->hash);
  if (!hash)
    return TC_EC_UNSUPPORTED;
  if (tc_hash_core_digest_bytes(hash) != digest.length)
    return TC_EC_ARGUMENT;
  if (!options->candidate_attempts)
    return TC_EC_LIMIT;
  tc_rfc6979 state;
  if (tc_rfc6979_init(&state, options->hash, private_key, digest, bytes) != TC_OK) {
    TC_secure_zero(&state, sizeof state);
    return TC_EC_ERROR;
  }
  TC_EC_execution execution = {{tc_rfc6979_fill, &state}, options->candidate_attempts, *work};
  const TC_EC_result result = TC_ECDSA_sign_digest_external_random(
      curve, private_key, public_key, digest, signature, workspace, &execution);
  *work = execution.work;
  TC_secure_zero(&state, sizeof state);
  return result;
#else
  (void)private_key;
  (void)public_key;
  (void)digest;
  (void)signature;
  (void)workspace;
  return TC_EC_UNSUPPORTED;
#endif
}

TC_EC_result TC_ECDSA_sign_digest_external_random(TC_EC_curve curve, TC_bytes private_key,
                                                  TC_bytes public_key, TC_bytes digest,
                                                  TC_buffer signature,
                                                  TC_ECDSA_workspace* workspace,
                                                  TC_EC_execution* execution)
{
  ec_state s;
  const size_t bytes = TC_EC_coordinate_bytes(curve);
  uint8_t nonce[TC_EC_MAX_BYTES];
  uint8_t candidate[2 * TC_EC_MAX_BYTES];
  const TC_bytes spans[] = {private_key,
                            digest,
                            {signature.data, signature.capacity},
                            {(const uint8_t*)workspace, sizeof *workspace},
                            {(const uint8_t*)execution, sizeof *execution}};
  if (!workspace || !execution || !execution->random.fill || !private_key.data ||
      !public_key.data || !digest.data || !digest.length || !signature.data ||
      !spans_disjoint(spans, 5) ||
      !tc_internal_ranges_disjoint(public_key.data, public_key.length, signature.data,
                                   signature.capacity) ||
      !tc_internal_ranges_disjoint(public_key.data, public_key.length, workspace,
                                   sizeof *workspace) ||
      !tc_internal_ranges_disjoint(public_key.data, public_key.length, execution,
                                   sizeof *execution))
    return TC_EC_ARGUMENT;
  if (!bytes)
    return TC_EC_UNSUPPORTED;
  if (private_key.length != bytes || public_key.length != 1 + 2 * bytes)
    return TC_EC_INVALID;
  /* Preflight the first attempt so a short budget leaves the workspace unchanged. */
  if (signature.capacity < 2 * bytes || !execution->random_attempts ||
      execution->work.remaining < TC_EC_operation_work(curve, TC_EC_OPERATION_SIGN))
    return TC_EC_LIMIT;
  TC_EC_result status = TC_EC_LIMIT;
  initialize(&s, &workspace->ec, bytes);
  tc_mp_from_be(workspace->scalars[0], private_key.data, s.bytes);
  if (!scalar_valid(&s, workspace->scalars[0])) {
    status = TC_EC_INVALID;
    goto done;
  }
  digest_scalar(&s, workspace->scalars[1], digest.data, digest.length);

  for (size_t attempt = 0; attempt < execution->random_attempts; ++attempt) {
    status = charge(curve, TC_EC_OPERATION_SIGN, &execution->work);
    if (status != TC_EC_OK)
      goto done;
    status = TC_EC_LIMIT;
    initialize(&s, &workspace->ec, bytes);
    if (execution->random.fill(execution->random.context, nonce, bytes) != TC_OK) {
      status = TC_EC_ERROR;
      goto done;
    }
    tc_mp_from_be(F(&s, EC_SCALAR), nonce, s.bytes);
    if (!scalar_valid(&s, F(&s, EC_SCALAR)))
      continue;
    copy(&s, workspace->point[2], F(&s, EC_SCALAR));
    prepare_generator(&s);
    multiply_point(&s);
    if (tc_mp_zero_mask(F(&s, 2), s.words))
      continue;
    point_to_affine(&s);
    memset(F(&s, 3), 0, bytes);
    F(&s, 3)[0] = 1;
    mul(&s, F(&s, 0), F(&s, 0), F(&s, 3));
    tc_mp_reduce(workspace->point[0], F(&s, 0), 0, F(&s, EC_N), s.words, s.w->reduced);
    if (tc_mp_zero_mask(workspace->point[0], s.words))
      continue;

    initialize(&s, &workspace->ec, bytes);
    copy(&s, F(&s, 0), F(&s, EC_SCALAR));
    memset(F(&s, 1), 0, bytes);
    F(&s, 1)[0] = 1;
    order_mul(&s, F(&s, 2), F(&s, 1), F(&s, 0));
    order_mul(&s, F(&s, 3), workspace->point[0], F(&s, 0));
    order_mul(&s, F(&s, 4), workspace->scalars[0], F(&s, 0));
    order_mul(&s, F(&s, 5), F(&s, 3), F(&s, 4));
    order_mul(&s, F(&s, 6), workspace->scalars[1], F(&s, 0));
    tc_mp_add_mod(F(&s, 7), F(&s, 5), F(&s, 6), F(&s, EC_N), s.words, s.w->reduced);
    order_mul(&s, F(&s, 8), workspace->point[2], F(&s, 0));
    invert_prime(&s, F(&s, 9), F(&s, 8), F(&s, 2), F(&s, EC_N), s.order_factor);
    order_mul(&s, F(&s, 10), F(&s, 7), F(&s, 9));
    order_mul(&s, F(&s, 11), F(&s, 10), F(&s, 1));
    if (tc_mp_zero_mask(F(&s, 11), s.words))
      continue;
    tc_mp_to_be(candidate, workspace->point[0], bytes);
    tc_mp_to_be(candidate + bytes, F(&s, 11), bytes);
#if defined(TC_TEST_ECDSA_FAULT)
    if (tc_test_ecdsa_fault)
      tc_test_ecdsa_fault(candidate, 2 * bytes);
#endif
#if TC_ECDSA_SIGN_VERIFY
    /* Release only a signature that verifies under the caller's public key. */
    if (verify(bytes, public_key.data, digest.data, digest.length, candidate, workspace) !=
        TC_EC_OK) {
      status = TC_EC_ERROR;
      goto done;
    }
#endif
    memcpy(signature.data, candidate, 2 * bytes);
    status = TC_EC_OK;
    break;
  }
done:
  TC_secure_zero(nonce, sizeof nonce);
  TC_secure_zero(candidate, sizeof candidate);
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}
#endif
