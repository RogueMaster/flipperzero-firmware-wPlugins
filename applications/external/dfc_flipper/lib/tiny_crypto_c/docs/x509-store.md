<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Certificate store

Include `<tiny_crypto/x509_store.h>`. The store publishes caller-owned certificate
sources and keeps old sources alive while readers use them. It allocates no memory
and does not copy certificate bytes.

A source provides separate callbacks for untrusted candidate certificates and
explicit trust anchors. Each anchor can carry name constraints. Source callbacks
return borrowed spans and charge reads against the supplied work budget.

## Updating a source

Zero-initialize the store and snapshot slots. Serialize store calls with the
application's lock.

1. Build an immutable source in unused storage.
2. Call `TC_X509_store_prepare` with a free slot and that source.
3. Validate the proposed records, authorize the trust change, and persist it.
4. Call `TC_X509_store_publish` with the revision used to prepare the update.
5. If the update is abandoned, call `TC_X509_store_discard` on the prepared slot.

Preparation checks callback configuration only. It does not parse certificates,
grant trust, or write flash. Publication rejects a stale revision without changing
the current source. The application owns persistent storage and recovery after
power loss.

## Reader lifetimes

Acquire the current snapshot with `TC_X509_store_acquire`. Keep the reference until
all uses of its certificate bytes and borrowed validation results finish, then
call `TC_X509_store_release` exactly once. Do not use a released reference.

Publication retires the previous snapshot. Its backing storage can be reused only
when its state becomes `TC_X509_SNAPSHOT_FREE`. Allocate enough slots for the
current source, a prepared update, and any retired sources still held by readers.

Existing readers retain the previous trust configuration. Removing an anchor does
not cancel their operations. Applications requiring immediate distrust must cancel
or revalidate those operations before using their results.

## Constructing a path

Pass the acquired snapshot's `source` to `TC_X509_path_build`, along with the target
certificate, validation options, and caller-owned validation and search workspaces.
The builder tries candidate issuers and explicit anchors, then validates each
complete path. It returns the first valid path; candidate ordering affects search
cost and which valid path is selected.

Search needs a `TC_bytes` array and a `TC_X509_search_frame` array with the same
capacity. The smaller of that capacity and `options.max_certificates` bounds path
depth. `options.max_work` covers failed branches and storage callbacks as well as
the successful path. Exhausted resources return `TC_X509_PATH_LIMIT`.

[example_find_client_path](../examples/x509_client.c) shows a four-certificate
search using the client-authentication policy shared with the ordered-chain
example. Allocate `ExampleX509SearchWorkspace` outside a small task stack,
provide a signature verifier and current time, and pass the acquired snapshot's
source. The example leaves snapshot locking and release to the caller so its
returned certificate and policy spans remain usable.

The result's `path` references search storage in anchor-issued-first order, with
the target last. `anchor_index` identifies the selected source anchor. Keep the
snapshot and both workspaces alive until borrowed results are no longer needed.

Enrollment record validation and persistent storage adapters remain application
responsibilities. For validation options and signature providers, see
[X.509 paths](x509-path.md).
