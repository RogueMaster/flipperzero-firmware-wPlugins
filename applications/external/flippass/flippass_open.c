#include "flippass.h"
#include "flippass_db.h"
#include "kdbx/hmac.h"
#include "kdbx/memzero.h"
#include "kdbx/kdbx_protected.h"
#include "kdbx/sha2.h"
#include "plugins/flippass_open_acquire_plugin.h"
#include "plugins/flippass_open_inflate_plugin.h"
#include "plugins/flippass_open_model_plugin.h"
#include "plugins/flippass_open_stream_plugin.h"

#include <dialogs/dialogs.h>
#include <furi_hal_random.h>
#include <string.h>

#define FLIPPASS_OPEN_SAFETY_RESERVE_BYTES          (8U * 1024U)
#define FLIPPASS_OPEN_MODEL_GROWTH_RESERVE_BYTES    (4U * 1024U)
#define FLIPPASS_OPEN_ARENA_CHUNK_SIZE              256U
#define FLIPPASS_OPEN_MAX_FIELD_PLAIN_BYTES         (256U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_LIMIT           (16U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT     (16U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_GZIP_DICT_BYTES   (32U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_GZIP_MARGIN_BYTES (2U * 1024U)
#define FLIPPASS_OPEN_THEORETICAL_PAGED_RAM_BYTES   (4U * 1024U)

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
    KDBXFieldRef ref;
    size_t size;
} FlipPassOpenScratchContext;

typedef struct {
    KDBXVaultReader* reader;
    bool active;
} FlipPassOpenScratchReaderContext;

typedef struct {
    App* app;
    FlipPassOpenBuilderContext builder;
    FlipPassOpenScratchContext payload_scratch;
    FlipPassOpenScratchReaderContext payload_reader;
    FlipPassOpenScratchContext xml_scratch;
    KDBXVaultBackend requested_backend;
    bool allow_ext_promotion;
    bool resume_from_staged_xml;
    bool paged_window_crypto_ready;
    uint8_t paged_window_enc_key[32];
    uint8_t paged_window_mac_key[32];
    uint8_t paged_window_nonce_prefix[4];
    KDBXOpenProfile open_profile;
    bool open_profile_ready;
} FlipPassOpenSession;

static void flippass_open_scratch_reset(FlipPassOpenScratchContext* ctx);
static void flippass_open_scratch_reader_reset(FlipPassOpenScratchReaderContext* reader);
static void flippass_open_builder_cancel_session(void* context);

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
    UNUSED(suggested);
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
    case FlipPassOpenInflateKindNone:
    default:
        return "none";
    }
}
#endif

static void flippass_open_trim_runtime_modules(App* app) {
    furi_assert(app);

    FLIPPASS_MEMORY_LOG(app, "open_trim_before", flippass_open_theoretical_session_bytes());
    flippass_output_cleanup(app);
    flippass_module_unload(app, FlipPassModuleSlotOutputAction);
    flippass_module_unload(app, FlipPassModuleSlotOtherFields);
    flippass_module_unload(app, FlipPassModuleSlotFileOps);
    flippass_module_unload(app, FlipPassModuleSlotEditorCrud);
    flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
    flippass_module_unload(app, FlipPassModuleSlotPasswordGen);
    flippass_module_unload(app, FlipPassModuleSlotSaveWriter);
    flippass_module_unload(app, FlipPassModuleSlotOpenAcquire);
    flippass_module_unload(app, FlipPassModuleSlotOpenStream);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflateNonPaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflatePaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenModel);
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
    session->payload_scratch.app = app;
    session->xml_scratch.app = app;
    return session;
}

static void flippass_open_session_free(FlipPassOpenSession* session) {
    if(session == NULL) {
        return;
    }

    flippass_open_builder_cancel_session(&session->builder);
    flippass_open_scratch_reset(&session->payload_scratch);
    flippass_open_scratch_reader_reset(&session->payload_reader);
    flippass_open_scratch_reset(&session->xml_scratch);
    memzero(session, sizeof(*session));
    free(session);
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
        FLIPPASS_LOG_EVENT(session->app, "%s", message);
    }
}

static void flippass_open_host_clear_paged_window_crypto(void* context) {
    FlipPassOpenSession* session = context;
    if(session == NULL) {
        return;
    }

    memzero(session->paged_window_enc_key, sizeof(session->paged_window_enc_key));
    memzero(session->paged_window_mac_key, sizeof(session->paged_window_mac_key));
    memzero(session->paged_window_nonce_prefix, sizeof(session->paged_window_nonce_prefix));
    session->paged_window_crypto_ready = false;
}

static bool flippass_open_host_ensure_paged_window_crypto(FlipPassOpenSession* session) {
    static const uint8_t enc_label[4] = {'e', 'n', 'c', '1'};
    static const uint8_t mac_label[4] = {'m', 'a', 'c', '1'};
    uint8_t session_master[32];
    uint8_t material[sizeof(session_master) + sizeof(enc_label)];

    if(session == NULL) {
        return false;
    }
    if(session->paged_window_crypto_ready) {
        return true;
    }

    furi_hal_random_fill_buf(session_master, sizeof(session_master));
    furi_hal_random_fill_buf(
        session->paged_window_nonce_prefix, sizeof(session->paged_window_nonce_prefix));
    memcpy(material, session_master, sizeof(session_master));
    memcpy(material + sizeof(session_master), enc_label, sizeof(enc_label));
    sha256_Raw(material, sizeof(material), session->paged_window_enc_key);
    memcpy(material + sizeof(session_master), mac_label, sizeof(mac_label));
    sha256_Raw(material, sizeof(material), session->paged_window_mac_key);
    memzero(session_master, sizeof(session_master));
    memzero(material, sizeof(material));
    session->paged_window_crypto_ready = true;
    return true;
}

static void flippass_open_paged_window_nonce(
    const FlipPassOpenSession* session,
    uint16_t page_index,
    uint8_t nonce[12]) {
    furi_assert(session);
    furi_assert(nonce);

    memcpy(nonce, session->paged_window_nonce_prefix, sizeof(session->paged_window_nonce_prefix));
    for(size_t index = 0; index < 8U; index++) {
        nonce[4U + index] = (uint8_t)(((uint64_t)page_index >> (index * 8U)) & 0xFFU);
    }
}

static void flippass_open_paged_window_mac(
    const FlipPassOpenSession* session,
    uint16_t page_index,
    const uint8_t* ciphertext,
    size_t page_size,
    uint8_t mac[SHA256_DIGEST_LENGTH]) {
    HMAC_SHA256_CTX hmac_ctx;
    uint8_t page_le[4];

    furi_assert(session);
    furi_assert(ciphertext);
    furi_assert(mac);

    page_le[0] = (uint8_t)(page_index & 0xFFU);
    page_le[1] = (uint8_t)((page_index >> 8U) & 0xFFU);
    page_le[2] = 0U;
    page_le[3] = 0U;

    hmac_sha256_Init(
        &hmac_ctx, session->paged_window_mac_key, sizeof(session->paged_window_mac_key));
    hmac_sha256_Update(&hmac_ctx, page_le, sizeof(page_le));
    hmac_sha256_Update(&hmac_ctx, ciphertext, (uint32_t)page_size);
    hmac_sha256_Final(&hmac_ctx, mac);
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
    uint8_t actual_mac[SHA256_DIGEST_LENGTH];
    bool ok = false;

    if(session == NULL || page == NULL || page_size == 0U ||
       !flippass_open_host_ensure_paged_window_crypto(session)) {
        return false;
    }

    flippass_open_paged_window_nonce(session, page_index, nonce);
    if(encrypt) {
        if(out_mac == NULL || mac_size < sizeof(actual_mac)) {
            goto cleanup;
        }
        if(!kdbx_chacha20_xor(
               page,
               page_size,
               session->paged_window_enc_key,
               sizeof(session->paged_window_enc_key),
               nonce,
               sizeof(nonce),
               0U)) {
            goto cleanup;
        }
        flippass_open_paged_window_mac(session, page_index, page, page_size, out_mac);
        ok = true;
    } else {
        if(expected_mac == NULL) {
            goto cleanup;
        }
        flippass_open_paged_window_mac(session, page_index, page, page_size, actual_mac);
        if(memcmp(actual_mac, expected_mac, sizeof(actual_mac)) != 0) {
            goto cleanup;
        }
        ok = kdbx_chacha20_xor(
            page,
            page_size,
            session->paged_window_enc_key,
            sizeof(session->paged_window_enc_key),
            nonce,
            sizeof(nonce),
            0U);
    }

cleanup:
    memzero(nonce, sizeof(nonce));
    memzero(actual_mac, sizeof(actual_mac));
    return ok;
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
            furi_string_set_str(error, "The KDBX inner protected-value key is missing.");
        }
        return false;
    }

    *material_size = 0U;
    if(algorithm == KDBXProtectedStreamChaCha20) {
        uint8_t hash[SHA512_DIGEST_LENGTH];
        if(material_capacity < KDBX_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE) {
            if(error != NULL) {
                furi_string_set_str(error, "The KDBX protected-stream handoff is too small.");
            }
            return false;
        }

        sha512_Raw(key, key_size, hash);
        memcpy(material, hash, KDBX_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE);
        memzero(hash, sizeof(hash));
        *material_size = KDBX_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE;
        return true;
    }

    if(algorithm == KDBXProtectedStreamSalsa20) {
        if(material_capacity < KDBX_PROTECTED_STREAM_SALSA20_MATERIAL_SIZE) {
            if(error != NULL) {
                furi_string_set_str(error, "The KDBX protected-stream handoff is too small.");
            }
            return false;
        }

        sha256_Raw(key, key_size, material);
        *material_size = KDBX_PROTECTED_STREAM_SALSA20_MATERIAL_SIZE;
        return true;
    }

    if(error != NULL) {
        furi_string_set_str(error, "Only Salsa20 or ChaCha20 protected values are supported.");
    }
    return false;
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

    if(!kdbx_vault_writer_write(&scratch->writer, data, data_size)) {
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

    if(!kdbx_vault_writer_finish(&scratch->writer, &scratch->ref)) {
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
    if(scratch->vault == NULL || kdbx_vault_ref_is_empty(&scratch->ref)) {
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

    kdbx_vault_reader_reset(session->payload_reader.reader, scratch->vault, &scratch->ref);
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
    if(!session->payload_reader.active || session->payload_reader.reader == NULL || out == NULL ||
       out_size == NULL) {
        return false;
    }

    return kdbx_vault_reader_read(session->payload_reader.reader, out, capacity, out_size);
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
    return flippass_open_scratch_begin(
        &session->xml_scratch,
        false,
        preferred_backend,
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

static bool flippass_open_host_stream_staged_xml_callback(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    struct {
        FlipPassOpenChunkCallback callback;
        void* callback_context;
    }* stream = context;

    furi_assert(stream);
    return stream->callback != NULL && stream->callback(data, data_size, stream->callback_context);
}

static bool flippass_open_host_stream_staged_xml(
    void* context,
    FlipPassOpenChunkCallback callback,
    void* callback_context,
    FuriString* error) {
    FlipPassOpenSession* session = context;
    FlipPassOpenScratchContext* scratch = NULL;
    struct {
        FlipPassOpenChunkCallback callback;
        void* callback_context;
    } stream = {
        .callback = callback,
        .callback_context = callback_context,
    };
    bool ok = false;

    furi_assert(session);
    scratch = &session->xml_scratch;
    if(scratch->vault == NULL || kdbx_vault_ref_is_empty(&scratch->ref) || callback == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The staged XML scratch is unavailable.");
        }
        return false;
    }

    FLIPPASS_LOG_EVENT(
        session->app,
        "STAGED_XML_STREAM_START backend=%s bytes=%lu records=%lu free=%lu max=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(scratch->vault)),
        (unsigned long)scratch->size,
        (unsigned long)scratch->ref.record_count,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    ok = kdbx_vault_stream_ref(
        scratch->vault, &scratch->ref, flippass_open_host_stream_staged_xml_callback, &stream);
    if(!ok) {
        FLIPPASS_LOG_EVENT(
            session->app,
            "STAGED_XML_STREAM_FAIL reader=%s record=%lu storage=%s budget=%s size=%lu max=%lu",
            kdbx_vault_last_reader_failure(scratch->vault),
            (unsigned long)kdbx_vault_last_reader_failure_record(scratch->vault),
            kdbx_vault_storage_stage(scratch->vault),
            kdbx_vault_failure_reason(scratch->vault),
            (unsigned long)kdbx_vault_last_failed_size(scratch->vault),
            (unsigned long)kdbx_vault_last_failed_max_free_block(scratch->vault));
    } else {
        FLIPPASS_LOG_EVENT(session->app, "STAGED_XML_STREAM_DONE");
    }

    return ok;
}

static void flippass_open_host_clear_staged_xml(void* context) {
    FlipPassOpenSession* session = context;
    if(session != NULL) {
        flippass_open_scratch_reset(&session->xml_scratch);
    }
}

static void flippass_open_builder_refresh_budget(FlipPassOpenBuilderContext* ctx) {
    const size_t reserve = (ctx != NULL && ctx->vault != NULL &&
                            kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam) ?
                               (4U * 1024U) :
                               FLIPPASS_OPEN_SAFETY_RESERVE_BYTES;
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
#if !FLIPPASS_ENABLE_LOGS
    UNUSED(stage);
    UNUSED(request_size);
#endif
    DialogsApp* dialogs = NULL;
    DialogMessage* message = NULL;
    DialogMessageButton button = DialogMessageButtonBack;

    if(ctx == NULL || ctx->app == NULL) {
        return false;
    }

    FLIPPASS_LOG_EVENT(
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
        FLIPPASS_LOG_EVENT(ctx->app, "DEBUG_AUTO_CONTINUE_EXT");
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
        button = dialog_message_show(dialogs, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
    }

    if(button != DialogMessageButtonRight) {
        FLIPPASS_LOG_EVENT(ctx->app, "VAULT_FALLBACK_REPLY choice=cancel");
        if(error != NULL) {
            furi_string_set_str(error, "Unlock canceled.");
        }
        return false;
    }

    FLIPPASS_LOG_EVENT(ctx->app, "VAULT_FALLBACK_REPLY choice=continue");
    ctx->allow_ext_promotion = true;
    ctx->app->allow_ext_vault_promotion = true;
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
    FLIPPASS_LOG_EVENT(
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
    FLIPPASS_LOG_EVENT(
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

static bool
    flippass_open_builder_prepare_field_writer(FlipPassOpenBuilderContext* ctx, FuriString* error) {
    const bool keep_stream = flippass_open_builder_field_writer_uses_file_stream(ctx);

    furi_assert(ctx);

    if(ctx->field_writer.pending != NULL && ctx->field_writer.vault == ctx->vault &&
       !ctx->field_writer.failed && (!keep_stream || ctx->field_writer.stream_file_mode)) {
        return true;
    }

    if(ctx->field_writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->field_writer);
    }

    kdbx_vault_writer_reset(&ctx->field_writer, ctx->vault);
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

static bool flippass_open_builder_prepare_for_arena_alloc(
    FlipPassOpenBuilderContext* ctx,
    FuriString* error,
    const char* stage,
    size_t request_size) {
    size_t predicted = request_size;

    furi_assert(ctx);

    flippass_open_builder_refresh_budget(ctx);
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
            return false;
        }
    }

    return flippass_open_builder_promote_to_ext(ctx, error);
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
            return false;
        }
    }

    if(kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam) {
        return flippass_open_builder_promote_to_ext(ctx, error);
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
        if(!flippass_open_builder_prepare_field_writer(ctx, error)) {
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
                (unsigned long)out_ref->record_count);
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
    if(flippass_open_builder_should_promote(ctx, KDBX_VAULT_RECORD_PLAIN_MAX)) {
        if(!ctx->allow_ext_promotion) {
            if(!flippass_open_builder_offer_fallback(
                   ctx, error, key, KDBX_VAULT_RECORD_PLAIN_MAX)) {
                return false;
            }
        }
        if(!flippass_open_builder_promote_to_ext(ctx, error)) {
            return false;
        }
    }

    if(!flippass_open_builder_prepare_field_writer(ctx, error)) {
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
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    ctx->deferred_stream_active = false;

    if(strcmp(key, "UUID") == 0) {
        if(!kdbx_entry_set_uuid_ref(ctx->current_entry, &ref)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "UserName") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldUsername, &ref)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "Password") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldPassword, &ref)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "URL") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldUrl, &ref)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "Notes") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldNotes, &ref)) {
            if(error != NULL) {
                furi_string_set_str(
                    error, "Not enough RAM is available to keep this database open.");
            }
            return false;
        }
    } else if(strcmp(key, "AutoType") == 0) {
        if(!kdbx_entry_set_field_ref(ctx->current_entry, KDBXEntryFieldAutotype, &ref)) {
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
    const size_t free_heap = memmgr_get_free_heap();
    if(free_heap <= FLIPPASS_OPEN_SAFETY_RESERVE_BYTES) {
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to unlock this database.");
        }
        return false;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->allow_ext_promotion = allow_ext_promotion;
    ctx->commit_limit = free_heap - FLIPPASS_OPEN_SAFETY_RESERVE_BYTES;
    ctx->arena =
        kdbx_arena_alloc(FLIPPASS_OPEN_ARENA_CHUNK_SIZE, &ctx->committed_bytes, ctx->commit_limit);
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

    FLIPPASS_LOG_EVENT(
        app,
        "VAULT_MODE backend=%s free=%lu max=%lu limit=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->commit_limit);

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
    if(ctx->vault != NULL) {
        kdbx_vault_free(ctx->vault);
    }
    if(ctx->arena != NULL) {
        kdbx_arena_free(ctx->arena);
    }

    App* app = ctx->app;
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
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
        if(error != NULL) {
            furi_string_set_str(error, "Not enough RAM is available to keep this database open.");
        }
        return false;
    }

    entry->next = ctx->current_group->entries;
    ctx->current_group->entries = entry;
    ctx->current_entry = entry;
    ctx->entry_count++;
    return true;
}

static bool flippass_open_builder_end_entry(void* context, FuriString* error) {
    FlipPassOpenBuilderContext* ctx = context;
    UNUSED(error);
    furi_assert(ctx);
    ctx->current_entry = NULL;
    return true;
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
    furi_assert(ctx);

    if(!flippass_open_builder_prepare_for_arena_alloc(ctx, error, "entry_title", value_len + 1U)) {
        return false;
    }
    if(ctx->vault_promotion_attempted) {
        FLIPPASS_LOG_EVENT(
            ctx->app,
            "ENTRY_TITLE_AFTER_PROMOTE backend=%s len=%lu",
            kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)),
            (unsigned long)value_len);
    }

    if(ctx->current_entry == NULL ||
       !kdbx_entry_set_title(ctx->current_entry, ctx->arena, value)) {
        if(error != NULL) {
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
#if !FLIPPASS_ENABLE_LOGS
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

    FLIPPASS_LOG_EVENT(
        ctx->app,
        "PARSE_OK groups=%lu entries=%lu",
        (unsigned long)group_count,
        (unsigned long)entry_count);
    FLIPPASS_LOG_EVENT(ctx->app, "DATABASE_READY");

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
    const FlipPassOpenInflateHostApiV1 inflate_host_api = {
        .api_version = FLIPPASS_OPEN_INFLATE_HOST_API_VERSION,
        .context = session,
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
    };

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
        flippass_open_host_clear_paged_window_crypto(session);
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

    flippass_open_host_clear_paged_window_crypto(session);
    flippass_module_unload(app, slot);
    FLIPPASS_MEMORY_LOG(
        app,
        kind == FlipPassOpenInflateKindNonPaged ? "open_inflate_nonpaged_unloaded" :
                                                  "open_inflate_paged_unloaded",
        flippass_open_theoretical_session_bytes());
    flippass_open_host_clear_staged_payload(session);
    return true;
}

bool flippass_open_execute(App* app, FuriString* error) {
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
    KDBXVault* resume_scratch_vault = app->pending_gzip_scratch_vault;
    KDBXFieldRef resume_scratch_ref = app->pending_gzip_scratch_ref;
    size_t resume_scratch_plain_size = app->pending_gzip_plain_size;
    const bool resume_from_staged_xml = resume_scratch_vault != NULL && allow_ext_promotion;
    uint8_t resume_save_key[32];
    uint64_t resume_kdf_rounds = app->database_kdf_rounds;
    FlipPassKdbxCipher resume_cipher = app->database_cipher;
    uint32_t resume_compression = app->database_compression;
    bool resume_save_key_ready = false;
    bool ok = false;
    bool trace_capture_suspended = false;

    furi_assert(app);
    furi_assert(error);

    FLIPPASS_MEMORY_LOG(app, "open_begin", flippass_open_theoretical_session_bytes());

    if(!resume_from_staged_xml && app->master_password[0] == '\0') {
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
    session->resume_from_staged_xml = resume_from_staged_xml;
    memzero(resume_save_key, sizeof(resume_save_key));
    if(resume_from_staged_xml) {
        resume_save_key_ready = flippass_session_copy_save_key(app, resume_save_key);
    }

    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;

    load_error = furi_string_alloc();
    FLIPPASS_MEMORY_LOG(
        app, "open_load_error_allocated", flippass_open_theoretical_session_bytes());
    flippass_open_trim_runtime_modules(app);
    if(!resume_from_staged_xml) {
        FLIPPASS_MEMORY_LOG(
            app, "open_acquire_load_before", flippass_open_theoretical_acquire_bytes());
        acquire_plugin = flippass_open_acquire_plugin_get(app, load_error);
        FLIPPASS_MEMORY_LOG(
            app, "open_acquire_load_after", flippass_open_theoretical_acquire_bytes());
        if(acquire_plugin == NULL) {
            furi_string_set(error, load_error);
            goto cleanup;
        }
    }

    FLIPPASS_LOG_EVENT(app, "UNLOCK_START");
    flippass_system_log_capture_suspend();
    trace_capture_suspended = true;

    flippass_reset_database(app);
    app->requested_vault_backend = requested_backend;
    app->allow_ext_vault_promotion = allow_ext_promotion;
    FLIPPASS_MEMORY_LOG(app, "open_after_reset", flippass_open_theoretical_session_bytes());
    if(resume_from_staged_xml) {
        app->database_cipher = resume_cipher;
        app->database_compression = resume_compression;
        app->database_kdf_rounds = resume_kdf_rounds;
        if(resume_save_key_ready) {
            if(!flippass_session_store_save_key(app, resume_save_key)) {
                furi_string_set_str(error, "Unable to protect the resumed database credential.");
                goto cleanup;
            }
        }
    }
    if(resume_from_staged_xml) {
        session->xml_scratch.vault = resume_scratch_vault;
        session->xml_scratch.ref = resume_scratch_ref;
        session->xml_scratch.size = resume_scratch_plain_size;
        resume_scratch_vault = NULL;
        memset(&resume_scratch_ref, 0, sizeof(resume_scratch_ref));
        resume_scratch_plain_size = 0U;
        FLIPPASS_LOG_EVENT(
            app,
            "GZIP_STAGE_RESUME bytes=%lu records=%lu",
            (unsigned long)session->xml_scratch.size,
            (unsigned long)session->xml_scratch.ref.record_count);
    }

    if(!resume_from_staged_xml) {
        const FlipPassOpenAcquireRequestV1 acquire_request = {
            .api_version = FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION,
            .file_path = furi_string_get_cstr(app->file_path),
            .password = app->master_password,
        };
        const FlipPassOpenAcquireHostApiV1 acquire_host_api = {
            .api_version = FLIPPASS_OPEN_ACQUIRE_HOST_API_VERSION,
            .context = session,
            .progress = flippass_open_host_progress,
            .log = flippass_open_host_log,
        };

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
            if(!flippass_session_store_save_key(app, session->open_profile.composite_key)) {
                furi_string_set_str(error, "Unable to protect the database credential.");
                goto cleanup;
            }
        }
        flippass_module_unload(app, FlipPassModuleSlotOpenAcquire);
        acquire_plugin = NULL;
        FLIPPASS_MEMORY_LOG(
            app, "open_acquire_unloaded", flippass_open_theoretical_session_bytes());
    }

    if(!resume_from_staged_xml) {
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
        const FlipPassOpenStreamHostApiV1 stream_host_api = {
            .api_version = FLIPPASS_OPEN_STREAM_HOST_API_VERSION,
            .context = session,
            .progress = flippass_open_host_progress,
            .log = flippass_open_host_log,
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

        if(!stream_plugin->run(&stream_request, &stream_host_api, &stream_result, error)) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "Unable to prepare the staged open payload.");
            }
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

    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_load_before",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    model_plugin = flippass_open_model_plugin_get(app, load_error);
    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_load_after",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    if(model_plugin == NULL) {
        furi_string_set(error, load_error);
        goto cleanup;
    }

    const FlipPassOpenModelRequestV1 model_request = {
        .api_version = FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION,
        .requested_backend = requested_backend,
        .allow_ext_promotion = allow_ext_promotion,
        .staged_payload_plain_size = session->xml_scratch.size,
    };
    const FlipPassOpenModelHostApiV1 model_host_api = {
        .api_version = FLIPPASS_OPEN_MODEL_HOST_API_VERSION,
        .context = session,
        .progress = flippass_open_host_progress,
        .log = flippass_open_host_log,
        .stream_staged_xml = flippass_open_host_stream_staged_xml,
        .derive_protected_stream_material = flippass_open_host_derive_protected_stream_material,
    };
    const FlipPassOpenBuilderApiV1 builder_api = {
        .api_version = FLIPPASS_OPEN_MODEL_BUILDER_API_VERSION,
        .context = &session->builder,
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

    FLIPPASS_MEMORY_LOG(
        app,
        "open_model_run_begin",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    ok = model_plugin->run(&model_request, &model_host_api, &builder_api, error);
    FLIPPASS_MEMORY_LOG(
        app,
        ok ? "open_model_run_ok" : "open_model_run_fail",
        flippass_open_theoretical_model_bytes(session->xml_scratch.size));
    if(!ok) {
        flippass_open_builder_cancel_session(&session->builder);
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to unlock the selected database.");
        }
        FLIPPASS_LOG_EVENT(app, "PARSE_FAIL reason=%s", furi_string_get_cstr(error));
    } else {
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
                if(!flippass_session_store_save_key(app, session->open_profile.composite_key)) {
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
        flippass_clear_master_password(app);
        flippass_progress_update(app, "Ready", "", 100U);
    }

cleanup:
    if(!ok && session != NULL && app->pending_vault_fallback &&
       session->xml_scratch.vault != NULL && !kdbx_vault_ref_is_empty(&session->xml_scratch.ref)) {
        app->pending_gzip_scratch_vault = session->xml_scratch.vault;
        app->pending_gzip_scratch_ref = session->xml_scratch.ref;
        app->pending_gzip_plain_size = session->xml_scratch.size;
        session->xml_scratch.vault = NULL;
        memset(&session->xml_scratch.ref, 0, sizeof(session->xml_scratch.ref));
        session->xml_scratch.size = 0U;
        FLIPPASS_LOG_EVENT(
            app,
            "GZIP_STAGE_RESUME_CACHE bytes=%lu records=%lu",
            (unsigned long)app->pending_gzip_plain_size,
            (unsigned long)app->pending_gzip_scratch_ref.record_count);
    }
    if(resume_scratch_vault != NULL) {
        kdbx_vault_free(resume_scratch_vault);
    }
    memzero(resume_save_key, sizeof(resume_save_key));
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
        flippass_record_successful_open(app);
    }
    return ok;
}
