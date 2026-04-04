/*
---------------------------------------------------------------------------
Copyright (c) 1998-2014, Brian Gladman, Worcester, UK. All rights reserved.

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

This header file is an INTERNAL file which supports mode implementation
*/

#ifndef _MODE_HDR_H
#define _MODE_HDR_H

#include <string.h>
#include <limits.h>

#include "brg_endian.h"

/*  This define sets the units in which buffers are processed.  This code
    can provide significant speed gains if buffers can be processed in
    32 or 64 bit chunks rather than in bytes.  This define sets the units
    in which buffers will be accessed if possible
*/
#if !defined(UNIT_BITS)
#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN
#if 0
#define UNIT_BITS 32
#elif 1
#define UNIT_BITS 64
#endif
#elif defined(_WIN64)
#define UNIT_BITS 64
#else
#define UNIT_BITS 32
#endif
#endif

#if UNIT_BITS == 64 && !defined(NEED_UINT_64T)
#define NEED_UINT_64T
#endif

#include <stdint.h>
#define UI_TYPE(size)                uint##size##_t
#define UNIT_TYPEDEF(x, size)        typedef UI_TYPE(size) x
#define BUFR_TYPEDEF(x, size, bsize) typedef UI_TYPE(size) x[bsize / (size >> 3)]
#define UNIT_CAST(x, size)           ((UI_TYPE(size))(x))
#define UPTR_CAST(x, size)           ((UI_TYPE(size)*)(x))

/*  Use of inlines is preferred but code blocks can also be expanded inline
    using 'defines'.  But the latter approach will typically generate a LOT
    of code and is not recommended.
*/
#if 1 && !defined(USE_INLINING)
#define USE_INLINING
#endif

#if defined(_MSC_VER)
#if _MSC_VER >= 1400
#include <stdlib.h>
#include <intrin.h>
#pragma intrinsic(memset)
#pragma intrinsic(memcpy)
#define rotl32      _rotl
#define rotr32      _rotr
#define rotl64      _rotl64
#define rotr64      _rotl64
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#else
#define rotl32 _lrotl
#define rotr32 _lrotr
#endif
#endif

#if defined(USE_INLINING)
#if defined(_MSC_VER)
#define mh_decl __inline
#elif defined(__GNUC__) || defined(__GNU_LIBRARY__)
#define mh_decl static inline
#else
#define mh_decl static
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define UI8_PTR(x)  UPTR_CAST(x, 8)
#define UI16_PTR(x) UPTR_CAST(x, 16)
#define UI32_PTR(x) UPTR_CAST(x, 32)
#define UI64_PTR(x) UPTR_CAST(x, 64)
#define UNIT_PTR(x) UPTR_CAST(x, UNIT_BITS)

#define UI8_VAL(x)  UNIT_CAST(x, 8)
#define UI16_VAL(x) UNIT_CAST(x, 16)
#define UI32_VAL(x) UNIT_CAST(x, 32)
#define UI64_VAL(x) UNIT_CAST(x, 64)
#define UNIT_VAL(x) UNIT_CAST(x, UNIT_BITS)

#define BUF_INC     (UNIT_BITS >> 3)
#define BUF_ADRMASK ((UNIT_BITS >> 3) - 1)

#define rep2_u2(f, r, x) \
    f(0, r, x);          \
    f(1, r, x)
#define rep2_u4(f, r, x) \
    f(0, r, x);          \
    f(1, r, x);          \
    f(2, r, x);          \
    f(3, r, x)
#define rep2_u16(f, r, x) \
    f(0, r, x);           \
    f(1, r, x);           \
    f(2, r, x);           \
    f(3, r, x);           \
    f(4, r, x);           \
    f(5, r, x);           \
    f(6, r, x);           \
    f(7, r, x);           \
    f(8, r, x);           \
    f(9, r, x);           \
    f(10, r, x);          \
    f(11, r, x);          \
    f(12, r, x);          \
    f(13, r, x);          \
    f(14, r, x);          \
    f(15, r, x)

#define rep2_d2(f, r, x) \
    f(1, r, x);          \
    f(0, r, x)
#define rep2_d4(f, r, x) \
    f(3, r, x);          \
    f(2, r, x);          \
    f(1, r, x);          \
    f(0, r, x)
#define rep2_d16(f, r, x) \
    f(15, r, x);          \
    f(14, r, x);          \
    f(13, r, x);          \
    f(12, r, x);          \
    f(11, r, x);          \
    f(10, r, x);          \
    f(9, r, x);           \
    f(8, r, x);           \
    f(7, r, x);           \
    f(6, r, x);           \
    f(5, r, x);           \
    f(4, r, x);           \
    f(3, r, x);           \
    f(2, r, x);           \
    f(1, r, x);           \
    f(0, r, x)

#define rep3_u2(f, r, x, y, c) \
    f(0, r, x, y, c);          \
    f(1, r, x, y, c)
#define rep3_u4(f, r, x, y, c) \
    f(0, r, x, y, c);          \
    f(1, r, x, y, c);          \
    f(2, r, x, y, c);          \
    f(3, r, x, y, c)
#define rep3_u16(f, r, x, y, c) \
    f(0, r, x, y, c);           \
    f(1, r, x, y, c);           \
    f(2, r, x, y, c);           \
    f(3, r, x, y, c);           \
    f(4, r, x, y, c);           \
    f(5, r, x, y, c);           \
    f(6, r, x, y, c);           \
    f(7, r, x, y, c);           \
    f(8, r, x, y, c);           \
    f(9, r, x, y, c);           \
    f(10, r, x, y, c);          \
    f(11, r, x, y, c);          \
    f(12, r, x, y, c);          \
    f(13, r, x, y, c);          \
    f(14, r, x, y, c);          \
    f(15, r, x, y, c)

#define rep3_d2(f, r, x, y, c) \
    f(1, r, x, y, c);          \
    f(0, r, x, y, c)
#define rep3_d4(f, r, x, y, c) \
    f(3, r, x, y, c);          \
    f(2, r, x, y, c);          \
    f(1, r, x, y, c);          \
    f(0, r, x, y, c)
#define rep3_d16(f, r, x, y, c) \
    f(15, r, x, y, c);          \
    f(14, r, x, y, c);          \
    f(13, r, x, y, c);          \
    f(12, r, x, y, c);          \
    f(11, r, x, y, c);          \
    f(10, r, x, y, c);          \
    f(9, r, x, y, c);           \
    f(8, r, x, y, c);           \
    f(7, r, x, y, c);           \
    f(6, r, x, y, c);           \
    f(5, r, x, y, c);           \
    f(4, r, x, y, c);           \
    f(3, r, x, y, c);           \
    f(2, r, x, y, c);           \
    f(1, r, x, y, c);           \
    f(0, r, x, y, c)

/* function pointers might be used for fast XOR operations */

typedef void (*xor_function)(void* r, const void* p, const void* q);

/* left and right rotates on 32 and 64 bit variables */

#if !defined(rotl32) /* NOTE: 0 <= n <= 32 ASSUMED */
mh_decl uint32_t rotl32(uint32_t x, int n) {
    return (((x) << n) | ((x) >> (32 - n)));
}
#endif

#if !defined(rotr32) /* NOTE: 0 <= n <= 32 ASSUMED */
mh_decl uint32_t rotr32(uint32_t x, int n) {
    return (((x) >> n) | ((x) << (32 - n)));
}
#endif

#if(UNIT_BITS == 64) && !defined(rotl64) /* NOTE: 0 <= n <= 64 ASSUMED */
mh_decl uint64_t rotl64(uint64_t x, int n) {
    return (((x) << n) | ((x) >> (64 - n)));
}
#endif

#if(UNIT_BITS == 64) && !defined(rotr64) /* NOTE: 0 <= n <= 64 ASSUMED */
mh_decl uint64_t rotr64(uint64_t x, int n) {
    return (((x) >> n) | ((x) << (64 - n)));
}
#endif

/* byte order inversions for 16, 32 and 64 bit variables */

#if !defined(bswap_16)
mh_decl uint16_t bswap_16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
#endif

#if !defined(bswap_32)
mh_decl uint32_t bswap_32(uint32_t x) {
    return ((rotr32((x), 24) & 0x00ff00ff) | (rotr32((x), 8) & 0xff00ff00));
}
#endif

#if(UNIT_BITS == 64) && !defined(bswap_64)
mh_decl uint64_t bswap_64(uint64_t x) {
    return bswap_32((uint32_t)(x >> 32)) | ((uint64_t)bswap_32((uint32_t)x) << 32);
}
#endif

/* support for fast aligned buffer move, xor and byte swap operations -
   source and destination buffers for move and xor operations must not
   overlap, those for byte order revesal must either not overlap or
   must be identical
*/
#define f_copy(n, p, q)      p[n] = q[n]
#define f_xor(n, r, p, q, c) r[n] = c(p[n] ^ q[n])

mh_decl void copy_block(void* p, const void* q) {
    memcpy(p, q, 16);
}

mh_decl void copy_block_aligned(void* p, const void* q) {
#if UNIT_BITS == 8
    memcpy(p, q, 16);
#elif UNIT_BITS == 32
    rep2_u4(f_copy, UNIT_PTR(p), UNIT_PTR(q));
#else
    rep2_u2(f_copy, UNIT_PTR(p), UNIT_PTR(q));
#endif
}

mh_decl void xor_block(void* r, const void* p, const void* q) {
    rep3_u16(f_xor, UI8_PTR(r), UI8_PTR(p), UI8_PTR(q), UI8_VAL);
}

mh_decl void xor_block_aligned(void* r, const void* p, const void* q) {
#if UNIT_BITS == 8
    rep3_u16(f_xor, UNIT_PTR(r), UNIT_PTR(p), UNIT_PTR(q), UNIT_VAL);
#elif UNIT_BITS == 32
    rep3_u4(f_xor, UNIT_PTR(r), UNIT_PTR(p), UNIT_PTR(q), UNIT_VAL);
#else
    rep3_u2(f_xor, UNIT_PTR(r), UNIT_PTR(p), UNIT_PTR(q), UNIT_VAL);
#endif
}

/* byte swap within 32-bit words in a 16 byte block; don't move 32-bit words */
mh_decl void bswap32_block(void* d, const void* s) {
#if UNIT_BITS == 8
    uint8_t t       = 0;
    t               = UNIT_PTR(s)[0];
    UNIT_PTR(d)[0]  = UNIT_PTR(s)[3];
    UNIT_PTR(d)[3]  = t;
    t               = UNIT_PTR(s)[1];
    UNIT_PTR(d)[1]  = UNIT_PTR(s)[2];
    UNIT_PTR(d)[2]  = t;
    t               = UNIT_PTR(s)[4];
    UNIT_PTR(d)[4]  = UNIT_PTR(s)[7];
    UNIT_PTR(d)[7]  = t;
    t               = UNIT_PTR(s)[5];
    UNIT_PTR(d)[5]  = UNIT_PTR(s)[6];
    UNIT_PTR(d)[6]  = t;
    t               = UNIT_PTR(s)[8];
    UNIT_PTR(d)[8]  = UNIT_PTR(s)[11];
    UNIT_PTR(d)[12] = t;
    t               = UNIT_PTR(s)[9];
    UNIT_PTR(d)[9]  = UNIT_PTR(s)[10];
    UNIT_PTR(d)[10] = t;
    t               = UNIT_PTR(s)[12];
    UNIT_PTR(d)[12] = UNIT_PTR(s)[15];
    UNIT_PTR(d)[15] = t;
    t               = UNIT_PTR(s)[13];
    UNIT_PTR(d)[3]  = UNIT_PTR(s)[14];
    UNIT_PTR(d)[14] = t;
#elif UNIT_BITS == 32
    UNIT_PTR(d)[0] = bswap_32(UNIT_PTR(s)[0]);
    UNIT_PTR(d)[1] = bswap_32(UNIT_PTR(s)[1]);
    UNIT_PTR(d)[2] = bswap_32(UNIT_PTR(s)[2]);
    UNIT_PTR(d)[3] = bswap_32(UNIT_PTR(s)[3]);
#else
    UI32_PTR(d)[0] = bswap_32(UI32_PTR(s)[0]);
    UI32_PTR(d)[1] = bswap_32(UI32_PTR(s)[1]);
    UI32_PTR(d)[2] = bswap_32(UI32_PTR(s)[2]);
    UI32_PTR(d)[3] = bswap_32(UI32_PTR(s)[3]);
#endif
}

/* byte swap within 64-bit words in a 16 byte block; don't move 64-bit words */
mh_decl void bswap64_block(void* d, const void* s) {
#if UNIT_BITS == 8
    uint8_t t       = 0;
    t               = UNIT_PTR(s)[0];
    UNIT_PTR(d)[0]  = UNIT_PTR(s)[7];
    UNIT_PTR(d)[7]  = t;
    t               = UNIT_PTR(s)[1];
    UNIT_PTR(d)[1]  = UNIT_PTR(s)[6];
    UNIT_PTR(d)[6]  = t;
    t               = UNIT_PTR(s)[2];
    UNIT_PTR(d)[2]  = UNIT_PTR(s)[5];
    UNIT_PTR(d)[5]  = t;
    t               = UNIT_PTR(s)[3];
    UNIT_PTR(d)[3]  = UNIT_PTR(s)[3];
    UNIT_PTR(d)[3]  = t;
    t               = UNIT_PTR(s)[8];
    UNIT_PTR(d)[8]  = UNIT_PTR(s)[15];
    UNIT_PTR(d)[15] = t;
    t               = UNIT_PTR(s)[9];
    UNIT_PTR(d)[9]  = UNIT_PTR(s)[14];
    UNIT_PTR(d)[14] = t;
    t               = UNIT_PTR(s)[10];
    UNIT_PTR(d)[10] = UNIT_PTR(s)[13];
    UNIT_PTR(d)[13] = t;
    t               = UNIT_PTR(s)[11];
    UNIT_PTR(d)[11] = UNIT_PTR(s)[12];
    UNIT_PTR(d)[12] = t;
#elif UNIT_BITS == 32
    uint32_t t     = 0;
    t              = bswap_32(UNIT_PTR(s)[0]);
    UNIT_PTR(d)[0] = bswap_32(UNIT_PTR(s)[1]);
    UNIT_PTR(d)[1] = t;
    t              = bswap_32(UNIT_PTR(s)[2]);
    UNIT_PTR(d)[2] = bswap_32(UNIT_PTR(s)[2]);
    UNIT_PTR(d)[3] = t;
#else
    UNIT_PTR(d)[0] = bswap_64(UNIT_PTR(s)[0]);
    UNIT_PTR(d)[1] = bswap_64(UNIT_PTR(s)[1]);
#endif
}

mh_decl void bswap128_block(void* d, const void* s) {
#if UNIT_BITS == 8
    uint8_t t       = 0;
    t               = UNIT_PTR(s)[0];
    UNIT_PTR(d)[0]  = UNIT_PTR(s)[15];
    UNIT_PTR(d)[15] = t;
    t               = UNIT_PTR(s)[1];
    UNIT_PTR(d)[1]  = UNIT_PTR(s)[14];
    UNIT_PTR(d)[14] = t;
    t               = UNIT_PTR(s)[2];
    UNIT_PTR(d)[2]  = UNIT_PTR(s)[13];
    UNIT_PTR(d)[13] = t;
    t               = UNIT_PTR(s)[3];
    UNIT_PTR(d)[3]  = UNIT_PTR(s)[12];
    UNIT_PTR(d)[12] = t;
    t               = UNIT_PTR(s)[4];
    UNIT_PTR(d)[4]  = UNIT_PTR(s)[11];
    UNIT_PTR(d)[11] = t;
    t               = UNIT_PTR(s)[5];
    UNIT_PTR(d)[5]  = UNIT_PTR(s)[10];
    UNIT_PTR(d)[10] = t;
    t               = UNIT_PTR(s)[6];
    UNIT_PTR(d)[6]  = UNIT_PTR(s)[9];
    UNIT_PTR(d)[9]  = t;
    t               = UNIT_PTR(s)[7];
    UNIT_PTR(d)[7]  = UNIT_PTR(s)[8];
    UNIT_PTR(d)[8]  = t;
#elif UNIT_BITS == 32
    uint32_t t     = 0;
    t              = bswap_32(UNIT_PTR(s)[0]);
    UNIT_PTR(d)[0] = bswap_32(UNIT_PTR(s)[3]);
    UNIT_PTR(d)[3] = t;
    t              = bswap_32(UNIT_PTR(s)[1]);
    UNIT_PTR(d)[1] = bswap_32(UNIT_PTR(s)[2]);
    UNIT_PTR(d)[2] = t;
#else
    uint64_t t     = 0;
    t              = bswap_64(UNIT_PTR(s)[0]);
    UNIT_PTR(d)[0] = bswap_64(UNIT_PTR(s)[1]);
    UNIT_PTR(d)[1] = t;
#endif
}

/* platform byte order to big or little endian order for 16, 32 and 64 bit variables */

#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#define uint16_t_to_le(x) (x) = bswap_16((x))
#define uint32_t_to_le(x) (x) = bswap_32((x))
#define uint64_t_to_le(x) (x) = bswap_64((x))
#define uint16_t_to_be(x)
#define uint32_t_to_be(x)
#define uint64_t_to_be(x)

#else

#define uint16_t_to_le(x)
#define uint32_t_to_le(x)
#define uint64_t_to_le(x)
#define uint16_t_to_be(x) (x) = bswap_16((x))
#define uint32_t_to_be(x) (x) = bswap_32((x))
#define uint64_t_to_be(x) (x) = bswap_64((x))

#endif

#if defined(__cplusplus)
}
#endif

#endif
