/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_TWIC_CCL_IMPORT_H_
#define EXAMPLE_TWIC_CCL_IMPORT_H_
#include <tiny_crypto/twic_ccl.h>
#include <tiny_crypto/md5.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_TWIC_CCL_stream reader;
  struct TC_MD5_ctx checksum;
  uint8_t expected[TC_MD5_DIGESTLEN];
  TC_TWIC_CCL_result error;
  uint8_t finished;
} ExampleTwicCclImport;

/* expected is the decoded MD5 from a trusted retrieval of the checksum file.
 * The callback appends every record to private staging storage. Inputs, state,
 * callback storage and outputs require disjoint storage. */
TC_TWIC_CCL_result example_twic_ccl_import_init(ExampleTwicCclImport* state,
    const uint8_t expected[TC_MD5_DIGESTLEN], size_t max_bytes, size_t max_records,
    TC_TWIC_CCL_visit append, void* context);
TC_TWIC_CCL_result example_twic_ccl_import_update(ExampleTwicCclImport* state, TC_bytes chunk);
/* After download completion, sort the staged keys and expose the immutable
 * source. Retain duplicates so its count equals the parsed record count.
 * Metadata must describe this download. Only success prepares slot; publication
 * follows application persistence and policy checks under the store lock.
 * Failures are sticky and leave slot unchanged. Discard staging and warn on
 * download, parse, checksum or storage failure, keeping the active list. */
TC_TWIC_CCL_result example_twic_ccl_import_finish(ExampleTwicCclImport* state,
    const TC_TWIC_CCL_source* staged, const TC_TWIC_CCL_metadata* metadata,
    TC_TWIC_CCL_snapshot* slot);
void example_twic_ccl_import_clear(ExampleTwicCclImport* state);

#ifdef __cplusplus
}
#endif
#endif
