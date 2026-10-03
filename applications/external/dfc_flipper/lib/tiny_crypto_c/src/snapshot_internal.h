/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_SNAPSHOT_INTERNAL_H_
#define TC_SNAPSHOT_INTERNAL_H_
#include <tiny_crypto/snapshot.h>
#include <tiny_crypto/tlv.h>
#include <stddef.h>

/* Shared lifecycle of the snapshot stores (x509_store.c, twic_ccl.c).
 * Domain wrappers validate their payloads, add domain rules such as the CCL
 * publication-date order, store the typed current pointer and translate the
 * TC_TLV_result status. This core owns state transitions, reader counts,
 * revision checks, store and slot overlap checks and payload recycling.
 *
 * A slot struct has fields named readers and state. Recycling zeroes the
 * whole slot when it becomes FREE without readers, which matches a
 * zero-initialized slot. */
typedef struct {
  void* object;    /* the typed slot, or NULL */
  size_t size;     /* sizeof the typed slot */
  size_t* readers; /* NULL when object is NULL */
  TC_snapshot_state* state;
} tc_snapshot_slot;

/* A store holds the typed current pointer and a revision counter. */
typedef struct {
  const void* object; /* the typed store, or NULL */
  size_t size;
  size_t* revision; /* NULL when object is NULL */
} tc_snapshot_store;

/* Describe a typed slot pointer, which may be NULL. */
#define TC_SNAPSHOT_SLOT(slot)                                                                     \
  ((slot) ? (tc_snapshot_slot){(slot), sizeof *(slot), &(slot)->readers, &(slot)->state}           \
          : (tc_snapshot_slot){NULL, 0, NULL, NULL})

/* Describe a typed store pointer, which may be NULL. */
#define TC_SNAPSHOT_STORE(store)                                                                   \
  ((store) ? (tc_snapshot_store){(store), sizeof *(store), &(store)->revision}                     \
           : (tc_snapshot_store){NULL, 0, NULL})

/* A reader holds a slot while it is CURRENT or RETIRED with readers. */
static inline int tc_snapshot_held(TC_snapshot_state state, size_t readers)
{
  return readers && (state == TC_SNAPSHOT_CURRENT || state == TC_SNAPSHOT_RETIRED);
}

/* FREE without readers to PREPARED. The wrapper validates its payload first
 * and copies it into the slot after OK.
 * Returns OK. ARGUMENT for a NULL slot. LIMIT for any other state or for
 * readers. The slot changes only on OK. */
TC_TLV_result tc_snapshot_prepare(const tc_snapshot_slot* slot);

/* PREPARED without readers to FREE with the payload cleared.
 * Returns OK, or ARGUMENT for a NULL slot, readers or any other state with
 * the slot unchanged. */
TC_TLV_result tc_snapshot_discard(const tc_snapshot_slot* slot);

/* Check that next can replace previous, the store's published slot, which
 * has a NULL object when nothing is published. Wrappers with ordering rules
 * that follow these checks apply them between this call and
 * tc_snapshot_publish_commit.
 * Returns OK. ARGUMENT for a NULL store or next, overlap between the store
 * and both slots, next outside PREPARED or with readers, or previous outside
 * CURRENT. Changes nothing. */
TC_TLV_result tc_snapshot_publish_check(const tc_snapshot_store* store,
                                        const tc_snapshot_slot* next,
                                        const tc_snapshot_slot* previous);

/* Publish next after tc_snapshot_publish_check returned OK for the same
 * arguments. expected must equal the store revision, which then increments.
 * previous becomes RETIRED while it has readers and FREE with its payload
 * cleared otherwise. The wrapper then points the store at next.
 * Returns OK. INVALID for a stale revision. LIMIT for exhausted revision
 * space. Failures change nothing. */
TC_TLV_result tc_snapshot_publish_commit(const tc_snapshot_store* store,
                                         const tc_snapshot_slot* next,
                                         const tc_snapshot_slot* previous, size_t expected);

/* Add a reader to current, the store's published slot, which has a NULL
 * object when nothing is published. out is the caller's result pointer. The
 * wrapper writes current to out after OK.
 * Returns OK. END when nothing is published. ARGUMENT for a NULL store or
 * out, overlap between out, the store and current, or current outside
 * CURRENT. LIMIT when the reader count would overflow. Changes nothing on
 * failure. */
TC_TLV_result tc_snapshot_acquire(const tc_snapshot_store* store, const tc_snapshot_slot* current,
                                  const void* out, size_t out_size);

/* Remove one reader. The last reader of a RETIRED slot makes it FREE and
 * clears its payload.
 * Returns OK, or ARGUMENT for a NULL slot, no readers, or a slot outside
 * CURRENT and RETIRED, with the slot unchanged. */
TC_TLV_result tc_snapshot_release(const tc_snapshot_slot* slot);
#endif
