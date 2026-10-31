#pragma once

#include "kdbx_includes.h"

typedef enum {
    KDBXVaultBackendNone = 0,
    KDBXVaultBackendRam,
    KDBXVaultBackendFileInt,
    KDBXVaultBackendFileExt,
} KDBXVaultBackend;

/* Compact 4-byte field locator. record_count is always derivable as
 * ceil(plain_len / KDBX_VAULT_RECORD_PLAIN_MAX) and is no longer stored here;
 * it is tracked internally by KDBXVaultWriter and derived on read. */
typedef struct {
    uint16_t first_record;
    uint16_t plain_len;
} KDBXFieldRef;

typedef struct KDBXVault KDBXVault;
typedef bool (*KDBXVaultChunkCallback)(const uint8_t* data, size_t data_size, void* context);

#define KDBX_VAULT_RECORD_PLAIN_MAX     1024U
#define KDBX_VAULT_STREAM_SYNC_INTERVAL (8U * 1024U)

/* KDBXScratchRef — multi-chunk handle for staged scratchs that may exceed the
 * 64 KB plain-len cap of a single KDBXFieldRef. Each chunk is a contiguous
 * KDBXFieldRef of up to UINT16_MAX bytes. Used by xml_scratch / payload_scratch
 * to back inflated XML and streamed payload that can reach FLIPPASS_OPEN_MAX_XML_STREAM_BYTES.
 *
 * 32 chunks * 64 KB = 2 MB, matching the current FLIPPASS_OPEN_MAX_XML_STREAM_BYTES.
 * Cost per scratch: 32*4 + 8 = 136 B (negligible).
 */
#define KDBX_VAULT_SCRATCH_MAX_CHUNKS 32U

typedef struct {
    KDBXFieldRef chunks[KDBX_VAULT_SCRATCH_MAX_CHUNKS];
    uint16_t chunk_count;
    uint16_t reserved;
    uint32_t total_plain_len;
} KDBXScratchRef;

/* Streaming options for xml_scratch consumption.
 * - release_consumed_pages: when vault->backend == Ram, free pages whose records
 *   have all been delivered to the callback. Pages are freed in batches of ~8
 *   records to amortize free() overhead.
 * - checkpoint: called BEFORE each record read with the next record_id. Returns
 *   false to abort the stream. May swap the active vault via vault_swap_ptr (used
 *   to implement mid-stream RAM->file promotion).
 * - vault_swap_ptr: when non-NULL, the streamer reads *vault_swap_ptr at the
 *   start of every record iteration. The checkpoint callback may overwrite
 *   *vault_swap_ptr to redirect subsequent reads to a different vault (e.g.
 *   after a partial promotion). When NULL, the initial vault parameter is used
 *   for all reads.
 * - on_pages_released: optional callback fired right after the streamer freed
 *   one or more RAM pages. freed_bytes > 0 always when called. Used by the
 *   host for telemetry without polluting the vault layer with logging.
 */
typedef bool (*KDBXVaultStreamCheckpoint)(void* context, uint32_t next_record_id);
typedef void (
    *KDBXVaultStreamOnPagesReleased)(void* context, uint32_t consumed_up_to, size_t freed_bytes);

typedef struct {
    bool release_consumed_pages;
    KDBXVaultStreamCheckpoint checkpoint;
    void* checkpoint_context;
    KDBXVault** vault_swap_ptr;
    KDBXVaultStreamOnPagesReleased on_pages_released;
    void* on_pages_released_context;
    /* Optional borrowed workspace, held for the entire synchronous stream.
     * At least kdbx_vault_stream_workspace_size() bytes; cleared but not freed. */
    uint8_t* workspace;
    size_t workspace_size;
} KDBXVaultStreamOptions;

typedef struct {
    KDBXVault* vault;
    KDBXFieldRef ref;
    uint16_t record_count; /* Internal tally; not stored in ref */
    uint16_t _pad;
    uint8_t* pending;
    size_t pending_capacity;
    size_t pending_len;
    bool pending_owned;
    bool failed;
    bool stream_file_mode;
    size_t stream_unsynced_bytes;
    File* stream_file;
} KDBXVaultWriter;

typedef struct {
    KDBXVault* vault;
    KDBXFieldRef ref;
    uint16_t record_index;
    uint8_t record_plain[KDBX_VAULT_RECORD_PLAIN_MAX];
    size_t record_plain_len;
    size_t record_plain_offset;
    bool failed;
} KDBXVaultReader;

#define KDBX_VAULT_SESSION_INT_PATH      INT_PATH("apps_data/flippass/session.bin")
#define KDBX_VAULT_SESSION_EXT_PATH      EXT_PATH("apps_data/flippass/session.bin")
#define KDBX_VAULT_SCRATCH_INT_PATH      INT_PATH("apps_data/flippass/gzip.bin")
#define KDBX_VAULT_SCRATCH_EXT_PATH      EXT_PATH("apps_data/flippass/gzip.bin")
#define KDBX_VAULT_MEMBER_INT_PATH       INT_PATH("apps_data/flippass/gzip_member.bin")
#define KDBX_VAULT_MEMBER_EXT_PATH       EXT_PATH("apps_data/flippass/gzip_member.bin")
#define KDBX_VAULT_WINDOW_INT_PATH       INT_PATH("apps_data/flippass/gzip_window.bin")
#define KDBX_VAULT_WINDOW_EXT_PATH       EXT_PATH("apps_data/flippass/gzip_window.bin")
#define KDBX_VAULT_SAVE_SCRATCH_EXT_PATH EXT_PATH("apps_data/flippass/save_xml.bin")

/* A non-NULL counter enables the budget even when commit_limit is zero. */
KDBXVault*
    kdbx_vault_alloc(KDBXVaultBackend backend, size_t* committed_bytes, size_t commit_limit);
KDBXVault* kdbx_vault_alloc_with_path(
    KDBXVaultBackend backend,
    const char* file_path,
    size_t* committed_bytes,
    size_t commit_limit);
void kdbx_vault_set_budget(KDBXVault* vault, size_t* committed_bytes, size_t commit_limit);
/* Prepare the first locator before a temporary producer module maps.
 * Charges the normal index budget; idempotent and does not create a record. */
bool kdbx_vault_prepare_first_record(KDBXVault* vault);
void kdbx_vault_free(KDBXVault* vault);
bool kdbx_vault_backend_supported(KDBXVaultBackend backend);
const char* kdbx_vault_backend_unavailable_reason(KDBXVaultBackend backend);
KDBXVaultBackend kdbx_vault_get_backend(const KDBXVault* vault);
Storage* kdbx_vault_get_storage(const KDBXVault* vault);
bool kdbx_vault_budget_failed(const KDBXVault* vault);
bool kdbx_vault_storage_failed(const KDBXVault* vault);
const char* kdbx_vault_backend_label(KDBXVaultBackend backend);
const char* kdbx_vault_backend_path(KDBXVaultBackend backend);
bool kdbx_vault_cleanup_backend(Storage* storage, KDBXVaultBackend backend);
void kdbx_vault_cleanup_all_sessions(Storage* storage);
void kdbx_vault_cleanup_runtime_sessions(Storage* storage);
bool kdbx_vault_promote_ram_to_file(KDBXVault* source, KDBXVault* target);
const char* kdbx_vault_failure_reason(const KDBXVault* vault);
size_t kdbx_vault_record_overhead_bytes(void);
size_t kdbx_vault_ram_page_payload_size(void);
size_t kdbx_vault_ram_page_overhead_bytes(void);
size_t kdbx_vault_estimate_index_bytes(uint32_t record_count);
size_t kdbx_vault_last_failed_size(const KDBXVault* vault);
size_t kdbx_vault_last_failed_committed(const KDBXVault* vault);
size_t kdbx_vault_last_failed_max_free_block(const KDBXVault* vault);
const char* kdbx_vault_last_reader_failure(const KDBXVault* vault);
uint32_t kdbx_vault_last_reader_failure_record(const KDBXVault* vault);
size_t kdbx_vault_index_bytes(const KDBXVault* vault);
size_t kdbx_vault_page_bytes(const KDBXVault* vault);
uint32_t kdbx_vault_record_count(const KDBXVault* vault);
const char* kdbx_vault_storage_stage(const KDBXVault* vault);

void kdbx_vault_writer_reset(KDBXVaultWriter* writer, KDBXVault* vault);
void kdbx_vault_writer_reset_with_pending(
    KDBXVaultWriter* writer,
    KDBXVault* vault,
    uint8_t* pending,
    size_t pending_capacity);
void kdbx_vault_writer_set_file_streaming(KDBXVaultWriter* writer, bool enabled);
/* Sync/close an idle descriptor while preserving pending bytes, refs and mode.
 * The next record flush reopens it. False marks an I/O failure. */
bool kdbx_vault_writer_pause_stream(KDBXVaultWriter* writer);
void kdbx_vault_writer_close(KDBXVaultWriter* writer);
void kdbx_vault_writer_abort(KDBXVaultWriter* writer);
bool kdbx_vault_writer_write(KDBXVaultWriter* writer, const uint8_t* data, size_t len);
bool kdbx_vault_writer_finish(KDBXVaultWriter* writer, KDBXFieldRef* out_ref);
bool kdbx_vault_writer_finish_keep_stream(KDBXVaultWriter* writer, KDBXFieldRef* out_ref);

/* KDBXScratchRef writer/streamer family.
 *
 * kdbx_vault_writer_write_scratch appends bytes and rolls over to a new chunk
 * when the current ref would overflow UINT16_MAX. The same writer instance is
 * reused across chunks; only its internal ref state is reset between rolls.
 *
 * kdbx_vault_writer_finish_scratch closes the current ref into the trailing
 * chunk slot and finalizes the writer.
 *
 * Both functions return false on overflow / failure. On overflow the writer
 * is marked failed and the scratch_ref is left in a partial-but-valid state
 * (the caller may free the vault pages via kdbx_vault_free).
 */
void kdbx_vault_scratch_ref_reset(KDBXScratchRef* scratch_ref);
bool kdbx_vault_scratch_ref_is_empty(const KDBXScratchRef* scratch_ref);
bool kdbx_vault_writer_write_scratch(
    KDBXVaultWriter* writer,
    const uint8_t* data,
    size_t len,
    KDBXScratchRef* scratch_ref);
bool kdbx_vault_writer_finish_scratch(KDBXVaultWriter* writer, KDBXScratchRef* scratch_ref);

void kdbx_vault_reader_reset(KDBXVaultReader* reader, KDBXVault* vault, const KDBXFieldRef* ref);
bool kdbx_vault_reader_read(
    KDBXVaultReader* reader,
    uint8_t* out,
    size_t capacity,
    size_t* out_size);
bool kdbx_vault_ref_is_empty(const KDBXFieldRef* ref);
bool kdbx_vault_load_text(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    char** out_text,
    size_t* out_size);
bool kdbx_vault_load_bytes(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    uint8_t** out_data,
    size_t* out_size);
bool kdbx_vault_stream_ref(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    KDBXVaultChunkCallback callback,
    void* context);

/* kdbx_vault_stream_scratch — iterate every chunk in scratch_ref through the
 * existing kdbx_vault_stream_ref machinery. No page recycling, no checkpoint.
 * Equivalent to calling kdbx_vault_stream_ref once per chunk.
 */
bool kdbx_vault_stream_scratch(
    KDBXVault* vault,
    const KDBXScratchRef* scratch_ref,
    KDBXVaultChunkCallback callback,
    void* context);

/* kdbx_vault_stream_scratch_with_options — the open path's streaming entry.
 * Adds page recycling (release pages of consumed records when backend == Ram)
 * and a per-record checkpoint that may swap the active vault for partial
 * RAM->file promotion. See KDBXVaultStreamOptions documentation above.
 */
bool kdbx_vault_stream_scratch_with_options(
    KDBXVault* initial_vault,
    const KDBXScratchRef* scratch_ref,
    KDBXVaultChunkCallback callback,
    void* context,
    const KDBXVaultStreamOptions* options);
size_t kdbx_vault_stream_workspace_size(void);

/* kdbx_vault_promote_partial_ram_to_file — variant of kdbx_vault_promote_ram_to_file
 * that may be called mid-stream. Records with id < start_record_id are NOT
 * written to the target file (their locators stay stale, but they will not be
 * re-read). Records with id >= start_record_id are written and their locators
 * are updated to file offsets. RAM pages are freed in either case.
 *
 * On success: target holds source's keys, file, and index (with mixed valid/stale
 * locators). Caller must replace the vault pointer (xml_scratch.vault = target;
 * kdbx_vault_free(source)) — same convention as kdbx_vault_promote_ram_to_file.
 */
bool kdbx_vault_promote_partial_ram_to_file(
    KDBXVault* source,
    KDBXVault* target,
    uint32_t start_record_id);
