/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* TWIC canceled card list import, lookup and freshness from the TSA CSV feed.
 * Standards: TWIC Reader Specification Part 3 sections 4.4.3 and 4.4.4,
 * 33 CFR 101.525.
 * Configuration: TC_ENABLE_TWIC_CCL.
 * Limitations: download authenticity comes from the application's transport
 * or provisioning.
 * Contracts: docs/api.md. Guide: docs/twic-ccl.md. */
#ifndef TINY_CRYPTO_TWIC_CCL_H_
#define TINY_CRYPTO_TWIC_CCL_H_
#include <tiny_crypto/common.h>
#include <tiny_crypto/snapshot.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TC_TWIC_CCL_FASCN_BYTES 25
#define TC_TWIC_CCL_RECORD_BYTES (2 * TC_TWIC_CCL_FASCN_BYTES + 1 + 9)

typedef TC_result TC_TWIC_CCL_result;
#define TC_TWIC_CCL_OK TC_RESULT_OK
#define TC_TWIC_CCL_INVALID TC_RESULT_INVALID
#define TC_TWIC_CCL_LIMIT TC_RESULT_LIMIT
#define TC_TWIC_CCL_ARGUMENT TC_RESULT_ARGUMENT
#define TC_TWIC_CCL_SINK_ERROR TC_RESULT_SINK_ERROR
#define TC_TWIC_CCL_SOURCE_ERROR TC_RESULT_SOURCE_ERROR
#define TC_TWIC_CCL_STALE TC_RESULT_STALE
#define TC_TWIC_CCL_UNAVAILABLE TC_RESULT_UNAVAILABLE
#define TC_TWIC_CCL_CHECKSUM_MISMATCH TC_RESULT_CHECKSUM_MISMATCH

typedef struct {
  uint8_t fascn[TC_TWIC_CCL_FASCN_BYTES];
  /* Date the identifier was added to the list. */
  uint16_t year;
  uint8_t month, day;
} TC_TWIC_CCL_record;

#if TC_ENABLE_TWIC_CCL
/* Read one CSV record without its line ending: 50 hexadecimal FASC-N digits,
 * a comma and the date the card was listed as DDMmmYYYY, for example
 * 29Feb2024 (TWIC Part 3 section 4.4.3). line is borrowed for the call and
 * must be disjoint from out. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out or NULL data with a
 * length. INVALID for another length, a missing comma, a bad digit or an
 * invalid date. out changes only on OK. */
TC_TWIC_CCL_result TC_TWIC_CCL_read(TC_bytes line, TC_TWIC_CCL_record* out);
#endif

/* The record is valid during the call. Copy it into staging storage as needed.
 * Return TC_OK after accepting it. Any other value aborts the import. */
typedef TC_status (*TC_TWIC_CCL_visit)(void* context, const TC_TWIC_CCL_record* record);

/* Caller-owned state. Treat members as private after initialization. */
typedef struct {
  size_t bytes_left, records_left, records, used;
  TC_TWIC_CCL_visit visit;
  void* context;
  TC_TWIC_CCL_result error;
  uint8_t pending[TC_TWIC_CCL_RECORD_BYTES + 1];
  uint8_t finished;
} TC_TWIC_CCL_stream;

#if TC_ENABLE_TWIC_CCL
/* Initialize stream for an import of at most max_bytes bytes and max_records
 * records. Limits are inclusive, and zero permits none. visit receives each
 * record. The callback must avoid reentering or modifying the stream. Its
 * storage and every input chunk must be disjoint from the stream.
 * Returns OK with the stream reset. ARGUMENT for a NULL stream or visit,
 * with the stream unchanged. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_init(TC_TWIC_CCL_stream* stream, size_t max_bytes,
                                           size_t max_records, TC_TWIC_CCL_visit visit,
                                           void* context);
/* Consume one chunk of CRLF or LF terminated records. The chunk is borrowed
 * for this call. Records split across chunks are held in the stream.
 * Returns OK after every complete record in the chunk is accepted. ARGUMENT
 * for a NULL stream. Other failures are sticky: the stream keeps returning
 * the first error until init, so a rejected chunk can never be skipped. They
 * are ARGUMENT for NULL chunk data with a length, an uninitialized stream or
 * an update after finish, LIMIT for a chunk beyond max_bytes or a record
 * beyond max_records, INVALID for a malformed or overlong record and
 * SINK_ERROR for a rejected callback. Earlier callback writes remain in
 * staging storage. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_update(TC_TWIC_CCL_stream* stream, TC_bytes chunk);
/* Require a nonempty list that ends at a record boundary. Publish staged
 * records only after this succeeds and application provenance and freshness
 * checks pass. A second finish returns OK again.
 * Returns OK. ARGUMENT for a NULL or uninitialized stream. INVALID for a
 * partial final record or an empty list. Errors are sticky as in
 * TC_TWIC_CCL_stream_update, and an earlier error is returned unchanged. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_finish(TC_TWIC_CCL_stream* stream);

/* Scan a complete borrowed CSV buffer for a 25-byte FASC-N such as
 * chuid.fascn. Validates every row, including rows after a match, with at
 * most max_records rows. Stack use is one TC_TWIC_CCL_stream.
 * Returns OK and writes listed as 1 when the FASC-N is present and 0
 * otherwise. ARGUMENT for NULL listed, a FASC-N without 25 bytes or NULL csv
 * data with a length. Other statuses follow the stream functions. listed
 * changes only on OK. A zero result reports absence from this input only.
 * Callers apply freshness, credential authentication and access policy
 * separately. */
TC_TWIC_CCL_result TC_TWIC_CCL_contains(TC_bytes csv, TC_bytes fascn, size_t max_records,
                                        int* listed);
#endif

/* Keys are sorted by unsigned byte order. A successful read returns exactly
 * 25 bytes, borrowed until the next read. The callback may reuse a read buffer.
 * It returns TC_OK only after setting out. Other results become SOURCE_ERROR.
 * Keep the key values, count and ordering stable throughout index use. */
typedef struct {
  void* context;
  size_t count;
  TC_status (*read)(void* context, size_t position, TC_bytes* out);
} TC_TWIC_CCL_source;

/* Initialized by index_prepare. Treat fields as private afterward. */
typedef struct {
  TC_TWIC_CCL_source source;
} TC_TWIC_CCL_index;

#if TC_ENABLE_TWIC_CCL
/* Validate every key of source and their ascending order with one read per
 * key. Duplicate keys are accepted. The index copies the source descriptor,
 * so its context, count and key bytes stay stable throughout index use.
 * Returns OK with out written. ARGUMENT for NULL source, out or read. INVALID
 * for an empty source, a key whose length differs from 25 bytes, or keys out of
 * order. LIMIT for more than max_records keys, before any read. SOURCE_ERROR
 * when a read fails. out changes only on OK. The application binds the index to
 * a completed, trusted CCL import. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_prepare(const TC_TWIC_CCL_source* source, size_t max_records,
                                             TC_TWIC_CCL_index* out);
/* Open a packed image of sorted 25-byte FASC-Ns in caller-owned memory
 * through TC_TWIC_CCL_index_prepare. Keys are borrowed from the image. Keep
 * the span descriptor and its bytes unchanged until the index and every
 * derived snapshot are released. out must be disjoint from the descriptor and
 * the image.
 * Returns OK with out written. ARGUMENT for NULL arguments, NULL image data
 * with a length, or overlap. INVALID for a length outside the multiples of
 * 25, an empty image or keys out of order. LIMIT for more than max_records
 * keys. out changes only on OK. The application binds the image to its
 * trusted import metadata before publishing a snapshot. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_from_memory(const TC_bytes* image, size_t max_records,
                                                 TC_TWIC_CCL_index* out);
/* Exact binary search of a prepared index for a 25-byte FASC-N. max_reads
 * bounds callback calls, and ceil(log2(count + 1)) reads always suffice. The
 * query is copied before the first read, so it may borrow the source's
 * reusable read buffer.
 * Returns OK and writes listed as 1 when present and 0 otherwise. ARGUMENT
 * for NULL arguments, an unprepared index or a FASC-N without 25 bytes.
 * LIMIT when max_reads runs out. SOURCE_ERROR or INVALID for a failed read or
 * a key without 25 bytes. listed changes only on OK. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_contains(const TC_TWIC_CCL_index* index, TC_bytes fascn,
                                              size_t max_reads, int* listed);
#endif

/* Unix seconds from trusted provisioning metadata and the local clock.
 * published_at describes the list. received_at records completed retrieval. */
typedef struct {
  uint64_t published_at, received_at;
} TC_TWIC_CCL_metadata;
typedef struct {
  uint64_t now, max_age;
  /* Persist this floor to reject older publications after a restart. */
  uint64_t minimum_publication;
} TC_TWIC_CCL_freshness_policy;

#if TC_ENABLE_TWIC_CCL
/* Check trusted list metadata against the freshness policy (TWIC Part 3
 * section 4.4.4). Age is measured from publication and includes max_age.
 * Zero permits only publication at now. Metadata authenticity is supplied by
 * the application. Per-record cancellation dates carry no publication time.
 * Returns OK for a fresh list. ARGUMENT for NULL arguments. INVALID for
 * receipt before publication or after now. STALE for a publication older
 * than minimum_publication or max_age. */
TC_TWIC_CCL_result TC_TWIC_CCL_check_freshness(const TC_TWIC_CCL_metadata* metadata,
                                               const TC_TWIC_CCL_freshness_policy* policy);
#endif

/* Zero-initialize stores and slots. These fields are managed by the API.
 * state follows the TC_snapshot_state lifecycle in snapshot.h. */
typedef struct {
  TC_TWIC_CCL_index index;
  TC_TWIC_CCL_metadata metadata;
  size_t readers;
  TC_snapshot_state state;
} TC_TWIC_CCL_snapshot;
typedef struct {
  TC_TWIC_CCL_snapshot* current;
  size_t revision;
} TC_TWIC_CCL_store;

#if TC_ENABLE_TWIC_CCL
/* Snapshot store. Serialize store and snapshot operations with the
 * application's lock and keep store, slots, inputs and outputs disjoint. Keep
 * backing keys and context stable until their slot becomes FREE. A prepared
 * index must represent a completed, authenticated import with metadata bound
 * to that image. The state machine is described in snapshot.h.
 *
 * prepare copies index and metadata into a FREE slot and makes it PREPARED.
 * Returns OK. ARGUMENT for NULL arguments, an unprepared index or overlap.
 * INVALID for metadata received before publication. LIMIT for a slot outside
 * FREE or with readers. The slot changes only on OK. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_prepare(TC_TWIC_CCL_snapshot* slot,
                                             const TC_TWIC_CCL_index* index,
                                             const TC_TWIC_CCL_metadata* metadata);
/* Return a PREPARED slot without readers to FREE and clear its payload.
 * Returns OK, or ARGUMENT for a NULL slot or any other state, with the slot
 * unchanged. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_discard(TC_TWIC_CCL_snapshot* slot);
/* Make slot CURRENT. Persist the image and rollback floor before
 * publication. revision must equal store->revision, which then increments.
 * The previous slot becomes RETIRED while it has readers and FREE otherwise.
 * Returns OK. ARGUMENT for NULL arguments, overlap, a slot outside PREPARED
 * or with readers, or a current slot outside CURRENT. STALE for a
 * publication date older than the current snapshot's. INVALID for a stale
 * revision. LIMIT for exhausted revision space. Failures leave the store and
 * the active list unchanged and the proposed slot PREPARED. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_publish(TC_TWIC_CCL_store* store, size_t revision,
                                             TC_TWIC_CCL_snapshot* slot);
/* Add a reader to the current snapshot and write it to out. Each successful
 * acquire needs one release. Existing readers keep their storage across
 * updates. A superseded snapshot returns STALE from snapshot_contains.
 * Returns OK with out written. UNAVAILABLE when no list is published.
 * ARGUMENT for NULL arguments, overlap or a current slot outside CURRENT.
 * LIMIT when the reader count would overflow. out changes only on OK. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_acquire(TC_TWIC_CCL_store* store, TC_TWIC_CCL_snapshot** out);
/* Remove one reader. The last release of a RETIRED slot makes it FREE and
 * clears its payload. Returns OK, or ARGUMENT for a NULL slot, no readers, or
 * a slot outside CURRENT and RETIRED, with the slot unchanged. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_release(TC_TWIC_CCL_snapshot* slot);
/* Query a held snapshot with TC_TWIC_CCL_index_contains. On STALE, acquire
 * the new current list and repeat the check. The application evaluates age
 * and update policy explicitly. Release the snapshot after the result is
 * used.
 * Returns the TC_TWIC_CCL_index_contains status for a CURRENT slot. STALE for
 * a RETIRED slot. ARGUMENT for NULL arguments, a slot without readers or
 * outside CURRENT and RETIRED, or listed overlapping the slot. listed changes
 * only on OK. */
TC_TWIC_CCL_result TC_TWIC_CCL_snapshot_contains(const TC_TWIC_CCL_snapshot* slot, TC_bytes fascn,
                                                 size_t max_reads, int* listed);
#endif

#ifdef __cplusplus
}
#endif
#endif
