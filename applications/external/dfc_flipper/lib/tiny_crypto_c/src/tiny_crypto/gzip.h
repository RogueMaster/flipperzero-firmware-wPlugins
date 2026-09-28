/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_GZIP_H_
#define TINY_CRYPTO_GZIP_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TC_GZIP_CODE_BITS = 15, TC_GZIP_LITERAL_CODES = 288, TC_GZIP_DISTANCE_CODES = 32 };
/* Workspace members are private. No initialization is required. */
typedef struct { uint16_t counts[TC_GZIP_CODE_BITS + 1]; uint16_t* symbols; } TC_GZIP_tree;
typedef struct {
  TC_GZIP_tree literal, distance;
  uint16_t literal_symbols[TC_GZIP_LITERAL_CODES], distance_symbols[TC_GZIP_DISTANCE_CODES];
  uint8_t lengths[TC_GZIP_LITERAL_CODES + TC_GZIP_DISTANCE_CODES];
} TC_GZIP_workspace;
typedef enum {
  TC_GZIP_OK, TC_GZIP_INVALID, TC_GZIP_LIMIT, TC_GZIP_UNSUPPORTED, TC_GZIP_ARGUMENT
} TC_GZIP_result;

/* Decode complete GZIP members with CRC and size checks. Concatenated members
 * share capacity and the remaining work budget. Trailing non-member bytes fail.
 * Input, output capacity, workspace, work and output_length must be disjoint.
 * NULL output is allowed for zero capacity. Bad arguments preserve storage.
 * Processing failures wipe output capacity; output_length changes only on OK.
 * Workspace is wiped after processing. Work measures bounded decoding steps,
 * including bits, table entries and output/checksum bytes. Requires GZIP support. */
TC_GZIP_result TC_GZIP_decode(const uint8_t* input, size_t input_length,
    uint8_t* output, size_t capacity, TC_GZIP_workspace* workspace,
    size_t* work, size_t* output_length);
#ifdef __cplusplus
}
#endif
#endif
