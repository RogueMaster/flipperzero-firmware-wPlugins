/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_INFLATE_INTERNAL_H_
#define TC_INFLATE_INTERNAL_H_
#include <tiny_crypto/gzip.h>

/* Internal DEFLATE and GZIP stages. TC_GZIP_decode validates pointers and
 * disjoint storage once. These helpers trust that validation and check only
 * received data and the work budget. Every pointer argument is non-NULL.
 * Input, workspace, output and the work counter are disjoint. */

typedef enum {
  TC_INFLATE_COMPLETE_TREE,
  TC_INFLATE_LITERAL_TREE,
  TC_INFLATE_DISTANCE_TREE
} tc_inflate_tree_kind;

/* Input cursor. input.data is non-NULL when input.length is nonzero.
 * offset <= input.length, bit < 8 and bit is zero when offset == input.length.
 * work is the remaining budget and is non-NULL. */
typedef struct {
  TC_bytes input;
  size_t offset;
  unsigned bit;
  size_t* work;
} tc_inflate_bits;

/* Decoded bytes occupy the first length bytes of buffer.
 * length <= buffer.capacity, and buffer.data is non-NULL when capacity is nonzero. */
typedef struct {
  TC_buffer buffer;
  size_t length;
} tc_inflate_output;

/* Decode one DEFLATE stream and advance to the next byte boundary.
 * Output and cursors are scratch on failure. A public wrapper must clear
 * provisional output before returning. Existing output before the initial
 * length is excluded from stream history. */
TC_GZIP_result tc_inflate_decode(tc_inflate_bits* bits, TC_GZIP_workspace* tables,
                                 tc_inflate_output* output);
/* Decode all GZIP members. Output remains provisional until all checks pass. */
TC_GZIP_result tc_gzip_decode(TC_bytes input, TC_GZIP_workspace* tables, tc_inflate_output* output,
                              size_t* work);
/* type is BTYPE and must be 1 for fixed codes or 2 for dynamic codes. */
TC_GZIP_result tc_inflate_tables_read(tc_inflate_bits* bits, unsigned type,
                                      TC_GZIP_workspace* tables);

/* Read count <= 16 bits, least-significant bit first. Symbol codes use the
 * same input reader but assemble their value most-significant bit first.
 * tree->symbols points at storage built by tc_inflate_tree_build.
 * Scratch cursors may advance on failure. out changes only on OK. */
TC_GZIP_result tc_inflate_bits_read(tc_inflate_bits* bits, unsigned count, unsigned* out);
TC_GZIP_result tc_inflate_symbol(tc_inflate_bits* bits, const TC_GZIP_tree* tree, unsigned* out);

/* count is the alphabet size, from 1 to TC_GZIP_LITERAL_CODES. tree->symbols
 * holds count entries. Output is scratch and may change on failure. Work is
 * charged before loops. */
TC_GZIP_result tc_inflate_tree_build(const uint8_t* lengths, size_t count,
                                     tc_inflate_tree_kind kind, TC_GZIP_tree* tree, size_t* work);
#endif
