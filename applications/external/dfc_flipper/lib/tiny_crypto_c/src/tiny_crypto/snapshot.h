/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Snapshot slot states shared by the certificate store and TWIC canceled
 * card lists: prepare, publish, acquire, release and retire.
 * Limitations: the caller serializes store calls with its own lock.
 * Contracts: docs/api.md. Guide: docs/x509-store.md. */
#ifndef TINY_CRYPTO_SNAPSHOT_H_
#define TINY_CRYPTO_SNAPSHOT_H_

/* Lifecycle of one store slot. Store functions change the state while the
 * caller holds the application's lock.
 *
 * FREE      Unused. prepare copies a payload in and moves to PREPARED.
 * PREPARED  Holds a payload with no readers. discard returns it to FREE.
 *           publish makes it CURRENT and increments the store revision.
 * CURRENT   The published slot. acquire adds a reader and release removes
 *           one. A later publish moves it to RETIRED while readers remain,
 *           or directly to FREE when it has none.
 * RETIRED   Superseded with readers still attached. Readers keep using the
 *           old payload. The last release returns it to FREE.
 *
 * The caller may reclaim a slot's payload storage only while the slot is
 * FREE. Only a CURRENT slot accepts new readers. */
typedef enum {
  TC_SNAPSHOT_FREE,
  TC_SNAPSHOT_PREPARED,
  TC_SNAPSHOT_CURRENT,
  TC_SNAPSHOT_RETIRED
} TC_snapshot_state;

#endif
