<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Certificate store

Include `<tiny_crypto/x509_store.h>`. The store publishes caller-owned certificate
sources and keeps old sources alive while readers use them. Certificate bytes
stay in caller storage.

A source provides separate callbacks for untrusted candidate certificates and
explicit trust anchors. Each anchor can carry path constraints. Source callbacks
return borrowed spans and charge reads against the supplied work budget.
`TC_X509_store_array_source` describes a fixed `TC_X509_store_array` of candidate
spans and anchor records as a source. Each record read charges one work unit.
An RFC 5914 list can supply anchors through the
[trust-anchor reader](x509-trust-anchors.md). Build an anchor record from a
single parsed root certificate with `TC_X509_store_anchor_from_certificate`,
which carries the root's path controls into the record.

## Updating a source

Zero-initialize the store and snapshot slots. Serialize store calls with the
application's lock.

1. Build an immutable source in unused storage.
1. Call `TC_X509_store_prepare` with a free slot and that source.
1. Validate the proposed records, authorize the trust change, and persist it.
1. Call `TC_X509_store_publish` with the revision used to prepare the update.
1. If the update is abandoned, call `TC_X509_store_discard` on the prepared slot.

Preparation checks callback configuration only. Certificate parsing, trust
decisions, and flash writes belong to the application. Publication returns
`TC_TLV_INVALID` for a stale revision and `TC_TLV_LIMIT` when the revision
counter is exhausted. Neither changes the current source. The application owns
persistent storage and recovery after power loss.

## Slot states

Each slot follows the `TC_snapshot_state` lifecycle from
`<tiny_crypto/snapshot.h>`, which the TWIC canceled-card-list store shares.

| State                  | Entered by                                                   | Leaves by        |
| ---------------------- | ------------------------------------------------------------ | ---------------- |
| `TC_SNAPSHOT_FREE`     | zero initialization, discard, last release of a retired slot | prepare          |
| `TC_SNAPSHOT_PREPARED` | prepare                                                      | publish, discard |
| `TC_SNAPSHOT_CURRENT`  | publish                                                      | a later publish  |
| `TC_SNAPSHOT_RETIRED`  | a later publish while readers remain                         | last release     |

Only a `TC_SNAPSHOT_CURRENT` slot accepts new readers. A superseded slot with
no readers goes directly to `TC_SNAPSHOT_FREE`. Publish and acquire return
`TC_TLV_ARGUMENT` without changes when the store, the published slot and the
argument objects overlap.

## Reader lifetimes

Acquire the current snapshot with `TC_X509_store_acquire`. It returns `TC_TLV_END`
when no source is published. Keep the reference until all uses of its certificate
bytes and borrowed validation results finish, then call `TC_X509_store_release`
exactly once. Releasing a slot without readers returns `TC_TLV_ARGUMENT`.
Do not use a released reference.

Publication retires the previous snapshot. Its backing storage can be reused only
when its state becomes `TC_SNAPSHOT_FREE`. Allocate enough slots for the
current source, a prepared update, and any retired sources still held by readers.

Existing readers retain the previous trust configuration. Removing an anchor does
not cancel their operations. Applications requiring immediate distrust must cancel
or revalidate those operations before using their results.

## Constructing a path

Pass the acquired snapshot's `source` to `TC_X509_path_build`, along with the target
certificate, validation options, and caller-owned validation and search workspaces.
The builder tries candidate issuers and explicit anchors, then validates each
complete path. It returns the first valid path. Candidate ordering affects search
cost and which valid path is selected.

Search needs a `TC_bytes` array and a `TC_X509_search_frame` array with the same
capacity. The smaller of that capacity and `options.max_certificates` bounds path
depth. `options.max_work` covers failed branches and storage callbacks as well as
the successful path. Exhausted resources return `TC_X509_PATH_LIMIT`.

[example_find_client_path](../examples/x509_client.c) shows a four-certificate
search using the client-authentication policy shared with the ordered-chain
example. Allocate its validation arena and a search workspace of
`EXAMPLE_CLIENT_PATH_CAPACITY` entries outside a small task stack, provide a
signature verifier and current time, and pass the acquired snapshot's source.
The example leaves snapshot locking and release to the caller so its returned
certificate and policy spans remain usable.

The result's `path` references search storage in anchor-issued-first order, with
the target last. `anchor_index` identifies the selected source anchor. Keep the
snapshot and both workspaces alive until borrowed results are no longer needed.

Enrollment record validation and persistent storage adapters remain application
responsibilities. For validation options and signature providers, see
[X.509 paths](x509-path.md).
