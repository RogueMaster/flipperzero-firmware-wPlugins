/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SOURCE_HASH_INTERNAL_H_
#define TINY_CRYPTO_SOURCE_HASH_INTERNAL_H_
#include "source_internal.h"
#include "hash_dispatch_internal.h"
#include <string.h>

typedef struct {
  tc_source_reader* reader;
  tc_hash_workspace hash;
  uint64_t offset, end;
  TC_hash_algorithm algorithm;
  int active;
} tc_source_hash;

/* State, source reader, window and outputs are disjoint. Hold the reader and its
 * immutable source through finalization. Clear state when abandoning a stream. */
static inline TC_result tc_source_hash_init(tc_source_hash* state,
    tc_source_reader* reader, TC_hash_algorithm algorithm, uint64_t offset, uint64_t length)
{
  if (!state || !reader || offset > reader->source.length ||
      length > reader->source.length - offset) return TC_RESULT_ARGUMENT;
  if (!tc_hash_available(algorithm)) return TC_RESULT_UNSUPPORTED;
  tc_source_hash parsed;
  memset(&parsed,0,sizeof parsed);
  parsed.reader = reader;
  parsed.offset = offset;
  parsed.end = offset + length;
  parsed.algorithm = algorithm;
  if (tc_hash_init(algorithm,&parsed.hash) != TC_OK) {
    TC_secure_zero(&parsed,sizeof parsed);
    return TC_RESULT_ERROR;
  }
  parsed.active = 1;
  *state = parsed;
  TC_secure_zero(&parsed,sizeof parsed);
  return TC_RESULT_OK;
}

/* Hash at most max_bytes per call. The reader separately bounds physical I/O.
 * Failures abandon the hash. complete changes only on success. */
static inline TC_result tc_source_hash_step(tc_source_hash* state,
    size_t max_bytes, int* complete)
{
  if (!state || !state->active || !state->reader || !complete || !max_bytes ||
      state->offset > state->end) return TC_RESULT_ARGUMENT;
  size_t left = max_bytes;
  while (left && state->offset < state->end) {
    TC_bytes chunk;
    uint64_t remaining = state->end - state->offset;
    const size_t request = remaining < left ? (size_t)remaining : left;
    TC_result result = tc_source_reader_view(state->reader,state->offset,request,&chunk);
    if (result == TC_RESULT_OK && tc_hash_update(state->algorithm,&state->hash,chunk) != TC_OK)
      result = TC_RESULT_ERROR;
    if (result != TC_RESULT_OK) {
      TC_secure_zero(&state->hash,sizeof state->hash);
      state->active = 0;
      return result;
    }
    state->offset += chunk.length;
    left -= chunk.length;
  }
  *complete = state->offset == state->end;
  return TC_RESULT_OK;
}

/* Finalize only after the complete source range has been consumed. */
static inline TC_result tc_source_hash_final(tc_source_hash* state, TC_buffer output)
{
  enum { MAX_DIGEST_BYTES = 64 };
  uint8_t digest[MAX_DIGEST_BYTES];
  tc_hash_info info;
  if (!state || !state->active || state->offset != state->end || !output.data ||
      !tc_hash_info_get(state->algorithm,&info)) return TC_RESULT_ARGUMENT;
  if (output.capacity < info.digest_length) return TC_RESULT_LIMIT;
  TC_status result = tc_hash_final(state->algorithm,&state->hash,digest);
  state->active = 0;
  if (result == TC_OK) memcpy(output.data,digest,info.digest_length);
  TC_secure_zero(digest,sizeof digest);
  return result == TC_OK ? TC_RESULT_OK : TC_RESULT_ERROR;
}
#endif
