#include "flippass.h"
#include "scenes/flippass_progress_view.h"
#include "flippass_crypto.h"
#include "flippass_db.h"
#include "kdbx/memzero.h"
#include "kdbx/xml_parser.h"
#include "plugins/flippass_open_acquire_plugin.h"
#include "plugins/flippass_open_host_plugin.h"
#include "plugins/flippass_open_host_room.h"
#include "plugins/flippass_open_inflate_plugin.h"
#include "plugins/flippass_open_model_plugin.h"
#include "plugins/flippass_open_stream_plugin.h"

#include <dialogs/dialogs.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OPEN_SAFETY_RESERVE_BYTES (8U * 1024U)
#if FLIPPASS_ENABLE_PROBE_LOGS
#define FLIPPASS_OPEN_FILE_BACKED_RESERVE_BYTES (2U * 1024U)
#else
#define FLIPPASS_OPEN_FILE_BACKED_RESERVE_BYTES (4U * 1024U)
#endif
#define FLIPPASS_OPEN_MODEL_GROWTH_RESERVE_BYTES        (4U * 1024U)
#define FLIPPASS_OPEN_MODEL_MAPPED_SAFETY_RESERVE_BYTES (3U * 1024U)
#define FLIPPASS_OPEN_MODEL_MAPPED_FILE_RESERVE_BYTES   (1U * 1024U)
#define FLIPPASS_OPEN_FIELD_WRITER_MIN_PENDING_BYTES    64U
#ifndef FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES
#define FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES 1280U
#endif
#define FLIPPASS_OPEN_ARENA_CHUNK_SIZE                   256U
#define FLIPPASS_OPEN_MODEL_PRESCAN_CHUNK_SLACK_BYTES    64U
#define FLIPPASS_OPEN_MODEL_PRESCAN_MAX_FREE_GUARD_BYTES 512U
#define FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES              UINT16_MAX
/* Predictive /ext threshold: when staged XML grows past this size, the model
 * is started directly on /ext instead of waiting for RAM-vault pressure to
 * trigger mid-stream promotion. Empirically, 80 KB is the smallest threshold
 * that does not over-promote small databases on the May 9 2026 hardware
 * baseline. Production behavior, not a probe-only gate. */
#define FLIPPASS_OPEN_PREDICTIVE_EXT_XML_BYTES           (80U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_LIMIT                (16U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT          (16U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_GZIP_DICT_BYTES        (32U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_GZIP_MARGIN_BYTES      (2U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_PAGED_RAM_BYTES        (4U * 1024U)

typedef struct FlipPassOpenSession FlipPassOpenSession;

typedef struct {
    App* app;
    KDBXArena* arena;
    KDBXVault* vault;
    KDBXVaultWriter field_writer;
    KDBXGroup* root_group;
    KDBXGroup* current_group;
    KDBXEntry* current_entry;
    size_t committed_bytes;
    size_t commit_limit;
    size_t group_count;
    size_t entry_count;
    size_t deferred_field_count;
    size_t deferred_plain_bytes;
    size_t deferred_stream_plain_bytes;
    FlipPassOpenSession* session;
    bool session_active;
    bool allow_ext_promotion;
    bool deferred_stream_active;
    bool deferred_stream_protected;
    bool vault_promotion_attempted;
} FlipPassOpenBuilderContext;

typedef struct {
    App* app;
    KDBXVault* vault;
    KDBXVaultWriter writer;
    KDBXScratchRef ref;
    size_t size;
} FlipPassOpenScratchContext;

typedef struct {
    KDBXVaultReader* reader;
    KDBXFieldRef chunk_ref;
    uint16_t chunk_index;
    bool active;
} FlipPassOpenScratchReaderContext;

typedef struct {
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t nonce_prefix[4];
    bool ready;
} FlipPassOpenPagedWindowCrypto;

typedef struct FlipPassOpenSession {
    App* app;
    FlipPassOpenBuilderContext builder;
    FlipPassOpenScratchContext payload_scratch;
    FlipPassOpenScratchReaderContext payload_reader;
    FlipPassOpenScratchContext xml_scratch;
    KDBXVaultBackend requested_backend;
    bool allow_ext_promotion;
    KDBXOpenProfile open_profile;
    bool open_profile_ready;
    size_t predicted_arena_payload;
    uint8_t* model_stream_workspace;
    FlipPassOpenPagedWindowCrypto* paged_window_crypto;
} FlipPassOpenSession;

/* Budget grew from 512 B to 1024 B in the page-recycling refactor. The two
 * KDBXScratchRef members of payload_scratch / xml_scratch contribute ~136 B
 * each (32 chunk slots * 4 B + 8 B header) — necessary to support staged
 * XML > 64 KB. Below 1024 B keeps the session comfortably under any practical
 * stack/heap pressure. */
_Static_assert(
    sizeof(FlipPassOpenSession) <= 1024U,
    "FlipPassOpenSession exceeded its 1024-byte budget; revisit before growing further.");

static void flippass_open_scratch_reset(FlipPassOpenScratchContext* ctx);
static void flippass_open_scratch_reader_reset(FlipPassOpenScratchReaderContext* reader);
static void flippass_open_builder_cancel_session(void* context);
static void flippass_open_paged_window_crypto_clear(FlipPassOpenSession* session);

static size_t flippass_open_theoretical_session_bytes(void) {
    return sizeof(FlipPassOpenSession);
}

static size_t flippass_open_theoretical_acquire_bytes(void) {
    return flippass_open_theoretical_session_bytes() + sizeof(FlipPassOpenAcquireRequestV1) +
           sizeof(FlipPassOpenAcquireHostApiV1) + sizeof(KDBXOpenProfile);
}

static size_t flippass_open_theoretical_stream_bytes(void) {
    return flippass_open_theoretical_session_bytes() + sizeof(FlipPassOpenStreamRequestV1) +
           sizeof(FlipPassOpenStreamHostApiV1) + sizeof(FlipPassOpenStreamResultV2);
}

static size_t flippass_open_theoretical_inflate_bytes(
    FlipPassOpenInflateKind kind,
    const KDBXGzipMemberInfo* member_info) {
    const size_t member_size = (member_info != NULL) ? member_info->member_size : 0U;
    size_t theoretical =
        flippass_open_theoretical_session_bytes() + sizeof(FlipPassOpenInflateRequestV1) +
        sizeof(FlipPassOpenInflateHostApiV1) + sizeof(FlipPassOpenInflateResultV1);

    if(kind == FlipPassOpenInflateKindNonPaged) {
        theoretical += member_size + FLIPPASS_OPEN_THEORETICAL_GZIP_DICT_BYTES +
                       FLIPPASS_OPEN_THEORETICAL_GZIP_MARGIN_BYTES;
    } else {
        theoretical += FLIPPASS_OPEN_THEORETICAL_PAGED_RAM_BYTES;
    }

    return theoretical;
}

static size_t flippass_open_theoretical_model_bytes(size_t staged_payload_plain_size) {
    UNUSED(staged_payload_plain_size);
    return flippass_open_theoretical_session_bytes() + sizeof(FlipPassOpenModelRequestV1) +
           sizeof(FlipPassOpenModelHostApiV1) + sizeof(FlipPassOpenBuilderApiV1) +
           sizeof(FlipPassOpenBuilderContext) + FLIPPASS_OPEN_MODEL_GROWTH_RESERVE_BYTES;
}

static bool flippass_open_can_use_nonpaged_inflate(const KDBXGzipMemberInfo* member_info) {
    if(member_info == NULL || member_info->member_size == 0U ||
       member_info->expected_output_size == 0U ||
       member_info->member_size > FLIPPASS_OPEN_GZIP_NONPAGED_LIMIT ||
       member_info->expected_output_size > FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT) {
        return false;
    }

    const size_t required_max_free = member_info->member_size +
                                     FLIPPASS_OPEN_THEORETICAL_GZIP_DICT_BYTES +
                                     FLIPPASS_OPEN_THEORETICAL_GZIP_MARGIN_BYTES;
    return memmgr_heap_get_max_free_block() >= required_max_free;
}

static FlipPassOpenInflateKind flippass_open_select_inflate_kind_after_stream(
    FlipPassOpenInflateKind suggested,
    const KDBXGzipMemberInfo* member_info) {
    if(suggested == FlipPassOpenInflateKindStoredCandidate) {
        return FlipPassOpenInflateKindNonPaged;
    }
    return flippass_open_can_use_nonpaged_inflate(member_info) ? FlipPassOpenInflateKindNonPaged :
                                                                 FlipPassOpenInflateKindPaged;
}

#if FLIPPASS_ENABLE_LOGS
static const char* flippass_open_inflate_kind_label(FlipPassOpenInflateKind kind) {
    switch(kind) {
    case FlipPassOpenInflateKindNonPaged:
        return "nonpaged";
    case FlipPassOpenInflateKindPaged:
        return "paged";
    case FlipPassOpenInflateKindStoredCandidate:
        return "stored_candidate";
    case FlipPassOpenInflateKindNone:
    default:
        return "none";
    }
}
#endif

static void flippass_open_trim_runtime_modules(App* app) {
    furi_assert(app);

    /* Mirrors flippass_save_unload_runtime_modules: both helpers wrap
     * flippass_module_unload_all_runtime() and add stage-specific prep work
     * around it. The prep work intentionally diverges (open does not need
     * to deactivate an entry; save does), so the helpers stay separate. Do
     * not attempt to "unify" them again - the unload itself is already
     * symmetric via flippass_module_unload_all_runtime(). */
    FLIPPASS_MEMORY_LOG(app, "open_trim_before", flippass_open_theoretical_session_bytes());
    flippass_output_cleanup(app);
    flippass_module_unload_all_runtime(app);
    FLIPPASS_MEMORY_LOG(app, "open_trim_after", flippass_open_theoretical_session_bytes());
}

static const char* flippass_open_field_log_name(uint32_t field_mask) {
    switch(field_mask) {
    case KDBXEntryFieldUsername:
        return "UserName";
    case KDBXEntryFieldPassword:
        return "Password";
    case KDBXEntryFieldUrl:
        return "URL";
    case KDBXEntryFieldNotes:
        return "Notes";
    case KDBXEntryFieldAutotype:
        return "AutoType";
    case KDBXEntryFieldUuid:
        return "UUID";
    case KDBXEntryFieldTitle:
        return "Title";
    default:
        return "Unknown";
    }
}

static bool flippass_open_model_module_is_loaded(const App* app) {
    return app != NULL && app->module_loader.slot[FlipPassModuleSlotOpenModel].application != NULL;
}

/* Budget only the positive editor-resident delta after the entire pinned
 * coordinator is unmapped. The generator checks the handover geometry separately;
 * its padding is not the only memory returned at that boundary. */
static size_t flippass_open_builder_reserve_bytes(
    const FlipPassOpenSession* session,
    bool file_backed_backend) {
    const App* app = session != NULL ? session->app : NULL;
    if(flippass_open_model_module_is_loaded(app)) {
        const size_t floor = file_backed_backend ? FLIPPASS_OPEN_MODEL_MAPPED_FILE_RESERVE_BYTES :
                                                   FLIPPASS_OPEN_MODEL_MAPPED_SAFETY_RESERVE_BYTES;
        /* A real held allocation has already paid for the record workspace.
         * Credit only its requested bytes, never allocator headers/slack. */
        const size_t prepaid =
            session->model_stream_workspace != NULL ? kdbx_vault_stream_workspace_size() : 0U;
        return (floor > prepaid ? floor - prepaid : 0U) + FLIPPASS_OPEN_SUCCESSOR_RESERVE_BYTES;
    }

    return (file_backed_backend ? FLIPPASS_OPEN_FILE_BACKED_RESERVE_BYTES :
                                  FLIPPASS_OPEN_SAFETY_RESERVE_BYTES) +
           FLIPPASS_OPEN_SUCCESSOR_RESERVE_BYTES;
}

static void flippass_open_builder_log_failure(
    const FlipPassOpenBuilderContext* ctx,
    const char* stage,
    const char* detail,
    size_t request_size) {
#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(stage);
    UNUSED(detail);
    UNUSED(request_size);
#endif
    if(ctx == NULL || ctx->app == NULL) {
        return;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        ctx->app,
        "BUILDER_FAIL stage=%s detail=%s groups=%lu entries=%lu backend=%s request=%lu committed=%lu limit=%lu arena=%s/%lu/%lu vault=%s/%lu/%lu free=%lu max=%lu",
        stage != NULL ? stage : "-",
        detail != NULL ? detail : "-",
        (unsigned long)ctx->group_count,
        (unsigned long)ctx->entry_count,
        kdbx_vault_backend_label(
            ctx->vault != NULL ? kdbx_vault_get_backend(ctx->vault) : KDBXVaultBackendNone),
        (unsigned long)request_size,
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit,
        kdbx_arena_failure_reason(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_size(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_max_free_block(ctx->arena),
        kdbx_vault_failure_reason(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_size(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_max_free_block(ctx->vault),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
}

static void
    flippass_open_builder_log_progress(const FlipPassOpenBuilderContext* ctx, const char* mark) {
#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(mark);
#endif
    if(ctx == NULL || ctx->app == NULL) {
        return;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        ctx->app,
        "BUILDER_PROGRESS mark=%s groups=%lu entries=%lu backend=%s committed=%lu limit=%lu free=%lu max=%lu arena=%s/%lu/%lu vault=%s/%lu/%lu",
        mark != NULL ? mark : "-",
        (unsigned long)ctx->group_count,
        (unsigned long)ctx->entry_count,
        kdbx_vault_backend_label(
            ctx->vault != NULL ? kdbx_vault_get_backend(ctx->vault) : KDBXVaultBackendNone),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        kdbx_arena_failure_reason(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_size(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_max_free_block(ctx->arena),
        kdbx_vault_failure_reason(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_size(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_max_free_block(ctx->vault));
}

static FlipPassOpenSession* flippass_open_session_alloc(App* app) {
    FlipPassOpenSession* session = NULL;

    furi_assert(app);
    session = malloc(sizeof(*session));
    if(session == NULL) {
        return NULL;
    }

    memset(session, 0, sizeof(*session));
    session->app = app;
    session->builder.app = app;
    session->builder.session = session;
    session->payload_scratch.app = app;
    session->xml_scratch.app = app;
    session->predicted_arena_payload = 0U;
    return session;
}

static void flippass_open_model_workspace_clear(FlipPassOpenSession* session) {
    if(session->model_stream_workspace != NULL) {
        memzero(session->model_stream_workspace, kdbx_vault_stream_workspace_size());
        free(session->model_stream_workspace);
        session->model_stream_workspace = NULL;
    }
}

static void flippass_open_session_free(FlipPassOpenSession* session) {
    if(session == NULL) {
        return;
    }

    flippass_open_builder_cancel_session(&session->builder);
    flippass_open_scratch_reset(&session->payload_scratch);
    flippass_open_scratch_reader_reset(&session->payload_reader);
    flippass_open_scratch_reset(&session->xml_scratch);
    flippass_open_paged_window_crypto_clear(session);
    flippass_open_model_workspace_clear(session);
    memzero(session, sizeof(*session));
    free(session);
}

static bool flippass_open_host_should_cancel(void* context) {
    const FlipPassOpenSession* session = context;
    return session != NULL && flippass_progress_guard_canceled(session->app->progress_view);
}

static void flippass_open_host_progress(
    void* context,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    FlipPassOpenSession* session = context;
    if(session != NULL && session->app != NULL) {
        flippass_progress_update(session->app, stage, detail, percent);
    }
}

static void flippass_open_host_log(void* context, const char* message) {
    FlipPassOpenSession* session = context;
    if(session != NULL && session->app != NULL && message != NULL && message[0] != '\0') {
        FLIPPASS_PROBE_LOG_EVENT(session->app, "%s", message);
    }
}

static bool flippass_open_host_read_cached_transformed_key(
    void* context,
    const char* password,
    const char* file_path,
    const uint8_t* kdf_salt,
    size_t kdf_salt_size,
    uint64_t kdf_rounds,
    uint8_t* out_transformed_key,
    size_t out_transformed_key_size) {
    FlipPassOpenSession* session = context;

    if(session == NULL || session->app == NULL || out_transformed_key == NULL ||
       out_transformed_key_size != 32U) {
        return false;
    }

    return flippass_derivation_cache_try_read(
        session->app,
        password,
        file_path,
        kdf_salt,
        kdf_salt_size,
        kdf_rounds,
        out_transformed_key);
}

static KDBXVaultBackend
    flippass_open_select_gzip_scratch_backend(KDBXVaultBackend preferred_backend) {
    if(kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return KDBXVaultBackendFileExt;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendFileInt)) {
        return KDBXVaultBackendFileInt;
    }

    if(preferred_backend != KDBXVaultBackendNone &&
       kdbx_vault_backend_supported(preferred_backend)) {
        return preferred_backend;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendRam)) {
        return KDBXVaultBackendRam;
    }

    return KDBXVaultBackendNone;
}

static const char* flippass_open_scratch_path(bool payload_scratch, KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return payload_scratch ? KDBX_VAULT_MEMBER_INT_PATH : KDBX_VAULT_SCRATCH_INT_PATH;
    case KDBXVaultBackendFileExt:
        return payload_scratch ? KDBX_VAULT_MEMBER_EXT_PATH : KDBX_VAULT_SCRATCH_EXT_PATH;
    default:
        return NULL;
    }
}

static void flippass_open_scratch_reset(FlipPassOpenScratchContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    if(ctx->writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->writer);
    }
    if(ctx->vault != NULL) {
        kdbx_vault_free(ctx->vault);
    }

    App* app = ctx->app;
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
}

static void flippass_open_scratch_reader_reset(FlipPassOpenScratchReaderContext* reader) {
    if(reader != NULL) {
        if(reader->reader != NULL) {
            memzero(reader->reader, sizeof(*reader->reader));
            free(reader->reader);
        }
        memset(reader, 0, sizeof(*reader));
    }
}

static void flippass_open_paged_window_crypto_clear(FlipPassOpenSession* session) {
    if(session == NULL || session->paged_window_crypto == NULL) {
        return;
    }

    memzero(session->paged_window_crypto, sizeof(*session->paged_window_crypto));
    free(session->paged_window_crypto);
    session->paged_window_crypto = NULL;
}

static void flippass_open_write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8U) & 0xFFU);
    out[2] = (uint8_t)((value >> 16U) & 0xFFU);
    out[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static bool flippass_open_paged_window_crypto_ensure(FlipPassOpenSession* session) {
    static const uint8_t enc_label[4] = {'e', 'n', 'c', '1'};
    static const uint8_t mac_label[4] = {'m', 'a', 'c', '1'};
    uint8_t session_master[32];
    uint8_t material[sizeof(session_master) + sizeof(enc_label)];
    bool ok = false;

    if(session == NULL) {
        return false;
    }
    if(session->paged_window_crypto != NULL && session->paged_window_crypto->ready) {
        return true;
    }
    if(session->paged_window_crypto == NULL) {
        session->paged_window_crypto = malloc(sizeof(*session->paged_window_crypto));
        if(session->paged_window_crypto == NULL) {
            return false;
        }
        memset(session->paged_window_crypto, 0, sizeof(*session->paged_window_crypto));
    }

    furi_hal_random_fill_buf(session_master, sizeof(session_master));
    furi_hal_random_fill_buf(
        session->paged_window_crypto->nonce_prefix,
        sizeof(session->paged_window_crypto->nonce_prefix));
    memcpy(material, session_master, sizeof(session_master));
    memcpy(material + sizeof(session_master), enc_label, sizeof(enc_label));
    ok = flippass_crypto_sha256_raw(
        material, sizeof(material), session->paged_window_crypto->enc_key);
    memcpy(material + sizeof(session_master), mac_label, sizeof(mac_label));
    ok = ok && flippass_crypto_sha256_raw(
                   material, sizeof(material), session->paged_window_crypto->mac_key);
    session->paged_window_crypto->ready = ok;

    memzero(session_master, sizeof(session_master));
    memzero(material, sizeof(material));
    if(!ok) {
        flippass_open_paged_window_crypto_clear(session);
    }
    return ok;
}

static bool flippass_open_paged_window_mac(
    FlipPassOpenPagedWindowCrypto* crypto,
    uint16_t page_index,
    const uint8_t* ciphertext,
    size_t page_size,
    uint8_t out_mac[32]) {
    uint8_t page_le[4];

    if(crypto == NULL || ciphertext == NULL || out_mac == NULL) {
        return false;
    }

    flippass_open_write_u32_le(page_le, page_index);
    const FlipPassCryptoChunk chunks[] = {
        {.data = page_le, .size = sizeof(page_le)},
        {.data = ciphertext, .size = page_size},
    };
    return flippass_crypto_hmac_sha256_chunks(
        crypto->mac_key, sizeof(crypto->mac_key), chunks, COUNT_OF(chunks), out_mac);
}

static bool flippass_open_host_crypt_paged_window(
    void* context,
    uint16_t page_index,
    uint8_t* page,
    size_t page_size,
    bool encrypt,
    const uint8_t* expected_mac,
    uint8_t* out_mac,
    size_t mac_size) {
    FlipPassOpenSession* session = context;
    uint8_t nonce[12];
    uint8_t actual_mac[32];
    bool ok = false;

    if(session == NULL || page == NULL || !flippass_open_paged_window_crypto_ensure(session)) {
        return false;
    }

    memcpy(
        nonce,
        session->paged_window_crypto->nonce_prefix,
        sizeof(session->paged_window_crypto->nonce_prefix));
    for(size_t index = 0U; index < 8U; index++) {
        nonce[4U + index] = (uint8_t)(((uint64_t)page_index >> (index * 8U)) & 0xFFU);
    }

    if(encrypt) {
        if(out_mac == NULL || mac_size < sizeof(actual_mac)) {
            goto cleanup;
        }
        ok = flippass_crypto_chacha20_xor(
                 page,
                 page_size,
                 session->paged_window_crypto->enc_key,
                 sizeof(session->paged_window_crypto->enc_key),
                 nonce,
                 sizeof(nonce),
                 0U) &&
             flippass_open_paged_window_mac(
                 session->paged_window_crypto, page_index, page, page_size, out_mac);
    } else {
        if(expected_mac == NULL) {
            goto cleanup;
        }
        if(!flippass_open_paged_window_mac(
               session->paged_window_crypto, page_index, page, page_size, actual_mac) ||
           memcmp(actual_mac, expected_mac, sizeof(actual_mac)) != 0) {
            goto cleanup;
        }
        ok = flippass_crypto_chacha20_xor(
            page,
            page_size,
            session->paged_window_crypto->enc_key,
            sizeof(session->paged_window_crypto->enc_key),
            nonce,
            sizeof(nonce),
            0U);
    }

cleanup:
    memzero(nonce, sizeof(nonce));
    memzero(actual_mac, sizeof(actual_mac));
    return ok;
}

static void flippass_open_host_clear_paged_window_crypto(void* context) {
    flippass_open_paged_window_crypto_clear(context);
}

static bool flippass_open_scratch_begin(
    FlipPassOpenScratchContext* scratch,
    bool payload_scratch,
    KDBXVaultBackend preferred_backend,
    const char* unavailable_message,
    const char* alloc_message,
    const char* storage_message,
    const char* prepare_message,
    FuriString* error) {
    const KDBXVaultBackend backend = flippass_open_select_gzip_scratch_backend(preferred_backend);
    const char* path = flippass_open_scratch_path(payload_scratch, backend);

    furi_assert(scratch);
    flippass_open_scratch_reset(scratch);
    if(backend == KDBXVaultBackendNone) {
        if(error != NULL) {
            furi_string_set_str(error, unavailable_message);
        }
        return false;
    }

    scratch->vault = path == NULL ? kdbx_vault_alloc(backend, NULL, 0U) :
                                    kdbx_vault_alloc_with_path(backend, path, NULL, 0U);
    if(scratch->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, alloc_message);
        }
        return false;
    }
    if(kdbx_vault_storage_failed(scratch->vault)) {
        if(error != NULL) {
            furi_string_set_str(error, storage_message);
        }
        return false;
    }

    kdbx_vault_writer_reset(&scratch->writer, scratch->vault);
    kdbx_vault_writer_set_file_streaming(&scratch->writer, true);
    if(scratch->writer.failed) {
        if(error != NULL) {
            furi_string_set_str(error, prepare_message);
        }
        return false;
    }

    return true;
}

static bool flippass_open_scratch_append(
    FlipPassOpenScratchContext* scratch,
    const uint8_t* data,
    size_t data_size,
    const char* storage_message,
    const char* budget_message,
    FuriString* error) {
    furi_assert(scratch);
    if(scratch->vault == NULL || scratch->writer.vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The staged scratch writer is not ready.");
        }
        return false;
    }

    if(data_size == 0U) {
        return true;
    }

    if(!kdbx_vault_writer_write_scratch(&scratch->writer, data, data_size, &scratch->ref)) {
        if(error != NULL) {
            furi_string_set_str(
                error,
                kdbx_vault_storage_failed(scratch->vault) ? storage_message : budget_message);
        }
        return false;
    }

    return true;
}

static bool flippass_open_scratch_finish(
    FlipPassOpenScratchContext* scratch,
    size_t size,
    const char* finalize_message,
    FuriString* error) {
    furi_assert(scratch);
    if(scratch->vault == NULL || scratch->writer.vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The staged scratch writer is not ready.");
        }
        return false;
    }

    if(!kdbx_vault_writer_finish_scratch(&scratch->writer, &scratch->ref)) {
        if(error != NULL) {
            furi_string_set_str(error, finalize_message);
        }
        return false;
    }

    scratch->size = size;
    return true;
}

static bool flippass_open_host_begin_staged_payload(
    void* context,
    KDBXVaultBackend preferred_backend,
    FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    flippass_open_scratch_reader_reset(&session->payload_reader);
    /* Reuse the empty vault/first locator prepared before Stream mapped. */
    FlipPassOpenScratchContext* scratch = &session->payload_scratch;
    if(scratch->vault != NULL && scratch->writer.vault == NULL && scratch->size == 0U &&
       kdbx_vault_scratch_ref_is_empty(&scratch->ref)) {
        kdbx_vault_writer_reset(&scratch->writer, scratch->vault);
        kdbx_vault_writer_set_file_streaming(&scratch->writer, true);
        if(scratch->writer.failed && error != NULL)
            furi_string_set_str(error, "The staged payload writer could not be prepared.");
        return !scratch->writer.failed;
    }
    return flippass_open_scratch_begin(
        &session->payload_scratch,
        true,
        preferred_backend,
        "No encrypted storage backend is available for staged payload storage.",
        "Not enough RAM is available to prepare staged payload storage.",
        "The staged payload scratch file could not be created on the selected storage.",
        "The staged payload scratch file could not be prepared.",
        error);
}

static bool flippass_open_host_append_staged_payload(
    void* context,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    if(flippass_open_host_should_cancel(session)) return false;
    return flippass_open_scratch_append(
        &session->payload_scratch,
        data,
        data_size,
        "The staged payload scratch file could not be written safely.",
        "Not enough RAM is available to stage the decrypted payload.",
        error);
}

static bool flippass_open_host_finish_staged_payload(
    void* context,
    size_t payload_size,
    FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    return flippass_open_scratch_finish(
        &session->payload_scratch,
        payload_size,
        "The staged payload scratch file could not be finalized safely.",
        error);
}

static void flippass_open_host_clear_staged_payload(void* context) {
    FlipPassOpenSession* session = context;
    if(session != NULL) {
        flippass_open_scratch_reader_reset(&session->payload_reader);
        flippass_open_scratch_reset(&session->payload_scratch);
    }
}

static bool flippass_open_host_begin_staged_payload_stream(void* context, FuriString* error) {
    FlipPassOpenSession* session = context;
    FlipPassOpenScratchContext* scratch = NULL;

    furi_assert(session);
    flippass_open_scratch_reader_reset(&session->payload_reader);
    scratch = &session->payload_scratch;
    if(scratch->vault == NULL || kdbx_vault_scratch_ref_is_empty(&scratch->ref)) {
        if(error != NULL) {
            furi_string_set_str(error, "The staged payload scratch is unavailable.");
        }
        return false;
    }

    session->payload_reader.reader = malloc(sizeof(*session->payload_reader.reader));
    if(session->payload_reader.reader == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to read the staged payload.");
        }
        return false;
    }

    /* Multi-chunk reader: start on chunk 0, advance through scratch->ref.chunks
     * as each KDBXVaultReader hits EOF on its sub-ref. */
    session->payload_reader.chunk_index = 0U;
    session->payload_reader.chunk_ref = scratch->ref.chunks[0];
    kdbx_vault_reader_reset(
        session->payload_reader.reader, scratch->vault, &session->payload_reader.chunk_ref);
    session->payload_reader.active = true;
    return true;
}

static bool flippass_open_host_read_staged_payload_stream(
    void* context,
    uint8_t* out,
    size_t capacity,
    size_t* out_size) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    if(out_size != NULL) {
        *out_size = 0U;
    }
    if(flippass_open_host_should_cancel(session)) return false;
    if(!session->payload_reader.active || session->payload_reader.reader == NULL || out == NULL ||
       out_size == NULL) {
        return false;
    }

    FlipPassOpenScratchContext* scratch = &session->payload_scratch;
    if(scratch->vault == NULL) {
        return false;
    }

    size_t total_filled = 0U;
    while(total_filled < capacity &&
          session->payload_reader.chunk_index < scratch->ref.chunk_count) {
        size_t chunk_filled = 0U;
        const KDBXVaultReader* reader = session->payload_reader.reader;
        const size_t buffered = reader->record_plain_offset <= reader->record_plain_len ?
                                    reader->record_plain_len - reader->record_plain_offset :
                                    0U;
        /* The dictionary owns one open file. Do not retain a second XML-writer
         * descriptor while the payload reader opens a third one. Pausing keeps
         * the pending record and stream mode; only a future flush reopens it. */
        if(capacity - total_filled > buffered && session->xml_scratch.writer.stream_file != NULL &&
           !kdbx_vault_writer_pause_stream(&session->xml_scratch.writer)) {
            return false;
        }
        if(!kdbx_vault_reader_read(
               session->payload_reader.reader,
               out + total_filled,
               capacity - total_filled,
               &chunk_filled)) {
            return false;
        }
        total_filled += chunk_filled;

        if(total_filled < capacity) {
            /* Current chunk is fully drained; advance to the next chunk. */
            session->payload_reader.chunk_index++;
            if(session->payload_reader.chunk_index < scratch->ref.chunk_count) {
                session->payload_reader.chunk_ref =
                    scratch->ref.chunks[session->payload_reader.chunk_index];
                kdbx_vault_reader_reset(
                    session->payload_reader.reader,
                    scratch->vault,
                    &session->payload_reader.chunk_ref);
            }
        }
    }
    *out_size = total_filled;
    return true;
}

static void flippass_open_host_end_staged_payload_stream(void* context) {
    FlipPassOpenSession* session = context;
    if(session != NULL) {
        flippass_open_scratch_reader_reset(&session->payload_reader);
    }
}

static bool flippass_open_host_begin_staged_xml(
    void* context,
    KDBXVaultBackend preferred_backend,
    FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);

    KDBXVaultBackend effective_preferred = preferred_backend;
    const bool payload_in_ram = session->payload_scratch.vault != NULL &&
                                kdbx_vault_get_backend(session->payload_scratch.vault) ==
                                    KDBXVaultBackendRam;
    if(payload_in_ram && effective_preferred == KDBXVaultBackendRam) {
        if(kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
            effective_preferred = KDBXVaultBackendFileExt;
        } else if(kdbx_vault_backend_supported(KDBXVaultBackendFileInt)) {
            effective_preferred = KDBXVaultBackendFileInt;
        }
    }

    return flippass_open_scratch_begin(
        &session->xml_scratch,
        false,
        effective_preferred,
        "No encrypted storage backend is available for staged XML storage.",
        "Not enough RAM is available to prepare staged XML storage.",
        "The staged XML scratch file could not be created on the selected storage.",
        "The staged XML scratch file could not be prepared.",
        error);
}

static bool flippass_open_host_append_staged_xml(
    void* context,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    if(flippass_open_host_should_cancel(session)) return false;
    return flippass_open_scratch_append(
        &session->xml_scratch,
        data,
        data_size,
        "The staged XML scratch file could not be written safely.",
        "Not enough RAM is available to stage the XML payload.",
        error);
}

static bool
    flippass_open_host_finish_staged_xml(void* context, size_t plain_size, FuriString* error) {
    FlipPassOpenSession* session = context;

    furi_assert(session);
    return flippass_open_scratch_finish(
        &session->xml_scratch,
        plain_size,
        "The staged XML scratch file could not be finalized safely.",
        error);
}

typedef struct {
    FlipPassOpenSession* session;
    FlipPassOpenChunkCallback callback;
    void* callback_context;
} FlipPassOpenXmlStreamState;

static bool flippass_open_host_stream_staged_xml_callback(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    FlipPassOpenXmlStreamState* stream = context;

    furi_assert(stream);
    if(flippass_open_host_should_cancel(stream->session)) return false;
    return stream->callback != NULL && stream->callback(data, data_size, stream->callback_context);
}

/* Mid-stream RAM->/ext promotion threshold for xml_scratch. Below this RAM
 * residency, page recycling is enough; above it, partial promotion runs to
 * release the bulk of the xml_scratch immediately. */
#define FLIPPASS_OPEN_XML_PROMOTE_RAM_THRESHOLD (8U * 1024U)

static bool flippass_open_xml_should_promote_first(const FlipPassOpenSession* session) {
    if(session == NULL || session->xml_scratch.vault == NULL ||
       kdbx_vault_get_backend(session->xml_scratch.vault) != KDBXVaultBackendRam) {
        return false;
    }
    if(!session->allow_ext_promotion || !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return false;
    }
    if(kdbx_vault_page_bytes(session->xml_scratch.vault) <
       FLIPPASS_OPEN_XML_PROMOTE_RAM_THRESHOLD) {
        return false;
    }
    /* Only promote if the builder is signalling pressure. The builder's
     * commit_limit shrinks below committed_bytes when free heap runs out;
     * use that as the trigger. We compute a conservative "needs promotion"
     * check by inspecting the builder's residual budget. */
    const FlipPassOpenBuilderContext* builder = &session->builder;
    if(builder->vault == NULL || kdbx_vault_get_backend(builder->vault) != KDBXVaultBackendRam) {
        /* Builder already on /ext or not yet active — recycling alone is fine. */
        return false;
    }
    const size_t remaining = (builder->commit_limit > builder->committed_bytes) ?
                                 (builder->commit_limit - builder->committed_bytes) :
                                 0U;
    if(remaining > (16U * 1024U)) {
        return false;
    }
    return memmgr_heap_get_max_free_block() <
           (FLIPPASS_OPEN_XML_PROMOTE_RAM_THRESHOLD + KDBX_VAULT_RECORD_PLAIN_MAX);
}

static bool flippass_open_xml_promote_partial_to_ext(
    FlipPassOpenSession* session,
    uint32_t next_record_id) {
    KDBXVault* target = NULL;

    furi_assert(session);
    if(session->xml_scratch.vault == NULL) {
        return false;
    }

    target = kdbx_vault_alloc(KDBXVaultBackendFileExt, NULL, 0U);
    if(target == NULL) {
        return false;
    }
    if(kdbx_vault_storage_failed(target)) {
        kdbx_vault_free(target);
        return false;
    }

    if(!kdbx_vault_promote_partial_ram_to_file(
           session->xml_scratch.vault, target, next_record_id)) {
        kdbx_vault_free(target);
        return false;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        session->app,
        "XML_SCRATCH_PROMOTE_OK record=%lu free=%lu max=%lu",
        (unsigned long)next_record_id,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    kdbx_vault_free(session->xml_scratch.vault);
    session->xml_scratch.vault = target;
    return true;
}

static bool flippass_open_xml_stream_checkpoint(void* ctx, uint32_t next_record_id) {
    FlipPassOpenXmlStreamState* state = ctx;
    if(state == NULL || state->session == NULL) {
        return true;
    }

    if(!flippass_open_xml_should_promote_first(state->session)) {
        return true;
    }

    return flippass_open_xml_promote_partial_to_ext(state->session, next_record_id);
}

static void flippass_open_xml_stream_on_pages_released(
    void* ctx,
    uint32_t consumed_up_to,
    size_t freed_bytes) {
    FlipPassOpenXmlStreamState* state = ctx;
    if(state == NULL || state->session == NULL) {
        return;
    }
#if FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
    FLIPPASS_LOG_EVENT(
        state->session->app,
        "XML_PAGE_RECYCLED record=%lu freed=%lu xml_pages=%lu free=%lu max=%lu",
        (unsigned long)consumed_up_to,
        (unsigned long)freed_bytes,
        (unsigned long)kdbx_vault_page_bytes(state->session->xml_scratch.vault),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
#else
    UNUSED(consumed_up_to);
    UNUSED(freed_bytes);
#endif
}

static bool flippass_open_host_stream_staged_xml(
    void* context,
    FlipPassOpenChunkCallback callback,
    void* callback_context,
    FuriString* error) {
    FlipPassOpenSession* session = context;
    FlipPassOpenScratchContext* scratch = NULL;
    bool ok = false;

    furi_assert(session);
    scratch = &session->xml_scratch;
    if(scratch->vault == NULL || kdbx_vault_scratch_ref_is_empty(&scratch->ref) ||
       callback == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The staged XML scratch is unavailable.");
        }
        return false;
    }

    FlipPassOpenXmlStreamState stream = {
        .session = session,
        .callback = callback,
        .callback_context = callback_context,
    };

    /* Total record count across all chunks for the start log. */
    uint32_t total_records = 0U;
    for(uint16_t ci = 0U; ci < scratch->ref.chunk_count; ci++) {
        const uint16_t plain_len = scratch->ref.chunks[ci].plain_len;
        if(plain_len == 0U) continue;
        total_records +=
            (plain_len + KDBX_VAULT_RECORD_PLAIN_MAX - 1U) / KDBX_VAULT_RECORD_PLAIN_MAX;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        session->app,
        "STAGED_XML_STREAM_START backend=%s bytes=%lu records=%lu chunks=%u free=%lu max=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(scratch->vault)),
        (unsigned long)scratch->size,
        (unsigned long)total_records,
        (unsigned)scratch->ref.chunk_count,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    /* Page recycling + partial promotion checkpoint. The checkpoint may swap
     * session->xml_scratch.vault from Ram backend to FileExt mid-stream; the
     * stream options vault_swap_ptr makes the streamer pick up the new vault
     * on the next record read. */
    KDBXVaultStreamOptions options = {
        .release_consumed_pages = true,
        .checkpoint = flippass_open_xml_stream_checkpoint,
        .checkpoint_context = &stream,
        .vault_swap_ptr = &scratch->vault,
        .on_pages_released = flippass_open_xml_stream_on_pages_released,
        .on_pages_released_context = &stream,
        .workspace = session->model_stream_workspace,
        .workspace_size =
            session->model_stream_workspace != NULL ? kdbx_vault_stream_workspace_size() : 0U,
    };

    ok = kdbx_vault_stream_scratch_with_options(
        scratch->vault,
        &scratch->ref,
        flippass_open_host_stream_staged_xml_callback,
        &stream,
        &options);
    if(!ok) {
        FLIPPASS_PROBE_LOG_EVENT(
            session->app,
            "STAGED_XML_STREAM_FAIL reader=%s record=%lu storage=%s budget=%s size=%lu max=%lu",
            kdbx_vault_last_reader_failure(scratch->vault),
            (unsigned long)kdbx_vault_last_reader_failure_record(scratch->vault),
            kdbx_vault_storage_stage(scratch->vault),
            kdbx_vault_failure_reason(scratch->vault),
            (unsigned long)kdbx_vault_last_failed_size(scratch->vault),
            (unsigned long)kdbx_vault_last_failed_max_free_block(scratch->vault));
    } else {
        FLIPPASS_PROBE_LOG_EVENT(session->app, "STAGED_XML_STREAM_DONE");
    }

    return ok;
}

static void flippass_open_host_clear_staged_xml(void* context) {
    FlipPassOpenSession* session = context;
    if(session != NULL) {
        flippass_open_scratch_reset(&session->xml_scratch);
    }
}

static bool flippass_open_host_derive_protected_stream_material(
    void* context,
    uint32_t algorithm,
    const uint8_t* key,
    size_t key_size,
    uint8_t* material,
    size_t material_capacity,
    size_t* material_size,
    FuriString* error) {
    UNUSED(context);

    if(key == NULL || material == NULL || material_size == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The KDBX protected stream key is invalid.");
        }
        return false;
    }

    *material_size = 0U;
    if(algorithm == 3U) {
        uint8_t hash[64];
        if(material_capacity < 44U || !flippass_crypto_sha512_raw(key, key_size, hash)) {
            memzero(hash, sizeof(hash));
            if(error != NULL) {
                furi_string_set_str(error, "Unable to derive the ChaCha20 protected stream.");
            }
            return false;
        }
        memcpy(material, hash, 44U);
        *material_size = 44U;
        memzero(hash, sizeof(hash));
        return true;
    }

    if(algorithm == 2U) {
        if(material_capacity < 32U || !flippass_crypto_sha256_raw(key, key_size, material)) {
            if(error != NULL) {
                furi_string_set_str(error, "Unable to derive the Salsa20 protected stream.");
            }
            return false;
        }
        *material_size = 32U;
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Only Salsa20 or ChaCha20 protected values are supported.");
    }
    return false;
}

static void flippass_open_builder_refresh_budget(FlipPassOpenBuilderContext* ctx) {
    const bool file_backed_backend = ctx != NULL && ctx->vault != NULL &&
                                     kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam;
    const size_t reserve = flippass_open_builder_reserve_bytes(
        ctx != NULL ? ctx->session : NULL, file_backed_backend);
    const size_t free_heap = memmgr_get_free_heap();

    furi_assert(ctx);

    ctx->committed_bytes = kdbx_arena_bytes(ctx->arena);
    if(ctx->vault != NULL) {
        ctx->committed_bytes += kdbx_vault_index_bytes(ctx->vault);
        ctx->committed_bytes += kdbx_vault_page_bytes(ctx->vault);
    }

    ctx->commit_limit = (free_heap <= reserve) ? ctx->committed_bytes :
                                                 (ctx->committed_bytes + free_heap - reserve);
    kdbx_arena_set_budget(ctx->arena, &ctx->committed_bytes, ctx->commit_limit);
    if(ctx->vault != NULL) {
        kdbx_vault_set_budget(ctx->vault, &ctx->committed_bytes, ctx->commit_limit);
    }
}

static bool flippass_open_builder_offer_fallback(
    FlipPassOpenBuilderContext* ctx,
    FuriString* error,
    const char* stage,
    size_t request_size) {
#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(stage);
    UNUSED(request_size);
#endif
    DialogsApp* dialogs = NULL;
    DialogMessage* message = NULL;
    DialogMessageButton button = DialogMessageButtonBack;

    if(ctx == NULL || ctx->app == NULL) {
        return false;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        ctx->app,
        "VAULT_FALLBACK_OFFER stage=%s remaining=%lu max=%lu request=%lu",
        stage != NULL ? stage : "-",
        (unsigned long)((ctx->commit_limit > ctx->committed_bytes) ?
                            (ctx->commit_limit - ctx->committed_bytes) :
                            0U),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)request_size);

    if(ctx->app->rpc_mode) {
        ctx->app->pending_vault_fallback = true;
        if(error != NULL) {
            furi_string_set_str(
                error,
                "The encrypted RAM vault needs /ext to finish this database. Retry unlock with backend 'ext'.");
        }
        return false;
    }

    ctx->app->pending_vault_fallback = false;

    if(ctx->app->debug_auto_continue_vault_fallback) {
        FLIPPASS_PROBE_LOG_EVENT(ctx->app, "DEBUG_AUTO_CONTINUE_EXT");
        button = DialogMessageButtonRight;
    } else {
        dialogs = furi_record_open(RECORD_DIALOGS);
        message = dialog_message_alloc();
        if(message == NULL) {
            furi_record_close(RECORD_DIALOGS);
            if(error != NULL) {
                furi_string_set_str(
                    error,
                    "Not enough RAM is available to ask for the encrypted /ext session file.");
            }
            return false;
        }
        dialog_message_set_header(message, "Need /ext Session", 64, 8, AlignCenter, AlignCenter);
        dialog_message_set_text(
            message,
            "FlipPass needs an encrypted\n/ext session file to finish\nopening this database.",
            64,
            30,
            AlignCenter,
            AlignCenter);
        dialog_message_set_buttons(message, "Cancel", NULL, "Continue");
        flippass_progress_guard_suspend(ctx->app->progress_view, true);
        button = dialog_message_show(dialogs, message);
        flippass_progress_guard_suspend(ctx->app->progress_view, false);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
    }

    if(button != DialogMessageButtonRight) {
        FLIPPASS_PROBE_LOG_EVENT(ctx->app, "VAULT_FALLBACK_REPLY choice=cancel");
        if(error != NULL) {
            furi_string_set_str(error, "Unlock canceled.");
        }
        return false;
    }

    FLIPPASS_PROBE_LOG_EVENT(ctx->app, "VAULT_FALLBACK_REPLY choice=continue");
    ctx->allow_ext_promotion = true;
    ctx->app->allow_ext_vault_promotion = true;
    return true;
}

/* Select before the pre-scan and open_model load: a RAM-sized arena retains
 * space for resident UUIDs even if its vault later moves to /ext. Asking only
 * under model pressure also leaves less heap for the consent dialog itself. */
static bool flippass_open_prepare_model_backend(
    FlipPassOpenSession* session,
    KDBXVaultBackend* model_backend,
    FuriString* error) {
    *model_backend = session->requested_backend;
    if(*model_backend != KDBXVaultBackendRam ||
       session->xml_scratch.size < FLIPPASS_OPEN_PREDICTIVE_EXT_XML_BYTES ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return true;
    }

    if(!session->allow_ext_promotion) {
        if(!flippass_open_builder_offer_fallback(
               &session->builder, error, "model_preflight", session->xml_scratch.size)) {
            return false;
        }
        session->allow_ext_promotion = true;
    }

    *model_backend = KDBXVaultBackendFileExt;
    FLIPPASS_PROBE_LOG_EVENT(
        session->app,
        "MODEL_BACKEND_SELECT requested=%s selected=%s xml=%lu",
        kdbx_vault_backend_label(session->requested_backend),
        kdbx_vault_backend_label(*model_backend),
        (unsigned long)session->xml_scratch.size);
    return true;
}

static bool flippass_open_builder_should_promote(
    const FlipPassOpenBuilderContext* ctx,
    size_t next_plain_len) {
    if(ctx == NULL || ctx->vault == NULL ||
       kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return false;
    }

    const size_t remaining = (ctx->commit_limit > ctx->committed_bytes) ?
                                 (ctx->commit_limit - ctx->committed_bytes) :
                                 0U;
    const size_t guard = KDBX_VAULT_RECORD_PLAIN_MAX;
    const size_t required = (next_plain_len <= (SIZE_MAX - guard)) ? (next_plain_len + guard) :
                                                                     SIZE_MAX;
    if(remaining < required) {
        return true;
    }

    return next_plain_len >= KDBX_VAULT_RECORD_PLAIN_MAX &&
           memmgr_heap_get_max_free_block() < required;
}

static bool
    flippass_open_builder_promote_to_ext(FlipPassOpenBuilderContext* ctx, FuriString* error) {
    KDBXVault* target = NULL;

    furi_assert(ctx);

    if(ctx->vault == NULL || kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam ||
       !ctx->allow_ext_promotion || !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return false;
    }

    ctx->vault_promotion_attempted = true;
    FLIPPASS_PROBE_LOG_EVENT(
        ctx->app,
        "VAULT_PROMOTE_START committed=%lu limit=%lu free=%lu max=%lu",
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    flippass_progress_update(ctx->app, "Continuing on /ext", "", ctx->app->progress_percent);
    target = kdbx_vault_alloc(KDBXVaultBackendFileExt, NULL, 0U);
    if(target == NULL || kdbx_vault_storage_failed(target)) {
        if(target != NULL) {
            kdbx_vault_free(target);
        }
        if(error != NULL) {
            furi_string_set_str(
                error,
                "The RAM vault filled up and FlipPass could not continue on the encrypted SD-card session file.");
        }
        return false;
    }

    if(!kdbx_vault_promote_ram_to_file(ctx->vault, target)) {
        kdbx_vault_free(target);
        if(error != NULL) {
            furi_string_set_str(
                error,
                "The RAM vault filled up and FlipPass could not continue on the encrypted SD-card session file.");
        }
        return false;
    }

    kdbx_vault_free(ctx->vault);
    ctx->vault = target;
    FLIPPASS_PROBE_LOG_EVENT(
        ctx->app,
        "VAULT_PROMOTE_OK free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    flippass_open_builder_refresh_budget(ctx);
    return true;
}

static bool
    flippass_open_builder_field_writer_uses_file_stream(const FlipPassOpenBuilderContext* ctx) {
    return ctx != NULL && ctx->vault != NULL &&
           kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam;
}

static void flippass_open_builder_note_flash_write(const FlipPassOpenBuilderContext* ctx) {
    const char* detail = NULL;

    if(ctx == NULL || ctx->app == NULL || ctx->vault == NULL) {
        return;
    }

    switch(kdbx_vault_get_backend(ctx->vault)) {
    case KDBXVaultBackendFileExt:
        detail = "Storing encrypted /ext session";
        break;
    case KDBXVaultBackendFileInt:
        detail = "Storing encrypted session";
        break;
    default:
        return;
    }

    flippass_progress_update(ctx->app, "Modeling", detail, ctx->app->progress_percent);
}

static size_t flippass_open_builder_field_writer_capacity(
    const FlipPassOpenBuilderContext* ctx,
    size_t value_len) {
    if(!flippass_open_builder_field_writer_uses_file_stream(ctx) ||
       value_len > KDBX_VAULT_RECORD_PLAIN_MAX) {
        return KDBX_VAULT_RECORD_PLAIN_MAX;
    }
    if(value_len < FLIPPASS_OPEN_FIELD_WRITER_MIN_PENDING_BYTES) {
        return FLIPPASS_OPEN_FIELD_WRITER_MIN_PENDING_BYTES;
    }
    return value_len;
}

static bool flippass_open_builder_has_model_heap_room(void) {
#if FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES > 0
    return memmgr_get_free_heap() >= FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES &&
           memmgr_heap_get_max_free_block() >= FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES;
#else
    return true;
#endif
}

static bool flippass_open_builder_prepare_field_writer(
    FlipPassOpenBuilderContext* ctx,
    size_t value_len,
    FuriString* error) {
    const bool keep_stream = flippass_open_builder_field_writer_uses_file_stream(ctx);
    const size_t pending_capacity = flippass_open_builder_field_writer_capacity(ctx, value_len);

    furi_assert(ctx);

    if(ctx->field_writer.pending != NULL && ctx->field_writer.vault == ctx->vault &&
       !ctx->field_writer.failed && ctx->field_writer.pending_capacity >= pending_capacity &&
       (!keep_stream || ctx->field_writer.stream_file_mode)) {
        return true;
    }

    /* A RAM vault also maps Crypto from SD when a new writer starts. Check
     * before resetting/opening it; an existing writer needs no new workspace. */
    if(!flippass_open_builder_has_model_heap_room()) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    if(ctx->field_writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->field_writer);
    }

    kdbx_vault_writer_reset_with_pending(&ctx->field_writer, ctx->vault, NULL, pending_capacity);
    if(ctx->field_writer.failed) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    if(keep_stream) {
        kdbx_vault_writer_set_file_streaming(&ctx->field_writer, true);
    }

    return true;
}

static bool flippass_open_builder_finish_field_writer(
    FlipPassOpenBuilderContext* ctx,
    KDBXFieldRef* out_ref) {
    furi_assert(ctx);
    furi_assert(out_ref);

    if(flippass_open_builder_field_writer_uses_file_stream(ctx)) {
        return kdbx_vault_writer_finish_keep_stream(&ctx->field_writer, out_ref);
    }

    return kdbx_vault_writer_finish(&ctx->field_writer, out_ref);
}

static void flippass_open_builder_close_field_writer(FlipPassOpenBuilderContext* ctx) {
    if(ctx == NULL || ctx->field_writer.pending == NULL) {
        return;
    }

    kdbx_vault_writer_close(&ctx->field_writer);
}

static bool flippass_open_builder_has_safe_heap_floor(
    FlipPassOpenBuilderContext* ctx,
    FuriString* error,
    const char* stage,
    size_t request_size) {
    furi_assert(ctx);

#if FLIPPASS_OPEN_MODEL_LOW_HEAP_ABORT_BYTES > 0
    /* Short RAM-vault values can also map Crypto from SD. Apply the same floor
     * before those writes, not only before streamed/file-backed values. */
    if(flippass_open_builder_has_model_heap_room()) {
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
    }
    flippass_open_builder_log_failure(ctx, stage, "low_heap_guard", request_size);
    return false;
#else
    UNUSED(error);
    UNUSED(stage);
    UNUSED(request_size);
    return true;
#endif
}

typedef enum {
    FlipPassOpenModelArenaEstimateTextNone = 0,
    FlipPassOpenModelArenaEstimateTextGroupName,
    FlipPassOpenModelArenaEstimateTextGroupUuid,
    FlipPassOpenModelArenaEstimateTextEntryUuid,
    FlipPassOpenModelArenaEstimateTextStringKey,
    FlipPassOpenModelArenaEstimateTextStringValue,
} FlipPassOpenModelArenaEstimateTextState;

typedef struct {
    XmlParser* parser;
    KDBXVaultBackend model_backend;
    size_t predicted_payload_used;
    uint32_t group_depth;
    bool in_entry;
    bool in_string;
    bool overflow;
    bool xml_started;
    uint8_t inner_header_prefix[5];
    size_t inner_header_prefix_len;
    uint8_t inner_field_id;
    uint32_t inner_field_size;
    size_t inner_field_remaining;
    int history_depth;
    char current_key[64];
    size_t current_key_len;
    size_t current_text_len;
    FlipPassOpenModelArenaEstimateTextState text_state;
} FlipPassOpenModelArenaEstimateContext;

static bool flippass_open_model_arena_estimate_uuid_is_resident(
    const FlipPassOpenModelArenaEstimateContext* ctx) {
    return ctx != NULL && ctx->model_backend == KDBXVaultBackendRam;
}

static size_t flippass_open_model_arena_align_up(size_t value, size_t alignment) {
    const size_t mask = alignment - 1U;
    return (value + mask) & ~mask;
}

static void flippass_open_model_arena_estimate_add_alloc(
    FlipPassOpenModelArenaEstimateContext* ctx,
    size_t size) {
    if(ctx == NULL || ctx->overflow || size == 0U) {
        return;
    }

    const size_t alignment = sizeof(void*);
    const size_t mask = alignment - 1U;
    if(ctx->predicted_payload_used > (SIZE_MAX - mask)) {
        ctx->overflow = true;
        return;
    }

    const size_t offset =
        flippass_open_model_arena_align_up(ctx->predicted_payload_used, alignment);
    if(size > (SIZE_MAX - offset)) {
        ctx->overflow = true;
        return;
    }

    ctx->predicted_payload_used = offset + size;
}

static void flippass_open_model_arena_estimate_add_string(
    FlipPassOpenModelArenaEstimateContext* ctx,
    size_t value_len) {
    if(value_len >= SIZE_MAX) {
        if(ctx != NULL) {
            ctx->overflow = true;
        }
        return;
    }

    flippass_open_model_arena_estimate_add_alloc(ctx, value_len + 1U);
}

static bool flippass_open_model_arena_key_is_standard_string_field(const char* key) {
    return key != NULL && (strcmp(key, "Title") == 0 || strcmp(key, "UserName") == 0 ||
                           strcmp(key, "Password") == 0 || strcmp(key, "URL") == 0 ||
                           strcmp(key, "Notes") == 0 || strcmp(key, "UUID") == 0);
}

static void
    flippass_open_model_arena_estimate_reset_string(FlipPassOpenModelArenaEstimateContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    ctx->current_key[0] = '\0';
    ctx->current_key_len = 0U;
    ctx->current_text_len = 0U;
}

static void flippass_open_model_arena_estimate_begin_text(
    FlipPassOpenModelArenaEstimateContext* ctx,
    FlipPassOpenModelArenaEstimateTextState text_state) {
    if(ctx == NULL) {
        return;
    }

    ctx->text_state = text_state;
    ctx->current_text_len = 0U;
    if(text_state == FlipPassOpenModelArenaEstimateTextStringKey) {
        ctx->current_key[0] = '\0';
        ctx->current_key_len = 0U;
    }
}

static void flippass_open_model_arena_estimate_commit_string_value(
    FlipPassOpenModelArenaEstimateContext* ctx) {
    if(ctx == NULL || ctx->overflow) {
        return;
    }

    if(strcmp(ctx->current_key, "UUID") == 0) {
        if(flippass_open_model_arena_estimate_uuid_is_resident(ctx)) {
            flippass_open_model_arena_estimate_add_string(ctx, ctx->current_text_len);
        }
    } else if(
        ctx->current_key_len > 0U &&
        !flippass_open_model_arena_key_is_standard_string_field(ctx->current_key) &&
        ctx->current_text_len > 0U) {
        flippass_open_model_arena_estimate_add_alloc(ctx, sizeof(KDBXCustomField));
        flippass_open_model_arena_estimate_add_string(ctx, ctx->current_key_len);
    }
}

static bool flippass_open_model_arena_estimate_feed_xml(
    FlipPassOpenModelArenaEstimateContext* ctx,
    const uint8_t* data,
    size_t data_size) {
    if(ctx == NULL || ctx->parser == NULL || ctx->overflow) {
        return false;
    }

    if(data == NULL && data_size > 0U) {
        return false;
    }

    return xml_parser_feed(ctx->parser, (const char*)data, data_size, false);
}

static bool flippass_open_model_arena_estimate_feed_payload(
    FlipPassOpenModelArenaEstimateContext* ctx,
    const uint8_t* data,
    size_t data_size) {
    size_t consumed = 0U;

    if(ctx == NULL || ctx->overflow || (data == NULL && data_size > 0U)) {
        return false;
    }

    while(consumed < data_size) {
        if(!ctx->xml_started) {
            if(ctx->inner_header_prefix_len == 0U && ctx->inner_field_remaining == 0U &&
               ctx->inner_field_id == 0U && (data[consumed] == '<' || data[consumed] == 0xEFU)) {
                ctx->xml_started = true;
                continue;
            }

            if(ctx->inner_field_remaining == 0U) {
                while(consumed < data_size &&
                      ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
                    ctx->inner_header_prefix[ctx->inner_header_prefix_len++] = data[consumed++];
                }
                if(ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
                    return true;
                }

                ctx->inner_field_id = ctx->inner_header_prefix[0];
                ctx->inner_field_size = ((uint32_t)ctx->inner_header_prefix[1]) |
                                        ((uint32_t)ctx->inner_header_prefix[2] << 8) |
                                        ((uint32_t)ctx->inner_header_prefix[3] << 16) |
                                        ((uint32_t)ctx->inner_header_prefix[4] << 24);
                ctx->inner_field_remaining = ctx->inner_field_size;
                ctx->inner_header_prefix_len = 0U;
            }

            if(ctx->inner_field_remaining > 0U) {
                const size_t available = data_size - consumed;
                const size_t take = (available < ctx->inner_field_remaining) ?
                                        available :
                                        ctx->inner_field_remaining;
                consumed += take;
                ctx->inner_field_remaining -= take;
                if(ctx->inner_field_remaining != 0U) {
                    return true;
                }
            }

            if(ctx->inner_field_id == 0U) {
                ctx->xml_started = true;
            }
            ctx->inner_field_id = 0U;
            ctx->inner_field_size = 0U;
            continue;
        }

        return flippass_open_model_arena_estimate_feed_xml(
            ctx, data + consumed, data_size - consumed);
    }

    return true;
}

static size_t
    flippass_open_model_initial_arena_payload_limit(size_t commit_limit, size_t max_free_block) {
    const size_t overhead = kdbx_arena_chunk_overhead_bytes();
    size_t limit = SIZE_MAX;

    if(commit_limit > overhead) {
        limit = commit_limit - overhead;
    }
    if(max_free_block > (overhead + FLIPPASS_OPEN_MODEL_PRESCAN_MAX_FREE_GUARD_BYTES)) {
        const size_t max_free_payload =
            max_free_block - overhead - FLIPPASS_OPEN_MODEL_PRESCAN_MAX_FREE_GUARD_BYTES;
        if(max_free_payload < limit) {
            limit = max_free_payload;
        }
    }

    return limit;
}

static void flippass_open_model_arena_estimate_start_element(
    void* context,
    const char* name,
    const char** attributes) {
    FlipPassOpenModelArenaEstimateContext* ctx = context;
    (void)attributes;

    if(ctx == NULL || name == NULL) {
        return;
    }

    if(ctx->history_depth > 0) {
        if(strcmp(name, "History") == 0) {
            ctx->history_depth++;
        }
        return;
    }
    if(strcmp(name, "History") == 0 && ctx->in_entry) {
        ctx->history_depth = 1;
        return;
    }

    if(strcmp(name, "Group") == 0) {
        flippass_open_model_arena_estimate_add_alloc(ctx, sizeof(KDBXGroup));
        ctx->group_depth++;
    } else if(strcmp(name, "Entry") == 0) {
        flippass_open_model_arena_estimate_add_alloc(ctx, sizeof(KDBXEntry));
        ctx->in_entry = true;
    } else if(strcmp(name, "Name") == 0 && ctx->group_depth > 0U && !ctx->in_entry) {
        flippass_open_model_arena_estimate_begin_text(
            ctx, FlipPassOpenModelArenaEstimateTextGroupName);
    } else if(strcmp(name, "UUID") == 0 && ctx->group_depth > 0U && !ctx->in_entry) {
        flippass_open_model_arena_estimate_begin_text(
            ctx, FlipPassOpenModelArenaEstimateTextGroupUuid);
    } else if(strcmp(name, "UUID") == 0 && ctx->in_entry) {
        flippass_open_model_arena_estimate_begin_text(
            ctx, FlipPassOpenModelArenaEstimateTextEntryUuid);
    } else if(strcmp(name, "String") == 0 && ctx->in_entry) {
        ctx->in_string = true;
        flippass_open_model_arena_estimate_reset_string(ctx);
    } else if(strcmp(name, "Key") == 0 && ctx->in_string) {
        flippass_open_model_arena_estimate_begin_text(
            ctx, FlipPassOpenModelArenaEstimateTextStringKey);
    } else if(strcmp(name, "Value") == 0 && ctx->in_string) {
        flippass_open_model_arena_estimate_begin_text(
            ctx, FlipPassOpenModelArenaEstimateTextStringValue);
    }
}

static void flippass_open_model_arena_estimate_end_element(void* context, const char* name) {
    FlipPassOpenModelArenaEstimateContext* ctx = context;
    if(ctx == NULL || name == NULL) {
        return;
    }

    if(ctx->history_depth > 0) {
        if(strcmp(name, "History") == 0) {
            ctx->history_depth--;
        }
        return;
    }

    if(strcmp(name, "Key") == 0 &&
       ctx->text_state == FlipPassOpenModelArenaEstimateTextStringKey) {
        ctx->text_state = FlipPassOpenModelArenaEstimateTextNone;
        ctx->current_text_len = 0U;
        return;
    }

    if(strcmp(name, "Value") == 0 &&
       ctx->text_state == FlipPassOpenModelArenaEstimateTextStringValue) {
        flippass_open_model_arena_estimate_commit_string_value(ctx);
        ctx->text_state = FlipPassOpenModelArenaEstimateTextNone;
        ctx->current_text_len = 0U;
        return;
    }

    if(strcmp(name, "Name") == 0 &&
       ctx->text_state == FlipPassOpenModelArenaEstimateTextGroupName) {
        flippass_open_model_arena_estimate_add_string(ctx, ctx->current_text_len);
        ctx->text_state = FlipPassOpenModelArenaEstimateTextNone;
        ctx->current_text_len = 0U;
        return;
    }

    if(strcmp(name, "UUID") == 0 &&
       (ctx->text_state == FlipPassOpenModelArenaEstimateTextGroupUuid ||
        ctx->text_state == FlipPassOpenModelArenaEstimateTextEntryUuid)) {
        if(flippass_open_model_arena_estimate_uuid_is_resident(ctx)) {
            flippass_open_model_arena_estimate_add_string(ctx, ctx->current_text_len);
        }
        ctx->text_state = FlipPassOpenModelArenaEstimateTextNone;
        ctx->current_text_len = 0U;
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(ctx->group_depth > 0U) {
            ctx->group_depth--;
        }
    } else if(strcmp(name, "Entry") == 0) {
        ctx->in_entry = false;
    } else if(strcmp(name, "String") == 0) {
        ctx->in_string = false;
        flippass_open_model_arena_estimate_reset_string(ctx);
    }
}

static void
    flippass_open_model_arena_estimate_character_data(void* context, const char* data, int len) {
    FlipPassOpenModelArenaEstimateContext* ctx = context;
    if(ctx == NULL || data == NULL || len <= 0) {
        return;
    }

    if(ctx->history_depth > 0) {
        return;
    }

    if(ctx->text_state == FlipPassOpenModelArenaEstimateTextStringKey) {
        const size_t max_key = sizeof(ctx->current_key) - 1U;
        const size_t stored_len = ctx->current_key_len < max_key ? ctx->current_key_len : max_key;
        const size_t available = max_key - stored_len;
        if(available > 0U) {
            const size_t copy_len = (size_t)len <= available ? (size_t)len : available;
            memcpy(&ctx->current_key[stored_len], data, copy_len);
            ctx->current_key[stored_len + copy_len] = '\0';
        }
        if((size_t)len > (SIZE_MAX - ctx->current_key_len) ||
           (size_t)len > (SIZE_MAX - ctx->current_text_len)) {
            ctx->overflow = true;
            return;
        }
        ctx->current_key_len += (size_t)len;
        ctx->current_text_len += (size_t)len;
        return;
    }

    if(ctx->text_state != FlipPassOpenModelArenaEstimateTextNone) {
        if((size_t)len > (SIZE_MAX - ctx->current_text_len)) {
            ctx->overflow = true;
            return;
        }
        ctx->current_text_len += (size_t)len;
    }
}

static bool flippass_open_model_arena_estimate_stream_callback(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    FlipPassOpenModelArenaEstimateContext* ctx = context;
    if(ctx == NULL) {
        return false;
    }

    if(data == NULL && data_size > 0U) {
        return false;
    }

    return flippass_open_model_arena_estimate_feed_payload(ctx, data, data_size);
}

static size_t flippass_open_model_predict_arena_payload(
    FlipPassOpenSession* session,
    KDBXVaultBackend model_backend) {
    if(session == NULL || session->xml_scratch.vault == NULL) {
        return 0U;
    }

    XmlParser* parser = xml_parser_alloc();
    if(parser == NULL) {
        return 0U;
    }

    FlipPassOpenModelArenaEstimateContext estimate_ctx;
    memset(&estimate_ctx, 0, sizeof(estimate_ctx));
    estimate_ctx.parser = parser;
    estimate_ctx.model_backend = model_backend;
    xml_parser_set_callback_context(parser, &estimate_ctx);
    xml_parser_set_element_handlers(
        parser,
        flippass_open_model_arena_estimate_start_element,
        flippass_open_model_arena_estimate_end_element);
    xml_parser_set_character_data_handler(
        parser, flippass_open_model_arena_estimate_character_data);

    if(!kdbx_vault_stream_scratch(
           session->xml_scratch.vault,
           &session->xml_scratch.ref,
           flippass_open_model_arena_estimate_stream_callback,
           &estimate_ctx)) {
        xml_parser_free(parser);
        return 0U;
    }

    if(!estimate_ctx.xml_started || estimate_ctx.overflow ||
       !xml_parser_feed(parser, NULL, 0U, true)) {
        xml_parser_free(parser);
        return 0U;
    }

    xml_parser_free(parser);
    if(estimate_ctx.predicted_payload_used == 0U ||
       estimate_ctx.predicted_payload_used >
           (SIZE_MAX - FLIPPASS_OPEN_MODEL_PRESCAN_CHUNK_SLACK_BYTES)) {
        return 0U;
    }

    return estimate_ctx.predicted_payload_used + FLIPPASS_OPEN_MODEL_PRESCAN_CHUNK_SLACK_BYTES;
}

static bool flippass_open_builder_prepare_for_arena_alloc(
    FlipPassOpenBuilderContext* ctx,
    FuriString* error,
    const char* stage,
    size_t request_size) {
    size_t predicted = request_size;

    furi_assert(ctx);

    flippass_open_builder_refresh_budget(ctx);
    if(!flippass_open_builder_has_safe_heap_floor(ctx, error, stage, request_size)) {
        return false;
    }
    if(predicted <= (SIZE_MAX - FLIPPASS_OPEN_MODEL_GROWTH_RESERVE_BYTES)) {
        predicted += FLIPPASS_OPEN_MODEL_GROWTH_RESERVE_BYTES;
    } else {
        predicted = SIZE_MAX;
    }

    if(!flippass_open_builder_should_promote(ctx, predicted)) {
        return true;
    }

    if(!ctx->allow_ext_promotion) {
        if(!flippass_open_builder_offer_fallback(ctx, error, stage, request_size)) {
            flippass_open_builder_log_failure(ctx, stage, "fallback_unavailable", request_size);
            return false;
        }
    }

    if(!flippass_open_builder_promote_to_ext(ctx, error)) {
        flippass_open_builder_log_failure(ctx, stage, "promote_failed", request_size);
        return false;
    }

    return true;
}

static bool flippass_open_builder_prepare_string_value_stream(
    void* context,
    const char* key,
    size_t buffered_size,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    size_t predicted = buffered_size;

    furi_assert(ctx);
    furi_assert(key);

    if(ctx->current_entry == NULL || ctx->vault == NULL || key[0] == '\0' ||
       strcmp(key, "Title") == 0) {
        return true;
    }

    flippass_open_builder_refresh_budget(ctx);
    if(!flippass_open_builder_has_safe_heap_floor(ctx, error, key, buffered_size)) {
        return false;
    }
    if(predicted <= (SIZE_MAX - KDBX_VAULT_RECORD_PLAIN_MAX)) {
        predicted += KDBX_VAULT_RECORD_PLAIN_MAX;
    } else {
        predicted = SIZE_MAX;
    }

    if(!flippass_open_builder_should_promote(ctx, predicted)) {
        return true;
    }

    if(!ctx->allow_ext_promotion) {
        if(!flippass_open_builder_offer_fallback(ctx, error, key, buffered_size)) {
            flippass_open_builder_log_failure(
                ctx, key, "stream_prepare_fallback_unavailable", buffered_size);
            return false;
        }
    }

    if(kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam) {
        if(!flippass_open_builder_promote_to_ext(ctx, error)) {
            flippass_open_builder_log_failure(
                ctx, key, "stream_prepare_promote_failed", buffered_size);
            return false;
        }
    }

    return true;
}

static bool flippass_open_builder_write_value(
    FlipPassOpenBuilderContext* ctx,
    FuriString* error,
    const char* field_name,
    const char* value,
    size_t value_len,
    KDBXFieldRef* out_ref) {
    furi_assert(ctx);
    furi_assert(field_name);
    furi_assert(out_ref);

    flippass_open_builder_refresh_budget(ctx);
    if(!flippass_open_builder_has_safe_heap_floor(ctx, error, field_name, value_len)) {
        return false;
    }
    if(flippass_open_builder_should_promote(ctx, value_len)) {
        if(!ctx->allow_ext_promotion) {
            if(!flippass_open_builder_offer_fallback(ctx, error, field_name, value_len)) {
                return false;
            }
        }
        if(!flippass_open_builder_promote_to_ext(ctx, error)) {
            return false;
        }
    }

    for(uint8_t attempt = 0U; attempt < 2U; attempt++) {
        if(!flippass_open_builder_prepare_field_writer(ctx, value_len, error)) {
            flippass_open_builder_log_failure(ctx, field_name, "field_writer_prepare", value_len);
            return false;
        }

        if(ctx->vault_promotion_attempted && ctx->deferred_field_count < 8U) {
            FLIPPASS_LOG_EVENT(
                ctx->app,
                "VALUE_WRITE_START field=%s backend=%s bytes=%lu",
                field_name,
                kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)),
                (unsigned long)value_len);
        }
        if(flippass_open_builder_field_writer_uses_file_stream(ctx)) {
            flippass_open_builder_note_flash_write(ctx);
        }
        if(value_len > 0U &&
           !kdbx_vault_writer_write(&ctx->field_writer, (const uint8_t*)value, value_len)) {
            kdbx_vault_writer_abort(&ctx->field_writer);
            if(attempt == 0U && kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam &&
               kdbx_vault_budget_failed(ctx->vault)) {
                if(!ctx->allow_ext_promotion) {
                    if(!flippass_open_builder_offer_fallback(ctx, error, field_name, value_len)) {
                        return false;
                    }
                }
                if(flippass_open_builder_promote_to_ext(ctx, error)) {
                    continue;
                }
            }

            flippass_open_builder_log_failure(ctx, field_name, "field_writer_write", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }

        if(!flippass_open_builder_finish_field_writer(ctx, out_ref)) {
            kdbx_vault_writer_abort(&ctx->field_writer);
            if(attempt == 0U && kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam &&
               kdbx_vault_budget_failed(ctx->vault)) {
                if(!ctx->allow_ext_promotion) {
                    if(!flippass_open_builder_offer_fallback(ctx, error, field_name, value_len)) {
                        return false;
                    }
                }
                if(flippass_open_builder_promote_to_ext(ctx, error)) {
                    continue;
                }
            }

            flippass_open_builder_log_failure(ctx, field_name, "field_writer_finish", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }

        ctx->deferred_field_count++;
        ctx->deferred_plain_bytes += value_len;
        if(ctx->vault_promotion_attempted && ctx->deferred_field_count <= 8U) {
            FLIPPASS_LOG_EVENT(
                ctx->app,
                "VALUE_WRITE_OK field=%s records=%lu",
                field_name,
                (unsigned long)((out_ref->plain_len + KDBX_VAULT_RECORD_PLAIN_MAX - 1U) /
                                KDBX_VAULT_RECORD_PLAIN_MAX));
        }
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
    }
    return false;
}

static bool flippass_open_builder_should_stream_string_value(void* context, const char* key) {
    const FlipPassOpenBuilderContext* ctx = context;

    if(ctx == NULL || ctx->current_entry == NULL || ctx->vault == NULL || key == NULL ||
       key[0] == '\0' || strcmp(key, "Title") == 0) {
        return false;
    }

    return kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam;
}

static bool flippass_open_builder_begin_streamed_value(
    void* context,
    const char* key,
    bool protected_value,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;

    furi_assert(ctx);
    furi_assert(key);

    if(ctx->current_entry == NULL || ctx->vault == NULL || key[0] == '\0') {
        if(error != NULL) {
            furi_string_set_str(error, "The XML entry field could not be streamed safely.");
        }
        return false;
    }

    ctx->deferred_stream_active = false;
    ctx->deferred_stream_protected = protected_value;
    ctx->deferred_stream_plain_bytes = 0U;

    flippass_open_builder_refresh_budget(ctx);
    if(!flippass_open_builder_has_safe_heap_floor(ctx, error, key, KDBX_VAULT_RECORD_PLAIN_MAX)) {
        return false;
    }
    if(flippass_open_builder_should_promote(ctx, KDBX_VAULT_RECORD_PLAIN_MAX)) {
        if(!ctx->allow_ext_promotion) {
            if(!flippass_open_builder_offer_fallback(
                   ctx, error, key, KDBX_VAULT_RECORD_PLAIN_MAX)) {
                flippass_open_builder_log_failure(
                    ctx, key, "stream_begin_fallback_unavailable", KDBX_VAULT_RECORD_PLAIN_MAX);
                return false;
            }
        }
        if(!flippass_open_builder_promote_to_ext(ctx, error)) {
            flippass_open_builder_log_failure(
                ctx, key, "stream_begin_promote_failed", KDBX_VAULT_RECORD_PLAIN_MAX);
            return false;
        }
    }

    if(!flippass_open_builder_prepare_field_writer(ctx, KDBX_VAULT_RECORD_PLAIN_MAX, error)) {
        flippass_open_builder_log_failure(
            ctx, key, "stream_begin_writer_prepare", KDBX_VAULT_RECORD_PLAIN_MAX);
        return false;
    }

    ctx->deferred_stream_active = true;
    if(flippass_open_builder_field_writer_uses_file_stream(ctx)) {
        flippass_open_builder_note_flash_write(ctx);
    }
    return true;
}

static bool flippass_open_builder_write_streamed_value_chunk(
    void* context,
    const char* key,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;

    furi_assert(ctx);
    furi_assert(key);
    furi_assert(data);

    if(!ctx->deferred_stream_active) {
        return false;
    }

    if(ctx->deferred_stream_plain_bytes > (FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES - data_size)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        ctx->deferred_stream_protected = false;
        ctx->deferred_stream_plain_bytes = 0U;
        if(error != NULL) {
            furi_string_printf(
                error,
                "A database field exceeded FlipPass's %lu-byte field limit.",
                (unsigned long)FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES);
        }
        return false;
    }

    if(!kdbx_vault_writer_write(&ctx->field_writer, data, data_size)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        ctx->deferred_stream_protected = false;
        ctx->deferred_stream_plain_bytes = 0U;
        flippass_open_builder_log_failure(ctx, key, "stream_write", data_size);
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    ctx->deferred_stream_plain_bytes += data_size;
    return true;
}

static bool
    flippass_open_builder_commit_streamed_value(void* context, const char* key, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;
    const size_t value_len = (ctx != NULL) ? ctx->deferred_stream_plain_bytes : 0U;

    furi_assert(ctx);
    furi_assert(key);

    if(ctx->current_entry == NULL || !ctx->deferred_stream_active) {
        return false;
    }

    memset(&ref, 0, sizeof(ref));
    if(!flippass_open_builder_finish_field_writer(ctx, &ref)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        ctx->deferred_stream_plain_bytes = 0U;
        flippass_open_builder_log_failure(ctx, key, "stream_finish", value_len);
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    ctx->deferred_stream_active = false;

    if(strcmp(key, "UUID") == 0) {
        if(!kdbx_entry_set_uuid_ref(ctx->current_entry, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "UserName") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldUsername, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "Password") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldPassword, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "URL") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldUrl, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "Notes") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldNotes, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "AutoType") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldAutotype, &ref)) {
            flippass_open_builder_log_failure(ctx, key, "stream_ref_attach", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else {
        if(!flippass_open_builder_prepare_for_arena_alloc(
               ctx, error, "custom_field", sizeof(KDBXCustomField) + strlen(key) + 1U)) {
            return false;
        }

        if(kdbx_entry_add_custom_field_ex(
               ctx->current_entry, ctx->arena, key, &ref, ctx->deferred_stream_protected) ==
           NULL) {
            flippass_open_builder_log_failure(ctx, key, "stream_custom_field_alloc", value_len);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    }

    ctx->deferred_field_count++;
    ctx->deferred_plain_bytes += value_len;
    ctx->deferred_stream_plain_bytes = 0U;
    ctx->deferred_stream_protected = false;
    return true;
}

static void flippass_open_builder_abort_streamed_value(void* context) {
    FlipPassOpenBuilderContext* ctx = context;

    if(ctx == NULL) {
        return;
    }

    if(ctx->field_writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->field_writer);
    }
    ctx->deferred_stream_active = false;
    ctx->deferred_stream_protected = false;
    ctx->deferred_stream_plain_bytes = 0U;
}

static bool flippass_open_builder_begin_session(
    void* context,
    KDBXVaultBackend backend,
    bool allow_ext_promotion,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    App* app = NULL;

    furi_assert(ctx);
    if(ctx->session_active) {
        return false;
    }

    if(!kdbx_vault_backend_supported(backend)) {
        if(error != NULL) {
            furi_string_set_str(error, kdbx_vault_backend_unavailable_reason(backend));
        }
        return false;
    }

    app = ctx->app;
    FlipPassOpenSession* session = ctx->session;
    size_t free_heap = memmgr_get_free_heap();
    size_t max_free = memmgr_heap_get_max_free_block();
    /* File-backed vaults keep pages on disk; only the index lives in RAM.
     * Use a smaller reserve (matching refresh_budget) so the arena can use
     * more of the available heap from the very first allocation. */
    bool file_backed_backend =
        (backend == KDBXVaultBackendFileExt || backend == KDBXVaultBackendFileInt);
    size_t session_reserve = flippass_open_builder_reserve_bytes(session, file_backed_backend);
    if(free_heap <= session_reserve && backend == KDBXVaultBackendRam &&
       kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        if(!allow_ext_promotion &&
           !flippass_open_builder_offer_fallback(ctx, error, "model_admission", session_reserve)) {
            return false;
        }
        backend = KDBXVaultBackendFileExt;
        file_backed_backend = true;
        allow_ext_promotion = true;
        session->allow_ext_promotion = true;
        free_heap = memmgr_get_free_heap();
        max_free = memmgr_heap_get_max_free_block();
        session_reserve = flippass_open_builder_reserve_bytes(session, true);
    }
    if(free_heap <= session_reserve) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to unlock this database.");
        }
        return false;
    }

    FlipPassOpenSession* preserved_session = ctx->session;
    memset(ctx, 0, sizeof(*ctx));
    ctx->session = preserved_session;
    ctx->app = app;
    ctx->allow_ext_promotion = allow_ext_promotion;
    ctx->commit_limit = free_heap - session_reserve;

    /* Larger chunks reduce fragmentation when vault pages live on /ext:
     * fewer but larger arena mallocs interleave less with vault index nodes,
     * keeping max_free_block healthy throughout modeling. */
    size_t arena_chunk_size = file_backed_backend ? (FLIPPASS_OPEN_ARENA_CHUNK_SIZE * 4U) :
                                                    FLIPPASS_OPEN_ARENA_CHUNK_SIZE;
    if(session != NULL && session->predicted_arena_payload > arena_chunk_size) {
        size_t predicted_target = session->predicted_arena_payload;
        const size_t payload_limit =
            flippass_open_model_initial_arena_payload_limit(ctx->commit_limit, max_free);
        if(predicted_target > payload_limit) {
            predicted_target = payload_limit;
        }
        if(predicted_target > arena_chunk_size) {
            arena_chunk_size = predicted_target;
        }
    }

    if(session != NULL) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "MODEL_CHUNK_SELECT default=%lu predicted=%lu selected=%lu reserve=%lu",
            (unsigned long)(file_backed_backend ? (FLIPPASS_OPEN_ARENA_CHUNK_SIZE * 4U) :
                                                  FLIPPASS_OPEN_ARENA_CHUNK_SIZE),
            (unsigned long)session->predicted_arena_payload,
            (unsigned long)arena_chunk_size,
            (unsigned long)session_reserve);
    }

    ctx->arena = kdbx_arena_alloc(arena_chunk_size, &ctx->committed_bytes, ctx->commit_limit);
    ctx->vault = kdbx_vault_alloc(backend, &ctx->committed_bytes, ctx->commit_limit);
    ctx->session_active = (ctx->arena != NULL && ctx->vault != NULL);

    if(!ctx->session_active) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to unlock this database.");
        }
        if(ctx->vault != NULL) {
            kdbx_vault_free(ctx->vault);
            ctx->vault = NULL;
        }
        if(ctx->arena != NULL) {
            kdbx_arena_free(ctx->arena);
            ctx->arena = NULL;
        }
        return false;
    }

    if(kdbx_vault_storage_failed(ctx->vault)) {
        if(error != NULL) {
            furi_string_set_str(
                error,
                "The encrypted session vault could not be created on the selected storage.");
        }
        kdbx_vault_free(ctx->vault);
        kdbx_arena_free(ctx->arena);
        ctx->vault = NULL;
        ctx->arena = NULL;
        ctx->session_active = false;
        return false;
    }

    FLIPPASS_PROBE_LOG_EVENT(
        app,
        "VAULT_MODE backend=%s free=%lu max=%lu limit=%lu reserve=%lu arena_chunk=%lu model_loaded=%u",
        kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->commit_limit,
        (unsigned long)session_reserve,
        (unsigned long)arena_chunk_size,
        flippass_open_model_module_is_loaded(app) ? 1U : 0U);

    return true;
}

static void flippass_open_builder_cancel_session(void* context) {
    FlipPassOpenBuilderContext* ctx = context;

    if(ctx == NULL) {
        return;
    }

    if(ctx->field_writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->field_writer);
    }
    if(ctx->root_group != NULL) {
        kdbx_group_free(ctx->root_group);
    }
    /* Model nodes are gone. Reclaim their arena before vault cleanup, whose
     * storage teardown can need a temporary native File/FatFs allocation. The
     * vault's budget counter belongs to ctx and outlives both owners. */
    if(ctx->arena != NULL) {
        kdbx_arena_free(ctx->arena);
    }
    if(ctx->vault != NULL) {
        kdbx_vault_free(ctx->vault);
    }

    App* app = ctx->app;
    FlipPassOpenSession* session = ctx->session;
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->session = session;
}

static bool flippass_open_builder_begin_group(void* context, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    furi_assert(ctx);

    if(!flippass_open_builder_prepare_for_arena_alloc(
           ctx, error, "group_alloc", sizeof(KDBXGroup))) {
        return false;
    }

    KDBXGroup* group = kdbx_group_alloc(ctx->arena);
    if(group == NULL) {
        flippass_open_builder_log_failure(ctx, "group_alloc", "arena_alloc", sizeof(KDBXGroup));
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    if(ctx->current_group != NULL) {
        group->parent = ctx->current_group;
        group->next = ctx->current_group->children;
        ctx->current_group->children = group;
    } else {
        ctx->root_group = group;
    }

    ctx->current_group = group;
    ctx->group_count++;
    if(ctx->group_count <= 4U || (ctx->group_count % 8U) == 0U) {
        flippass_open_builder_log_progress(ctx, "group");
    }
    return true;
}

static bool flippass_open_builder_end_group(void* context, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    UNUSED(error);
    furi_assert(ctx);

    if(ctx->current_group != NULL) {
        ctx->current_group = ctx->current_group->parent;
    }
    return true;
}

static bool flippass_open_builder_begin_entry(void* context, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    furi_assert(ctx);

    if(ctx->current_group == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The XML entry appeared outside of any group.");
        }
        return false;
    }

    if(!flippass_open_builder_prepare_for_arena_alloc(
           ctx, error, "entry_alloc", sizeof(KDBXEntry))) {
        return false;
    }

    KDBXEntry* entry = kdbx_entry_alloc(ctx->arena);
    if(entry == NULL) {
        flippass_open_builder_log_failure(ctx, "entry_alloc", "arena_alloc", sizeof(KDBXEntry));
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    entry->next = ctx->current_group->entries;
    ctx->current_group->entries = entry;
    ctx->current_entry = entry;
    ctx->entry_count++;
    if(ctx->entry_count <= 4U || (ctx->entry_count % 16U) == 0U) {
        flippass_open_builder_log_progress(ctx, "entry");
    }
    return true;
}

static bool flippass_open_builder_end_entry(void* context, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    UNUSED(error);
    furi_assert(ctx);
    ctx->current_entry = NULL;
    return true;
}

static void flippass_open_builder_set_icon_id(void* context, uint8_t icon_id) {
    FlipPassOpenBuilderContext* ctx = context;
    furi_assert(ctx);

    if(ctx->current_entry != NULL) {
        ctx->current_entry->icon_id = icon_id;
    } else if(ctx->current_group != NULL) {
        ctx->current_group->icon_id = icon_id;
    }
}

static bool flippass_open_builder_set_group_name(
    void* context,
    const char* value,
    size_t value_len,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    furi_assert(ctx);

    if(!flippass_open_builder_prepare_for_arena_alloc(ctx, error, "group_name", value_len + 1U)) {
        return false;
    }

    if(ctx->current_group == NULL || !kdbx_group_set_name(ctx->current_group, ctx->arena, value)) {
        flippass_open_builder_log_failure(ctx, "group_name", "arena_strdup", value_len + 1U);
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    return true;
}

static bool flippass_open_builder_set_group_uuid(
    void* context,
    const char* value,
    size_t value_len,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;

    furi_assert(ctx);
    if(ctx->current_group == NULL) {
        return false;
    }

    if(kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam) {
        if(!flippass_open_builder_prepare_for_arena_alloc(
               ctx, error, "group_uuid", value_len + 1U)) {
            return false;
        }

        if(!kdbx_group_set_uuid(ctx->current_group, ctx->arena, value)) {
            flippass_open_builder_log_failure(ctx, "group_uuid", "arena_strdup", value_len + 1U);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }

        return true;
    }

    memset(&ref, 0, sizeof(ref));
    if(!flippass_open_builder_write_value(ctx, error, "GroupUUID", value, value_len, &ref) ||
       !kdbx_group_set_uuid_ref(ctx->current_group, &ref)) {
        flippass_open_builder_log_failure(ctx, "group_uuid_ref", "ref_attach", value_len);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    return true;
}

static bool flippass_open_builder_set_entry_title(
    void* context,
    const char* value,
    size_t value_len,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;

    furi_assert(ctx);
    if(ctx->current_entry == NULL) {
        return false;
    }

    if(value_len > FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "A database field exceeded FlipPass's %lu-byte field limit.",
                (unsigned long)FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES);
        }
        return false;
    }

    memset(&ref, 0, sizeof(ref));
    if(value_len > 0U) {
        if(!flippass_open_builder_write_value(ctx, error, "Title", value, value_len, &ref)) {
            return false;
        }
    }

    if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldTitle, &ref)) {
        flippass_open_builder_log_failure(ctx, "Title", "ref_attach", value_len);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    if(ctx->vault_promotion_attempted) {
        FLIPPASS_LOG_EVENT(ctx->app, "ENTRY_TITLE_SET_OK");
    }

    return true;
}

static bool flippass_open_builder_set_entry_uuid(
    void* context,
    const char* value,
    size_t value_len,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;

    furi_assert(ctx);
    if(ctx->current_entry == NULL) {
        return false;
    }

    if(kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam) {
        if(!flippass_open_builder_prepare_for_arena_alloc(
               ctx, error, "entry_uuid", value_len + 1U)) {
            return false;
        }
        if(ctx->vault_promotion_attempted) {
            FLIPPASS_LOG_EVENT(
                ctx->app,
                "ENTRY_UUID_AFTER_PROMOTE backend=%s len=%lu",
                kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)),
                (unsigned long)value_len);
        }
        if(!kdbx_entry_set_uuid(ctx->current_entry, ctx->arena, value)) {
            flippass_open_builder_log_failure(ctx, "UUID", "arena_strdup", value_len + 1U);
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
        return true;
    }

    memset(&ref, 0, sizeof(ref));
    if(!flippass_open_builder_write_value(ctx, error, "UUID", value, value_len, &ref) ||
       !kdbx_entry_set_uuid_ref(ctx->current_entry, &ref)) {
        flippass_open_builder_log_failure(ctx, "UUID", "ref_attach", value_len);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    return true;
}

static bool flippass_open_builder_set_entry_standard_field(
    void* context,
    uint32_t field_mask,
    const char* value,
    size_t value_len,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;

    furi_assert(ctx);
    if(ctx->current_entry == NULL) {
        return false;
    }

    if(value_len > FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "A database field exceeded FlipPass's %lu-byte field limit.",
                (unsigned long)FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES);
        }
        return false;
    }

    memset(&ref, 0, sizeof(ref));
    if(!flippass_open_builder_write_value(
           ctx, error, flippass_open_field_log_name(field_mask), value, value_len, &ref) ||
       !kdbx_entry_set_field_ref(ctx->current_entry, field_mask, &ref)) {
        flippass_open_builder_log_failure(
            ctx, flippass_open_field_log_name(field_mask), "ref_attach", value_len);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    return true;
}

static bool flippass_open_builder_add_custom_field(
    void* context,
    const char* key,
    const char* value,
    size_t value_len,
    bool protected_value,
    FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    KDBXFieldRef ref;

    furi_assert(ctx);
    if(ctx->current_entry == NULL || key == NULL || key[0] == '\0') {
        return false;
    }

    if(value_len > FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES) {
        if(error != NULL) {
            furi_string_printf(
                error,
                "A database field exceeded FlipPass's %lu-byte field limit.",
                (unsigned long)FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES);
        }
        return false;
    }

    memset(&ref, 0, sizeof(ref));
    if(!flippass_open_builder_write_value(ctx, error, key, value, value_len, &ref)) {
        return false;
    }

    if(!flippass_open_builder_prepare_for_arena_alloc(
           ctx, error, "custom_field", sizeof(KDBXCustomField) + strlen(key) + 1U)) {
        return false;
    }

    if(kdbx_entry_add_custom_field_ex(
           ctx->current_entry, ctx->arena, key, &ref, protected_value) == NULL) {
        flippass_open_builder_log_failure(ctx, key, "custom_field_alloc", strlen(key) + 1U);
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    return true;
}

static bool flippass_open_builder_finish_session(
    void* context,
    size_t group_count,
    size_t entry_count,
    FuriString* error) {
#if !FLIPPASS_EVENT_LOG_ENABLED
    UNUSED(group_count);
    UNUSED(entry_count);
#endif
    FlipPassOpenBuilderContext* ctx = context;
    furi_assert(ctx);

    if(ctx->root_group == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The decrypted XML payload did not contain any groups.");
        }
        return false;
    }

    flippass_open_builder_close_field_writer(ctx);

    /* Entries are prepended while parsing; recover their XML order without
     * storing an extra tail pointer in every group. */
    kdbx_group_restore_source_entry_order(ctx->root_group);

    ctx->app->db_arena = ctx->arena;
    ctx->app->vault = ctx->vault;
    ctx->app->root_group = ctx->root_group;
    ctx->app->current_group = ctx->root_group;
    ctx->app->current_entry = NULL;
    ctx->app->active_group = ctx->root_group;
    ctx->app->active_entry = NULL;
    ctx->app->database_loaded = true;
    ctx->app->active_vault_backend = (ctx->vault != NULL) ? kdbx_vault_get_backend(ctx->vault) :
                                                            KDBXVaultBackendNone;
    flippass_db_refresh_model_budget(ctx->app);

    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        ctx->app,
        "PARSE_OK groups=%lu entries=%lu",
        (unsigned long)group_count,
        (unsigned long)entry_count);
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(ctx->app, "DATABASE_READY");

    ctx->arena = NULL;
    ctx->vault = NULL;
    ctx->root_group = NULL;
    ctx->current_group = NULL;
    ctx->current_entry = NULL;
    ctx->session_active = false;
    return true;
}

static const FlipPassOpenAcquirePluginV1*
    flippass_open_acquire_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOpenAcquire,
        NULL,
        FLIPPASS_OPEN_ACQUIRE_PLUGIN_APP_ID,
        FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass open acquire plugin is unavailable.");
        }
        return NULL;
    }

    return descriptor->entry_point;
}

static const FlipPassOpenStreamPluginV1*
    flippass_open_stream_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOpenStream,
        NULL,
        FLIPPASS_OPEN_STREAM_PLUGIN_APP_ID,
        FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass open stream plugin is unavailable.");
        }
        return NULL;
    }

    return descriptor->entry_point;
}

static const FlipPassOpenInflatePluginV1* flippass_open_inflate_plugin_get(
    App* app,
    FlipPassModuleSlot slot,
    const char* expected_appid,
    const char* unavailable_message,
    FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app, slot, NULL, expected_appid, FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION, error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, unavailable_message);
        }
        return NULL;
    }

    return descriptor->entry_point;
}

static const FlipPassOpenModelPluginV1*
    flippass_open_model_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOpenModel,
        NULL,
        FLIPPASS_OPEN_MODEL_PLUGIN_APP_ID,
        FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass open model plugin is unavailable.");
        }
        return NULL;
    }

    return descriptor->entry_point;
}

static bool flippass_open_run_inflate_stage(
    App* app,
    FlipPassOpenSession* session,
    FlipPassOpenInflateKind kind,
    const KDBXGzipMemberInfo* member_info,
    KDBXVaultBackend requested_backend,
    FuriString* load_error,
    FuriString* error) {
    const FlipPassOpenInflatePluginV1* inflate_plugin = NULL;
    FlipPassModuleSlot slot = FlipPassModuleSlotOpenInflatePaged;
    const char* appid = FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_APP_ID;
    const char* unavailable_message = "FlipPass open inflate paged plugin is unavailable.";
    FlipPassOpenInflateResultV1 inflate_result = {
        .api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
        .retry_with_paged = false,
    };
    const FlipPassOpenInflateRequestV1 inflate_request = {
        .api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
        .preferred_backend = requested_backend,
        .member_info = *member_info,
    };
    static FlipPassOpenInflateHostApiV1 inflate_host_api = {
        .api_version = FLIPPASS_OPEN_INFLATE_HOST_API_VERSION,
        .progress = flippass_open_host_progress,
        .log = flippass_open_host_log,
        .begin_staged_payload_stream = flippass_open_host_begin_staged_payload_stream,
        .read_staged_payload_stream = flippass_open_host_read_staged_payload_stream,
        .end_staged_payload_stream = flippass_open_host_end_staged_payload_stream,
        .begin_staged_xml = flippass_open_host_begin_staged_xml,
        .append_staged_xml = flippass_open_host_append_staged_xml,
        .finish_staged_xml = flippass_open_host_finish_staged_xml,
        .clear_staged_xml = flippass_open_host_clear_staged_xml,
        .crypt_paged_window = flippass_open_host_crypt_paged_window,
        .clear_paged_window_crypto = flippass_open_host_clear_paged_window_crypto,
        .backend_supported = kdbx_vault_backend_supported,
    };
    inflate_host_api.context = session;

    if(kind == FlipPassOpenInflateKindNonPaged) {
        slot = FlipPassModuleSlotOpenInflateNonPaged;
        appid = FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_APP_ID;
        unavailable_message = "FlipPass open inflate nonpaged plugin is unavailable.";
    }

    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_load_before" :
                                                  "open_inflate_paged_load_before",
        flippass_open_theoretical_inflate_bytes(kind, member_info));
    inflate_plugin =
        flippass_open_inflate_plugin_get(app, slot, appid, unavailable_message, load_error);
    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_load_after" :
                                                  "open_inflate_paged_load_after",
        flippass_open_theoretical_inflate_bytes(kind, member_info));
    if(inflate_plugin == NULL) {
        furi_string_set(error, load_error);
        return false;
    }

    furi_string_reset(error);
    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_run_begin" :
                                                  "open_inflate_paged_run_begin",
        flippass_open_theoretical_inflate_bytes(kind, member_info));
    if(!inflate_plugin->run(&inflate_request, &inflate_host_api, &inflate_result, error)) {
        const bool retry_with_paged = kind == FlipPassOpenInflateKindNonPaged &&
                                      inflate_result.retry_with_paged;
        FLIPPASS_MEMORY_LOG(
            app,
            kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_run_fail" :
                                                      "open_inflate_paged_run_fail",
            flippass_open_theoretical_inflate_bytes(kind, member_info));
        flippass_module_unload(app, slot);
        FLIPPASS_MEMORY_LOG(
            app,
            kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_unloaded" :
                                                      "open_inflate_paged_unloaded",
            flippass_open_theoretical_session_bytes());
        if(retry_with_paged) {
            furi_string_reset(error);
            return flippass_open_run_inflate_stage(
                app,
                session,
                FlipPassOpenInflateKindPaged,
                member_info,
                requested_backend,
                load_error,
                error);
        }
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to inflate the staged GZip payload.");
        }
        return false;
    }
    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_run_ok" :
                                                  "open_inflate_paged_run_ok",
        flippass_open_theoretical_inflate_bytes(kind, member_info));

    flippass_module_unload(app, slot);
    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_unloaded" :
                                                  "open_inflate_paged_unloaded",
        flippass_open_theoretical_session_bytes());
    flippass_open_host_clear_staged_payload(session);
    return true;
}

bool flippass_open_host_execute(App* app, FuriString* error) {
    FlipPassOpenSession* session = NULL;
    FuriString* load_error = NULL;
    const FlipPassOpenAcquirePluginV1* acquire_plugin = NULL;
    const FlipPassOpenStreamPluginV1* stream_plugin = NULL;
    const FlipPassOpenModelPluginV1* model_plugin = NULL;
    FlipPassOpenStreamResultV2 stream_result = {
        .api_version = FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION,
        .output_kind = FlipPassOpenStreamOutputKindNone,
        .staged_payload_size = 0U,
        .gzip_member_info = {0},
        .suggested_inflate_kind = FlipPassOpenInflateKindNone,
    };
    const KDBXVaultBackend requested_backend =
        (app->requested_vault_backend == KDBXVaultBackendNone) ? KDBXVaultBackendRam :
                                                                 app->requested_vault_backend;
    const bool allow_ext_promotion = app->allow_ext_vault_promotion;
    bool ok = false;
    bool trace_capture_suspended = false;

    furi_assert(app);
    furi_assert(error);

    FLIPPASS_MEMORY_LOG(app, "open_begin", flippass_open_theoretical_session_bytes());
    app->derivation_cache_used = false;
    app->derivation_cache_failed = false;

    /* Page-recycling design: xml_scratch is consumed and freed in place during
     * model->run, so there is no staged-resume cache. The fallback recovery
     * path is a full pipeline replay forced to /ext via the existing
     * select_gzip_scratch_backend (FileExt > FileInt > requested > Ram).
     *
     * Snapshot+clear resuming_after_fallback: the UI signal lives only for the
     * single open attempt that follows the /ext fallback dialog. Subsequent
     * attempts (e.g. retry after error) start clean. */
    const bool replay_after_fallback = app->resuming_after_fallback;
    app->resuming_after_fallback = false;
    if(replay_after_fallback) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "OPEN_REPLAY_AFTER_FALLBACK backend=%s",
            kdbx_vault_backend_label(requested_backend));
    }

    if(app->master_password[0] == '\0') {
        furi_string_set_str(error, "Enter the database password to continue.");
        return false;
    }

    session = flippass_open_session_alloc(app);
    if(session == NULL) {
        furi_string_set_str(
            error, "Not enough RAM is available to start unlocking this database.");
        return false;
    }
    FLIPPASS_MEMORY_LOG(app, "open_session_allocated", flippass_open_theoretical_session_bytes());
    session->requested_backend = requested_backend;
    session->allow_ext_promotion = allow_ext_promotion;

    flippass_open_trim_runtime_modules(app);
    load_error = furi_string_alloc();
    FLIPPASS_MEMORY_LOG(
        app, "open_load_error_allocated", flippass_open_theoretical_session_bytes());
    FLIPPASS_MEMORY_LOG(
        app, "open_acquire_load_before", flippass_open_theoretical_acquire_bytes());
    acquire_plugin = flippass_open_acquire_plugin_get(app, load_error);
    FLIPPASS_MEMORY_LOG(app, "open_acquire_load_after", flippass_open_theoretical_acquire_bytes());
    if(acquire_plugin == NULL) {
        furi_string_set(error, load_error);
        goto cleanup;
    }

    FLIPPASS_PROBE_LOG_EVENT(app, "UNLOCK_START");
    flippass_system_log_capture_suspend();
    trace_capture_suspended = true;

    flippass_reset_database(app);
    app->requested_vault_backend = requested_backend;
    app->allow_ext_vault_promotion = allow_ext_promotion;
    FLIPPASS_MEMORY_LOG(app, "open_after_reset", flippass_open_theoretical_session_bytes());

    {
        const FlipPassOpenAcquireRequestV1 acquire_request = {
            .api_version = FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION,
            .file_path = furi_string_get_cstr(app->file_path),
            .password = app->master_password,
        };
        static FlipPassOpenAcquireHostApiV1 acquire_host_api = {
            .api_version = FLIPPASS_OPEN_ACQUIRE_HOST_API_VERSION,
            .progress = flippass_open_host_progress,
            .log = flippass_open_host_log,
            .read_cached_transformed_key = flippass_open_host_read_cached_transformed_key,
            .should_cancel = flippass_open_host_should_cancel,
        };
        acquire_host_api.context = session;

        if(!acquire_plugin->run(
               &acquire_request, &acquire_host_api, &session->open_profile, error)) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "Unable to derive the database open profile.");
            }
            goto cleanup;
        }
        FLIPPASS_MEMORY_LOG(app, "open_acquire_run_ok", flippass_open_theoretical_acquire_bytes());

        {
            char profile_error[128] = {0};
            if(!kdbx_open_profile_validate_for_stream(
                   &session->open_profile, profile_error, sizeof(profile_error))) {
                furi_string_set_str(error, profile_error);
                goto cleanup;
            }
        }

        session->open_profile_ready = true;
        /* The returned profile owns all values. Release acquire before sealing
         * can lazily map Crypto; no acquire code or borrowed pointer remains. */
        flippass_module_unload(app, FlipPassModuleSlotOpenAcquire);
        acquire_plugin = NULL;
        FLIPPASS_MEMORY_LOG(
            app, "open_acquire_unloaded", flippass_open_theoretical_session_bytes());
        app->database_cipher = (memcmp(
                                    session->open_profile.encryption_algorithm_uuid,
                                    KDBX_UUID_CHACHA20,
                                    sizeof(KDBX_UUID_CHACHA20)) == 0) ?
                                   FlipPassKdbxCipherChaCha20 :
                                   FlipPassKdbxCipherAes256;
        app->database_compression = session->open_profile.compression_algorithm;
        app->database_kdf_rounds = session->open_profile.kdf_rounds != 0U ?
                                       session->open_profile.kdf_rounds :
                                       FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
        if(session->open_profile.composite_key_ready) {
            const uint8_t* transformed_key =
                (session->open_profile.transformed_key_ready &&
                 session->open_profile.kdf_salt_size == sizeof(session->open_profile.kdf_salt)) ?
                    session->open_profile.transformed_key :
                    NULL;
            const uint8_t* kdf_salt = transformed_key != NULL ? session->open_profile.kdf_salt :
                                                                NULL;
            if(!flippass_session_store_save_material(
                   app,
                   session->open_profile.composite_key,
                   transformed_key,
                   kdf_salt,
                   transformed_key != NULL ? app->database_kdf_rounds : 0U)) {
                furi_string_set_str(error, "Unable to protect the database credential.");
                goto cleanup;
            }
        }
    }

    {
        const FlipPassCryptoApiV1* stream_crypto_api = flippass_crypto_get_api();
        if(stream_crypto_api == NULL) {
            furi_string_set_str(error, "FlipPass crypto plugin is unavailable.");
            goto cleanup;
        }

        /* These metadata outlive Stream. Allocate them before its code can
         * become a free block split by a late-created locator node. The writer
         * buffer is temporary; close it now and recreate it inside Stream. */
        if(session->open_profile.compression_algorithm == KDBX_COMPRESSION_GZIP) {
            if(!flippass_open_host_begin_staged_payload(session, requested_backend, error))
                goto cleanup;
            if(!kdbx_vault_prepare_first_record(session->payload_scratch.vault)) {
                furi_string_set_str(error, "Not enough RAM for the payload record index.");
                goto cleanup;
            }
            kdbx_vault_writer_close(&session->payload_scratch.writer);
        }

        FLIPPASS_MEMORY_LOG(
            app, "open_stream_load_before", flippass_open_theoretical_stream_bytes());
        stream_plugin = flippass_open_stream_plugin_get(app, load_error);
        FLIPPASS_MEMORY_LOG(
            app, "open_stream_load_after", flippass_open_theoretical_stream_bytes());
        if(stream_plugin == NULL) {
            furi_string_set(error, load_error);
            goto cleanup;
        }

        const FlipPassOpenStreamRequestV1 stream_request = {
            .api_version = FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION,
            .file_path = furi_string_get_cstr(app->file_path),
            .open_profile = session->open_profile_ready ? &session->open_profile : NULL,
            .preferred_backend = requested_backend,
        };
        static FlipPassOpenStreamHostApiV1 stream_host_api = {
            .api_version = FLIPPASS_OPEN_STREAM_HOST_API_VERSION,
            .validate_profile = kdbx_open_profile_validate_for_stream,
            .progress = flippass_open_host_progress,
            .log = flippass_open_host_log,
            .crypto = NULL,
            .begin_staged_payload = flippass_open_host_begin_staged_payload,
            .append_staged_payload = flippass_open_host_append_staged_payload,
            .finish_staged_payload = flippass_open_host_finish_staged_payload,
            .clear_staged_payload = flippass_open_host_clear_staged_payload,
            .begin_staged_payload_stream = flippass_open_host_begin_staged_payload_stream,
            .read_staged_payload_stream = flippass_open_host_read_staged_payload_stream,
            .end_staged_payload_stream = flippass_open_host_end_staged_payload_stream,
            .begin_staged_xml = flippass_open_host_begin_staged_xml,
            .append_staged_xml = flippass_open_host_append_staged_xml,
            .finish_staged_xml = flippass_open_host_finish_staged_xml,
            .clear_staged_xml = flippass_open_host_clear_staged_xml,
        };
        stream_host_api.context = session;
        stream_host_api.crypto = stream_crypto_api;

        if(!stream_plugin->run(&stream_request, &stream_host_api, &stream_result, error)) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "Unable to prepare the staged open payload.");
            }
            FLIPPASS_PROBE_LOG_EVENT(
                app, "OPEN_STREAM_FAIL reason=%s", furi_string_get_cstr(error));
            goto cleanup;
        }
        FLIPPASS_MEMORY_LOG(app, "open_stream_run_ok", flippass_open_theoretical_stream_bytes());

        flippass_module_unload(app, FlipPassModuleSlotOpenStream);
        stream_plugin = NULL;
        FLIPPASS_MEMORY_LOG(
            app, "open_stream_unloaded", flippass_open_theoretical_session_bytes());

        if(stream_result.output_kind == FlipPassOpenStreamOutputKindGzipMember) {
            FlipPassOpenInflateKind inflate_kind = flippass_open_select_inflate_kind_after_stream(
                stream_result.suggested_inflate_kind, &stream_result.gzip_member_info);

#if FLIPPASS_ENABLE_LOGS
            FLIPPASS_LOG_EVENT(
                app,
                "GZIP_INFLATE_SELECT suggested=%s selected=%s member=%lu out=%lu free=%lu max=%lu",
                flippass_open_inflate_kind_label(stream_result.suggested_inflate_kind),
                flippass_open_inflate_kind_label(inflate_kind),
                (unsigned long)stream_result.gzip_member_info.member_size,
                (unsigned long)stream_result.gzip_member_info.expected_output_size,
                (unsigned long)memmgr_get_free_heap(),
                (unsigned long)memmgr_heap_get_max_free_block());
#endif

            FLIPPASS_MEMORY_LOG(
                app,
                "open_inflate_before",
                flippass_open_theoretical_inflate_bytes(
                    inflate_kind, &stream_result.gzip_member_info));
            if(!flippass_open_run_inflate_stage(
                   app,
                   session,
                   inflate_kind,
                   &stream_result.gzip_member_info,
                   requested_backend,
                   load_error,
                   error)) {
                goto cleanup;
            }
            FLIPPASS_MEMORY_LOG(
                app,
                "open_inflate_after",
                flippass_open_theoretical_model_bytes(session->xml_scratch.size));
        } else if(stream_result.output_kind != FlipPassOpenStreamOutputKindXml) {
            furi_string_set_str(
                error, "The staged open payload did not expose a usable output kind.");
            goto cleanup;
        }
    }

    flippass_open_trim_runtime_modules(app);
    flippass_db_browser_clear_title_cache(app);
    if(load_error != NULL) {
        furi_string_free(load_error);
        load_error = NULL;
    }
    furi_string_reset(error);

    /* Use the same predictive policy with remembered or one-shot consent.
     * Smaller databases still start in RAM and may promote during modeling. */
    KDBXVaultBackend model_backend;
    if(!flippass_open_prepare_model_backend(session, &model_backend, error)) {
        goto cleanup;
    }

    flippass_progress_update(app, "Pre-Scanning", "", 80U);
    session->predicted_arena_payload =
        flippass_open_model_predict_arena_payload(session, model_backend);
    if(session->predicted_arena_payload > 0U) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "MODEL_PRESCAN_RESULT predicted=%lu xml=%lu backend=%s",
            (unsigned long)session->predicted_arena_payload,
            (unsigned long)session->xml_scratch.size,
            kdbx_vault_backend_label(model_backend));
    } else {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "MODEL_PRESCAN_RESULT predicted=0 xml=%lu backend=%s",
            (unsigned long)session->xml_scratch.size,
            kdbx_vault_backend_label(model_backend));
    }

    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_load_before",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    model_plugin = flippass_open_model_plugin_get(app, error);
    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_load_after",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    if(model_plugin == NULL) {
        goto cleanup;
    }
    /* Reserve the next reader's actual contiguous allocation, rather than
     * rejecting a fragmented heap solely because no arbitrary 4 KiB hole exists.
     * The same buffer is borrowed by replay and credited against its reserve. */
    session->model_stream_workspace = malloc(kdbx_vault_stream_workspace_size());
    if(session->model_stream_workspace == NULL) {
        furi_string_set_str(error, "Not enough contiguous RAM is available for XML replay.");
        goto cleanup;
    }

    const FlipPassOpenModelRequestV1 model_request = {
        .api_version = FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION,
        .requested_backend = model_backend,
        .allow_ext_promotion = session->allow_ext_promotion,
        .staged_payload_plain_size = session->xml_scratch.size,
    };
    static FlipPassOpenModelHostApiV1 model_host_api = {
        .api_version = FLIPPASS_OPEN_MODEL_HOST_API_VERSION,
        .progress = flippass_open_host_progress,
        .log = flippass_open_host_log,
        .stream_staged_xml = flippass_open_host_stream_staged_xml,
        .derive_protected_stream_material = flippass_open_host_derive_protected_stream_material,
        .xml_alloc = xml_parser_alloc,
        .xml_free = xml_parser_free,
        .xml_feed = xml_parser_feed,
        .xml_error = xml_parser_get_last_error,
        .xml_set_context = xml_parser_set_callback_context,
        .xml_set_elements = xml_parser_set_element_handlers,
        .xml_set_text = xml_parser_set_character_data_handler,
    };
    model_host_api.context = session;
    static FlipPassOpenBuilderApiV1 builder_api = {
        .api_version = FLIPPASS_OPEN_MODEL_BUILDER_API_VERSION,
        .begin_session = flippass_open_builder_begin_session,
        .cancel_session = flippass_open_builder_cancel_session,
        .begin_group = flippass_open_builder_begin_group,
        .end_group = flippass_open_builder_end_group,
        .begin_entry = flippass_open_builder_begin_entry,
        .end_entry = flippass_open_builder_end_entry,
        .set_group_name = flippass_open_builder_set_group_name,
        .set_group_uuid = flippass_open_builder_set_group_uuid,
        .set_entry_title = flippass_open_builder_set_entry_title,
        .set_entry_uuid = flippass_open_builder_set_entry_uuid,
        .set_icon_id = flippass_open_builder_set_icon_id,
        .set_entry_standard_field = flippass_open_builder_set_entry_standard_field,
        .add_custom_field = flippass_open_builder_add_custom_field,
        .should_stream_string_value = flippass_open_builder_should_stream_string_value,
        .prepare_string_value_stream = flippass_open_builder_prepare_string_value_stream,
        .begin_streamed_value = flippass_open_builder_begin_streamed_value,
        .write_streamed_value_chunk = flippass_open_builder_write_streamed_value_chunk,
        .commit_streamed_value = flippass_open_builder_commit_streamed_value,
        .abort_streamed_value = flippass_open_builder_abort_streamed_value,
        .finish_session = flippass_open_builder_finish_session,
    };
    builder_api.context = &session->builder;

    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_run_begin",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    flippass_module_pin(app, FlipPassModuleSlotOpenModel);
    ok = model_plugin->run(&model_request, &model_host_api, &builder_api, error);
    flippass_module_unpin(app, FlipPassModuleSlotOpenModel);
    flippass_open_model_workspace_clear(session);
    FLIPPASS_MEMORY_LOG(
        app,
        ok ? "open_model_run_ok" : "open_model_run_fail",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    /* Surface the arena near-limit fallback whenever it fired during this
     * model run, regardless of success/failure. The IMMEDIATE plan A2 needs
     * this signal to confirm that the smaller-chunk retry actually helped
     * and to bound the chunk-header overhead it cost. */
    if(session->builder.arena != NULL &&
       kdbx_arena_fallback_chunk_count(session->builder.arena) > 0U) {
        FLIPPASS_PROBE_LOG_EVENT(
            app,
            "ARENA_FALLBACK chunks=%lu smallest=%lu total_bytes=%lu",
            (unsigned long)kdbx_arena_fallback_chunk_count(session->builder.arena),
            (unsigned long)kdbx_arena_fallback_smallest_payload(session->builder.arena),
            (unsigned long)kdbx_arena_bytes(session->builder.arena));
    }
    if(!ok) {
        flippass_open_builder_cancel_session(&session->builder);
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to unlock the selected database.");
        }
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            app, "PARSE_FAIL reason=%s", furi_string_get_cstr(error));
    } else {
        flippass_module_unload(app, FlipPassModuleSlotOpenModel);
        model_plugin = NULL;
        flippass_open_scratch_reset(&session->xml_scratch);
        FLIPPASS_MEMORY_LOG(
            app, "open_model_unloaded_early", flippass_open_theoretical_session_bytes());
        if(session->open_profile_ready) {
            app->database_cipher = (memcmp(
                                        session->open_profile.encryption_algorithm_uuid,
                                        KDBX_UUID_CHACHA20,
                                        sizeof(KDBX_UUID_CHACHA20)) == 0) ?
                                       FlipPassKdbxCipherChaCha20 :
                                       FlipPassKdbxCipherAes256;
            app->database_compression = session->open_profile.compression_algorithm;
            app->database_kdf_rounds = session->open_profile.kdf_rounds != 0U ?
                                           session->open_profile.kdf_rounds :
                                           FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
            if(session->open_profile.composite_key_ready && !app->database_save_key_ready) {
                const uint8_t* transformed_key = (session->open_profile.transformed_key_ready &&
                                                  session->open_profile.kdf_salt_size ==
                                                      sizeof(session->open_profile.kdf_salt)) ?
                                                     session->open_profile.transformed_key :
                                                     NULL;
                const uint8_t* kdf_salt =
                    transformed_key != NULL ? session->open_profile.kdf_salt : NULL;
                if(!flippass_session_store_save_material(
                       app,
                       session->open_profile.composite_key,
                       transformed_key,
                       kdf_salt,
                       transformed_key != NULL ? app->database_kdf_rounds : 0U)) {
                    furi_string_set_str(error, "Unable to protect the database credential.");
                    ok = false;
                    goto cleanup;
                }
            }
        }
        app->database_dirty = false;
        app->database_new = false;
        if(app->editor_mode == FlipPassEditorModeModifyDatabase &&
           app->editor_return_scene == FlipPassScene_FileBrowser) {
            app->editor_database_password[0] = '\0';
        }
        flippass_progress_update(app, "Ready", "", 100U);
    }

cleanup:
    if(flippass_progress_guard_canceled(app->progress_view)) {
        ok = false;
        app->pending_vault_fallback = false;
        if(app->database_loaded) flippass_reset_database(app);
        furi_string_set_str(error, "Unlock canceled.");
    }

    /* Page-recycling design: xml_scratch is consumed and freed in place by the
     * model streamer, so it cannot be cached for staged-resume. The fallback
     * recovery path is a full pipeline replay forced to /ext via
     * flippass_open_select_gzip_scratch_backend (which already prefers FileExt).
     * Master password and credentials survive across the replay because the
     * "if(!ok && !pending_vault_fallback) clear" guard below preserves them
     * exactly when the fallback dialog is going to ask for confirmation. */
    if(trace_capture_suspended) {
        flippass_system_log_capture_resume();
    }
    if(!ok && !app->pending_vault_fallback) {
        flippass_session_clear_credentials(app);
        app->database_kdf_rounds = FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
        flippass_clear_master_password(app);
    }
    flippass_module_unload(app, FlipPassModuleSlotOpenAcquire);
    flippass_module_unload(app, FlipPassModuleSlotOpenStream);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflateNonPaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflatePaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenModel);
    FLIPPASS_MEMORY_LOG(app, "open_modules_unloaded", flippass_open_theoretical_session_bytes());
    if(load_error != NULL) {
        furi_string_free(load_error);
    }
    flippass_open_session_free(session);
    FLIPPASS_MEMORY_LOG(app, "open_session_freed", 0U);
    if(ok) {
        if(!app->phantom_open && app->derivation_cache_save_requested) {
            if(flippass_derivation_cache_store_from_session(app, app->master_password)) {
                FLIPPASS_LOG_EVENT(app, "DERIVATION_CACHE_STORED");
            } else {
                FLIPPASS_LOG_EVENT(app, "DERIVATION_CACHE_STORE_FAIL");
            }
        } else if(!app->phantom_open && app->derivation_cache_failed) {
            flippass_derivation_cache_clear(app);
            FLIPPASS_LOG_EVENT(app, "DERIVATION_CACHE_CLEARED");
        }
        flippass_record_successful_open(app);
        flippass_clear_master_password(app);
    }
    return ok;
}
