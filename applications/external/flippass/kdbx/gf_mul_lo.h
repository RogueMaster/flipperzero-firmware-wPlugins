/*
---------------------------------------------------------------------------
Copyright (c) 1998-2010, Brian Gladman, Worcester, UK. All rights reserved.

The redistribution and use of this software (with or without changes)
is allowed without the payment of fees or royalties provided that:

  source code distributions include the above copyright notice, this
  list of conditions and the following disclaimer;

  binary distributions include the above copyright notice, this list
  of conditions and the following disclaimer in their documentation.

This software is provided 'as is' with no explicit or implied warranties
in respect of its operation, including, but not limited to, correctness
and fitness for purpose.
---------------------------------------------------------------------------
Issue Date: 18/02/2014

 This file provides the low level primitives needed for Galois Field
 operations in GF(2^128) for the four most likely field representations.
*/

#ifndef _GF_MUL_LO_H
#define _GF_MUL_LO_H

#if defined(USE_INLINING)
#if defined(_MSC_VER)
#define gf_decl __inline
#elif defined(__GNUC__) || defined(__GNU_LIBRARY__)
#define gf_decl static inline
#else
#define gf_decl static
#endif
#endif

#if 0 /* used for testing only: t1(UNIT_BITS), t2(UNIT_BITS)  */
#define _t1(n) bswap##n##_block(x, x)
#define t1(n)  _t1(n)
#define _t2(n)              \
    bswap##n##_block(x, x); \
    bswap##n##_block(r, r)
#define t2(n) _t2(n)
#endif

#define gf_m(n, x)  gf_mulx##n##x
#define gf_mulx1(x) gf_m(1, x)
#define gf_mulx4(x) gf_m(4, x)
#define gf_mulx8(x) gf_m(8, x)

#define MASK(x) ((x) * (UNIT_CAST(-1, UNIT_BITS) / 0xff))

#define DATA_256(q)                                                                               \
    {                                                                                             \
        q(0x00), q(0x01), q(0x02), q(0x03), q(0x04), q(0x05), q(0x06), q(0x07), q(0x08), q(0x09), \
            q(0x0a), q(0x0b), q(0x0c), q(0x0d), q(0x0e), q(0x0f), q(0x10), q(0x11), q(0x12),      \
            q(0x13), q(0x14), q(0x15), q(0x16), q(0x17), q(0x18), q(0x19), q(0x1a), q(0x1b),      \
            q(0x1c), q(0x1d), q(0x1e), q(0x1f), q(0x20), q(0x21), q(0x22), q(0x23), q(0x24),      \
            q(0x25), q(0x26), q(0x27), q(0x28), q(0x29), q(0x2a), q(0x2b), q(0x2c), q(0x2d),      \
            q(0x2e), q(0x2f), q(0x30), q(0x31), q(0x32), q(0x33), q(0x34), q(0x35), q(0x36),      \
            q(0x37), q(0x38), q(0x39), q(0x3a), q(0x3b), q(0x3c), q(0x3d), q(0x3e), q(0x3f),      \
            q(0x40), q(0x41), q(0x42), q(0x43), q(0x44), q(0x45), q(0x46), q(0x47), q(0x48),      \
            q(0x49), q(0x4a), q(0x4b), q(0x4c), q(0x4d), q(0x4e), q(0x4f), q(0x50), q(0x51),      \
            q(0x52), q(0x53), q(0x54), q(0x55), q(0x56), q(0x57), q(0x58), q(0x59), q(0x5a),      \
            q(0x5b), q(0x5c), q(0x5d), q(0x5e), q(0x5f), q(0x60), q(0x61), q(0x62), q(0x63),      \
            q(0x64), q(0x65), q(0x66), q(0x67), q(0x68), q(0x69), q(0x6a), q(0x6b), q(0x6c),      \
            q(0x6d), q(0x6e), q(0x6f), q(0x70), q(0x71), q(0x72), q(0x73), q(0x74), q(0x75),      \
            q(0x76), q(0x77), q(0x78), q(0x79), q(0x7a), q(0x7b), q(0x7c), q(0x7d), q(0x7e),      \
            q(0x7f), q(0x80), q(0x81), q(0x82), q(0x83), q(0x84), q(0x85), q(0x86), q(0x87),      \
            q(0x88), q(0x89), q(0x8a), q(0x8b), q(0x8c), q(0x8d), q(0x8e), q(0x8f), q(0x90),      \
            q(0x91), q(0x92), q(0x93), q(0x94), q(0x95), q(0x96), q(0x97), q(0x98), q(0x99),      \
            q(0x9a), q(0x9b), q(0x9c), q(0x9d), q(0x9e), q(0x9f), q(0xa0), q(0xa1), q(0xa2),      \
            q(0xa3), q(0xa4), q(0xa5), q(0xa6), q(0xa7), q(0xa8), q(0xa9), q(0xaa), q(0xab),      \
            q(0xac), q(0xad), q(0xae), q(0xaf), q(0xb0), q(0xb1), q(0xb2), q(0xb3), q(0xb4),      \
            q(0xb5), q(0xb6), q(0xb7), q(0xb8), q(0xb9), q(0xba), q(0xbb), q(0xbc), q(0xbd),      \
            q(0xbe), q(0xbf), q(0xc0), q(0xc1), q(0xc2), q(0xc3), q(0xc4), q(0xc5), q(0xc6),      \
            q(0xc7), q(0xc8), q(0xc9), q(0xca), q(0xcb), q(0xcc), q(0xcd), q(0xce), q(0xcf),      \
            q(0xd0), q(0xd1), q(0xd2), q(0xd3), q(0xd4), q(0xd5), q(0xd6), q(0xd7), q(0xd8),      \
            q(0xd9), q(0xda), q(0xdb), q(0xdc), q(0xdd), q(0xde), q(0xdf), q(0xe0), q(0xe1),      \
            q(0xe2), q(0xe3), q(0xe4), q(0xe5), q(0xe6), q(0xe7), q(0xe8), q(0xe9), q(0xea),      \
            q(0xeb), q(0xec), q(0xed), q(0xee), q(0xef), q(0xf0), q(0xf1), q(0xf2), q(0xf3),      \
            q(0xf4), q(0xf5), q(0xf6), q(0xf7), q(0xf8), q(0xf9), q(0xfa), q(0xfb), q(0xfc),      \
            q(0xfd), q(0xfe), q(0xff)                                                             \
    }

/*  Within the 16 bytes of the field element the top and bottom field bits
    are within bytes as follows (bit numbers in bytes 0 from ls up) for
    each of the four field representations supported (see gf128mul.txt):

    GF_BIT   127 126 125 124 123 122 121 120     .....  7 6 5 4 3 2 1 0
                                                  0x87  1 0 0 0 0 1 1 1
    BL x[ 0]   7   6   5   4   3   2   1   0     x[15]  7 6 5 4 3 2 1 0
    LL x[15]   7   6   5   4   3   2   1   0     x[ 0]  7 6 5 4 3 2 1 0

    GF_BIT   120 121 122 123 124 125 126 127     .....  0 1 2 3 4 5 6 7
                                                  0xc1  1 1 1 0 0 0 0 1
    BB x[ 0]   7   6   5   4   3   2   1   0     x[15]  7 6 5 4 3 2 1 0
    LB x[15]   7   6   5   4   3   2   1   0     x[ 0]  7 6 5 4 3 2 1 0

    When the field element is multiplied by x^n, the high bits overflow
    and are used to form an overflow byte. For the BL and LL modes this
    byte has the lowest overflow bit in bit 0 whereas for the BB and LB
    modes this bit is in biit 7.  So we have for this byte:

    bit (bit n = 2^n)    7   6   5   4   3   2   1   0
    BL and LL          x^7 x^6 x^5 x^4 x^3 x^2 x^1 x^0
    BB and LB          x^0 x^1 x^2 x^3 x^4 x^5 x^6 x^7

    This byte then has to be multiplied by the low bits of the field
    polynomial, which produces a value of 16 bits to be xored into the
    left shifted field value. For the BL and LL modes bit 0 gives the
    word value 0x0087, bit 1 gives 0x010e (0x87 left shifted 1), 0x021c
    (0x87 left shifted 2), ... For the BB and LB modes, bit 7 gives the
    value 0x00e1, bit 6 gives 0x8070, bit 5 gives 0x4038, ... Each bit
    in the overflow byte is expanded in this way and is xored into the
    overall result, so eaach of the 256 byte values will produce a
    corresponding word value that is computed by the gf_uint16_xor(i)
    macros below.

    These word values have to be xored into the low 16 bits of the
    field value. If the byte endianess of the mode matches that of
    the architecture xoring the word value will be correct. But if
    the mode has the opposite endianess, the word value has to be
    xored in byte reversed order. This is done by the ord() macro.
*/

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN && (defined(GF_MODE_LB) || defined(GF_MODE_LL)) || \
    PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN && (defined(GF_MODE_BB) || defined(GF_MODE_BL))
#define ord(hi, lo) 0x##hi##lo
#else
#define ord(hi, lo) 0x##lo##hi
#endif

#if defined(GF_MODE_BL) || defined(GF_MODE_LL)

/* field and numeric bit significance correspond */

#define gf_uint16_xor(i)                                                                          \
    ((i & 0x01 ? ord(00, 87) : 0) ^ (i & 0x02 ? ord(01, 0e) : 0) ^ (i & 0x04 ? ord(02, 1c) : 0) ^ \
     (i & 0x08 ? ord(04, 38) : 0) ^ (i & 0x10 ? ord(08, 70) : 0) ^ (i & 0x20 ? ord(10, e0) : 0) ^ \
     (i & 0x40 ? ord(21, c0) : 0) ^ (i & 0x80 ? ord(43, 80) : 0))

enum x_bit {
    X_0 = 0x01,
    X_1 = 0x02,
    X_2 = 0x04,
    X_3 = 0x08,
    X_4 = 0x10,
    X_5 = 0x20,
    X_6 = 0x40,
    X_7 = 0x80
};

#elif defined(GF_MODE_BB) || defined(GF_MODE_LB)

/* field and numeric bit significance are in reverse */

#define gf_uint16_xor(i)                                                                          \
    ((i & 0x80 ? ord(00, e1) : 0) ^ (i & 0x40 ? ord(80, 70) : 0) ^ (i & 0x20 ? ord(40, 38) : 0) ^ \
     (i & 0x10 ? ord(20, 1c) : 0) ^ (i & 0x08 ? ord(10, 0e) : 0) ^ (i & 0x04 ? ord(08, 07) : 0) ^ \
     (i & 0x02 ? ord(84, 03) : 0) ^ (i & 0x01 ? ord(c2, 01) : 0))

enum x_bit {
    X_0 = 0x80,
    X_1 = 0x40,
    X_2 = 0x20,
    X_3 = 0x10,
    X_4 = 0x08,
    X_5 = 0x04,
    X_6 = 0x02,
    X_7 = 0x01
};

#else
#error Galois Field representation has not been set
#endif

const uint16_t gf_tab[256] = DATA_256(gf_uint16_xor);

/* LL Mode Galois Field operations

  x[0]     x[1]     x[2]     x[3]     x[4]     x[5]     x[6]    x[7]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
10000111 ........ ........ ........ ........ ........ ........ ........
07....00 15....08 23....16 31....24 39....32 47....40 55....48 63....56
  x[8]    x[9]   x[10]   x[11]   x[12]   x[13]   x[14]  x[15]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
........ ........ ........ ........ ........ ........ ........ M.......
71....64 79....72 87....80 95....88 103...96 111..104 119..112 127..120
*/

#if UNIT_BITS == 64

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_ll(n, r, x) r[n] = (x[n] << 1) | (n ? x[n - 1] >> 63 : 0)
#define f4_ll(n, r, x) r[n] = (x[n] << 4) | (n ? x[n - 1] >> 60 : 0)
#define f8_ll(n, r, x) r[n] = (x[n] << 8) | (n ? x[n - 1] >> 56 : 0)
#else
#define f1_ll(n, r, x) \
    r[n] = ((x[n] << 1) & ~MASK(0x01)) | (((x[n] >> 15) | (n ? x[n - 1] << 49 : 0)) & MASK(0x01))
#define f4_ll(n, r, x) \
    r[n] = ((x[n] << 4) & ~MASK(0x0f)) | (((x[n] >> 12) | (n ? x[n - 1] << 52 : 0)) & MASK(0x0f))
#define f8_ll(n, r, x) r[n] = (x[n] >> 8) | (n ? x[n - 1] << 56 : 0)
#endif

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 63) & 0x01];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[1] >> 7) & 0x01])) << 48;
#endif
    rep2_d2(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_ll(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 60) & 0x0f];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[1] >> 4) & 0x0f])) << 48;
#endif
    rep2_d2(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_ll(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[UNIT_PTR(x)[1] >> 56];
#else
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[1] & 0xff])) << 48;
#endif
    rep2_d2(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#elif UNIT_BITS == 32

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_ll(n, r, x) r[n] = (x[n] << 1) | (n ? x[n - 1] >> 31 : 0)
#define f4_ll(n, r, x) r[n] = (x[n] << 4) | (n ? x[n - 1] >> 28 : 0)
#define f8_ll(n, r, x) r[n] = (x[n] << 8) | (n ? x[n - 1] >> 24 : 0)
#else
#define f1_ll(n, r, x) \
    r[n] = ((x[n] << 1) & ~MASK(0x01)) | (((x[n] >> 15) | (n ? x[n - 1] << 17 : 0)) & MASK(0x01))
#define f4_ll(n, r, x) \
    r[n] = ((x[n] << 4) & ~MASK(0x0f)) | (((x[n] >> 12) | (n ? x[n - 1] << 20 : 0)) & MASK(0x0f))
#define f8_ll(n, r, x) r[n] = (x[n] >> 8) | (n ? x[n - 1] << 24 : 0)
#endif

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 31) & 0x01];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[3] >> 7) & 0x01])) << 16;
#endif
    rep2_d4(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_ll(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 28) & 0x0f];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[3] >> 4) & 0x0f])) << 16;
#endif
    rep2_d4(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_ll(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[UNIT_PTR(x)[3] >> 24];
#else
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[3] & 0xff])) << 16;
#endif
    rep2_d4(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#else

#define f1_ll(n, r, x) r[n] = (x[n] << 1) | (n ? x[n - 1] >> 7 : 0)
#define f4_ll(n, r, x) r[n] = (x[n] << 4) | (n ? x[n - 1] >> 4 : 0)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[15] >> 7) & 0x01];
    rep2_d16(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(r)[0] ^= _tt & 0xff;
#else
    UNIT_PTR(r)[0] ^= _tt >> 8;
#endif
}

gf_decl void gf_mulx4_ll(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[15] >> 4) & 0x0f];
    rep2_d16(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] ^= _tt & 0xff;
#else
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] = _tt >> 8;
#endif
}

gf_decl void gf_mulx8_ll(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] = _tt & 0xff;
#else
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] = _tt >> 8;
#endif
}

#endif

/* BL Mode Galois Field operations

  x[0]     x[1]     x[2]     x[3]     x[4]     x[5]     x[6]     x[7]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
M....... ........ ........ ........ ........ ........ ........ ........
127..120 119..112 111..104 103...96 95....88 87....80 79....72 71....64
  x[8]     x[9]    x[10]    x[11]    x[12]    x[13]    x[14]    x[15]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
........ ........ ........ ........ ........ ........ ........ 10000111
63....56 55....48 47....40 39....32 31....24 23....16 15....08 07....00
*/

#if UNIT_BITS == 64

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_bl(n, r, x) \
    r[n] = ((x[n] << 1) & ~MASK(0x01)) | (((x[n] >> 15) | (!n ? x[n + 1] << 49 : 0)) & MASK(0x01))
#define f4_bl(n, r, x) \
    r[n] = ((x[n] << 4) & ~MASK(0x0f)) | (((x[n] >> 12) | (!n ? x[n + 1] << 52 : 0)) & MASK(0x0f))
#define f8_bl(n, r, x) r[n] = (x[n] >> 8) | (!n ? x[n + 1] << 56 : 0)
#else
#define f1_bl(n, r, x) r[n] = (x[n] << 1) | (!n ? x[n + 1] >> 63 : 0)
#define f4_bl(n, r, x) r[n] = (x[n] << 4) | (!n ? x[n + 1] >> 60 : 0)
#define f8_bl(n, r, x) r[n] = (x[n] << 8) | (!n ? x[n + 1] >> 56 : 0)
#endif

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 63) & 0x01];
#endif
    rep2_u2(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= _tt;
}

gf_decl void gf_mulx4_bl(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 60) & 0x0f];
#endif
    rep2_u2(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

gf_decl void gf_mulx8_bl(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[0] & 0xff])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 56) & 0xff];
#endif
    rep2_u2(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

#elif UNIT_BITS == 32

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_bl(n, r, x)                   \
    r[n] = ((x[n] << 1) & ~MASK(0x01)) | \
           (((x[n] >> 15) | (n < 3 ? x[n + 1] << 17 : 0)) & MASK(0x01))
#define f4_bl(n, r, x)                   \
    r[n] = ((x[n] << 4) & ~MASK(0x0f)) | \
           (((x[n] >> 12) | (n < 3 ? x[n + 1] << 20 : 0)) & MASK(0x0f))
#define f8_bl(n, r, x) r[n] = (x[n] >> 8) | (n < 3 ? x[n + 1] << 24 : 0)
#else
#define f1_bl(n, r, x) r[n] = (x[n] << 1) | (n < 3 ? x[n + 1] >> 31 : 0)
#define f4_bl(n, r, x) r[n] = (x[n] << 4) | (n < 3 ? x[n + 1] >> 28 : 0)
#define f8_bl(n, r, x) r[n] = (x[n] << 8) | (n < 3 ? x[n + 1] >> 24 : 0)
#endif

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 31) & 0x01];
#endif
    rep2_u4(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= _tt;
}

gf_decl void gf_mulx4_bl(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 28) & 0x0f];
#endif
    rep2_u4(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

gf_decl void gf_mulx8_bl(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[0] & 0xff])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 24) & 0xff];
#endif
    rep2_u4(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

#else

#define f1_bl(n, r, x) r[n] = (x[n] << 1) | (n < 15 ? x[n + 1] >> 7 : 0)
#define f4_bl(n, r, x) r[n] = (x[n] << 4) | (n < 15 ? x[n + 1] >> 4 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01];
    rep2_u16(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(r)[15] ^= _tt >> 8;
#else
    UNIT_PTR(r)[15] ^= _tt & 0xff;
#endif
}

gf_decl void gf_mulx4_bl(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f];
    rep2_u16(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] ^= _tt >> 8;
#else
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] = _tt & 0xff;
#endif
}

gf_decl void gf_mulx8_bl(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] = _tt >> 8;
#else
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] = _tt & 0xff;
#endif
}

#endif

/* LB Mode Galois Field operations

   x[0]    x[1]     x[2]     x[3]     x[4]     x[5]     x[6]     x[7]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
11100001 ........ ........ ........ ........ ........ ........ ........
00....07 08....15 16....23 24....31 32....39 40....47 48....55 56....63
   x[8]    x[9]    x[10]    x[11]    x[12]    x[13]    x[14]    x[15]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
........ ........ ........ ........ ........ ........ ........ .......M
64....71 72....79 80....87 88....95 96...103 104..111 112..119 120..127
*/

#if UNIT_BITS == 64

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_lb(n, r, x) \
    r[n] = ((x[n] >> 1) & ~MASK(0x80)) | (((x[n] << 15) | (n ? x[n - 1] >> 49 : 0)) & MASK(0x80))
#define f4_lb(n, r, x) \
    r[n] = ((x[n] >> 4) & ~MASK(0xf0)) | (((x[n] << 12) | (n ? x[n - 1] >> 52 : 0)) & MASK(0xf0))
#define f8_lb(n, r, x) r[n] = (x[n] << 8) | (n ? x[n - 1] >> 56 : 0)
#else
#define f1_lb(n, r, x) r[n] = (x[n] >> 1) | (n ? x[n - 1] << 63 : 0)
#define f4_lb(n, r, x) r[n] = (x[n] >> 4) | (n ? x[n - 1] << 60 : 0)
#define f8_lb(n, r, x) x[n] = (x[n] >> 8) | (n ? x[n - 1] << 56 : 0)
#endif

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 49) & MASK(0x80)];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[1] << 7) & 0xff])) << 48;
#endif
    rep2_d2(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_lb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 52) & MASK(0xf0)];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[1] << 4) & 0xff])) << 48;
#endif
    rep2_d2(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_lb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[UNIT_PTR(x)[1] >> 56];
#else
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[1] & 0xff])) << 48;
#endif
    rep2_d2(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#elif UNIT_BITS == 32

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_lb(n, r, x) \
    r[n] = ((x[n] >> 1) & ~MASK(0x80)) | (((x[n] << 15) | (n ? x[n - 1] >> 17 : 0)) & MASK(0x80))
#define f4_lb(n, r, x) \
    r[n] = ((x[n] >> 4) & ~MASK(0xf0)) | (((x[n] << 12) | (n ? x[n - 1] >> 20 : 0)) & MASK(0xf0))
#define f8_lb(n, r, x) r[n] = (x[n] << 8) | (n ? x[n - 1] >> 24 : 0)
#else
#define f1_lb(n, r, x) r[n] = (x[n] >> 1) | (n ? x[n - 1] << 31 : 0)
#define f4_lb(n, r, x) r[n] = (x[n] >> 4) | (n ? x[n - 1] << 28 : 0)
#define f8_lb(n, r, x) r[n] = (x[n] >> 8) | (n ? x[n - 1] << 24 : 0)
#endif

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 17) & MASK(0x80)];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[3] << 7) & 0xff])) << 16;
#endif
    rep2_d4(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_lb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 20) & MASK(0xf0)];
#else
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[3] << 4) & 0xff])) << 16;
#endif
    rep2_d4(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_lb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = gf_tab[UNIT_PTR(x)[3] >> 24];
#else
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[3] & 0xff])) << 16;
#endif
    rep2_d4(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#else

#define f1_lb(n, r, x) r[n] = (x[n] >> 1) | (n ? x[n - 1] << 7 : 0)
#define f4_lb(n, r, x) r[n] = (x[n] >> 4) | (n ? x[n - 1] << 4 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[15] << 7) & 0x80];
    rep2_d16(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(r)[0] ^= _tt;
#else
    UNIT_PTR(r)[0] ^= _tt >> 8;
#endif
}

gf_decl void gf_mulx4_lb(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[15] << 4) & 0xf0];
    rep2_d16(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] ^= _tt & 0xff;
#else
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] ^= _tt >> 8;
#endif
}

gf_decl void gf_mulx8_lb(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] = _tt & 0xff;
#else
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] = _tt >> 8;
#endif
}

#endif

/* BB Mode Galois Field operations

  x[0]     x[1]     x[2]     x[3]     x[4]     x[5]     x[6]     x[7]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
.......M ........ ........ ........ ........ ........ ........ ........
120..127 112..119 104..111 96...103 88....95 80....87 72....79 64....71
  x[8]     x[9]     x[10]    x[11]    x[12]    x[13]    x[14]   x[15]
ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls ms    ls
........ ........ ........ ........ ........ ........ ........ 11100001
56....63 48....55 40....47 32....39 24....31 16....23 08....15 00....07
*/

#if UNIT_BITS == 64

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_bb(n, r, x) r[n] = (x[n] >> 1) | (!n ? x[n + 1] << 63 : 0)
#define f4_bb(n, r, x) r[n] = (x[n] >> 4) | (!n ? x[n + 1] << 60 : 0)
#define f8_bb(n, r, x) r[n] = (x[n] >> 8) | (!n ? x[n + 1] << 56 : 0)
#else
#define f1_bb(n, r, x) \
    r[n] = ((x[n] >> 1) & ~MASK(0x80)) | (((x[n] << 15) | (!n ? x[n + 1] >> 49 : 0)) & MASK(0x80))
#define f4_bb(n, r, x) \
    r[n] = ((x[n] >> 4) & ~MASK(0xf0)) | (((x[n] << 12) | (!n ? x[n + 1] >> 52 : 0)) & MASK(0xf0))
#define f8_bb(n, r, x) r[n] = (x[n] << 8) | (!n ? x[n + 1] >> 56 : 0)
#endif

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 49) & 0x80];
#endif
    rep2_u2(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= _tt;
}

gf_decl void gf_mulx4_bb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 52) & 0xf0];
#endif
    rep2_u2(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

gf_decl void gf_mulx8_bb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[0] & 0xff])) << 48;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 56) & 0xff];
#endif
    rep2_u2(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

#elif UNIT_BITS == 32

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
#define f1_bb(n, r, x) r[n] = (x[n] >> 1) | (n < 3 ? x[n + 1] << 31 : 0)
#define f4_bb(n, r, x) r[n] = (x[n] >> 4) | (n < 3 ? x[n + 1] << 28 : 0)
#define f8_bb(n, r, x) r[n] = (x[n] >> 8) | (n < 3 ? x[n + 1] << 24 : 0)
#else
#define f1_bb(n, r, x)                   \
    r[n] = ((x[n] >> 1) & ~MASK(0x80)) | \
           (((x[n] << 15) | (n < 3 ? x[n + 1] >> 17 : 0)) & MASK(0x80))
#define f4_bb(n, r, x)                   \
    r[n] = ((x[n] >> 4) & ~MASK(0xf0)) | \
           (((x[n] << 12) | (n < 3 ? x[n + 1] >> 20 : 0)) & MASK(0xf0))
#define f8_bb(n, r, x) r[n] = (x[n] << 8) | (n < 3 ? x[n + 1] >> 24 : 0)
#endif

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 17) & 0x80];
#endif
    rep2_u4(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= _tt;
}

gf_decl void gf_mulx4_bb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 20) & 0xf0];
#endif
    rep2_u4(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

gf_decl void gf_mulx8_bb(gf_t x) {
    gf_unit_t _tt = 0;
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    _tt = ((gf_unit_t)(gf_tab[UNIT_PTR(x)[0] & 0xff])) << 16;
#else
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 24) & 0xff];
#endif
    rep2_u4(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

#else

#define f1_bb(n, r, x) r[n] = (x[n] >> 1) | (n < 15 ? x[n + 1] << 7 : 0)
#define f4_bb(n, r, x) r[n] = (x[n] >> 4) | (n < 15 ? x[n + 1] << 4 : 0)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80];
    rep2_u16(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(r)[15] ^= _tt >> 8;
#else
    UNIT_PTR(r)[15] ^= _tt;
#endif
}

gf_decl void gf_mulx4_bb(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0];
    rep2_u16(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] ^= _tt >> 8;
#else
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] ^= _tt & 0xff;
#endif
}

gf_decl void gf_mulx8_bb(gf_t x) {
    uint16_t _tt = 0;
    _tt          = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] = _tt >> 8;
#else
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] = _tt & 0xff;
#endif
}

#endif

#endif
