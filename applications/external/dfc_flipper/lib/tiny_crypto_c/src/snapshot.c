/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "snapshot_internal.h"
#include "pki_storage_internal.h"
#include <stdint.h>
#include <string.h>

/* Called once the slot is FREE with no readers. Zeroing the whole slot drops
 * every reference to borrowed storage and leaves the zero-initialized FREE
 * state, since TC_SNAPSHOT_FREE is 0. */
static void recycle(const tc_snapshot_slot* slot)
{
  memset(slot->object, 0, slot->size);
}

TC_TLV_result tc_snapshot_prepare(const tc_snapshot_slot* slot)
{
  if (!slot->object)
    return TC_TLV_ARGUMENT;
  if (*slot->state != TC_SNAPSHOT_FREE || *slot->readers)
    return TC_TLV_LIMIT;
  *slot->state = TC_SNAPSHOT_PREPARED;
  return TC_TLV_OK;
}

TC_TLV_result tc_snapshot_discard(const tc_snapshot_slot* slot)
{
  if (!slot->object || *slot->state != TC_SNAPSHOT_PREPARED || *slot->readers)
    return TC_TLV_ARGUMENT;
  *slot->state = TC_SNAPSHOT_FREE;
  recycle(slot);
  return TC_TLV_OK;
}

TC_TLV_result tc_snapshot_publish_check(const tc_snapshot_store* store,
                                        const tc_snapshot_slot* next,
                                        const tc_snapshot_slot* previous)
{
  /* Store and slots must be disjoint so a state change through one pointer
   * never rewrites another. */
  if (!store->object || !next->object ||
      !tc_pki_storage_separate(store->object, store->size, next->object, next->size))
    return TC_TLV_ARGUMENT;
  if (previous->object &&
      (!tc_pki_storage_separate(previous->object, previous->size, store->object, store->size) ||
       !tc_pki_storage_separate(previous->object, previous->size, next->object, next->size)))
    return TC_TLV_ARGUMENT;
  if (*next->state != TC_SNAPSHOT_PREPARED || *next->readers ||
      (previous->object && *previous->state != TC_SNAPSHOT_CURRENT))
    return TC_TLV_ARGUMENT;
  return TC_TLV_OK;
}

TC_TLV_result tc_snapshot_publish_commit(const tc_snapshot_store* store,
                                         const tc_snapshot_slot* next,
                                         const tc_snapshot_slot* previous, size_t expected)
{
  if (expected != *store->revision)
    return TC_TLV_INVALID;
  if (*store->revision == SIZE_MAX)
    return TC_TLV_LIMIT;
  *next->state = TC_SNAPSHOT_CURRENT;
  ++*store->revision;
  if (previous->object) {
    if (*previous->readers) {
      *previous->state = TC_SNAPSHOT_RETIRED;
    } else {
      *previous->state = TC_SNAPSHOT_FREE;
      recycle(previous);
    }
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_snapshot_acquire(const tc_snapshot_store* store, const tc_snapshot_slot* current,
                                  const void* out, size_t out_size)
{
  if (!store->object || !out || !tc_pki_storage_separate(store->object, store->size, out, out_size))
    return TC_TLV_ARGUMENT;
  if (!current->object)
    return TC_TLV_END;
  if (!tc_pki_storage_separate(store->object, store->size, current->object, current->size) ||
      !tc_pki_storage_separate(current->object, current->size, out, out_size) ||
      *current->state != TC_SNAPSHOT_CURRENT)
    return TC_TLV_ARGUMENT;
  if (*current->readers == SIZE_MAX)
    return TC_TLV_LIMIT;
  ++*current->readers;
  return TC_TLV_OK;
}

TC_TLV_result tc_snapshot_release(const tc_snapshot_slot* slot)
{
  if (!slot->object || !tc_snapshot_held(*slot->state, *slot->readers))
    return TC_TLV_ARGUMENT;
  --*slot->readers;
  if (!*slot->readers && *slot->state == TC_SNAPSHOT_RETIRED) {
    *slot->state = TC_SNAPSHOT_FREE;
    recycle(slot);
  }
  return TC_TLV_OK;
}
