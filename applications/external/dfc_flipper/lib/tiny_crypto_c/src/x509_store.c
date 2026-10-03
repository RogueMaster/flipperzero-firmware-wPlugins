/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_PATH
#include <tiny_crypto/x509_store.h>
#include "pki_storage_internal.h"
#include "snapshot_internal.h"

static TC_TLV_result array_candidate(void* context, size_t index, size_t* work, TC_bytes* out)
{
  const TC_X509_store_array* array = (const TC_X509_store_array*)context;
  if (!array || !work || !out || index >= array->candidate_count ||
      !tc_pki_storage_separate(out, sizeof *out, &array->candidates[index],
                               sizeof array->candidates[index]))
    return TC_TLV_ARGUMENT;
  if (!*work)
    return TC_TLV_LIMIT;
  --*work;
  *out = array->candidates[index];
  return TC_TLV_OK;
}

static TC_TLV_result array_anchor(void* context, size_t index, size_t* work,
                                  TC_X509_store_anchor* out)
{
  const TC_X509_store_array* array = (const TC_X509_store_array*)context;
  if (!array || !work || !out || index >= array->anchor_count ||
      !tc_pki_storage_separate(out, sizeof *out, &array->anchors[index],
                               sizeof array->anchors[index]))
    return TC_TLV_ARGUMENT;
  if (!*work)
    return TC_TLV_LIMIT;
  --*work;
  *out = array->anchors[index];
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_store_array_source(const TC_X509_store_array* array,
                                         TC_X509_store_source* out)
{
  TC_X509_store_source source;
  if (!array || !out || (array->candidate_count && !array->candidates) ||
      (array->anchor_count && !array->anchors) ||
      !tc_pki_storage_separate(array, sizeof *array, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  source.context = (void*)array;
  source.candidate_count = array->candidate_count;
  source.anchor_count = array->anchor_count;
  source.candidate = array_candidate;
  source.anchor = array_anchor;
  *out = source;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_store_prepare(TC_X509_store_snapshot* slot,
                                    const TC_X509_store_source* source)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  if (!slot || !source || !tc_pki_storage_separate(slot, sizeof *slot, source, sizeof *source) ||
      (source->candidate_count && !source->candidate) || (source->anchor_count && !source->anchor))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_snapshot_prepare(&view);
  if (result == TC_TLV_OK)
    slot->source = *source;
  return result;
}

TC_TLV_result TC_X509_store_discard(TC_X509_store_snapshot* slot)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  return tc_snapshot_discard(&view);
}

TC_TLV_result TC_X509_store_publish(TC_X509_store* store, size_t revision,
                                    TC_X509_store_snapshot* slot)
{
  const tc_snapshot_store store_view = TC_SNAPSHOT_STORE(store);
  const tc_snapshot_slot next = TC_SNAPSHOT_SLOT(slot);
  const tc_snapshot_slot previous = TC_SNAPSHOT_SLOT(store ? store->current : NULL);
  TC_TLV_result result = tc_snapshot_publish_check(&store_view, &next, &previous);
  if (result == TC_TLV_OK)
    result = tc_snapshot_publish_commit(&store_view, &next, &previous, revision);
  if (result == TC_TLV_OK)
    store->current = slot;
  return result;
}

TC_TLV_result TC_X509_store_acquire(TC_X509_store* store, TC_X509_store_snapshot** out)
{
  const tc_snapshot_store store_view = TC_SNAPSHOT_STORE(store);
  const tc_snapshot_slot current = TC_SNAPSHOT_SLOT(store ? store->current : NULL);
  TC_TLV_result result = tc_snapshot_acquire(&store_view, &current, out, sizeof *out);
  if (result == TC_TLV_OK)
    *out = store->current;
  return result;
}

TC_TLV_result TC_X509_store_release(TC_X509_store_snapshot* slot)
{
  const tc_snapshot_slot view = TC_SNAPSHOT_SLOT(slot);
  return tc_snapshot_release(&view);
}
#endif
