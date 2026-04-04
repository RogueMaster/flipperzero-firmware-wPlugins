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
Issue Date: 20/12/2007

 This file provides fast multiplication in GF(128) as required by several
 cryptographic authentication modes (see gfmul128.h).
*/

/*  Speed critical loops can be unrolled to gain speed but consume more memory */
#if 1
#define UNROLL_LOOPS
#endif

/* The order of these includes matters */
#include "mode_hdr.h"
#include "gf128mul.h"
#include "gf_mul_lo.h"

#if defined(GF_MODE_LL)
#define mode _ll
#elif defined(GF_MODE_BL)
#define mode _bl
#elif defined(GF_MODE_LB)
#define mode _lb
#elif defined(GF_MODE_BB)
#define mode _bb
#else
#error mode is not defined
#endif

#if defined(GF_MODE_LL) || defined(GF_MODE_LB)
#define GF_INDEX(i) (i)
#else
#define GF_INDEX(i) (15 - (i))
#endif

/* A slow field multiplier */

void gf_mul(gf_t a, const gf_t b) {
    gf_t p[8]  = {0};
    uint8_t *q = NULL, ch = 0;
    int i = 0;

    copy_block_aligned(p[0], a);
    for(i = 0; i < 7; ++i)
        gf_mulx1(mode)(p[i + 1], p[i]);

    q = (uint8_t*)(a == b ? p[0] : b);
    memset(a, 0, GF_BYTE_LEN);
    for(i = 15;;) {
        ch = q[GF_INDEX(i)];
        if(ch & X_0) xor_block_aligned(a, a, p[0]);
        if(ch & X_1) xor_block_aligned(a, a, p[1]);
        if(ch & X_2) xor_block_aligned(a, a, p[2]);
        if(ch & X_3) xor_block_aligned(a, a, p[3]);
        if(ch & X_4) xor_block_aligned(a, a, p[4]);
        if(ch & X_5) xor_block_aligned(a, a, p[5]);
        if(ch & X_6) xor_block_aligned(a, a, p[6]);
        if(ch & X_7) xor_block_aligned(a, a, p[7]);
        if(!i--) break;
        gf_mulx8(mode)(a);
    }
}

#if defined(TABLES_64K)

/*  This version uses 64k bytes of table space on the stack.
    An input variable field value in a[] has to be multiplied
    by a key value in g[] that changes far less frequently.

    To do this a[] is split up into 16 smaller field values,
    each one byte in length. For the 256 values of each of
    these smaller values, we can precompute the result of
    mulltiplying g by this field value. We can then combine
    these values to provide the full multiply. So for each
    of 16 bytes we have a table of 256 field values each of
    16 bytes - 64k bytes in total.
*/

void init_64k_table(const gf_t g, gf_t64k_t t) {
    int i = 0, j = 0, k = 0;

    /*
    depending on the representation we have to process bits
    within bytes high to low (0xe1 style ) or low to high
    (0x87 style).  We start by producing the powers x ,x^2
    .. x^7 and put them in t[0][1], t[0][2] .. t[128] or in
    t[128], t[64] .. t[1] depending on the bit order in use.
    */

    /* clear the element for the zero field element */
    memset(t[0][0], 0, GF_BYTE_LEN);

#if defined(GF_MODE_LL) || defined(GF_MODE_BL)

    /* g -> t[0][1], generate t[0][2] ...           */
    memcpy(t[0][1], g, GF_BYTE_LEN);
    for(j = 1; j <= 64; j <<= 1)
        gf_mulx1(mode)(t[0][j + j], t[0][j]);
#else

    /* g -> t[0][128], generate t[0][64] ...        */
    memcpy(t[0][128], g, GF_BYTE_LEN);
    for(j = 64; j >= 1; j >>= 1)
        gf_mulx1(mode)(t[0][j], t[0][j + j]);
#endif

    for(;;) {
        /*  if { n } stands for the field value represented by
            the integer n, we can express higher multiplies in
            the table as follows:

                1. g * { 3} = g * {2} ^ g * {1}

                2. g * { 5} = g * {4} ^ g * {1}
                   g * { 6} = g * {4} ^ g * {2}
                   g * { 7} = g * {4} ^ g * {3}

                3. g * { 9} = g * {8} ^ g * {1}
                   g * {10} = g * {8} ^ g * {2}
                   ....

           and so on.  This is what the following loops do.
        */
        for(j = 2; j < 256; j += j)
            for(k = 1; k < j; ++k)
                xor_block_aligned(t[i][j + k], t[i][j], t[i][k]);

        if(++i == GF_BYTE_LEN) /* all 16 byte positions done */
            return;

        /*  We now move to the next byte up and set up its eight
            starting values by multiplying the values in the
            lower table by x^8
        */
        memset(t[i][0], 0, GF_BYTE_LEN);
        for(j = 128; j > 0; j >>= 1) {
            memcpy(t[i][j], t[i - 1][j], GF_BYTE_LEN);
            gf_mulx8(mode)(t[i][j]);
        }
    }
}

#define xor_64k(i, ap, t, r) xor_block_aligned(r, r, t[i][ap[GF_INDEX(i)]])

#if defined(UNROLL_LOOPS)

void gf_mul_64k(gf_t a, const gf_t64k_t t, gf_t r) {
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    xor_64k(15, ap, t, r);
    xor_64k(14, ap, t, r);
    xor_64k(13, ap, t, r);
    xor_64k(12, ap, t, r);
    xor_64k(11, ap, t, r);
    xor_64k(10, ap, t, r);
    xor_64k(9, ap, t, r);
    xor_64k(8, ap, t, r);
    xor_64k(7, ap, t, r);
    xor_64k(6, ap, t, r);
    xor_64k(5, ap, t, r);
    xor_64k(4, ap, t, r);
    xor_64k(3, ap, t, r);
    xor_64k(2, ap, t, r);
    xor_64k(1, ap, t, r);
    xor_64k(0, ap, t, r);
    copy_block_aligned(a, r);
}

#else

void gf_mul_64k(gf_t a, const gf_t64k_t t, gf_t r) {
    int i       = 0;
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    for(i = 15; i >= 0; --i) {
        xor_64k(i, ap, t, r);
    }
    copy_block_aligned(a, r);
}

#endif

#endif

#if defined(TABLES_8K)

/*  This version uses 8k bytes of table space on the stack.
    An input field value in a[] has to be multiplied by a
    key value in g[]. To do this a[] is split up into 32
    smaller field values each 4-bits in length. For the
    16 values of each of these smaller field values we can
    precompute the result of mulltiplying g[] by the field
    value in question. So for each of 32 nibbles we have a
    table of 16 field values, each of 16 bytes - 8k bytes
    in total.
*/
void init_8k_table(const gf_t g, gf_t8k_t t) {
    int i = 0, j = 0, k = 0;

    /*  do the low 4-bit nibble first - t[0][16] - and note
        that the unit multiplier sits at 0x01 - t[0][1] in
        the table. Then multiplies by x go at 2, 4, 8
    */
    /* set the table elements for a zero multiplier */
    memset(t[0][0], 0, GF_BYTE_LEN);
    memset(t[1][0], 0, GF_BYTE_LEN);

#if defined(GF_MODE_LL) || defined(GF_MODE_BL)

    /* t[0][1] = g, compute t[0][2], t[0][4], t[0][8]   */
    memcpy(t[0][1], g, GF_BYTE_LEN);
    for(j = 1; j <= 4; j <<= 1)
        gf_mulx1(mode)(t[0][j + j], t[0][j]);
    /* t[1][1] = t[0][1] * x^4 = t[0][8] * x            */
    gf_mulx1(mode)(t[1][1], t[0][8]);
    for(j = 1; j <= 4; j <<= 1)
        gf_mulx1(mode)(t[1][j + j], t[1][j]);
#else

    /* g -> t[0][8], compute t[0][4], t[0][2], t[0][1]  */
    memcpy(t[1][8], g, GF_BYTE_LEN);
    for(j = 4; j >= 1; j >>= 1)
        gf_mulx1(mode)(t[1][j], t[1][j + j]);
    /* t[1][1] = t[0][1] * x^4 = t[0][8] * x            */
    gf_mulx1(mode)(t[0][8], t[1][1]);
    for(j = 4; j >= 1; j >>= 1)
        gf_mulx1(mode)(t[0][j], t[0][j + j]);
#endif

    for(;;) {
        for(j = 2; j < 16; j += j)
            for(k = 1; k < j; ++k)
                xor_block_aligned(t[i][j + k], t[i][j], t[i][k]);

        if(++i == 2 * GF_BYTE_LEN) return;

        if(i > 1) {
            memset(t[i][0], 0, GF_BYTE_LEN);
            for(j = 8; j > 0; j >>= 1) {
                memcpy(t[i][j], t[i - 2][j], GF_BYTE_LEN);
                gf_mulx8(mode)(t[i][j]);
            }
        }
    }
}

#define xor_8k(i, ap, t, r)                                  \
    xor_block_aligned(r, r, t[i + i][ap[GF_INDEX(i)] & 15]); \
    xor_block_aligned(r, r, t[i + i + 1][ap[GF_INDEX(i)] >> 4])

#if defined(UNROLL_LOOPS)

void gf_mul_8k(gf_t a, const gf_t8k_t t, gf_t r) {
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    xor_8k(15, ap, t, r);
    xor_8k(14, ap, t, r);
    xor_8k(13, ap, t, r);
    xor_8k(12, ap, t, r);
    xor_8k(11, ap, t, r);
    xor_8k(10, ap, t, r);
    xor_8k(9, ap, t, r);
    xor_8k(8, ap, t, r);
    xor_8k(7, ap, t, r);
    xor_8k(6, ap, t, r);
    xor_8k(5, ap, t, r);
    xor_8k(4, ap, t, r);
    xor_8k(3, ap, t, r);
    xor_8k(2, ap, t, r);
    xor_8k(1, ap, t, r);
    xor_8k(0, ap, t, r);
    copy_block_aligned(a, r);
}

#else

void gf_mul_8k(gf_t a, const gf_t8k_t t, gf_t r) {
    int i       = 0;
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    for(i = 15; i >= 0; --i) {
        xor_8k(i, ap, t, r);
    }
    memcpy(a, r, GF_BYTE_LEN);
}

#endif

#endif

#if defined(TABLES_4K)

/*  This version uses 4k bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in a
    single byte, we can construct a table of the 256 16
    byte values that result from multiplying g by the 256
    values of this byte.  This requires 4096 bytes.

    If we take the highest byte in the buffer and use this
    table to multiply it by g, we then have to multiply it
    by x^120 to get the final value. For the next highest
    byte the result has to be multiplied by x^112 and so on.

    But we can do this by accumulating the result in an
    accumulator starting with the result for the top byte.
    We repeatedly multiply the accumulator value by x^8 and
    then add in (i.e. xor) the 16 bytes of the next lower
    byte in the buffer, stopping when we reach the lowest
    byte. This requires a 4096 byte table.
*/

void init_4k_table(const gf_t g, gf_t4k_t t) {
    int j = 0, k = 0;

    memset(t[0], 0, GF_BYTE_LEN);

#if defined(GF_MODE_LL) || defined(GF_MODE_BL)

    memcpy(t[1], g, GF_BYTE_LEN);
    for(j = 1; j <= 64; j <<= 1)
        gf_mulx1(mode)(t[j + j], t[j]);
#else

    memcpy(t[128], g, GF_BYTE_LEN);
    for(j = 64; j >= 1; j >>= 1)
        gf_mulx1(mode)(t[j], t[j + j]);
#endif

    for(j = 2; j < 256; j += j)
        for(k = 1; k < j; ++k)
            xor_block_aligned(t[j + k], t[j], t[k]);
}

#define xor_4k(i, ap, t, r) \
    gf_mulx8(mode)(r);      \
    xor_block_aligned(r, r, t[ap[GF_INDEX(i)]])

#if defined(UNROLL_LOOPS)

void gf_mul_4k(gf_t a, const gf_t4k_t t, gf_t r) {
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    xor_4k(15, ap, t, r);
    xor_4k(14, ap, t, r);
    xor_4k(13, ap, t, r);
    xor_4k(12, ap, t, r);
    xor_4k(11, ap, t, r);
    xor_4k(10, ap, t, r);
    xor_4k(9, ap, t, r);
    xor_4k(8, ap, t, r);
    xor_4k(7, ap, t, r);
    xor_4k(6, ap, t, r);
    xor_4k(5, ap, t, r);
    xor_4k(4, ap, t, r);
    xor_4k(3, ap, t, r);
    xor_4k(2, ap, t, r);
    xor_4k(1, ap, t, r);
    xor_4k(0, ap, t, r);
    copy_block_aligned(a, r);
}

#else

void gf_mul_4k(gf_t a, const gf_t4k_t t, gf_t r) {
    int i       = 15;
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    for(i = 15; i >= 0; --i) {
        xor_4k(i, ap, t, r);
    }
    copy_block_aligned(a, r);
}

#endif

#endif

#if defined(TABLES_256)

/*  This version uses 256 bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in a
    single 4-bit nibble, we can construct a table of the 16
    16 byte  values that result from the 16 values of this
    byte.  This requires 256 bytes. If we take the highest
    4-bit nibble in the buffer and use this table to get the
    result, we then have to multiply by x^124 to get the
    final value. For the next highest byte the result has to
    be multiplied by x^120 and so on. But we can do this by
    accumulating the result in an accumulator starting with
    the result for the top nibble.  We repeatedly multiply
    the accumulator value by x^4 and then add in (i.e. xor)
    the 16 bytes of the next lower nibble in the buffer,
    stopping when we reach the lowest nibble. This uses a
    256 byte table.
*/

void init_256_table(const gf_t g, gf_t256_t t) {
    int j = 0, k = 0;

    memset(t[0], 0, GF_BYTE_LEN);

#if defined(GF_MODE_LL) || defined(GF_MODE_BL)

    memcpy(t[1], g, GF_BYTE_LEN);
    for(j = 1; j <= 4; j <<= 1)
        gf_mulx1(mode)(t[j + j], t[j]);
#else

    memcpy(t[8], g, GF_BYTE_LEN);
    for(j = 4; j >= 1; j >>= 1)
        gf_mulx1(mode)(t[j], t[j + j]);
#endif

    for(j = 2; j < 16; j += j)
        for(k = 1; k < j; ++k)
            xor_block_aligned(t[j + k], t[j], t[k]);
}

#define x_lo(i, ap, t, r) \
    gf_mulx4(mode)(r);    \
    xor_block_aligned(r, r, t[ap[GF_INDEX(i)] & 0x0f])
#define x_hi(i, ap, t, r) \
    gf_mulx4(mode)(r);    \
    xor_block_aligned(r, r, t[ap[GF_INDEX(i)] >> 4])

#if defined(GF_MODE_LL) || defined(GF_MODE_BL)
#define xor_256(a, b, c, d) \
    x_hi(a, b, c, d);       \
    x_lo(a, b, c, d)
#else
#define xor_256(a, b, c, d) \
    x_lo(a, b, c, d);       \
    x_hi(a, b, c, d)
#endif

#if defined(UNROLL_LOOPS)

void gf_mul_256(gf_t a, const gf_t256_t t, gf_t r) {
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    xor_256(15, ap, t, r);
    xor_256(14, ap, t, r);
    xor_256(13, ap, t, r);
    xor_256(12, ap, t, r);
    xor_256(11, ap, t, r);
    xor_256(10, ap, t, r);
    xor_256(9, ap, t, r);
    xor_256(8, ap, t, r);
    xor_256(7, ap, t, r);
    xor_256(6, ap, t, r);
    xor_256(5, ap, t, r);
    xor_256(4, ap, t, r);
    xor_256(3, ap, t, r);
    xor_256(2, ap, t, r);
    xor_256(1, ap, t, r);
    xor_256(0, ap, t, r);
    copy_block_aligned(a, r);
}

#else

void gf_mul_256(gf_t a, const gf_t256_t t, gf_t r) {
    int i       = 0;
    uint8_t* ap = (uint8_t*)a;
    memset(r, 0, GF_BYTE_LEN);
    for(i = 15; i >= 0; --i) {
        xor_256(i, ap, t, r);
    }
    copy_block_aligned(a, r);
}

#endif

#endif
