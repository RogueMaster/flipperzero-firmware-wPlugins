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
Issue Date: 11/01/2011

 I am grateful for the work done by Mark Rodenkirch and Jason Papadopoulos
 in helping to remove a bug in the operation of this code on big endian
 systems when fast buffer operations are enabled.
 ---------------------------------------------------------------------------

 An implementation of field multiplication in the Galois Field GF(2^128)

 A polynomial representation is used for the field with the coefficients
 held in bit sequences in which the bit numbers are the powers of x that
 a bit represents. The field polynomial used is (x^128+x^7+x^2+x+1).

 The obvious way of representing field elements in a computer system is
 to map 'x' in the field to the binary integer '2'. But this was way too
 obvious for cryptographers!

 Here bytes are numbered in their memory order and bits within bytes are
 numbered according to their integer numeric significance (that is as is
 now normal with bit 0 representing unity). The term 'little endian'
 will then used to describe mappings where numeric (power of 2) or field
 (power of x) significance increases with increasing bit or byte numbers
 with 'big endian' being used to describe the inverse situation.

 The GF bit sequence can then be mapped onto 8-bit bytes in computer
 memory in one of four simple ways:

     A mapping in which x maps to the integer 2 in little endian
     form for both bytes and bits within bytes:

         LL: bit for x^n ==> bit for 2^(n % 8) in byte[n / 8]

     A mapping in which x maps to the integer 2 in big endian form
     for both bytes and bits within bytes:

         BL: bit for x^n ==> bit for 2^(n % 8) in byte[15 - n / 8]

     A little endian mapping for bytes but with the bits within
     bytes in reverse order (big endian bytes):

         LB: bit for x^n ==> bit for 2^(7 - n % 8) in byte[n / 8]

     A big endian mapping for bytes but with the bits within
     bytes in reverse order (big endian bytes):

         BB: bit for x^n ==> bit for 2^(7 - n % 8) in byte[15 - n / 8]

 128-bit field elements are represented by 16 byte buffers but for
 processing efficiency reasons it is often desirable to process arrays
 of bytes using longer types such as, for example, unsigned long values.
 The type used for representing these buffers will be called a 'gf_unit'
 and the buffer itself will be referred to as a 'gf_t' type.

 THe field multiplier is based on the assumption that one of the two
 field elements involved in multiplication will change only relatively
 infrequently, making it worthwhile to precompute tables to speed up
 multiplication by this value.
*/

#ifndef _GF128MUL_H
#define _GF128MUL_H

#include <stdlib.h>
#include <string.h>

#include "brg_endian.h"

/* USER DEFINABLE OPTIONS */
/*  UNIT_BITS sets the size of variables used to process 16 byte buffers
    when the buffer alignment allows this.  When buffers are processed
    in bytes, 16 individual operations are invoolved.  But if, say, such
    a buffer is divided into 4 32 bit variables, it can then be processed
    in 4 operations, making the code typically much faster. In general
    it will pay to use the longest natively supported size, which will
    probably be 32 or 64 bits in 32 and 64 bit systems respectively.
*/

#if defined(UNIT_BITS)
#undef UNIT_BITS
#endif

#if !defined(UNIT_BITS)
#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN
#if 0
#define UNIT_BITS 8
#elif 0
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

#include "mode_hdr.h"

/* Choose the Galois Field representation to use (see above) */
#if 0
#define GF_MODE_LL
#elif 0
#define GF_MODE_BL
#elif 1
#define GF_MODE_LB /* the representation used by GCM */
#elif 0
#define GF_MODE_BB
#else
#error mode is not defined
#endif

/*  Table sizes for GF(128) Multiply.  Normally larger tables give
    higher speed but cache loading might change this. Normally only
    one table size (or none at all) will be specified here
*/
#if 0
#define TABLES_64K
#endif
#if 0
#define TABLES_8K
#endif
#if 0
#define TABLES_4K
#endif
#if 0
#define TABLES_256
#endif

/* END OF USER DEFINABLE OPTIONS */

#if !(defined(TABLES_64K) || defined(TABLES_8K) || defined(TABLES_4K) || defined(TABLES_256))
#define NO_TABLES
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define GF_BYTE_LEN 16
#define GF_UNIT_LEN (GF_BYTE_LEN / (UNIT_BITS >> 3))

UNIT_TYPEDEF(gf_unit_t, UNIT_BITS);
BUFR_TYPEDEF(gf_t, UNIT_BITS, GF_BYTE_LEN);

/*  Code for conversion between the four different galois field representations
    is optionally available using gf_convert.c
*/

typedef enum {
    REVERSE_NONE  = 0,
    REVERSE_BITS  = 1,
    REVERSE_BYTES = 2
} transform;

void convert_representation(gf_t dest, const gf_t source, transform rev);

void gf_mul(gf_t a, const gf_t b); /* slow field multiply  */

/* types and calls for 64k table driven field multiplier        */

typedef gf_t gf_t64k_a[16][256];
typedef gf_t (*gf_t64k_t)[256];

void init_64k_table(const gf_t g, gf_t64k_t t);
void gf_mul_64k(gf_t a, const gf_t64k_t t, void* r);

/* types and calls for 8k table driven field multiplier        */

typedef gf_t gf_t8k_a[32][16];
typedef gf_t (*gf_t8k_t)[16];

void init_8k_table(const gf_t g, gf_t8k_t t);
void gf_mul_8k(gf_t a, const gf_t8k_t t, gf_t r);

/* types and calls for 8k table driven field multiplier        */

typedef gf_t gf_t4k_a[256];
typedef gf_t(*gf_t4k_t);

void init_4k_table(const gf_t g, gf_t4k_t t);
void gf_mul_4k(gf_t a, const gf_t4k_t t, gf_t r);

/* types and calls for 8k table driven field multiplier        */

typedef gf_t gf_t256_a[16];
typedef gf_t(*gf_t256_t);

void init_256_table(const gf_t g, gf_t256_t t);
void gf_mul_256(gf_t a, const gf_t256_t t, gf_t r);

#if defined(__cplusplus)
}
#endif

#endif
