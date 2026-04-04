#pragma once

#include "kdbx_includes.h"

typedef enum {
    KDBXVaultBackendNone = 0,
    KDBXVaultBackendRam,
    KDBXVaultBackendFileInt,
    KDBXVaultBackendFileExt,
} KDBXVaultBackend;

typedef struct {
    uint16_t first_record;
    uint16_t record_count;
    uint32_t plain_len;
} KDBXFieldRef;

typedef struct KDBXVault KDBXVault;
typedef bool (*KDBXVaultChunkCallback)(const uint8_t* data, size_t data_size, void* context);

#define KDBX_VAULT_RECORD_PLAIN_MAX     1024U
#define KDBX_VAULT_STREAM_SYNC_INTERVAL (8U * 1024U)

typedef struct {
    KDBXVault* vault;
    KDBXFieldRef ref;
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

#define KDBX_VAULT_SESSION_INT_PATH INT_PATH("apps_data/flippass/session.bin")
#define KDBX_VAULT_SESSION_EXT_PATH EXT_PATH("apps_data/flippass/session.bin")
#define KDBX_VAULT_SCRATCH_INT_PATH INT_PATH("apps_data/flippass/gzip.bin")
#define KDBX_VAULT_SCRATCH_EXT_PATH EXT_PATH("apps_data/flippass/gzip.bin")
#define KDBX_VAULT_MEMBER_INT_PATH  INT_PATH("apps_data/flippass/gzip_member.bin")
#define KDBX_VAULT_MEMBER_EXT_PATH  EXT_PATH("apps_data/flippass/gzip_member.bin")
#define KDBX_VAULT_WINDOW_INT_PATH  INT_PATH("apps_data/flippass/gzip_window.bin")
#define KDBX_VAULT_WINDOW_EXT_PATH  EXT_PATH("apps_data/flippass/gzip_window.bin")

KDBXVault* kdbx_vault_alloc(
    KDBXVaultBackend backend,
    size_t* committed_bytes,
    size_t commit_limit);
KDBXVault* kdbx_vault_alloc_with_path(
    KDBXVaultBackend backend,
    const char* file_path,
    size_t* committed_bytes,
    size_t commit_limit);
void kdbx_vault_set_budget(
    KDBXVault* vault,
    size_t* committed_bytes,
    size_t commit_limit);
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
void kdbx_vault_writer_abort(KDBXVaultWriter* writer);
bool kdbx_vault_writer_write(KDBXVaultWriter* writer, const uint8_t* data, size_t len);
bool kdbx_vault_writer_finish(KDBXVaultWriter* writer, KDBXFieldRef* out_ref);
void kdbx_vault_reader_reset(KDBXVaultReader* reader, KDBXVault* vault, const KDBXFieldRef* ref);
bool kdbx_vault_reader_read(KDBXVaultReader* reader, uint8_t* out, size_t capacity, size_t* out_size);
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
