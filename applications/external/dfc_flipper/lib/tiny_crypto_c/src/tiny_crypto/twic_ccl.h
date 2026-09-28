/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_TWIC_CCL_H_
#define TINY_CRYPTO_TWIC_CCL_H_
#include <tiny_crypto/common.h>
#include <tiny_crypto/snapshot.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TC_TWIC_CCL_FASCN_BYTES 25
#define TC_TWIC_CCL_RECORD_BYTES (2 * TC_TWIC_CCL_FASCN_BYTES + 1 + 9)

typedef enum {
  TC_TWIC_CCL_OK, TC_TWIC_CCL_INVALID, TC_TWIC_CCL_LIMIT,
  TC_TWIC_CCL_ARGUMENT, TC_TWIC_CCL_SINK_ERROR, TC_TWIC_CCL_SOURCE_ERROR,
  TC_TWIC_CCL_STALE, TC_TWIC_CCL_UNAVAILABLE, TC_TWIC_CCL_CHECKSUM_MISMATCH
} TC_TWIC_CCL_result;

typedef struct {
  uint8_t fascn[TC_TWIC_CCL_FASCN_BYTES];
  /* Date the identifier was added to the list. */
  uint16_t year;
  uint8_t month, day;
} TC_TWIC_CCL_record;

/* Read one CSV record without its line ending. out changes only on success.
 * Inputs and outputs must occupy disjoint storage. */
TC_TWIC_CCL_result TC_TWIC_CCL_read(TC_bytes line, TC_TWIC_CCL_record* out);

/* The record is valid during the call. Copy it into staging storage as needed.
 * Return TC_OK after accepting it; any other value aborts the import. */
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

/* Limits are inclusive; zero permits no bytes or records. The callback must
 * avoid reentering or modifying the stream. Its storage and all input chunks
 * must be disjoint from the stream. init leaves state unchanged on failure. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_init(TC_TWIC_CCL_stream* stream,
    size_t max_bytes, size_t max_records, TC_TWIC_CCL_visit visit, void* context);
/* Accept CRLF or LF terminated records. A chunk is borrowed for this call.
 * Errors persist until init. Earlier callback writes remain in staging storage. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_update(TC_TWIC_CCL_stream* stream, TC_bytes chunk);
/* Require a nonempty list ending at a record boundary. Publish staged records
 * only after this succeeds and application provenance/freshness checks pass. */
TC_TWIC_CCL_result TC_TWIC_CCL_stream_finish(TC_TWIC_CCL_stream* stream);

/* Scan a complete borrowed CSV buffer for a 25-byte FASC-N (e.g. chuid.fascn).
 * Validate every row, including rows after a match. listed changes only on OK.
 * A zero result reports absence from this input; callers apply freshness,
 * credential authentication and access policy separately. */
TC_TWIC_CCL_result TC_TWIC_CCL_contains(TC_bytes csv, TC_bytes fascn,
    size_t max_records, int* listed);

/* Keys are sorted by unsigned byte order. A successful read returns exactly
 * 25 bytes, borrowed until the next read. The callback may reuse a read buffer.
 * It returns TC_OK only after setting out; other results become SOURCE_ERROR.
 * Keep the key values, count and ordering stable throughout index use. */
typedef struct {
  void* context;
  size_t count;
  TC_status (*read)(void* context, size_t position, TC_bytes* out);
} TC_TWIC_CCL_source;

/* Initialized by index_prepare. Treat fields as private afterward. */
typedef struct { TC_TWIC_CCL_source source; } TC_TWIC_CCL_index;

/* Validate every key and its ordering, with at most max_records reads.
 * Duplicate keys are accepted. The source must be nonempty. Output changes
 * only on OK. The application binds it to a completed, trusted CCL import. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_prepare(const TC_TWIC_CCL_source* source,
    size_t max_records, TC_TWIC_CCL_index* out);
/* Open a packed image of sorted 25-byte FASC-Ns in caller-owned memory.
 * Checks complete records and ordering through index_prepare. Keys are borrowed
 * directly from the image. Keep both the span descriptor and its bytes unchanged
 * until the index and every derived snapshot are released. out must be disjoint
 * from the descriptor and image; failures preserve out. The application binds
 * the image to its trusted import metadata before publishing a snapshot. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_from_memory(const TC_bytes* image,
    size_t max_records, TC_TWIC_CCL_index* out);
/* Exact binary search over a prepared index. max_reads bounds callback calls;
 * zero allows none. listed changes only on OK. Query bytes are copied before
 * the first read so a query may borrow the source's reusable read buffer. */
TC_TWIC_CCL_result TC_TWIC_CCL_index_contains(const TC_TWIC_CCL_index* index,
    TC_bytes fascn, size_t max_reads, int* listed);

/* Unix seconds from trusted provisioning metadata and the local clock.
 * published_at describes the list; received_at records completed retrieval. */
typedef struct { uint64_t published_at, received_at; } TC_TWIC_CCL_metadata;
typedef struct {
  uint64_t now, max_age;
  /* Persist this floor to reject older publications after a restart. */
  uint64_t minimum_publication;
} TC_TWIC_CCL_freshness_policy;

/* Age is measured from publication, inclusive of max_age. Zero permits only
 * publication at now. Future timestamps or receipt before publication return
 * INVALID. Old publications return STALE. Metadata authenticity is supplied
 * by the application; per-record cancellation dates cannot supply it. */
TC_TWIC_CCL_result TC_TWIC_CCL_check_freshness(const TC_TWIC_CCL_metadata* metadata,
    const TC_TWIC_CCL_freshness_policy* policy);

/* Zero-initialize stores and slots. These fields are managed by the API. */
typedef struct {
  TC_TWIC_CCL_index index;
  TC_TWIC_CCL_metadata metadata;
  size_t readers;
  TC_snapshot_state state;
} TC_TWIC_CCL_snapshot;
typedef struct { TC_TWIC_CCL_snapshot* current; size_t revision; } TC_TWIC_CCL_store;

/* Serialize store and snapshot operations with the application's lock. Keep
 * store, slots, inputs and outputs disjoint. Keep backing keys/context stable
 * until their slot becomes FREE. A prepared index must represent a completed,
 * authenticated import with metadata bound to that image. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_prepare(TC_TWIC_CCL_snapshot* slot,
    const TC_TWIC_CCL_index* index, const TC_TWIC_CCL_metadata* metadata);
TC_TWIC_CCL_result TC_TWIC_CCL_store_discard(TC_TWIC_CCL_snapshot* slot);
/* Persist the image and rollback floor before publication. Reject stale store
 * revisions and publication dates older than the current snapshot. Failures
 * preserve the active list and leave the proposed slot prepared. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_publish(TC_TWIC_CCL_store* store, size_t revision,
    TC_TWIC_CCL_snapshot* slot);
/* Acquire returns UNAVAILABLE when no list is published. Each successful
 * acquire needs one release. Existing readers retain storage across updates;
 * a superseded snapshot returns STALE from snapshot_contains. */
TC_TWIC_CCL_result TC_TWIC_CCL_store_acquire(TC_TWIC_CCL_store* store,
    TC_TWIC_CCL_snapshot** out);
TC_TWIC_CCL_result TC_TWIC_CCL_store_release(TC_TWIC_CCL_snapshot* slot);
/* Query the current held snapshot. On STALE, acquire the new current list and
 * repeat the check. The application evaluates age and update policy explicitly.
 * listed changes only on OK; release the snapshot after borrowed results expire. */
TC_TWIC_CCL_result TC_TWIC_CCL_snapshot_contains(const TC_TWIC_CCL_snapshot* slot,
    TC_bytes fascn, size_t max_reads, int* listed);

#ifdef __cplusplus
}
#endif
#endif
