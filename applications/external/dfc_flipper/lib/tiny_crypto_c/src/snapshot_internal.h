/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_SNAPSHOT_INTERNAL_H_
#define TC_SNAPSHOT_INTERNAL_H_
#include <tiny_crypto/snapshot.h>
#include <tiny_crypto/tlv.h>

/* Callers validate storage and domain-specific payloads before changing state.
 * Reclaim payloads after a successful transition to FREE. */
static inline TC_TLV_result tc_snapshot_prepare(TC_snapshot_state* state, size_t readers)
{
  if (*state != TC_SNAPSHOT_FREE || readers) return TC_TLV_LIMIT;
  *state = TC_SNAPSHOT_PREPARED;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_snapshot_discard(TC_snapshot_state* state, size_t readers)
{
  if (*state != TC_SNAPSHOT_PREPARED || readers) return TC_TLV_ARGUMENT;
  *state = TC_SNAPSHOT_FREE;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_snapshot_publish(TC_snapshot_state* next, size_t readers,
    TC_snapshot_state* previous, size_t previous_readers, size_t expected, size_t* revision)
{
  if (*next != TC_SNAPSHOT_PREPARED || readers) return TC_TLV_ARGUMENT;
  if (expected != *revision) return TC_TLV_INVALID;
  if (*revision == SIZE_MAX) return TC_TLV_LIMIT;
  if (previous && *previous != TC_SNAPSHOT_CURRENT) return TC_TLV_ARGUMENT;
  *next = TC_SNAPSHOT_CURRENT;
  ++*revision;
  if (previous) *previous = previous_readers ? TC_SNAPSHOT_RETIRED : TC_SNAPSHOT_FREE;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_snapshot_acquire(TC_snapshot_state state, size_t* readers)
{
  if (state != TC_SNAPSHOT_CURRENT) return TC_TLV_ARGUMENT;
  if (*readers == SIZE_MAX) return TC_TLV_LIMIT;
  ++*readers;
  return TC_TLV_OK;
}

static inline TC_TLV_result tc_snapshot_release(TC_snapshot_state* state, size_t* readers)
{
  if (!*readers || (*state != TC_SNAPSHOT_CURRENT && *state != TC_SNAPSHOT_RETIRED))
    return TC_TLV_ARGUMENT;
  --*readers;
  if (!*readers && *state == TC_SNAPSHOT_RETIRED) *state = TC_SNAPSHOT_FREE;
  return TC_TLV_OK;
}
#endif
