#include "kdbx_vault.h"
#include "kdbx_protected.h"

#include <furi_hal_random.h>
#include <stdlib.h>

#define KDBX_VAULT_TRACE_TAG "FlipPassVault"
#define KDBX_VAULT_INDEX_NODE_CAPACITY 64U
#define KDBX_VAULT_RAM_PAGE_SIZE       1024U
#define KDBX_VAULT_MAC_SIZE            32U
#define KDBX_VAULT_OPEN_RETRIES        5U
#define KDBX_VAULT_OPEN_RETRY_MS       20U

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
#define KDBX_VAULT_TRACE(...) FURI_LOG_T(__VA_ARGS__)
#else
#define KDBX_VAULT_TRACE(...) \
    do {                      \
    } while(0)
#endif

typedef struct {
    uint32_t record_id;
    uint16_t plain_len;
    uint16_t cipher_len;
} KDBXVaultRecordHeader;

typedef struct {
    uint32_t value;
} KDBXVaultLocator;

typedef struct KDBXVaultIndexNode {
    struct KDBXVaultIndexNode* next;
    uint32_t base_record_id;
    uint16_t count;
    uint16_t reserved;
    KDBXVaultLocator locators[KDBX_VAULT_INDEX_NODE_CAPACITY];
} KDBXVaultIndexNode;

typedef struct KDBXVaultRamPage {
    struct KDBXVaultRamPage* next;
    size_t size;
    size_t used;
    uint8_t data[];
} KDBXVaultRamPage;

struct KDBXVault {
    Storage* storage;
    KDBXVaultBackend backend;
    const char* file_path;
    KDBXVaultIndexNode* index_head;
    KDBXVaultIndexNode* index_tail;
    KDBXVaultRamPage* page_head;
    KDBXVaultRamPage* page_tail;
    size_t* committed_bytes;
    size_t commit_limit;
    uint32_t next_record_id;
    size_t index_bytes;
    size_t page_bytes;
    bool budget_failed;
    bool storage_failed;
    const char* failure_reason;
    const char* storage_stage;
    const char* reader_failure_stage;
    uint32_t reader_failure_record;
    size_t last_failed_size;
    size_t last_failed_committed;
    size_t last_failed_max_free_block;
    uint8_t session_master[32];
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t nonce_prefix[4];
};

static bool kdbx_vault_backend_uses_file(KDBXVaultBackend backend) {
    return backend == KDBXVaultBackendFileInt || backend == KDBXVaultBackendFileExt;
}

static const char* kdbx_vault_default_path(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return KDBX_VAULT_SESSION_INT_PATH;
    case KDBXVaultBackendFileExt:
        return KDBX_VAULT_SESSION_EXT_PATH;
    default:
        return NULL;
    }
}

static uint32_t kdbx_vault_record_offset(const KDBXVaultLocator* locator) {
    return locator != NULL ? locator->value : 0U;
}

static void* kdbx_vault_record_ram_ptr(const KDBXVaultLocator* locator) {
    return locator != NULL ? (void*)(uintptr_t)locator->value : NULL;
}

static size_t kdbx_vault_align_up(size_t value, size_t alignment) {
    const size_t mask = alignment - 1U;
    return (value + mask) & ~mask;
}

static void kdbx_vault_note_budget_failure(KDBXVault* vault, const char* reason, size_t size) {
    furi_assert(vault);

    vault->budget_failed = true;
    vault->failure_reason = reason;
    vault->last_failed_size = size;
    vault->last_failed_committed = vault->committed_bytes != NULL ? *vault->committed_bytes : 0U;
    vault->last_failed_max_free_block = memmgr_heap_get_max_free_block();
}

static void kdbx_vault_note_storage_failure(KDBXVault* vault, const char* stage) {
    furi_assert(vault);

    vault->storage_failed = true;
    vault->storage_stage = stage;
}

static void
    kdbx_vault_note_reader_failure(KDBXVault* vault, const char* stage, uint32_t record_id) {
    furi_assert(vault);

    vault->reader_failure_stage = stage;
    vault->reader_failure_record = record_id;
}

static bool kdbx_vault_budget_reserve(KDBXVault* vault, size_t size) {
    furi_assert(vault);

    if(vault->committed_bytes != NULL && vault->commit_limit > 0U) {
        if(*vault->committed_bytes > vault->commit_limit ||
           size > (vault->commit_limit - *vault->committed_bytes)) {
            kdbx_vault_note_budget_failure(vault, "commit_limit", size);
            return false;
        }
    }

    if(memmgr_heap_get_max_free_block() < size) {
        kdbx_vault_note_budget_failure(vault, "max_free_block", size);
        return false;
    }

    if(vault->committed_bytes != NULL) {
        *vault->committed_bytes += size;
    }

    return true;
}

static const char* kdbx_vault_directory_path(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return INT_PATH("apps_data/flippass");
    case KDBXVaultBackendFileExt:
        return EXT_PATH("apps_data/flippass");
    default:
        return NULL;
    }
}

const char* kdbx_vault_backend_path(KDBXVaultBackend backend) {
    return kdbx_vault_default_path(backend);
}

const char* kdbx_vault_backend_label(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendRam:
        return "encrypted RAM vault";
    case KDBXVaultBackendFileInt:
        return "encrypted internal session vault";
    case KDBXVaultBackendFileExt:
        return "encrypted SD-card session vault";
    default:
        return "no vault";
    }
}

bool kdbx_vault_backend_supported(KDBXVaultBackend backend) {
#ifdef FURI_RAM_EXEC
    if(backend == KDBXVaultBackendFileInt) {
        return false;
    }
#endif
    return backend != KDBXVaultBackendNone;
}

const char* kdbx_vault_backend_unavailable_reason(KDBXVaultBackend backend) {
#ifdef FURI_RAM_EXEC
    if(backend == KDBXVaultBackendFileInt) {
        return "This firmware runs FlipPass as an external app, so /int is unavailable. Retry with the encrypted SD-card vault instead.";
    }
#else
    UNUSED(backend);
#endif
    return "The selected encrypted vault backend is unavailable in this build.";
}

static void kdbx_vault_derive_keys(KDBXVault* vault) {
    uint8_t hash[64];

    furi_assert(vault);
    sha512_Raw(vault->session_master, sizeof(vault->session_master), hash);
    memcpy(vault->enc_key, hash, sizeof(vault->enc_key));
    memcpy(vault->mac_key, hash + sizeof(vault->enc_key), sizeof(vault->mac_key));
    memzero(hash, sizeof(hash));
}

static void kdbx_vault_record_nonce(const KDBXVault* vault, uint32_t record_id, uint8_t nonce[12]) {
    furi_assert(vault);
    furi_assert(nonce);

    memcpy(nonce, vault->nonce_prefix, sizeof(vault->nonce_prefix));
    for(size_t index = 0; index < 8U; index++) {
        nonce[4U + index] = (uint8_t)(((uint64_t)record_id >> (index * 8U)) & 0xFFU);
    }
}

static void kdbx_vault_record_mac(
    const KDBXVault* vault,
    const KDBXVaultRecordHeader* header,
    const uint8_t* ciphertext,
    uint8_t mac[KDBX_VAULT_MAC_SIZE]) {
    HMAC_SHA256_CTX hmac_ctx;

    furi_assert(vault);
    furi_assert(header);
    furi_assert(mac);

    hmac_sha256_Init(&hmac_ctx, vault->mac_key, sizeof(vault->mac_key));
    hmac_sha256_Update(&hmac_ctx, (const uint8_t*)header, sizeof(*header));
    if(ciphertext != NULL && header->cipher_len > 0U) {
        hmac_sha256_Update(&hmac_ctx, ciphertext, header->cipher_len);
    }
    hmac_sha256_Final(&hmac_ctx, mac);
}

static bool kdbx_vault_record_read(
    KDBXVault* vault,
    uint32_t record_id,
    KDBXVaultRecordHeader* header,
    uint8_t* ciphertext,
    uint8_t mac[KDBX_VAULT_MAC_SIZE]);

static bool kdbx_vault_write_bytes(File* file, const void* data, size_t size) {
    return storage_file_write(file, data, size) == size;
}

static bool kdbx_vault_cleanup_file(Storage* storage, const char* path) {
    if(storage == NULL || path == NULL || !storage_file_exists(storage, path)) {
        return true;
    }

    File* file = storage_file_alloc(storage);
    if(file == NULL) {
        return false;
    }

    bool ok = true;
    if(storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        uint64_t remaining = storage_file_size(file);
        uint8_t wipe[256];
        memset(wipe, 0, sizeof(wipe));
        storage_file_seek(file, 0U, true);

        while(remaining > 0U && ok) {
            const size_t chunk = (remaining > sizeof(wipe)) ? sizeof(wipe) : (size_t)remaining;
            ok = kdbx_vault_write_bytes(file, wipe, chunk);
            remaining -= chunk;
        }

        if(ok) {
            storage_file_sync(file);
            storage_file_seek(file, 0U, true);
            storage_file_truncate(file);
            storage_file_close(file);
        } else {
            storage_file_close(file);
        }
    } else {
        storage_file_close(file);
        ok = false;
    }

    storage_file_free(file);
    storage_simply_remove(storage, path);
    return ok;
}

bool kdbx_vault_cleanup_backend(Storage* storage, KDBXVaultBackend backend) {
    if(!kdbx_vault_backend_supported(backend)) {
        return true;
    }

    return kdbx_vault_cleanup_file(storage, kdbx_vault_backend_path(backend));
}

void kdbx_vault_cleanup_all_sessions(Storage* storage) {
    if(storage == NULL) {
        return;
    }

    kdbx_vault_cleanup_backend(storage, KDBXVaultBackendFileInt);
    kdbx_vault_cleanup_backend(storage, KDBXVaultBackendFileExt);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_SCRATCH_INT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_SCRATCH_EXT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_MEMBER_INT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_MEMBER_EXT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_WINDOW_INT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_WINDOW_EXT_PATH);
}

void kdbx_vault_cleanup_runtime_sessions(Storage* storage) {
    if(storage == NULL) {
        return;
    }

    kdbx_vault_cleanup_backend(storage, KDBXVaultBackendFileExt);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_SCRATCH_EXT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_MEMBER_EXT_PATH);
    kdbx_vault_cleanup_file(storage, KDBX_VAULT_WINDOW_EXT_PATH);
}

static KDBXVaultIndexNode* kdbx_vault_index_node_alloc(KDBXVault* vault) {
    furi_assert(vault);

    if(!kdbx_vault_budget_reserve(vault, sizeof(KDBXVaultIndexNode))) {
        return NULL;
    }

    KDBXVaultIndexNode* node = malloc(sizeof(KDBXVaultIndexNode));
    if(node == NULL) {
        kdbx_vault_note_budget_failure(vault, "malloc", sizeof(KDBXVaultIndexNode));
        return NULL;
    }

    memset(node, 0, sizeof(*node));
    node->base_record_id = vault->next_record_id;
    vault->index_bytes += sizeof(*node);

    if(vault->index_tail != NULL) {
        vault->index_tail->next = node;
    } else {
        vault->index_head = node;
    }
    vault->index_tail = node;
    return node;
}

static bool kdbx_vault_index_add(KDBXVault* vault, const KDBXVaultLocator* locator) {
    furi_assert(vault);
    furi_assert(locator);

    KDBXVaultIndexNode* node = vault->index_tail;
    if(node == NULL || node->count >= KDBX_VAULT_INDEX_NODE_CAPACITY) {
        node = kdbx_vault_index_node_alloc(vault);
        if(node == NULL) {
            return false;
        }
    }

    node->locators[node->count++] = *locator;
    return true;
}

static const KDBXVaultLocator* kdbx_vault_index_get(const KDBXVault* vault, uint32_t record_id) {
    furi_assert(vault);

    for(KDBXVaultIndexNode* node = vault->index_head; node != NULL; node = node->next) {
        const uint32_t first = node->base_record_id;
        const uint32_t last = first + node->count;
        if(record_id >= first && record_id < last) {
            return &node->locators[record_id - first];
        }
    }

    return NULL;
}

static KDBXVaultRamPage* kdbx_vault_page_alloc(KDBXVault* vault, size_t min_size) {
    furi_assert(vault);

    const size_t payload_size =
        (min_size > KDBX_VAULT_RAM_PAGE_SIZE) ? min_size : KDBX_VAULT_RAM_PAGE_SIZE;
    if(payload_size > (SIZE_MAX - sizeof(KDBXVaultRamPage))) {
        kdbx_vault_note_budget_failure(vault, "size_overflow", payload_size);
        return NULL;
    }

    const size_t alloc_size = sizeof(KDBXVaultRamPage) + payload_size;
    if(!kdbx_vault_budget_reserve(vault, alloc_size)) {
        return NULL;
    }

    KDBXVaultRamPage* page = malloc(alloc_size);
    if(page == NULL) {
        kdbx_vault_note_budget_failure(vault, "malloc", alloc_size);
        return NULL;
    }

    memset(page, 0, alloc_size);
    page->size = payload_size;
    vault->page_bytes += alloc_size;
    if(vault->page_tail != NULL) {
        vault->page_tail->next = page;
    } else {
        vault->page_head = page;
    }
    vault->page_tail = page;
    return page;
}

static void* kdbx_vault_page_alloc_block(KDBXVault* vault, size_t size, size_t alignment) {
    KDBXVaultRamPage* page = vault->page_tail;
    size_t offset = 0U;

    if(page != NULL) {
        offset = kdbx_vault_align_up(page->used, alignment);
        if(offset > page->size || size > (page->size - offset)) {
            page = NULL;
        }
    }

    if(page == NULL) {
        page = kdbx_vault_page_alloc(vault, size + alignment);
        if(page == NULL) {
            return NULL;
        }
        offset = kdbx_vault_align_up(page->used, alignment);
    }

    void* result = &page->data[offset];
    page->used = offset + size;
    return result;
}

static bool kdbx_vault_open_file(KDBXVault* vault) {
    furi_assert(vault);

    const char* dir_path = kdbx_vault_directory_path(vault->backend);
    const char* file_path = vault->file_path;

    if(dir_path == NULL || file_path == NULL) {
        kdbx_vault_note_storage_failure(vault, "open_invalid_path");
        return false;
    }

    storage_simply_mkdir(vault->storage, dir_path);
    kdbx_vault_cleanup_file(vault->storage, file_path);

    File* file = storage_file_alloc(vault->storage);
    if(file == NULL) {
        kdbx_vault_note_storage_failure(vault, "open_alloc");
        return false;
    }

    bool opened = false;
    for(size_t attempt = 0U; attempt < KDBX_VAULT_OPEN_RETRIES && !opened; attempt++) {
        opened = storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
        if(!opened) {
            storage_file_close(file);
            if((attempt + 1U) < KDBX_VAULT_OPEN_RETRIES) {
                furi_delay_ms(KDBX_VAULT_OPEN_RETRY_MS);
            }
        }
    }

    if(!opened) {
        storage_file_free(file);
        kdbx_vault_note_storage_failure(vault, "open_create");
        return false;
    }

    if(!storage_file_close(file)) {
        storage_file_free(file);
        kdbx_vault_note_storage_failure(vault, "open_close");
        return false;
    }

    storage_file_free(file);

    return true;
}

static File* kdbx_vault_session_file_open(
    KDBXVault* vault,
    FS_AccessMode access_mode,
    FS_OpenMode open_mode) {
    furi_assert(vault);

    if(vault->storage == NULL) {
        kdbx_vault_note_storage_failure(vault, "session_no_storage");
        return NULL;
    }

    File* file = storage_file_alloc(vault->storage);
    if(file == NULL) {
        kdbx_vault_note_storage_failure(vault, "session_alloc");
        return NULL;
    }

    if(!storage_file_open(file, vault->file_path, access_mode, open_mode)) {
        storage_file_close(file);
        storage_file_free(file);
        kdbx_vault_note_storage_failure(vault, "session_open");
        return NULL;
    }

    return file;
}

static void kdbx_vault_session_file_close(File* file) {
    if(file == NULL) {
        return;
    }

    storage_file_close(file);
    storage_file_free(file);
}

static void kdbx_vault_writer_close_stream_file(KDBXVaultWriter* writer, bool sync_before_close) {
    if(writer == NULL || writer->stream_file == NULL) {
        return;
    }

    if(sync_before_close) {
        storage_file_sync(writer->stream_file);
    }
    kdbx_vault_session_file_close(writer->stream_file);
    writer->stream_file = NULL;
    writer->stream_unsynced_bytes = 0U;
}

static void kdbx_vault_writer_release_pending(KDBXVaultWriter* writer) {
    if(writer == NULL || writer->pending == NULL) {
        return;
    }

    memzero(writer->pending, writer->pending_capacity);
    if(writer->pending_owned) {
        free(writer->pending);
    }
    writer->pending = NULL;
    writer->pending_capacity = 0U;
    writer->pending_owned = false;
}

KDBXVault* kdbx_vault_alloc(
    KDBXVaultBackend backend,
    size_t* committed_bytes,
    size_t commit_limit) {
    return kdbx_vault_alloc_with_path(backend, NULL, committed_bytes, commit_limit);
}

KDBXVault* kdbx_vault_alloc_with_path(
    KDBXVaultBackend backend,
    const char* file_path,
    size_t* committed_bytes,
    size_t commit_limit) {
    if(backend == KDBXVaultBackendNone) {
        return NULL;
    }

    KDBXVault* vault = malloc(sizeof(KDBXVault));
    if(vault == NULL) {
        return NULL;
    }

    memset(vault, 0, sizeof(*vault));
    vault->backend = backend;
    vault->file_path = file_path != NULL ? file_path : kdbx_vault_default_path(backend);
    vault->committed_bytes = committed_bytes;
    vault->commit_limit = commit_limit;
    vault->next_record_id = 1U;
    if(kdbx_vault_backend_uses_file(backend)) {
        vault->storage = furi_record_open(RECORD_STORAGE);
    }

    furi_hal_random_fill_buf(vault->session_master, sizeof(vault->session_master));
    furi_hal_random_fill_buf(vault->nonce_prefix, sizeof(vault->nonce_prefix));
    kdbx_vault_derive_keys(vault);

    if(kdbx_vault_backend_uses_file(backend) && !kdbx_vault_open_file(vault)) {
        return vault;
    }

    return vault;
}

void kdbx_vault_set_budget(
    KDBXVault* vault,
    size_t* committed_bytes,
    size_t commit_limit) {
    if(vault == NULL) {
        return;
    }

    vault->committed_bytes = committed_bytes;
    vault->commit_limit = commit_limit;
}

static uint8_t* kdbx_vault_ram_record_ciphertext(KDBXVaultRecordHeader* header) {
    return ((uint8_t*)header) + sizeof(*header);
}

static uint8_t* kdbx_vault_ram_record_mac(KDBXVaultRecordHeader* header) {
    return kdbx_vault_ram_record_ciphertext(header) + header->cipher_len;
}

static size_t kdbx_vault_ram_record_size(const KDBXVaultRecordHeader* header) {
    furi_assert(header);

    return sizeof(*header) + header->cipher_len + KDBX_VAULT_MAC_SIZE;
}

static KDBXVaultLocator* kdbx_vault_index_next_locator(
    KDBXVaultIndexNode** node,
    uint16_t* locator_index) {
    furi_assert(locator_index);

    while(node != NULL && *node != NULL && *locator_index >= (*node)->count) {
        *node = (*node)->next;
        *locator_index = 0U;
    }

    if(node == NULL || *node == NULL) {
        return NULL;
    }

    return &(*node)->locators[(*locator_index)++];
}

static void kdbx_vault_copy_session_state(KDBXVault* target, const KDBXVault* source) {
    furi_assert(target);
    furi_assert(source);

    memcpy(target->session_master, source->session_master, sizeof(target->session_master));
    memcpy(target->enc_key, source->enc_key, sizeof(target->enc_key));
    memcpy(target->mac_key, source->mac_key, sizeof(target->mac_key));
    memcpy(target->nonce_prefix, source->nonce_prefix, sizeof(target->nonce_prefix));
    target->next_record_id = source->next_record_id;
}

static void kdbx_vault_take_index_state(KDBXVault* target, KDBXVault* source) {
    furi_assert(target);
    furi_assert(source);

    target->index_head = source->index_head;
    target->index_tail = source->index_tail;
    target->index_bytes = source->index_bytes;

    source->index_head = NULL;
    source->index_tail = NULL;
    source->index_bytes = 0U;
    source->next_record_id = 1U;
}

static bool kdbx_vault_append_record(
    KDBXVault* vault,
    File* stream_file,
    bool sync_after_write,
    uint8_t* ciphertext,
    size_t plain_len,
    uint32_t* out_record_id) {
    KDBXVaultRecordHeader header;
    KDBXVaultLocator locator;
    uint8_t mac[KDBX_VAULT_MAC_SIZE];

    furi_assert(vault);
    furi_assert(ciphertext);
    furi_assert(out_record_id);

    if(plain_len > UINT16_MAX) {
        kdbx_vault_note_storage_failure(vault, "record_plain_len");
        return false;
    }

    header.record_id = vault->next_record_id;
    header.plain_len = (uint16_t)plain_len;
    header.cipher_len = (uint16_t)plain_len;
    kdbx_vault_record_mac(vault, &header, ciphertext, mac);

    memset(&locator, 0, sizeof(locator));
    if(vault->backend == KDBXVaultBackendRam) {
        const size_t record_size = sizeof(header) + plain_len + sizeof(mac);
        KDBXVaultRecordHeader* ram_record =
            kdbx_vault_page_alloc_block(vault, record_size, sizeof(uint32_t));
        if(ram_record == NULL) {
            return false;
        }

        memcpy(ram_record, &header, sizeof(header));
        memcpy(kdbx_vault_ram_record_ciphertext(ram_record), ciphertext, plain_len);
        memcpy(kdbx_vault_ram_record_mac(ram_record), mac, sizeof(mac));
        locator.value = (uint32_t)(uintptr_t)ram_record;
    } else {
        File* file = stream_file;
        const bool owns_file = file == NULL;
        if(file == NULL) {
            file = kdbx_vault_session_file_open(vault, FSAM_WRITE, FSOM_OPEN_APPEND);
        }
        if(file == NULL) {
            return false;
        }

        const uint64_t file_offset = storage_file_tell(file);
        if(file_offset > UINT32_MAX) {
            kdbx_vault_note_storage_failure(vault, "record_offset");
            kdbx_vault_session_file_close(file);
            return false;
        }

        locator.value = (uint32_t)file_offset;
        if(!kdbx_vault_write_bytes(file, &header, sizeof(header)) ||
           !kdbx_vault_write_bytes(file, ciphertext, plain_len) ||
           !kdbx_vault_write_bytes(file, mac, sizeof(mac)) ||
           (sync_after_write && !storage_file_sync(file))) {
            kdbx_vault_note_storage_failure(vault, "record_append");
            if(owns_file) {
                kdbx_vault_session_file_close(file);
            }
            return false;
        }

        if(owns_file) {
            kdbx_vault_session_file_close(file);
        }
    }

    if(!kdbx_vault_index_add(vault, &locator)) {
        return false;
    }

    *out_record_id = vault->next_record_id++;
    return true;
}

void kdbx_vault_writer_reset_with_pending(
    KDBXVaultWriter* writer,
    KDBXVault* vault,
    uint8_t* pending,
    size_t pending_capacity) {
    furi_assert(writer);

    memzero(writer, sizeof(*writer));
    writer->vault = vault;
    writer->failed = (vault == NULL) || (vault != NULL && vault->storage_failed);
    if(pending != NULL && pending_capacity > 0U) {
        writer->pending = pending;
        writer->pending_capacity = pending_capacity;
        writer->pending_owned = false;
        return;
    }

    writer->pending_capacity = KDBX_VAULT_RECORD_PLAIN_MAX;
    writer->pending = malloc(writer->pending_capacity);
    if(writer->pending == NULL) {
        if(vault != NULL) {
            kdbx_vault_note_budget_failure(vault, "writer_pending", writer->pending_capacity);
        }
        writer->failed = true;
        writer->pending_capacity = 0U;
    } else {
        writer->pending_owned = true;
    }
}

void kdbx_vault_writer_reset(KDBXVaultWriter* writer, KDBXVault* vault) {
    kdbx_vault_writer_reset_with_pending(writer, vault, NULL, 0U);
}

void kdbx_vault_writer_set_file_streaming(KDBXVaultWriter* writer, bool enabled) {
    furi_assert(writer);

    if(writer->stream_file != NULL && (!enabled || writer->vault == NULL ||
                                       !kdbx_vault_backend_uses_file(writer->vault->backend))) {
        kdbx_vault_writer_close_stream_file(writer, true);
    }

    writer->stream_file_mode =
        enabled && writer->vault != NULL && kdbx_vault_backend_uses_file(writer->vault->backend);
}

void kdbx_vault_writer_abort(KDBXVaultWriter* writer) {
    if(writer == NULL) {
        return;
    }

    kdbx_vault_writer_close_stream_file(writer, false);
    kdbx_vault_writer_release_pending(writer);
    writer->pending_len = 0U;
    writer->failed = true;
}

static bool kdbx_vault_writer_flush(KDBXVaultWriter* writer) {
    uint8_t nonce[12];
    uint32_t record_id = 0U;
    File* stream_file = NULL;

    furi_assert(writer);

    if(writer->failed || writer->vault == NULL) {
        writer->failed = true;
        return false;
    }

    if(writer->vault->storage_failed) {
        writer->failed = true;
        return false;
    }

    if(writer->pending_len == 0U) {
        return true;
    }

    kdbx_vault_record_nonce(writer->vault, writer->vault->next_record_id, nonce);
    if(writer->pending == NULL || writer->pending_capacity == 0U ||
       !kdbx_chacha20_xor(
           writer->pending,
           writer->pending_len,
           writer->vault->enc_key,
           sizeof(writer->vault->enc_key),
           nonce,
           sizeof(nonce),
           0U)) {
        writer->failed = true;
        return false;
    }

    if(writer->stream_file_mode) {
        if(writer->stream_file == NULL) {
            writer->stream_file =
                kdbx_vault_session_file_open(writer->vault, FSAM_WRITE, FSOM_OPEN_APPEND);
            if(writer->stream_file == NULL) {
                writer->failed = true;
                if(writer->pending != NULL) {
                    memzero(writer->pending, writer->pending_capacity);
                }
                writer->pending_len = 0U;
                return false;
            }
        }
        stream_file = writer->stream_file;
    }

    if(!kdbx_vault_append_record(
           writer->vault,
           stream_file,
           !writer->stream_file_mode,
           writer->pending,
           writer->pending_len,
           &record_id)) {
        writer->failed = true;
        kdbx_vault_writer_close_stream_file(writer, false);
        if(writer->pending != NULL) {
            memzero(writer->pending, writer->pending_capacity);
        }
        writer->pending_len = 0U;
        return false;
    }

    if(writer->stream_file_mode) {
        writer->stream_unsynced_bytes += sizeof(KDBXVaultRecordHeader) + writer->pending_len + sizeof(uint8_t) * KDBX_VAULT_MAC_SIZE;
        if(writer->stream_unsynced_bytes >= KDBX_VAULT_STREAM_SYNC_INTERVAL) {
            if(!storage_file_sync(writer->stream_file)) {
                kdbx_vault_note_storage_failure(writer->vault, "record_sync");
                writer->failed = true;
                kdbx_vault_writer_close_stream_file(writer, false);
                if(writer->pending != NULL) {
                    memzero(writer->pending, writer->pending_capacity);
                }
                writer->pending_len = 0U;
                return false;
            }
            writer->stream_unsynced_bytes = 0U;
        }
    }

    if(record_id > UINT16_MAX) {
        kdbx_vault_note_budget_failure(writer->vault, "record_id_overflow", record_id);
        writer->failed = true;
        if(writer->pending != NULL) {
            memzero(writer->pending, writer->pending_capacity);
        }
        writer->pending_len = 0U;
        return false;
    }

    if(writer->ref.record_count == 0U) {
        writer->ref.first_record = (uint16_t)record_id;
    }
    if(writer->ref.record_count == UINT16_MAX) {
        writer->failed = true;
        if(writer->pending != NULL) {
            memzero(writer->pending, writer->pending_capacity);
        }
        writer->pending_len = 0U;
        return false;
    }

    writer->ref.record_count++;
    writer->ref.plain_len += writer->pending_len;

    if(writer->pending != NULL) {
        memzero(writer->pending, writer->pending_capacity);
    }
    writer->pending_len = 0U;
    return true;
}

bool kdbx_vault_writer_write(KDBXVaultWriter* writer, const uint8_t* data, size_t len) {
    furi_assert(writer);
    furi_assert(data);

    if(writer->failed || writer->pending == NULL || writer->pending_capacity == 0U) {
        writer->failed = true;
        return false;
    }

    while(len > 0U) {
        const size_t available = writer->pending_capacity - writer->pending_len;
        const size_t chunk = (len < available) ? len : available;
        memcpy(writer->pending + writer->pending_len, data, chunk);
        writer->pending_len += chunk;
        data += chunk;
        len -= chunk;

        if(writer->pending_len == writer->pending_capacity && !kdbx_vault_writer_flush(writer)) {
            return false;
        }
    }

    return !writer->failed;
}

bool kdbx_vault_writer_finish(KDBXVaultWriter* writer, KDBXFieldRef* out_ref) {
    furi_assert(writer);
    furi_assert(out_ref);

    if(!kdbx_vault_writer_flush(writer)) {
        kdbx_vault_writer_close_stream_file(writer, false);
        kdbx_vault_writer_release_pending(writer);
        return false;
    }

    kdbx_vault_writer_close_stream_file(writer, true);
    kdbx_vault_writer_release_pending(writer);

    *out_ref = writer->ref;
    memzero(writer, sizeof(*writer));
    return true;
}

void kdbx_vault_reader_reset(KDBXVaultReader* reader, KDBXVault* vault, const KDBXFieldRef* ref) {
    furi_assert(reader);
    furi_assert(ref);

    memzero(reader, sizeof(*reader));
    reader->vault = vault;
    reader->ref = *ref;
    reader->failed = (vault == NULL) || (vault != NULL && vault->storage_failed);
    if(vault != NULL) {
        vault->reader_failure_stage = NULL;
        vault->reader_failure_record = 0U;
    }
}

bool kdbx_vault_reader_read(KDBXVaultReader* reader, uint8_t* out, size_t capacity, size_t* out_size) {
    furi_assert(reader);
    furi_assert(out);
    furi_assert(out_size);

    *out_size = 0U;
    if(reader->failed || reader->vault == NULL) {
        if(reader->vault != NULL) {
            kdbx_vault_note_reader_failure(reader->vault, "reader_unavailable", 0U);
        }
        reader->failed = true;
        return false;
    }

    while(*out_size < capacity) {
        if(reader->record_plain_offset < reader->record_plain_len) {
            const size_t available = reader->record_plain_len - reader->record_plain_offset;
            const size_t chunk = (capacity - *out_size) < available ? (capacity - *out_size) : available;
            memcpy(
                out + *out_size, reader->record_plain + reader->record_plain_offset, chunk);
            reader->record_plain_offset += chunk;
            *out_size += chunk;
            continue;
        }

        if(reader->record_index >= reader->ref.record_count) {
            return true;
        }

        KDBXVaultRecordHeader header;
        uint8_t ciphertext[KDBX_VAULT_RECORD_PLAIN_MAX];
        uint8_t mac[KDBX_VAULT_MAC_SIZE];
        uint8_t expected_mac[KDBX_VAULT_MAC_SIZE];
        uint8_t nonce[12];
        const uint32_t record_id = reader->ref.first_record + reader->record_index;

        if(reader->record_index < 4U) {
            KDBX_VAULT_TRACE(
                KDBX_VAULT_TRACE_TAG,
                "reader begin record=%lu offset=%lu count=%u cap=%lu",
                (unsigned long)record_id,
                (unsigned long)reader->record_plain_offset,
                reader->record_index,
                (unsigned long)capacity);
        }

        if(!kdbx_vault_record_read(
               reader->vault, record_id, &header, ciphertext, mac)) {
            memzero(ciphertext, sizeof(ciphertext));
            kdbx_vault_note_reader_failure(reader->vault, "record_read", record_id);
            reader->failed = true;
            KDBX_VAULT_TRACE(
                KDBX_VAULT_TRACE_TAG,
                "reader record read failed record=%lu",
                (unsigned long)record_id);
            return false;
        }

        if(reader->record_index < 4U) {
            KDBX_VAULT_TRACE(
                KDBX_VAULT_TRACE_TAG,
                "reader header record=%lu header_id=%lu plain=%u cipher=%u",
                (unsigned long)record_id,
                (unsigned long)header.record_id,
                header.plain_len,
                header.cipher_len);
        }

        if(header.record_id != record_id ||
           header.plain_len != header.cipher_len || header.plain_len > sizeof(ciphertext)) {
            memzero(ciphertext, sizeof(ciphertext));
            kdbx_vault_note_reader_failure(reader->vault, "header_invalid", record_id);
            reader->failed = true;
            KDBX_VAULT_TRACE(
                KDBX_VAULT_TRACE_TAG,
                "reader header invalid record=%lu header_id=%lu plain=%u cipher=%u",
                (unsigned long)record_id,
                (unsigned long)header.record_id,
                header.plain_len,
                header.cipher_len);
            return false;
        }

        kdbx_vault_record_mac(reader->vault, &header, ciphertext, expected_mac);
        if(memcmp(mac, expected_mac, KDBX_VAULT_MAC_SIZE) != 0) {
            memzero(expected_mac, sizeof(expected_mac));
            memzero(ciphertext, sizeof(ciphertext));
            kdbx_vault_note_reader_failure(reader->vault, "mac_mismatch", record_id);
            reader->failed = true;
            return false;
        }

        kdbx_vault_record_nonce(reader->vault, header.record_id, nonce);
        if(!kdbx_chacha20_xor(
               ciphertext,
               header.cipher_len,
               reader->vault->enc_key,
               sizeof(reader->vault->enc_key),
               nonce,
               sizeof(nonce),
               0U)) {
            memzero(expected_mac, sizeof(expected_mac));
            memzero(ciphertext, sizeof(ciphertext));
            kdbx_vault_note_reader_failure(reader->vault, "decrypt", record_id);
            reader->failed = true;
            return false;
        }
        memzero(expected_mac, sizeof(expected_mac));

        memcpy(reader->record_plain, ciphertext, header.plain_len);
        reader->record_plain_len = header.plain_len;
        reader->record_plain_offset = 0U;
        reader->record_index++;
        memzero(ciphertext, sizeof(ciphertext));
        if(reader->record_index <= 4U) {
            KDBX_VAULT_TRACE(
                KDBX_VAULT_TRACE_TAG,
                "reader ready record=%lu plain=%u next_index=%u",
                (unsigned long)record_id,
                header.plain_len,
                reader->record_index);
        }
    }

    return true;
}

bool kdbx_vault_ref_is_empty(const KDBXFieldRef* ref) {
    return ref == NULL || ref->record_count == 0U || ref->plain_len == 0U;
}

static bool kdbx_vault_record_read(
    KDBXVault* vault,
    uint32_t record_id,
    KDBXVaultRecordHeader* header,
    uint8_t* ciphertext,
    uint8_t mac[KDBX_VAULT_MAC_SIZE]) {
    const KDBXVaultLocator* locator = kdbx_vault_index_get(vault, record_id);
    if(locator == NULL) {
        return false;
    }

    if(vault->backend == KDBXVaultBackendRam) {
        KDBXVaultRecordHeader* ram_record = kdbx_vault_record_ram_ptr(locator);
        if(ram_record == NULL) {
            return false;
        }

        memcpy(header, ram_record, sizeof(*header));
        memcpy(ciphertext, kdbx_vault_ram_record_ciphertext(ram_record), header->cipher_len);
        memcpy(mac, kdbx_vault_ram_record_mac(ram_record), sizeof(uint8_t) * KDBX_VAULT_MAC_SIZE);
        return true;
    }

    File* file = kdbx_vault_session_file_open(vault, FSAM_READ, FSOM_OPEN_EXISTING);
    if(file == NULL) {
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG, "record open failed record=%lu", (unsigned long)record_id);
        return false;
    }

    if(!storage_file_seek(file, (uint32_t)kdbx_vault_record_offset(locator), true)) {
        kdbx_vault_session_file_close(file);
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG,
            "record seek failed record=%lu offset=%lu",
            (unsigned long)record_id,
            (unsigned long)kdbx_vault_record_offset(locator));
        return false;
    }
    if(storage_file_read(file, header, sizeof(*header)) != sizeof(*header)) {
        kdbx_vault_session_file_close(file);
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG,
            "record header read failed record=%lu",
            (unsigned long)record_id);
        return false;
    }
    if(header->cipher_len > KDBX_VAULT_RECORD_PLAIN_MAX ||
        header->plain_len > header->cipher_len) {
        kdbx_vault_session_file_close(file);
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG,
            "record header size invalid record=%lu plain=%u cipher=%u",
            (unsigned long)record_id,
            header->plain_len,
            header->cipher_len);
        return false;
    }
    if(storage_file_read(file, ciphertext, header->cipher_len) != header->cipher_len) {
        kdbx_vault_session_file_close(file);
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG,
            "record cipher read failed record=%lu cipher=%u",
            (unsigned long)record_id,
            header->cipher_len);
        return false;
    }
    if(storage_file_read(file, mac, KDBX_VAULT_MAC_SIZE) != KDBX_VAULT_MAC_SIZE) {
        kdbx_vault_session_file_close(file);
        KDBX_VAULT_TRACE(
            KDBX_VAULT_TRACE_TAG, "record mac read failed record=%lu", (unsigned long)record_id);
        return false;
    }

    kdbx_vault_session_file_close(file);
    KDBX_VAULT_TRACE(
        KDBX_VAULT_TRACE_TAG,
        "record read ok record=%lu offset=%lu plain=%u cipher=%u",
        (unsigned long)record_id,
        (unsigned long)kdbx_vault_record_offset(locator),
        header->plain_len,
        header->cipher_len);

    return true;
}

static bool kdbx_vault_read_ref_into(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    uint8_t* out,
    size_t out_size) {
    KDBXVaultReader reader;
    size_t offset = 0U;

    furi_assert(vault);
    furi_assert(ref);
    furi_assert(out);

    kdbx_vault_reader_reset(&reader, vault, ref);
    while(offset < out_size) {
        size_t chunk_size = 0U;
        if(!kdbx_vault_reader_read(&reader, out + offset, out_size - offset, &chunk_size)) {
            memzero(out, out_size);
            return false;
        }
        if(chunk_size == 0U) {
            break;
        }
        offset += chunk_size;
    }

    if(offset != out_size) {
        memzero(out, out_size);
        return false;
    }

    return true;
}

bool kdbx_vault_load_bytes(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    uint8_t** out_data,
    size_t* out_size) {
    furi_assert(vault);
    furi_assert(ref);
    furi_assert(out_data);
    furi_assert(out_size);

    *out_data = NULL;
    *out_size = 0U;

    if(kdbx_vault_ref_is_empty(ref)) {
        uint8_t* empty = malloc(1U);
        if(empty == NULL) {
            return false;
        }
        empty[0] = 0U;
        *out_data = empty;
        return true;
    }

    uint8_t* plain = malloc(ref->plain_len);
    if(plain == NULL) {
        return false;
    }

    if(!kdbx_vault_read_ref_into(vault, ref, plain, ref->plain_len)) {
        free(plain);
        return false;
    }

    *out_data = plain;
    *out_size = ref->plain_len;
    return true;
}

bool kdbx_vault_load_text(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    char** out_text,
    size_t* out_size) {
    char* text = NULL;

    furi_assert(vault);
    furi_assert(ref);
    furi_assert(out_text);
    furi_assert(out_size);

    *out_text = NULL;
    *out_size = 0U;

    if(kdbx_vault_ref_is_empty(ref)) {
        text = malloc(1U);
        if(text == NULL) {
            return false;
        }
        text[0] = '\0';
        *out_text = text;
        return true;
    }

    text = malloc(ref->plain_len + 1U);
    if(text == NULL) {
        return false;
    }

    if(!kdbx_vault_read_ref_into(vault, ref, (uint8_t*)text, ref->plain_len)) {
        memzero(text, ref->plain_len + 1U);
        free(text);
        return false;
    }

    text[ref->plain_len] = '\0';
    *out_text = text;
    *out_size = ref->plain_len;
    return true;
}

static bool kdbx_vault_process_record_plain(
    KDBXVault* vault,
    const KDBXVaultRecordHeader* header,
    uint8_t* ciphertext,
    const uint8_t mac[KDBX_VAULT_MAC_SIZE],
    KDBXVaultChunkCallback callback,
    void* context) {
    uint8_t nonce[12];
    uint8_t expected_mac[KDBX_VAULT_MAC_SIZE];

    furi_assert(vault);
    furi_assert(header);
    furi_assert(ciphertext);
    furi_assert(mac);
    furi_assert(callback);

    kdbx_vault_record_mac(vault, header, ciphertext, expected_mac);
    if(memcmp(mac, expected_mac, KDBX_VAULT_MAC_SIZE) != 0) {
        memzero(expected_mac, sizeof(expected_mac));
        return false;
    }

    kdbx_vault_record_nonce(vault, header->record_id, nonce);
    if(!kdbx_chacha20_xor(
           ciphertext,
           header->cipher_len,
           vault->enc_key,
           sizeof(vault->enc_key),
           nonce,
           sizeof(nonce),
           0U)) {
        memzero(expected_mac, sizeof(expected_mac));
        return false;
    }

    const bool ok = callback(ciphertext, header->plain_len, context);
    memzero(expected_mac, sizeof(expected_mac));
    memzero(ciphertext, header->cipher_len);
    return ok;
}

bool kdbx_vault_stream_ref(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    KDBXVaultChunkCallback callback,
    void* context) {
    furi_assert(vault);
    furi_assert(ref);
    furi_assert(callback);

    if(kdbx_vault_ref_is_empty(ref)) {
        return callback(NULL, 0U, context);
    }

    for(uint16_t index = 0U; index < ref->record_count; index++) {
        KDBXVaultRecordHeader header;
        uint8_t ciphertext[KDBX_VAULT_RECORD_PLAIN_MAX];
        uint8_t mac[KDBX_VAULT_MAC_SIZE];

        if(!kdbx_vault_record_read(vault, ref->first_record + index, &header, ciphertext, mac)) {
            memzero(ciphertext, sizeof(ciphertext));
            return false;
        }

        if(header.record_id != (ref->first_record + index) ||
           header.plain_len != header.cipher_len || header.plain_len > sizeof(ciphertext)) {
            memzero(ciphertext, sizeof(ciphertext));
            return false;
        }

        if(!kdbx_vault_process_record_plain(vault, &header, ciphertext, mac, callback, context)) {
            memzero(ciphertext, sizeof(ciphertext));
            return false;
        }
    }

    return true;
}

bool kdbx_vault_promote_ram_to_file(KDBXVault* source, KDBXVault* target) {
    File* file = NULL;
    KDBXVaultRamPage* page = NULL;
    KDBXVaultIndexNode* node = NULL;
    uint16_t locator_index = 0U;
    uint32_t processed_records = 0U;
    size_t unsynced_bytes = 0U;

    furi_assert(source);
    furi_assert(target);

    if(source->backend != KDBXVaultBackendRam || !kdbx_vault_backend_uses_file(target->backend) ||
       source->storage_failed || target->storage_failed) {
        return false;
    }

    if(target->index_head != NULL || target->index_tail != NULL || target->page_head != NULL ||
       target->page_tail != NULL || target->index_bytes != 0U || target->page_bytes != 0U ||
       target->next_record_id != 1U) {
        kdbx_vault_note_storage_failure(target, "promote_target_state");
        return false;
    }

    file = kdbx_vault_session_file_open(target, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(file == NULL) {
        return false;
    }

    page = source->page_head;
    node = source->index_head;
    while(page != NULL) {
        size_t offset = 0U;

        while(offset < page->used) {
            KDBXVaultRecordHeader* ram_record = NULL;
            KDBXVaultLocator* locator = NULL;
            uint64_t file_offset = 0U;

            offset = kdbx_vault_align_up(offset, sizeof(uint32_t));
            if(offset >= page->used) {
                break;
            }

            ram_record = (KDBXVaultRecordHeader*)&page->data[offset];
            if(ram_record->plain_len != ram_record->cipher_len ||
               ram_record->cipher_len > KDBX_VAULT_RECORD_PLAIN_MAX) {
                kdbx_vault_note_storage_failure(target, "promote_record_header");
                goto fail;
            }

            const size_t ram_record_size = kdbx_vault_ram_record_size(ram_record);
            if(ram_record_size > (page->used - offset)) {
                kdbx_vault_note_storage_failure(target, "promote_record_bounds");
                goto fail;
            }

            locator = kdbx_vault_index_next_locator(&node, &locator_index);
            if(locator == NULL) {
                kdbx_vault_note_storage_failure(target, "promote_index_missing");
                goto fail;
            }

            if(ram_record->record_id != (processed_records + 1U) ||
               locator->value != (uint32_t)(uintptr_t)ram_record) {
                kdbx_vault_note_storage_failure(target, "promote_record_order");
                goto fail;
            }

            file_offset = storage_file_tell(file);
            if(file_offset > UINT32_MAX) {
                kdbx_vault_note_storage_failure(target, "promote_record_offset");
                goto fail;
            }

            if(!kdbx_vault_write_bytes(file, ram_record, ram_record_size)) {
                kdbx_vault_note_storage_failure(target, "promote_record_write");
                goto fail;
            }

            locator->value = (uint32_t)file_offset;
            processed_records++;
            unsynced_bytes += ram_record_size;
            if(unsynced_bytes >= KDBX_VAULT_STREAM_SYNC_INTERVAL) {
                if(!storage_file_sync(file)) {
                    kdbx_vault_note_storage_failure(target, "promote_record_sync");
                    goto fail;
                }
                unsynced_bytes = 0U;
            }

            offset += ram_record_size;
        }

        KDBXVaultRamPage* next_page = page->next;
        const size_t page_bytes = sizeof(KDBXVaultRamPage) + page->size;

        source->page_head = next_page;
        if(next_page == NULL) {
            source->page_tail = NULL;
        }
        if(source->page_bytes >= page_bytes) {
            source->page_bytes -= page_bytes;
        } else {
            source->page_bytes = 0U;
        }

        memzero(page, page_bytes);
        free(page);
        page = next_page;
    }

    if(processed_records != kdbx_vault_record_count(source) ||
       kdbx_vault_index_next_locator(&node, &locator_index) != NULL) {
        kdbx_vault_note_storage_failure(target, "promote_record_count");
        goto fail;
    }

    if(!storage_file_sync(file)) {
        kdbx_vault_note_storage_failure(target, "promote_close_sync");
        goto fail;
    }

    kdbx_vault_session_file_close(file);
    file = NULL;

    kdbx_vault_copy_session_state(target, source);
    kdbx_vault_take_index_state(target, source);
    target->budget_failed = false;
    target->failure_reason = NULL;
    target->last_failed_size = 0U;
    target->last_failed_committed = 0U;
    target->last_failed_max_free_block = 0U;
    target->storage_failed = false;
    target->storage_stage = NULL;
    target->reader_failure_stage = NULL;
    target->reader_failure_record = 0U;
    return true;

fail:
    if(file != NULL) {
        kdbx_vault_session_file_close(file);
    }
    return false;
}

KDBXVaultBackend kdbx_vault_get_backend(const KDBXVault* vault) {
    return vault != NULL ? vault->backend : KDBXVaultBackendNone;
}

Storage* kdbx_vault_get_storage(const KDBXVault* vault) {
    return vault != NULL ? vault->storage : NULL;
}

bool kdbx_vault_budget_failed(const KDBXVault* vault) {
    return vault != NULL && vault->budget_failed;
}

bool kdbx_vault_storage_failed(const KDBXVault* vault) {
    return vault != NULL && vault->storage_failed;
}

const char* kdbx_vault_failure_reason(const KDBXVault* vault) {
    return (vault != NULL && vault->failure_reason != NULL) ? vault->failure_reason : "none";
}

size_t kdbx_vault_record_overhead_bytes(void) {
    return sizeof(KDBXVaultRecordHeader) + KDBX_VAULT_MAC_SIZE;
}

size_t kdbx_vault_ram_page_payload_size(void) {
    return KDBX_VAULT_RAM_PAGE_SIZE;
}

size_t kdbx_vault_ram_page_overhead_bytes(void) {
    return sizeof(KDBXVaultRamPage);
}

size_t kdbx_vault_estimate_index_bytes(uint32_t record_count) {
    if(record_count == 0U) {
        return 0U;
    }

    const uint32_t node_count =
        (record_count + (KDBX_VAULT_INDEX_NODE_CAPACITY - 1U)) / KDBX_VAULT_INDEX_NODE_CAPACITY;
    return (size_t)node_count * sizeof(KDBXVaultIndexNode);
}

size_t kdbx_vault_last_failed_size(const KDBXVault* vault) {
    return vault != NULL ? vault->last_failed_size : 0U;
}

size_t kdbx_vault_last_failed_committed(const KDBXVault* vault) {
    return vault != NULL ? vault->last_failed_committed : 0U;
}

size_t kdbx_vault_last_failed_max_free_block(const KDBXVault* vault) {
    return vault != NULL ? vault->last_failed_max_free_block : 0U;
}

const char* kdbx_vault_last_reader_failure(const KDBXVault* vault) {
    return (vault != NULL && vault->reader_failure_stage != NULL) ? vault->reader_failure_stage :
                                                                     "none";
}

uint32_t kdbx_vault_last_reader_failure_record(const KDBXVault* vault) {
    return vault != NULL ? vault->reader_failure_record : 0U;
}

size_t kdbx_vault_index_bytes(const KDBXVault* vault) {
    return vault != NULL ? vault->index_bytes : 0U;
}

size_t kdbx_vault_page_bytes(const KDBXVault* vault) {
    return vault != NULL ? vault->page_bytes : 0U;
}

uint32_t kdbx_vault_record_count(const KDBXVault* vault) {
    return (vault != NULL && vault->next_record_id > 0U) ? (vault->next_record_id - 1U) : 0U;
}

const char* kdbx_vault_storage_stage(const KDBXVault* vault) {
    return (vault != NULL && vault->storage_stage != NULL) ? vault->storage_stage : "none";
}

void kdbx_vault_free(KDBXVault* vault) {
    if(vault == NULL) {
        return;
    }

    if(vault->storage != NULL && kdbx_vault_backend_uses_file(vault->backend)) {
        kdbx_vault_cleanup_file(vault->storage, vault->file_path);
    }

    for(KDBXVaultIndexNode* node = vault->index_head; node != NULL;) {
        KDBXVaultIndexNode* next = node->next;
        memzero(node, sizeof(*node));
        free(node);
        node = next;
    }

    for(KDBXVaultRamPage* page = vault->page_head; page != NULL;) {
        KDBXVaultRamPage* next = page->next;
        memzero(page, sizeof(KDBXVaultRamPage) + page->size);
        free(page);
        page = next;
    }

    if(vault->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }

    memzero(vault, sizeof(*vault));
    free(vault);
}
