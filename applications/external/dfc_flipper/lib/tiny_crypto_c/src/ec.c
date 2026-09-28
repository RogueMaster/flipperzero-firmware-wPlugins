/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/ec.h>
#include "internal.h"
#if TC_ENABLE_EC
#define TC_MP_WORD_BITS TC_EC_WORD_BITS
#include "mp_internal.h"
#if defined(__AVR__) && TC_AVR_PROGMEM
#include <avr/pgmspace.h>
#define EC_STORAGE PROGMEM
#define EC_BYTE(p) pgm_read_byte(p)
#else
#define EC_STORAGE
#define EC_BYTE(p) (*(p))
#endif
#if TC_EC_WORD_BITS == 8
typedef uint16_t ec_wide;
#else
typedef uint64_t ec_wide;
#endif
typedef TC_EC_word word;
typedef struct {
  TC_EC_workspace* w;
  size_t words, bytes;
} ec_state;

/* SEC 2 v2.0 parameters. Rows are p, n, b, R^2 mod p, Gx, Gy;
 * R = 2^(8 * coordinate_bytes). These curves have a = -3, h = 1. */
#if TC_EC_ENABLE_P192
static const uint8_t params_192[6][24] EC_STORAGE = {
  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0x99,0xde,0xf8,0x36,0x14,0x6b,0xc9,0xb1,0xb4,0xd2,0x28,0x31},
  {0x64,0x21,0x05,0x19,0xe5,0x9c,0x80,0xe7,0x0f,0xa7,0xe9,0xab,
   0x72,0x24,0x30,0x49,0xfe,0xb8,0xde,0xec,0xc1,0x46,0xb9,0xb1},
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01},
  {0x18,0x8d,0xa8,0x0e,0xb0,0x30,0x90,0xf6,0x7c,0xbf,0x20,0xeb,
   0x43,0xa1,0x88,0x00,0xf4,0xff,0x0a,0xfd,0x82,0xff,0x10,0x12},
  {0x07,0x19,0x2b,0x95,0xff,0xc8,0xda,0x78,0x63,0x10,0x11,0xed,
   0x6b,0x24,0xcd,0xd5,0x73,0xf9,0x77,0xa1,0x1e,0x79,0x48,0x11}
};
#endif
#if TC_EC_ENABLE_P256
static const uint8_t params_256[6][32] EC_STORAGE = {
  {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  },
  {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
  },
  {
    0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc,
    0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
  },
  {
    0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
    0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03
  },
  {
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
    0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
  },
  {
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
  },
};
#endif
#if TC_EC_ENABLE_P384
static const uint8_t params_384[6][48] EC_STORAGE = {
  {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
  },
  {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0x63, 0x4d, 0x81, 0xf4, 0x37, 0x2d, 0xdf,
    0x58, 0x1a, 0x0d, 0xb2, 0x48, 0xb0, 0xa7, 0x7a, 0xec, 0xec, 0x19, 0x6a, 0xcc, 0xc5, 0x29, 0x73
  },
  {
    0xb3, 0x31, 0x2f, 0xa7, 0xe2, 0x3e, 0xe7, 0xe4, 0x98, 0x8e, 0x05, 0x6b, 0xe3, 0xf8, 0x2d, 0x19,
    0x18, 0x1d, 0x9c, 0x6e, 0xfe, 0x81, 0x41, 0x12, 0x03, 0x14, 0x08, 0x8f, 0x50, 0x13, 0x87, 0x5a,
    0xc6, 0x56, 0x39, 0x8d, 0x8a, 0x2e, 0xd1, 0x9d, 0x2a, 0x85, 0xc8, 0xed, 0xd3, 0xec, 0x2a, 0xef
  },
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x01
  },
  {
    0xaa, 0x87, 0xca, 0x22, 0xbe, 0x8b, 0x05, 0x37, 0x8e, 0xb1, 0xc7, 0x1e, 0xf3, 0x20, 0xad, 0x74,
    0x6e, 0x1d, 0x3b, 0x62, 0x8b, 0xa7, 0x9b, 0x98, 0x59, 0xf7, 0x41, 0xe0, 0x82, 0x54, 0x2a, 0x38,
    0x55, 0x02, 0xf2, 0x5d, 0xbf, 0x55, 0x29, 0x6c, 0x3a, 0x54, 0x5e, 0x38, 0x72, 0x76, 0x0a, 0xb7
  },
  {
    0x36, 0x17, 0xde, 0x4a, 0x96, 0x26, 0x2c, 0x6f, 0x5d, 0x9e, 0x98, 0xbf, 0x92, 0x92, 0xdc, 0x29,
    0xf8, 0xf4, 0x1d, 0xbd, 0x28, 0x9a, 0x14, 0x7c, 0xe9, 0xda, 0x31, 0x13, 0xb5, 0xf0, 0xb8, 0xc0,
    0x0a, 0x60, 0xb1, 0xce, 0x1d, 0x7e, 0x81, 0x9d, 0x7a, 0x43, 0x1d, 0x7c, 0x90, 0xea, 0x0e, 0x5f
  },
};
#endif

enum { EC_P = 24, EC_N, EC_B, EC_R2, EC_ONE, EC_SCALAR };
#define F(s, i) ((s)->w->fields[(i)])
#define T(s, i) F(s, 12 + (i))

static word zero_mask(const word* a, size_t n)
{
  word value = 0;
  size_t i;
  for (i = 0; i < n; ++i) value |= a[i];
  return (word)(0u - (unsigned)(value == 0));
}

static void select_words(word* out, const word* a, const word* b, word mask, size_t n)
{
  tc_mp_select(out,a,b,mask,n);
}

static word subtract(word* out, const word* a, const word* b, size_t n)
{
  return tc_mp_subtract(out,a,b,n);
}

static void add(ec_state* s, word* out, const word* a, const word* b)
{
  tc_mp_add_mod(out,a,b,F(s,EC_P),s->words,s->w->reduced);
}

static void sub(ec_state* s, word* out, const word* a, const word* b)
{
  word mask = (word)(0u - (unsigned)subtract(out, a, b, s->words));
  ec_wide carry = 0;
  size_t i;
  mask = (word)tc_internal_mask_barrier(mask);
  for (i = 0; i < s->words; ++i) {
    carry += (ec_wide)out[i] + (F(s, EC_P)[i] & mask);
    out[i] = (word)carry;
    carry >>= TC_EC_WORD_BITS;
  }
}

/* Montgomery multiplication. The low word of either prime is -1, so
 * -p^-1 mod 2^word_bits is 1. Carry propagation has a fixed loop bound. */
static void mul(ec_state* s, word* out, const word* a, const word* b)
{
  tc_mp_montgomery(out,a,b,F(s,EC_P),s->words,1,s->w->product,s->w->reduced);
}

static void copy(ec_state* s, word* out, const word* in)
{
  if (out != in) memcpy(out, in, s->bytes);
}

/* Jacobian coordinates: affine x = X/Z^2, y = Y/Z^3; Z = 0 is infinity.
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

/* Compute R0 + R1 in slots 6..8. Doubling R0 is already in slots 9..11.
 * Equal points and infinity use masked selection, not scalar-dependent paths. */
static void point_add(ec_state* s)
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
  equal = (word)(zero_mask(T(s, 3), s->words) & zero_mask(T(s, 5), s->words));
  first_infinity = zero_mask(F(s, 2), s->words);
  second_infinity = zero_mask(F(s, 5), s->words);
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
  for (i = 0; i < 3; ++i) {
    select_words(F(s, 6 + i), F(s, 9 + i), F(s, 6 + i), equal, s->words);
    select_words(F(s, 6 + i), F(s, 3 + i), F(s, 6 + i), first_infinity, s->words);
    select_words(F(s, 6 + i), F(s, i), F(s, 6 + i), second_infinity, s->words);
  }
}

static void swap_points(ec_state* s, unsigned bit)
{
  word mask = (word)tc_internal_mask_barrier((word)(0u - bit));
  size_t i;
  unsigned coordinate;
  for (coordinate = 0; coordinate < 3; ++coordinate) {
    for (i = 0; i < s->words; ++i) {
      word difference = (word)((F(s, coordinate)[i] ^ F(s, 3 + coordinate)[i]) & mask);
      F(s, coordinate)[i] ^= difference;
      F(s, 3 + coordinate)[i] ^= difference;
    }
  }
}

static void multiply_point(ec_state* s)
{
  size_t i;
  unsigned coordinate;
  for (i = s->bytes * 8; i > 0; --i) {
    unsigned bit = (unsigned)((F(s, EC_SCALAR)[(i - 1) / TC_EC_WORD_BITS] >>
                                ((i - 1) % TC_EC_WORD_BITS)) & 1u);
    swap_points(s, bit);
    point_double(s, 9, 0);
    point_add(s);
    for (coordinate = 0; coordinate < 3; ++coordinate) {
      copy(s, F(s, coordinate), F(s, 9 + coordinate));
      copy(s, F(s, 3 + coordinate), F(s, 6 + coordinate));
    }
    swap_points(s, bit);
  }
}

static void point_to_affine(ec_state* s)
{
  size_t i;
  /* Fermat inversion. The exponent p-2 is public and fixed for each curve. */
  copy(s, F(s, 3), F(s, EC_ONE));
  for (i = s->bytes * 8; i > 0; --i) {
    size_t index = (i - 1) / TC_EC_WORD_BITS;
    word exponent = F(s, EC_P)[index];
    if (index == 0) exponent = (word)(exponent - 2u);
    mul(s, F(s, 3), F(s, 3), F(s, 3));
    if ((exponent >> ((i - 1) % TC_EC_WORD_BITS)) & 1u)
      mul(s, F(s, 3), F(s, 3), F(s, 2));
  }
  mul(s, F(s, 4), F(s, 3), F(s, 3));
  mul(s, F(s, 0), F(s, 0), F(s, 4));
  mul(s, F(s, 4), F(s, 4), F(s, 3));
  mul(s, F(s, 1), F(s, 1), F(s, 4));
}

static size_t curve_bytes(TC_EC_curve curve)
{
  switch (curve) {
#if TC_EC_ENABLE_P192
    case TC_EC_P192: return 24;
#endif
#if TC_EC_ENABLE_P256
    case TC_EC_P256: return 32;
#endif
#if TC_EC_ENABLE_P384
    case TC_EC_P384: return 48;
#endif
    default: return 0;
  }
}

static void import_bytes(ec_state* s, word* out, const uint8_t* bytes, int rom)
{
  size_t i;
  memset(out, 0, s->bytes);
  for (i = 0; i < s->bytes; ++i) {
    size_t index = s->bytes - 1 - i;
    uint8_t value = rom ? EC_BYTE(bytes + index) : bytes[index];
    out[i / sizeof(word)] |= (word)((word)value << (8 * (i % sizeof(word))));
  }
}

static void initialize(ec_state* s, TC_EC_workspace* workspace, size_t bytes)
{
  const uint8_t* params = NULL;
  unsigned i;
  s->w = workspace; s->bytes = bytes; s->words = bytes / sizeof(word);
  memset(workspace, 0, sizeof *workspace);
#if TC_EC_ENABLE_P192
  if (bytes == 24) params = &params_192[0][0];
#endif
#if TC_EC_ENABLE_P256
  if (bytes == 32) params = &params_256[0][0];
#endif
#if TC_EC_ENABLE_P384
  if (bytes == 48) params = &params_384[0][0];
#endif
  for (i = 0; i < 4; ++i) import_bytes(s, F(s, EC_P + i), params + i * bytes, 1);
  import_bytes(s, F(s, 3), params + 4 * bytes, 1);
  import_bytes(s, F(s, 4), params + 5 * bytes, 1);
  F(s, EC_ONE)[0] = 1;
  mul(s, F(s, EC_ONE), F(s, EC_ONE), F(s, EC_R2));
  mul(s, F(s, EC_B), F(s, EC_B), F(s, EC_R2));
}

static int validate_point(ec_state* s)
{
  if (!subtract(s->w->reduced, F(s, 3), F(s, EC_P), s->words) ||
      !subtract(s->w->reduced, F(s, 4), F(s, EC_P), s->words)) return 0;
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
  return zero_mask(T(s, 0), s->words) != 0;
}

static void export_coordinate(ec_state* s, uint8_t* output, unsigned coordinate)
{
  size_t i;
  memset(T(s, 11), 0, s->bytes); T(s, 11)[0] = 1;
  mul(s, T(s, 10), F(s, coordinate), T(s, 11));
  for (i = 0; i < s->bytes; ++i)
    output[s->bytes - 1 - i] = (uint8_t)(T(s, 10)[i / sizeof(word)] >> (8 * (i % sizeof(word))));
}

static TC_status key_operation(TC_EC_curve curve, const uint8_t* scalar, size_t scalar_len,
    const uint8_t* public_key, size_t public_key_len, uint8_t* output,
    size_t output_len, TC_EC_workspace* workspace, int agreement)
{
  ec_state s;
  size_t bytes = curve_bytes(curve);
  TC_status status = TC_ERROR;
  if (!bytes || !workspace || !scalar || scalar_len != bytes || !output ||
      output_len != (agreement ? bytes : 1 + 2 * bytes) ||
      (agreement && (!public_key || public_key_len != 1 + 2 * bytes || public_key[0] != 4)) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, scalar, scalar_len) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, public_key, public_key_len) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, output, output_len) ||
      !tc_internal_ranges_disjoint(scalar, scalar_len, output, output_len) ||
      !tc_internal_ranges_disjoint(public_key, public_key_len, output, output_len)) return TC_ERROR;
  initialize(&s, workspace, bytes);
  import_bytes(&s, F(&s, EC_SCALAR), scalar, 0);
  if (zero_mask(F(&s, EC_SCALAR), s.words) ||
      !subtract(workspace->reduced, F(&s, EC_SCALAR), F(&s, EC_N), s.words)) goto done;
  if (agreement) {
    import_bytes(&s, F(&s, 3), public_key + 1, 0);
    import_bytes(&s, F(&s, 4), public_key + 1 + bytes, 0);
  }
  if (!validate_point(&s)) goto done;
  multiply_point(&s);
  if (zero_mask(F(&s, 2), s.words)) goto done;
  point_to_affine(&s);
  if (agreement) export_coordinate(&s, output, 0);
  else {
    output[0] = 4;
    export_coordinate(&s, output + 1, 0);
    export_coordinate(&s, output + 1 + bytes, 1);
  }
  status = TC_OK;
done:
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

TC_status TC_EC_public_key(TC_EC_curve curve, const uint8_t* scalar, size_t scalar_len,
    uint8_t* output, size_t output_len, TC_EC_workspace* workspace)
{
  return key_operation(curve, scalar, scalar_len, NULL, 0, output, output_len, workspace, 0);
}

TC_status TC_ECDH(TC_EC_curve curve, const uint8_t* scalar, size_t scalar_len,
    const uint8_t* public_key, size_t public_key_len, uint8_t* output,
    size_t output_len, TC_EC_workspace* workspace)
{
  return key_operation(curve, scalar, scalar_len, public_key, public_key_len,
                       output, output_len, workspace, 1);
}

TC_status TC_EC_validate_public_key(TC_EC_curve curve, const uint8_t* public_key,
    size_t public_key_len, TC_EC_workspace* workspace)
{
  ec_state s;
  size_t bytes = curve_bytes(curve);
  TC_status status;
  if (!bytes || !workspace || !public_key || public_key_len != 1 + 2 * bytes ||
      public_key[0] != 4 || !tc_internal_ranges_disjoint(workspace, sizeof *workspace,
                                                       public_key, public_key_len)) return TC_ERROR;
  initialize(&s, workspace, bytes);
  import_bytes(&s, F(&s, 3), public_key + 1, 0);
  import_bytes(&s, F(&s, 4), public_key + 1 + bytes, 0);
  status = validate_point(&s) ? TC_OK : TC_ERROR;
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

/* Order arithmetic uses a different Montgomery factor than field arithmetic. */
static void order_mul(ec_state* s, word* out, const word* a, const word* b)
{
  tc_mp_montgomery(out, a, b, F(s, EC_N), s->words,
      tc_mp_montgomery_factor(F(s, EC_N)[0]), s->w->product, s->w->reduced);
}

static int verification_scalars(ec_state* s, const uint8_t* digest, size_t digest_len,
    const uint8_t* signature, TC_ECDSA_workspace* workspace)
{
  size_t i;
  import_bytes(s, F(s, 6), signature, 0);
  import_bytes(s, F(s, 7), signature + s->bytes, 0);
  if (zero_mask(F(s, 6), s->words) || zero_mask(F(s, 7), s->words) ||
      !subtract(s->w->reduced, F(s, 6), F(s, EC_N), s->words) ||
      !subtract(s->w->reduced, F(s, 7), F(s, EC_N), s->words)) return 0;

  /* Supported orders fill their byte width. Short hashes are zero-extended. */
  if (digest_len > s->bytes) digest_len = s->bytes;
  memset(F(s, 8), 0, s->bytes);
  for (i = 0; i < digest_len; ++i)
    F(s, 8)[i / sizeof(word)] |= (word)((word)digest[digest_len - 1 - i] <<
        (8 * (i % sizeof(word))));
  tc_mp_reduce(F(s, 8), F(s, 8), 0, F(s, EC_N), s->words, s->w->reduced);
  tc_mp_montgomery_r2(F(s, 0), F(s, EC_N), s->words, s->w->reduced);
  memset(F(s, 1), 0, s->bytes);
  F(s, 1)[0] = 1;
  order_mul(s, F(s, 2), F(s, 1), F(s, 0));
  order_mul(s, F(s, 7), F(s, 7), F(s, 0));
  copy(s, F(s, 9), F(s, 2));
  /* s^-1 = s^(n-2); n is prime and s was checked nonzero. */
  for (i = s->bytes * 8; i > 0; --i) {
    size_t index = (i - 1) / TC_EC_WORD_BITS;
    word exponent = F(s, EC_N)[index];
    if (index == 0) exponent = (word)(exponent - 2u);
    order_mul(s, F(s, 9), F(s, 9), F(s, 9));
    if ((exponent >> ((i - 1) % TC_EC_WORD_BITS)) & 1u)
      order_mul(s, F(s, 9), F(s, 9), F(s, 7));
  }
  order_mul(s, F(s, 8), F(s, 8), F(s, 0));
  order_mul(s, F(s, 6), F(s, 6), F(s, 0));
  order_mul(s, F(s, 8), F(s, 8), F(s, 9));
  order_mul(s, F(s, 6), F(s, 6), F(s, 9));
  order_mul(s, workspace->scalars[0], F(s, 8), F(s, 1));
  order_mul(s, workspace->scalars[1], F(s, 6), F(s, 1));
  return 1;
}

TC_status TC_ECDSA_verify_digest(TC_EC_curve curve,
    const uint8_t* public_key, size_t public_key_len,
    const uint8_t* digest, size_t digest_len,
    const uint8_t* signature, size_t signature_len,
    TC_ECDSA_workspace* workspace)
{
  ec_state s;
  size_t bytes = curve_bytes(curve);
  unsigned i;
  TC_status status = TC_MISMATCH;
  if (!bytes || !workspace || !public_key || !digest || !digest_len || !signature ||
      public_key_len != 1 + 2 * bytes || signature_len != 2 * bytes ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, public_key, public_key_len) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, digest, digest_len) ||
      !tc_internal_ranges_disjoint(workspace, sizeof *workspace, signature, signature_len))
    return TC_ERROR;
  initialize(&s, &workspace->ec, bytes);
  if (public_key[0] != 4 ||
      !verification_scalars(&s, digest, digest_len, signature, workspace)) goto done;

  initialize(&s, &workspace->ec, bytes);
  import_bytes(&s, F(&s, 3), public_key + 1, 0);
  import_bytes(&s, F(&s, 4), public_key + 1 + bytes, 0);
  if (!validate_point(&s)) goto done;
  copy(&s, F(&s, EC_SCALAR), workspace->scalars[1]);
  multiply_point(&s);
  for (i = 0; i < 3; ++i) copy(&s, workspace->point[i], F(&s, i));

  initialize(&s, &workspace->ec, bytes);
  if (!validate_point(&s)) goto done;
  copy(&s, F(&s, EC_SCALAR), workspace->scalars[0]);
  multiply_point(&s);
  for (i = 0; i < 3; ++i) copy(&s, F(&s, 3 + i), workspace->point[i]);
  point_double(&s, 9, 0);
  point_add(&s);
  for (i = 0; i < 3; ++i) copy(&s, F(&s, i), F(&s, 6 + i));
  if (zero_mask(F(&s, 2), s.words)) goto done;
  point_to_affine(&s);
  memset(F(&s, 3), 0, bytes);
  F(&s, 3)[0] = 1;
  mul(&s, F(&s, 0), F(&s, 0), F(&s, 3));
  tc_mp_reduce(F(&s, 0), F(&s, 0), 0, F(&s, EC_N), s.words, s.w->reduced);
  import_bytes(&s, F(&s, 1), signature, 0);
  if (memcmp(F(&s, 0), F(&s, 1), bytes) == 0) status = TC_OK;
done:
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}
#endif
