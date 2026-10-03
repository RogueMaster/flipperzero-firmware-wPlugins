/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Bounded GZIP decompression of complete members with CRC32 and ISIZE checks.
 * Standards: RFC 1952 (GZIP), RFC 1951 (DEFLATE).
 * Configuration: TC_ENABLE_GZIP.
 * Limitations: complete input only. The output buffer holds the whole
 * decoded result and doubles as back-reference history.
 * Contracts: docs/api.md, including its size_t work units. Guide: docs/gzip.md. */
#ifndef TINY_CRYPTO_GZIP_H_
#define TINY_CRYPTO_GZIP_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TC_GZIP_CODE_BITS = 15, TC_GZIP_LITERAL_CODES = 288, TC_GZIP_DISTANCE_CODES = 32 };
/* Workspace members are private. No initialization is required. */
typedef struct {
  uint16_t counts[TC_GZIP_CODE_BITS + 1];
  uint16_t* symbols;
} TC_GZIP_tree;
typedef struct {
  TC_GZIP_tree literal, distance;
  uint16_t literal_symbols[TC_GZIP_LITERAL_CODES], distance_symbols[TC_GZIP_DISTANCE_CODES];
  uint8_t lengths[TC_GZIP_LITERAL_CODES + TC_GZIP_DISTANCE_CODES];
} TC_GZIP_workspace;
/* Results share the RSA and EC order.
 *   TC_GZIP_INVALID      malformed framing, compressed data, or a CRC32 or
 *                        size mismatch in the received input.
 *   TC_GZIP_LIMIT        output capacity or the work budget ran out.
 *   TC_GZIP_ARGUMENT     NULL pointers or overlapping storage.
 *   TC_GZIP_UNSUPPORTED  a compression method other than deflate. */
typedef TC_result TC_GZIP_result;
#define TC_GZIP_OK TC_RESULT_OK
#define TC_GZIP_INVALID TC_RESULT_INVALID
#define TC_GZIP_LIMIT TC_RESULT_LIMIT
#define TC_GZIP_ARGUMENT TC_RESULT_ARGUMENT
#define TC_GZIP_UNSUPPORTED TC_RESULT_UNSUPPORTED

#if TC_ENABLE_GZIP
/* Decode one or more complete GZIP members (RFC 1952 section 2.3) holding
 * DEFLATE data (RFC 1951 section 3.2), and check each member's CRC32 and
 * ISIZE. FHCRC, when present, is checked too.
 *   input          complete members, borrowed and read-only for the call.
 *                  Empty input and trailing non-member bytes return INVALID.
 *   workspace      caller-owned scratch of sizeof(TC_GZIP_workspace) bytes.
 *                  Needs no initialization and is wiped before any return
 *                  after the argument checks.
 *   work           remaining size_t work budget, reduced as decoding runs:
 *                  one unit per input bit read and per skipped header byte,
 *                  2 * code count + 15 per Huffman table, one per code-length
 *                  entry, one per output byte and one per CRC32 input byte.
 *                  The unspent remainder stays in *work on every result.
 *   output         caller-owned storage for the decoded bytes of all members.
 *                  output.data may be NULL when output.capacity is zero.
 *                  Decoded output doubles as back-reference history, and a
 *                  back reference stays inside its own member.
 *   output_length  receives the total decoded length, only on OK.
 * Input, output, workspace, work and output_length must be pairwise
 * disjoint.
 *
 * TC_GZIP_ARGUMENT     NULL workspace, work or output_length, a span with NULL
 *                      data and a nonzero size, or overlap. Every argument is
 *                      unchanged.
 * TC_GZIP_UNSUPPORTED  a member with a compression method other than
 *                      deflate (CM 8).
 * TC_GZIP_INVALID      a bad magic number, reserved flag bits, malformed
 *                      DEFLATE data, a back reference before the member
 *                      start, truncation, or a CRC32, FHCRC or ISIZE
 *                      mismatch.
 * TC_GZIP_LIMIT        output capacity or the work budget ran out.
 *
 * Every failure after the argument checks wipes output.capacity bytes and
 * leaves output_length unchanged. */
TC_GZIP_result TC_GZIP_decode(TC_bytes input, TC_GZIP_workspace* workspace, size_t* work,
                              TC_buffer output, size_t* output_length);
#endif
#ifdef __cplusplus
}
#endif
#endif
