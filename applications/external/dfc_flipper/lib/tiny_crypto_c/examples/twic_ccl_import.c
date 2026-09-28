/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "twic_ccl_import.h"
#include <string.h>

static TC_TWIC_CCL_result fail(ExampleTwicCclImport* state, TC_TWIC_CCL_result result)
{
  state->error = result;
  TC_MD5_ctx_clear(&state->checksum);
  return result;
}

TC_TWIC_CCL_result example_twic_ccl_import_init(ExampleTwicCclImport* state,
    const uint8_t expected[TC_MD5_DIGESTLEN], size_t max_bytes, size_t max_records,
    TC_TWIC_CCL_visit append, void* context)
{
  if (!state || !expected || !append) return TC_TWIC_CCL_ARGUMENT;
  memset(state,0,sizeof *state);
  memcpy(state->expected,expected,sizeof state->expected);
  TC_MD5_init(&state->checksum);
  return TC_TWIC_CCL_stream_init(&state->reader,max_bytes,max_records,append,context);
}

TC_TWIC_CCL_result example_twic_ccl_import_update(ExampleTwicCclImport* state, TC_bytes chunk)
{
  if (!state) return TC_TWIC_CCL_ARGUMENT;
  if (state->error != TC_TWIC_CCL_OK) return state->error;
  if (state->finished) return fail(state,TC_TWIC_CCL_ARGUMENT);
  TC_TWIC_CCL_result result = TC_TWIC_CCL_stream_update(&state->reader,chunk);
  if (result != TC_TWIC_CCL_OK) return fail(state,result);
  if (TC_MD5_update(&state->checksum,chunk.data,chunk.length) != TC_OK)
    return fail(state,TC_TWIC_CCL_ARGUMENT);
  return TC_TWIC_CCL_OK;
}

TC_TWIC_CCL_result example_twic_ccl_import_finish(ExampleTwicCclImport* state,
    const TC_TWIC_CCL_source* staged, const TC_TWIC_CCL_metadata* metadata,
    TC_TWIC_CCL_snapshot* slot)
{
  uint8_t digest[TC_MD5_DIGESTLEN];
  TC_TWIC_CCL_index index;
  if (!state) return TC_TWIC_CCL_ARGUMENT;
  if (state->error != TC_TWIC_CCL_OK) return state->error;
  if (!staged || !metadata || !slot || state->finished) return fail(state,TC_TWIC_CCL_ARGUMENT);
  TC_TWIC_CCL_result result = TC_TWIC_CCL_stream_finish(&state->reader);
  if (result != TC_TWIC_CCL_OK) return fail(state,result);
  if (TC_MD5_final(&state->checksum,digest) != TC_OK) return fail(state,TC_TWIC_CCL_ARGUMENT);
  if (TC_ct_equal(digest,state->expected,sizeof digest) != TC_OK)
    return fail(state,TC_TWIC_CCL_CHECKSUM_MISMATCH);
  if (staged->count != state->reader.records) return fail(state,TC_TWIC_CCL_INVALID);
  result = TC_TWIC_CCL_index_prepare(staged,state->reader.records,&index);
  if (result == TC_TWIC_CCL_OK) result = TC_TWIC_CCL_store_prepare(slot,&index,metadata);
  if (result != TC_TWIC_CCL_OK) return fail(state,result);
  state->finished = 1;
  TC_MD5_ctx_clear(&state->checksum);
  return TC_TWIC_CCL_OK;
}

void example_twic_ccl_import_clear(ExampleTwicCclImport* state)
{
  if (!state) return;
  TC_secure_zero(state,sizeof *state);
  state->error = TC_TWIC_CCL_ARGUMENT;
}
