/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "twic_ccl_storage.h"

static TC_status read_key(void* context, size_t position, TC_bytes* out)
{
  ExampleTwicCclStorage* storage = context;
  /* Division bounds the offset multiplication even on a narrow size_t. */
  if (position >= storage->length / sizeof storage->key) return TC_ERROR;
  if (storage->read_at(storage->context,position * sizeof storage->key,
      storage->key,sizeof storage->key) != TC_OK) return TC_ERROR;
  *out = (TC_bytes){storage->key,sizeof storage->key};
  return TC_OK;
}

TC_TWIC_CCL_result example_twic_ccl_open(ExampleTwicCclStorage* storage,
    size_t max_records, TC_TWIC_CCL_index* out)
{
  if (!storage || !storage->read_at || !out) return TC_TWIC_CCL_ARGUMENT;
  if (storage->length % sizeof storage->key) return TC_TWIC_CCL_INVALID;
  const TC_TWIC_CCL_source source = {
    storage,storage->length / sizeof storage->key,read_key
  };
  return TC_TWIC_CCL_index_prepare(&source,max_records,out);
}

TC_TWIC_CCL_result example_check_twic_cancellation(TC_TWIC_CCL_store* store,
    const TC_TWIC_CCL_freshness_policy* policy, uint64_t warn_age,
    TC_bytes fascn, size_t max_reads, ExampleTwicCclResult* out)
{
  TC_TWIC_CCL_snapshot* snapshot;
  ExampleTwicCclResult checked = {0,0};
  if (!store || !policy || !out) return TC_TWIC_CCL_ARGUMENT;
  TC_TWIC_CCL_result result = TC_TWIC_CCL_store_acquire(store,&snapshot);
  if (result != TC_TWIC_CCL_OK) return result;
  result = TC_TWIC_CCL_check_freshness(&snapshot->metadata,policy);
  if (result == TC_TWIC_CCL_OK) {
    checked.age_warning = policy->now - snapshot->metadata.published_at > warn_age;
    result = TC_TWIC_CCL_snapshot_contains(snapshot,fascn,max_reads,&checked.listed);
  }
  TC_TWIC_CCL_result released = TC_TWIC_CCL_store_release(snapshot);
  if (result != TC_TWIC_CCL_OK) return result;
  if (released != TC_TWIC_CCL_OK) return released;
  *out = checked;
  return TC_TWIC_CCL_OK;
}
