/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_TWIC_CCL_STORAGE_H_
#define EXAMPLE_TWIC_CCL_STORAGE_H_
#include <tiny_crypto/twic_ccl.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void* context;
  /* Read exactly length bytes from flash or a provisioned file. */
  TC_status (*read_at)(void* context, size_t offset, uint8_t* out, size_t length);
  size_t length;
  uint8_t key[TC_TWIC_CCL_FASCN_BYTES];
} ExampleTwicCclStorage;

/* Storage contains packed, sorted 25-byte FASC-Ns from a completed CCL import.
 * Keep storage and its backing image alive and unchanged while the index is
 * used. Serialize access to this instance's reusable read buffer. */
TC_TWIC_CCL_result example_twic_ccl_open(ExampleTwicCclStorage* storage,
    size_t max_records, TC_TWIC_CCL_index* out);

typedef struct { int listed, age_warning; } ExampleTwicCclResult;

/* Caller holds the store/source lock through this call and its validity decision.
 * policy.max_age is the application's acceptance limit. warn_age is a separate
 * publication-age warning threshold in seconds; UINT64_MAX disables it.
 * Store, policy, query and output storage must be disjoint. Outputs change only
 * on OK. Card authentication, expiration and access rights are checked by the
 * calling credential workflow. Every acquired snapshot is released. */
TC_TWIC_CCL_result example_check_twic_cancellation(TC_TWIC_CCL_store* store,
    const TC_TWIC_CCL_freshness_policy* policy, uint64_t warn_age,
    TC_bytes fascn, size_t max_reads, ExampleTwicCclResult* out);

#ifdef __cplusplus
}
#endif
#endif
