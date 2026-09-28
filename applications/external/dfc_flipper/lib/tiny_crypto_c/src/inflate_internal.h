/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_INFLATE_INTERNAL_H_
#define TC_INFLATE_INTERNAL_H_
#include <tiny_crypto/tlv.h>
#include <tiny_crypto/gzip.h>
enum { TC_INFLATE_CODE_BITS = TC_GZIP_CODE_BITS, TC_INFLATE_LITERAL_CODES = TC_GZIP_LITERAL_CODES,
  TC_INFLATE_DISTANCE_CODES = TC_GZIP_DISTANCE_CODES };
typedef enum {
  TC_INFLATE_COMPLETE_TREE, TC_INFLATE_LITERAL_TREE, TC_INFLATE_DISTANCE_TREE
} tc_inflate_tree_kind;
typedef TC_GZIP_tree tc_inflate_tree;
typedef struct {
  TC_bytes input;
  size_t offset;
  unsigned bit;
  size_t* work;
} tc_inflate_bits;
typedef TC_GZIP_workspace tc_inflate_tables;
typedef struct { uint8_t* data; size_t capacity, length; } tc_inflate_output;
/* Decode one DEFLATE stream and advance to the next byte boundary.
 * Input, tables and output are disjoint. Output and cursors are scratch on
 * failure; a public wrapper must clear provisional output before returning.
 * Existing output before the initial length is excluded from stream history. */
TC_TLV_result tc_inflate_decode(tc_inflate_bits* bits, tc_inflate_tables* tables,
    tc_inflate_output* output);
/* Decode all GZIP members. Output remains provisional until all checks pass. */
TC_TLV_result tc_gzip_decode(TC_bytes input, tc_inflate_tables* tables,
    tc_inflate_output* output, size_t* work);
/* type is BTYPE: 1 for fixed codes, 2 for dynamic codes. */
TC_TLV_result tc_inflate_tables_read(tc_inflate_bits* bits, unsigned type, tc_inflate_tables* tables);

/* Read at most 16 bits, least-significant bit first. Symbol codes use the
 * same input reader but assemble their value most-significant bit first.
 * Scratch cursors may advance on failure; outputs change only on OK. */
TC_TLV_result tc_inflate_bits_read(tc_inflate_bits* bits, unsigned count, unsigned* out);
TC_TLV_result tc_inflate_symbol(tc_inflate_bits* bits, const tc_inflate_tree* tree, unsigned* out);

/* Internal storage is disjoint and writable; count is the alphabet size.
 * Output is scratch and may change on failure. Work is charged before loops. */
TC_TLV_result tc_inflate_tree_build(const uint8_t* lengths, size_t count,
    tc_inflate_tree_kind kind, tc_inflate_tree* tree, size_t* work);
#endif
