/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_X509_STORE_H_
#define TINY_CRYPTO_X509_STORE_H_
#include <tiny_crypto/x509.h>
#include <tiny_crypto/snapshot.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_X509_trust_anchor trust;
  TC_X509_name_constraints names;
} TC_X509_store_anchor;

/* Candidates are untrusted certificates. Anchors carry explicit local trust.
 * Callbacks return borrowed records, consume work without increasing it, and
 * return OK only after writing out. An unavailable record is a read error.
 * Reads propagate LIMIT and UNSUPPORTED; other failure codes become ARGUMENT.
 * Record bytes and ordering remain stable while the source is in use. */
typedef struct {
  void* context;
  size_t candidate_count, anchor_count;
  TC_TLV_result (*candidate)(void* context, size_t index, size_t* work, TC_bytes* out);
  TC_TLV_result (*anchor)(void* context, size_t index, size_t* work, TC_X509_store_anchor* out);
} TC_X509_store_source;

typedef TC_snapshot_state TC_X509_snapshot_state;
#define TC_X509_SNAPSHOT_FREE TC_SNAPSHOT_FREE
#define TC_X509_SNAPSHOT_PREPARED TC_SNAPSHOT_PREPARED
#define TC_X509_SNAPSHOT_CURRENT TC_SNAPSHOT_CURRENT
#define TC_X509_SNAPSHOT_RETIRED TC_SNAPSHOT_RETIRED
/* Zero-initialize these objects. Fields are managed by the store functions. */
typedef struct {
  TC_X509_store_source source;
  size_t readers;
  TC_X509_snapshot_state state;
} TC_X509_store_snapshot;
typedef struct {
  TC_X509_store_snapshot* current;
  size_t revision;
} TC_X509_store;

/* Serialize every call with the application's lock. Store, slots, source and
 * result pointers must occupy disjoint storage. Do not copy live slots.
 * Keep source context and record bytes unchanged until the slot becomes FREE.
 * Certificate validation and trust authorization are caller responsibilities. */

/* Copy the callback configuration into a FREE slot. Retains borrowed context
 * and record storage. A busy slot returns LIMIT unchanged. */
TC_TLV_result TC_X509_store_prepare(TC_X509_store_snapshot* slot, const TC_X509_store_source* source);
/* Return a PREPARED slot to FREE and clear its source descriptor. Other states
 * return ARGUMENT unchanged. The caller owns the underlying record storage. */
TC_TLV_result TC_X509_store_discard(TC_X509_store_snapshot* slot);
/* Authorize and persist changes before publication. A stale revision returns
 * INVALID; exhausted revision space returns LIMIT. Neither changes the store.
 * Publishing an empty source removes all anchors for subsequent readers. */
TC_TLV_result TC_X509_store_publish(TC_X509_store* store, size_t revision, TC_X509_store_snapshot* slot);
/* Acquire returns END without changing out when no snapshot is published.
 * Each successful acquire needs one release, after all borrowed results expire.
 * Old readers retain the old trust configuration across publication. Applications
 * requiring immediate distrust must cancel or revalidate those operations. */
TC_TLV_result TC_X509_store_acquire(TC_X509_store* store, TC_X509_store_snapshot** out);
/* Release one acquired reference. The last reader of a RETIRED slot returns it
 * to FREE, allowing the caller to reclaim its context and record storage. */
TC_TLV_result TC_X509_store_release(TC_X509_store_snapshot* slot);
#ifdef __cplusplus
}
#endif
#endif
