#include "flippass.h"
#include "flippass_crypto.h"
#include "flippass_db.h"
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
#include "kdbx/aes.h"
#endif
#include "kdbx/kdbx_constants.h"
#include "kdbx/memzero.h"
#include "plugins/flippass_save_plugin.h"
#include "plugins/flippass_save_serial_plugin.h"
#include "plugins/flippass_save_host_plugin.h"
#include "scenes/flippass_scene.h"
#include "scenes/flippass_scene_db_entries.h"
#include "scenes/flippass_scene_editor.h"

#include <storage/storage.h>

#include <stdlib.h>
#include <string.h>

#define FLIPPASS_SAVE_DATABASE_NAME_SIZE 64U
#define FLIPPASS_SAVE_BLOCK_SIZE         512U

#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
typedef struct {
    Storage* storage;
    File* file;
    FuriString* temp_path;
    const char* target_path;
    uint8_t hmac_base[64];
    uint8_t block_buffer[FLIPPASS_SAVE_BLOCK_SIZE];
    size_t block_len;
    uint64_t block_index;
    aes_encrypt_ctx aes_ctx;
    uint8_t aes_iv[16];
    uint8_t aes_pending[16];
    size_t aes_pending_len;
    uint32_t payload_plain_total;
    uint32_t payload_plain_next_log;
    bool payload_active;
    bool payload_committed;
} FlipPassSaveHostPayload;
#endif

/* Encrypted vault scratch used to buffer serialized XML between the serial
 * and payload stages.  Lives only for the duration of save execution. */
typedef struct {
    KDBXVault* vault;
    KDBXVaultWriter writer;
    KDBXFieldRef xml_ref;
    bool xml_ready;
} FlipPassSaveXmlScratch;

typedef struct {
    App* app;
    FlipPassSaveXmlScratch* xml_scratch;
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    FlipPassSaveHostPayload* payload;
#endif
} FlipPassSaveHostContext;

static uint64_t flippass_save_normalize_kdf_rounds(uint64_t rounds) {
#if FLIPPASS_SAVE_ALLOW_LOW_KDF
    if(rounds == 0U) {
        return FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    }
    return rounds;
#else
    if(rounds < FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS) {
        return FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    }

    return rounds;
#endif
}

#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static void flippass_save_write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
    out[2] = (uint8_t)((value >> 16) & 0xFFU);
    out[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void flippass_save_write_u64_le(uint8_t* out, uint64_t value) {
    for(size_t index = 0U; index < 8U; index++) {
        out[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static bool flippass_save_host_compute_block_hmac(
    const uint8_t* hmac_base,
    size_t hmac_base_size,
    uint64_t block_index,
    const uint8_t* data,
    size_t data_size,
    uint8_t out[32]) {
    uint8_t block_key[64];
    uint8_t input[72];
    uint8_t index_bytes[8];
    uint8_t size_bytes[4];
    bool ok = false;

    if(hmac_base == NULL || hmac_base_size != 64U || data == NULL || data_size > 512U ||
       out == NULL) {
        return false;
    }

    flippass_save_write_u64_le(input, block_index);
    memcpy(input + 8U, hmac_base, hmac_base_size);
    flippass_save_write_u64_le(index_bytes, block_index);
    flippass_save_write_u32_le(size_bytes, (uint32_t)data_size);

    const FlipPassCryptoChunk hmac_chunks[] = {
        {.data = index_bytes, .size = sizeof(index_bytes)},
        {.data = size_bytes, .size = sizeof(size_bytes)},
        {.data = data, .size = data_size},
    };
    ok = flippass_crypto_sha512_raw(input, sizeof(input), block_key) &&
         flippass_crypto_hmac_sha256_chunks(
             block_key, sizeof(block_key), hmac_chunks, COUNT_OF(hmac_chunks), out);

    memzero(block_key, sizeof(block_key));
    memzero(input, sizeof(input));
    return ok;
}
#endif

static void flippass_save_extract_database_name(const char* path, char* out, size_t out_size) {
    const char* segment = path;
    const char* end = NULL;

    furi_assert(path);
    furi_assert(out);
    furi_assert(out_size > 0U);

    for(const char* cursor = path; *cursor != '\0'; cursor++) {
        if(*cursor == '/' || *cursor == '\\') {
            segment = cursor + 1;
        }
    }

    end = segment + strlen(segment);
    for(const char* cursor = end; cursor > segment; cursor--) {
        if(cursor[-1] == '.') {
            end = cursor - 1;
            break;
        }
    }

    size_t length = (end > segment) ? (size_t)(end - segment) : 0U;
    if(length == 0U) {
        segment = "Database";
        length = strlen(segment);
    }

    if(length >= out_size) {
        length = out_size - 1U;
    }
    memcpy(out, segment, length);
    out[length] = '\0';
}

static void flippass_save_host_progress(
    void* context,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    FlipPassSaveHostContext* host = context;
    if(host == NULL || host->app == NULL) {
        return;
    }

    flippass_progress_update(host->app, stage, detail, percent);
}

static void flippass_save_host_log(void* context, const char* message) {
    FlipPassSaveHostContext* host = context;
    if(host != NULL && host->app != NULL && message != NULL && message[0] != '\0') {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(host->app, "%s", message);
    }
}

static bool flippass_save_host_copy_group_uuid(
    void* context,
    const KDBXGroup* group,
    FuriString* out,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_db_copy_group_uuid(host->app, group, out, error);
}

static bool flippass_save_host_copy_entry_uuid(
    void* context,
    const KDBXEntry* entry,
    FuriString* out,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_db_copy_entry_uuid(host->app, entry, out, error);
}

static bool
    flippass_save_host_entry_has_field(void* context, const KDBXEntry* entry, uint32_t field_mask) {
    FlipPassSaveHostContext* host = context;
    UNUSED(host);
    return flippass_db_entry_has_field(entry, field_mask);
}

static bool flippass_save_host_stream_ref(
    void* context,
    const KDBXFieldRef* ref,
    KDBXVaultChunkCallback callback,
    void* callback_context,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;

    if(host == NULL || host->app == NULL || host->app->vault == NULL || ref == NULL ||
       callback == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault is not available.");
        }
        return false;
    }

    if(kdbx_vault_ref_is_empty(ref)) {
        return callback(NULL, 0U, callback_context);
    }

    KDBXVaultReader reader;
    uint8_t chunk[128];
    bool ok = true;
    kdbx_vault_reader_reset(&reader, host->app->vault, ref);

    while(ok) {
        size_t chunk_size = 0U;
        if(!kdbx_vault_reader_read(&reader, chunk, sizeof(chunk), &chunk_size)) {
            ok = false;
            break;
        }
        if(chunk_size == 0U) {
            break;
        }
        if(!callback(chunk, chunk_size, callback_context)) {
            ok = false;
            break;
        }
        memzero(chunk, sizeof(chunk));
    }

    memzero(chunk, sizeof(chunk));
    memzero(&reader, sizeof(reader));

    if(!ok) {
        if(error != NULL && furi_string_empty(error)) {
            const char* failure = kdbx_vault_failure_reason(host->app->vault);
            const char* reader_failure = kdbx_vault_last_reader_failure(host->app->vault);
            if(reader_failure != NULL && reader_failure[0] != '\0') {
                furi_string_printf(
                    error,
                    "The encrypted session vault could not stream a field (%s).",
                    reader_failure);
            } else {
                furi_string_set_str(
                    error,
                    failure != NULL ? failure : "The encrypted session vault could not be read.");
            }
        }
        return false;
    }

    return true;
}

#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static bool flippass_save_host_file_write(
    FlipPassSaveHostPayload* payload,
    const void* data,
    size_t data_size) {
    if(data_size == 0U) {
        return true;
    }

    return payload != NULL && payload->file != NULL &&
           storage_file_write(payload->file, data, data_size) == data_size;
}

static bool flippass_save_host_flush_block(
    FlipPassSaveHostPayload* payload,
    bool force_empty,
    FuriString* error) {
    uint8_t block_hmac[32];
    uint8_t size_bytes[4];
    bool ok = false;

    if(payload == NULL || (!force_empty && payload->block_len == 0U)) {
        return true;
    }

    flippass_save_write_u32_le(size_bytes, (uint32_t)payload->block_len);
    if(!flippass_save_host_compute_block_hmac(
           payload->hmac_base,
           sizeof(payload->hmac_base),
           payload->block_index,
           payload->block_buffer,
           payload->block_len,
           block_hmac)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not derive a KDBX block HMAC.");
        }
        memzero(block_hmac, sizeof(block_hmac));
        return false;
    }

    ok = flippass_save_host_file_write(payload, block_hmac, sizeof(block_hmac)) &&
         flippass_save_host_file_write(payload, size_bytes, sizeof(size_bytes)) &&
         flippass_save_host_file_write(payload, payload->block_buffer, payload->block_len);

    memzero(block_hmac, sizeof(block_hmac));
    if(!ok) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not write the target KDBX file.");
        }
        return false;
    }

    payload->block_len = 0U;
    payload->block_index++;
    return true;
}

static bool flippass_save_host_emit_ciphertext(
    FlipPassSaveHostPayload* payload,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    size_t offset = 0U;

    while(offset < data_size) {
        const size_t available = sizeof(payload->block_buffer) - payload->block_len;
        const size_t chunk = ((data_size - offset) > available) ? available : (data_size - offset);

        memcpy(payload->block_buffer + payload->block_len, data + offset, chunk);
        payload->block_len += chunk;
        offset += chunk;

        if(payload->block_len == sizeof(payload->block_buffer) &&
           !flippass_save_host_flush_block(payload, false, error)) {
            return false;
        }
    }

    return true;
}

static void flippass_save_host_payload_close(FlipPassSaveHostPayload* payload, bool remove_temp) {
    if(payload == NULL) {
        return;
    }

    if(payload->file != NULL) {
        storage_file_close(payload->file);
        storage_file_free(payload->file);
        payload->file = NULL;
    }
    if(remove_temp && payload->storage != NULL && payload->temp_path != NULL &&
       !furi_string_empty(payload->temp_path)) {
        storage_simply_remove(payload->storage, furi_string_get_cstr(payload->temp_path));
    }
    if(payload->temp_path != NULL) {
        furi_string_free(payload->temp_path);
        payload->temp_path = NULL;
    }
    if(payload->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
        payload->storage = NULL;
    }

    memzero(payload->hmac_base, sizeof(payload->hmac_base));
    memzero(payload->block_buffer, sizeof(payload->block_buffer));
    memzero(&payload->aes_ctx, sizeof(payload->aes_ctx));
    memzero(payload->aes_iv, sizeof(payload->aes_iv));
    memzero(payload->aes_pending, sizeof(payload->aes_pending));
    payload->block_len = 0U;
    payload->block_index = 0U;
    payload->aes_pending_len = 0U;
    payload->target_path = NULL;
    payload->payload_active = false;
}

static bool flippass_save_host_begin_payload(
    void* context,
    const FlipPassSaveRequestV1* request,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    FlipPassSaveHostPayload* payload = host != NULL ? host->payload : NULL;
    uint64_t file_size = 0U;

    if(payload == NULL || request == NULL || request->file_path == NULL ||
       request->cipher != FlipPassSaveCipherAes256 || request->cipher_key == NULL ||
       request->cipher_key_size != 32U || request->hmac_base == NULL ||
       request->hmac_base_size != 64U || request->iv == NULL || request->iv_size != 16U) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid host payload request.");
        }
        return false;
    }

    flippass_save_host_payload_close(payload, false);
    payload->storage = furi_record_open(RECORD_STORAGE);
    payload->target_path = request->file_path;
    payload->temp_path = furi_string_alloc_printf("%s.tmp", request->file_path);
    payload->file = payload->storage != NULL ? storage_file_alloc(payload->storage) : NULL;
    if(payload->storage == NULL || payload->temp_path == NULL || payload->file == NULL ||
       !storage_file_open(
           payload->file,
           furi_string_get_cstr(payload->temp_path),
           FSAM_WRITE,
           FSOM_OPEN_EXISTING)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not open the temporary KDBX file.");
        }
        flippass_save_host_payload_close(payload, true);
        return false;
    }

    file_size = storage_file_size(payload->file);
    if(file_size > (uint64_t)((uint32_t)-1) ||
       !storage_file_seek(payload->file, file_size, true)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not append to the temporary KDBX file.");
        }
        flippass_save_host_payload_close(payload, true);
        return false;
    }

    memcpy(payload->hmac_base, request->hmac_base, sizeof(payload->hmac_base));
    if(aes_encrypt_key256(request->cipher_key, &payload->aes_ctx) != EXIT_SUCCESS) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not initialize AES-256 for save.");
        }
        flippass_save_host_payload_close(payload, true);
        return false;
    }
    memcpy(payload->aes_iv, request->iv, sizeof(payload->aes_iv));
    payload->payload_plain_total = 0U;
    payload->payload_plain_next_log = 4096U;
    payload->payload_active = true;
    payload->payload_committed = false;
    if(FLIPPASS_SAVE_VERBOSE_PROBE && host != NULL && host->app != NULL) {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            host->app,
            "SAVE_HOST_PAYLOAD_BEGIN_OK free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }
    return true;
}

static bool flippass_save_host_write_payload_plain(
    void* context,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    FlipPassSaveHostPayload* payload = host != NULL ? host->payload : NULL;

    if(payload == NULL || !payload->payload_active || (data == NULL && data_size > 0U)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass save payload sink is not ready.");
        }
        return false;
    }

    if(data_size > 0U) {
        const uint32_t increment =
            (data_size > (size_t)(0xFFFFFFFFUL - payload->payload_plain_total)) ?
                (0xFFFFFFFFUL - payload->payload_plain_total) :
                (uint32_t)data_size;
        payload->payload_plain_total += increment;
        if(FLIPPASS_SAVE_VERBOSE_PROBE && host->app != NULL &&
           (payload->payload_plain_total == increment ||
            payload->payload_plain_total >= payload->payload_plain_next_log)) {
            FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
                host->app,
                "SAVE_HOST_PAYLOAD_WRITE plain=%lu free=%lu max=%lu",
                (unsigned long)payload->payload_plain_total,
                (unsigned long)memmgr_get_free_heap(),
                (unsigned long)memmgr_heap_get_max_free_block());
            while(payload->payload_plain_next_log <= payload->payload_plain_total &&
                  payload->payload_plain_next_log <= (0xFFFFFFFFUL - 4096UL)) {
                payload->payload_plain_next_log += 4096U;
            }
        }
    }

    for(size_t index = 0U; index < data_size; index++) {
        payload->aes_pending[payload->aes_pending_len++] = data[index];
        if(payload->aes_pending_len == sizeof(payload->aes_pending)) {
            if(aes_cbc_encrypt(
                   payload->aes_pending,
                   payload->aes_pending,
                   sizeof(payload->aes_pending),
                   payload->aes_iv,
                   &payload->aes_ctx) != EXIT_SUCCESS ||
               !flippass_save_host_emit_ciphertext(
                   payload, payload->aes_pending, sizeof(payload->aes_pending), error)) {
                if(error != NULL) {
                    furi_string_set_str(error, "FlipPass could not encrypt the KDBX payload.");
                }
                return false;
            }
            payload->aes_pending_len = 0U;
        }
    }

    return true;
}

static bool flippass_save_host_finish_payload(void* context, FuriString* error) {
    FlipPassSaveHostContext* host = context;
    FlipPassSaveHostPayload* payload = host != NULL ? host->payload : NULL;
    bool ok = false;

    if(payload == NULL || !payload->payload_active || payload->temp_path == NULL ||
       payload->file == NULL || host->app == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass save payload sink is not ready.");
        }
        return false;
    }

    if(FLIPPASS_SAVE_VERBOSE_PROBE) {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            host->app,
            "SAVE_HOST_PAYLOAD_FINISH_BEGIN plain=%lu free=%lu max=%lu",
            (unsigned long)payload->payload_plain_total,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    const uint8_t pad = (uint8_t)(sizeof(payload->aes_pending) - payload->aes_pending_len);
    while(payload->aes_pending_len < sizeof(payload->aes_pending)) {
        payload->aes_pending[payload->aes_pending_len++] = pad;
    }
    if(aes_cbc_encrypt(
           payload->aes_pending,
           payload->aes_pending,
           sizeof(payload->aes_pending),
           payload->aes_iv,
           &payload->aes_ctx) != EXIT_SUCCESS ||
       !flippass_save_host_emit_ciphertext(
           payload, payload->aes_pending, sizeof(payload->aes_pending), error) ||
       !flippass_save_host_flush_block(payload, false, error) ||
       !flippass_save_host_flush_block(payload, true, error)) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass could not finalize AES padding.");
        }
        flippass_save_host_payload_close(payload, true);
        return false;
    }

    storage_file_sync(payload->file);
    storage_file_close(payload->file);
    storage_file_free(payload->file);
    payload->file = NULL;

    storage_simply_remove(payload->storage, payload->target_path);
    ok = storage_common_rename(
             payload->storage, furi_string_get_cstr(payload->temp_path), payload->target_path) ==
         FSE_OK;
    if(!ok) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass could not replace the target KDBX file.");
        }
        flippass_save_host_payload_close(payload, true);
        return false;
    }

    payload->payload_committed = true;
    flippass_save_host_payload_close(payload, false);
    if(FLIPPASS_SAVE_VERBOSE_PROBE) {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            host->app,
            "SAVE_HOST_PAYLOAD_FINISH_OK free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }
    return true;
}
#endif

/* ---- XML scratch vault callbacks ---- */

static bool flippass_save_host_write_xml_chunk(
    void* context,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    FlipPassSaveXmlScratch* scratch = (host != NULL) ? host->xml_scratch : NULL;

    if(scratch == NULL || scratch->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass XML scratch vault is not ready.");
        }
        return false;
    }

    if(data_size == 0U) {
        return true;
    }

    if(!kdbx_vault_writer_write(&scratch->writer, data, data_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass XML scratch write failed.");
        }
        return false;
    }

    return true;
}

static bool flippass_save_host_stream_xml(
    void* context,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    FlipPassSaveXmlScratch* scratch = (host != NULL) ? host->xml_scratch : NULL;

    if(scratch == NULL || !scratch->xml_ready || scratch->vault == NULL || callback == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass XML scratch is not ready for streaming.");
        }
        return false;
    }

    if(!kdbx_vault_stream_ref(scratch->vault, &scratch->xml_ref, callback, callback_context)) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass could not stream serialized XML from scratch.");
        }
        return false;
    }

    return true;
}

static void flippass_save_remove_temp_file(const char* target_path) {
    Storage* storage = NULL;
    FuriString* temp_path = NULL;

    if(target_path == NULL) {
        return;
    }

    storage = furi_record_open(RECORD_STORAGE);
    temp_path = furi_string_alloc_printf("%s.tmp", target_path);
    if(storage != NULL && temp_path != NULL) {
        storage_simply_remove(storage, furi_string_get_cstr(temp_path));
    }
    if(temp_path != NULL) {
        furi_string_free(temp_path);
    }
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
}

static void flippass_save_unload_runtime_modules(App* app) {
    /* Mirrors flippass_open_trim_runtime_modules. The shared part is
     * flippass_module_unload_all_runtime(), which iterates every slot
     * including SaveHeader, Otp, RpcCommands, OutputUsb, and OutputBle.
     * The save path additionally deactivates the active entry and clears
     * the title cache, because save runs against an already-unlocked
     * database. Do not "unify" with the open trim helper. */
    flippass_db_deactivate_entry(app);
    flippass_db_browser_clear_title_cache(app);
    FLIPPASS_MEMORY_LOG(app, "save_after_entry_deactivate", 0U);
    flippass_output_cleanup(app);
    flippass_module_unload_all_runtime(app);
}

static void flippass_save_trim_ui_for_save(App* app) {
    const uint32_t current_scene = scene_manager_get_current_scene(app->scene_manager);

    FLIPPASS_MEMORY_LOG(app, "save_before_ui_trim", 0U);
    if(current_scene == FlipPassScene_DbEntries) {
        flippass_scene_db_entries_trim_for_save(app);
    } else if(current_scene == FlipPassScene_Editor) {
        flippass_scene_editor_trim_for_save(app);
    }
    FLIPPASS_MEMORY_LOG(app, "save_after_ui_trim", 0U);
}

bool flippass_save_host_current_database(
    App* app,
    const char* target_path,
    const char* password,
    FuriString* error) {
    furi_assert(app);

    FLIPPASS_LOG_EVENT(
        app,
        "SAVE_REQUEST cipher=%lu compression=%lu kdf_rounds=%lu",
        (unsigned long)app->database_cipher,
        (unsigned long)app->database_compression,
        (unsigned long)app->database_kdf_rounds);
#if FLIPPASS_SAVE_VERBOSE_PROBE
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_REQUEST cipher=%lu compression=%lu kdf_rounds=%lu",
        (unsigned long)app->database_cipher,
        (unsigned long)app->database_compression,
        (unsigned long)app->database_kdf_rounds);
#endif
    flippass_save_trim_ui_for_save(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_VIEW_UNLOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    flippass_save_unload_idle_views(app);
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_AFTER_VIEW_UNLOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    const bool ok = flippass_save_host_execute(
        app,
        target_path,
        password,
        app->database_cipher,
        app->database_compression,
        app->database_kdf_rounds,
        error);
    flippass_save_restore_idle_views(app);
    flippass_progress_reset(app);
    return ok;
}

bool flippass_save_host_execute(
    App* app,
    const char* target_path,
    const char* password,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    uint64_t kdf_rounds,
    FuriString* error) {
    const FlipperAppPluginDescriptor* header_descriptor = NULL;
    const FlipperAppPluginDescriptor* serial_descriptor = NULL;
    const FlipperAppPluginDescriptor* payload_descriptor = NULL;
    const FlipPassSaveHeaderPluginV1* header_plugin = NULL;
    const FlipPassSaveSerialPluginV1* serial_plugin = NULL;
    const FlipPassSavePluginV1* payload_plugin = NULL;
    uint8_t save_key[32];
    uint8_t transformed_key[32];
    uint8_t kdf_salt[32];
    uint64_t transformed_kdf_rounds = 0U;
    bool transformed_key_ready = false;
    FlipPassSaveHeaderResultV1 header_result;
    char database_name[FLIPPASS_SAVE_DATABASE_NAME_SIZE];
    FlipPassSaveXmlScratch xml_scratch;
    FlipPassSaveHostContext host;
    const FlipPassCryptoApiV1* save_crypto_api = NULL;
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    FlipPassSaveHostPayload* host_payload = NULL;
#endif
    bool ok = false;
    bool header_temp_created = false;
    FuriString* load_error = furi_string_alloc();

    furi_assert(app);
    furi_assert(target_path);
    furi_assert(error);

    FLIPPASS_MEMORY_LOG(
        app,
        "save_execute_begin",
        sizeof(FlipPassSaveHostApiV1) + sizeof(FlipPassSaveStageHostApiV1) + sizeof(save_key) +
            sizeof(header_result) + sizeof(database_name));
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_EXECUTE_BEGIN free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    memzero(save_key, sizeof(save_key));
    memzero(transformed_key, sizeof(transformed_key));
    memzero(kdf_salt, sizeof(kdf_salt));
    memzero(&header_result, sizeof(header_result));
    memzero(database_name, sizeof(database_name));
    memset(&host, 0, sizeof(host));
    memset(&xml_scratch, 0, sizeof(xml_scratch));
    host.app = app;
    host.xml_scratch = &xml_scratch;

    kdf_rounds = flippass_save_normalize_kdf_rounds(kdf_rounds);
    if(password != NULL && password[0] != '\0') {
        FLIPPASS_LOG_EVENT(app, "SAVE_KEY_SOURCE password");
        FLIPPASS_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
#if FLIPPASS_SAVE_VERBOSE_PROBE
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
#endif
        flippass_make_password_composite_key(password, save_key);
    } else if(!flippass_session_copy_save_key(app, save_key)) {
        furi_string_set_str(error, "A save password is required.");
        goto cleanup;
    } else {
        FLIPPASS_LOG_EVENT(app, "SAVE_KEY_SOURCE session");
        transformed_key_ready = flippass_session_copy_save_transformed_key(
            app, transformed_key, kdf_salt, &transformed_kdf_rounds);
        if(transformed_key_ready && transformed_kdf_rounds == kdf_rounds) {
            FLIPPASS_LOG_EVENT(app, "SAVE_KDF_SOURCE session_transformed");
#if FLIPPASS_SAVE_VERBOSE_PROBE
            FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "SAVE_KDF_SOURCE session_transformed");
#endif
        } else {
            transformed_key_ready = false;
            memzero(transformed_key, sizeof(transformed_key));
            memzero(kdf_salt, sizeof(kdf_salt));
            transformed_kdf_rounds = 0U;
            FLIPPASS_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
#if FLIPPASS_SAVE_VERBOSE_PROBE
            FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
#endif
        }
    }

    if(app->root_group == NULL) {
        furi_string_set_str(error, "No editable database is available to save.");
        goto cleanup;
    }

    flippass_save_extract_database_name(target_path, database_name, sizeof(database_name));

    flippass_save_unload_runtime_modules(app);
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_AFTER_RUNTIME_UNLOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    const FlipPassSaveCipher save_cipher = (cipher == FlipPassKdbxCipherChaCha20) ?
                                               FlipPassSaveCipherChaCha20 :
                                               FlipPassSaveCipherAes256;
#if FLIPPASS_SAVE_WRITER_AES_NOCOMP_ONLY
    if(save_cipher != FlipPassSaveCipherAes256 || compression != KDBX_COMPRESSION_NONE) {
        furi_string_set_str(
            error, "This debug build can only save AES-256 databases without compression.");
        goto cleanup;
    }
#endif
    static FlipPassSaveStageHostApiV1 stage_host_api = {
        .api_version = FLIPPASS_SAVE_STAGE_HOST_API_VERSION,
        .progress = flippass_save_host_progress,
        .log = flippass_save_host_log,
    };
    stage_host_api.context = &host;
    /* Writer host API: XML traversal removed (now in serial plugin).
     * stream_xml streams the serialized XML from the host's vault scratch. */
    static FlipPassSaveHostApiV1 host_api = {
        .api_version = FLIPPASS_SAVE_HOST_API_VERSION,
        .progress = flippass_save_host_progress,
        .log = flippass_save_host_log,
        .crypto = NULL,
        .stream_xml = flippass_save_host_stream_xml,
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
        .begin_payload = flippass_save_host_begin_payload,
        .write_payload_plain = flippass_save_host_write_payload_plain,
        .finish_payload = flippass_save_host_finish_payload,
#endif
    };
    host_api.context = &host;
    /* Serial host API: XML traversal callbacks + write_xml_chunk to vault scratch. */
    static FlipPassSaveSerialHostApiV1 serial_host_api = {
        .api_version = FLIPPASS_SAVE_SERIAL_HOST_API_VERSION,
        .progress = flippass_save_host_progress,
        .log = flippass_save_host_log,
        .crypto = NULL,
        .write_xml_chunk = flippass_save_host_write_xml_chunk,
        .stream_ref = flippass_save_host_stream_ref,
        .copy_group_uuid = flippass_save_host_copy_group_uuid,
        .copy_entry_uuid = flippass_save_host_copy_entry_uuid,
        .entry_has_field = flippass_save_host_entry_has_field,
    };
    serial_host_api.context = &host;
    const FlipPassSaveHeaderRequestV1 header_request = {
        .api_version = FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION,
        .file_path = target_path,
        .composite_key = save_key,
        .composite_key_size = sizeof(save_key),
        .transformed_key = transformed_key_ready ? transformed_key : NULL,
        .transformed_key_size = transformed_key_ready ? sizeof(transformed_key) : 0U,
        .kdf_salt = transformed_key_ready ? kdf_salt : NULL,
        .kdf_salt_size = transformed_key_ready ? sizeof(kdf_salt) : 0U,
        .cipher = save_cipher,
        .compression = compression,
        .kdf_rounds = kdf_rounds,
    };

    FLIPPASS_MEMORY_LOG(app, "save_before_header_load", sizeof(header_request));
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_HEADER_LOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    header_descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotSaveHeader,
        NULL,
        FLIPPASS_SAVE_HEADER_PLUGIN_APP_ID,
        FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION,
        load_error);
    if(header_descriptor == NULL || header_descriptor->entry_point == NULL) {
        furi_string_printf(
            error, "FlipPass save header is unavailable: %s", furi_string_get_cstr(load_error));
        goto cleanup;
    }

    header_plugin = header_descriptor->entry_point;
    if(header_plugin->api_version != FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION ||
       header_plugin->run == NULL) {
        furi_string_set_str(error, "FlipPass save header has an incompatible API.");
        goto cleanup;
    }
#if FLIPPASS_SAVE_VERBOSE_PROBE
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_HEADER_READY free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
#endif

    FLIPPASS_MEMORY_LOG(app, "save_before_header_run", sizeof(header_request));
#if FLIPPASS_SAVE_VERBOSE_PROBE
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_HEADER_RUN free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
#endif
    const bool header_ok =
        header_plugin->run(&header_request, &stage_host_api, &header_result, error);
#if FLIPPASS_SAVE_VERBOSE_PROBE
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_AFTER_HEADER_RUN ok=%u free=%lu max=%lu",
        header_ok ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
#endif
    if(!header_ok) {
        goto cleanup;
    }
    header_temp_created = true;
    FLIPPASS_MEMORY_LOG(app, "save_after_header_run", sizeof(header_result));
    flippass_module_unload(app, FlipPassModuleSlotSaveHeader);
    FLIPPASS_MEMORY_LOG(app, "save_after_header_unload", sizeof(header_result));
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_AFTER_HEADER_UNLOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    /* ---- Stage 2: Serial plugin — serialize XML into encrypted vault scratch ---- */

    furi_string_set_str(load_error, "");
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_SERIAL_VAULT free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    /* Create encrypted vault scratch on /ext for the serialized XML. */
    if(!kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        furi_string_set_str(
            error, "FlipPass requires an SD card to save databases (serial XML scratch).");
        goto cleanup;
    }
    xml_scratch.vault = kdbx_vault_alloc_with_path(
        KDBXVaultBackendFileExt, KDBX_VAULT_SAVE_SCRATCH_EXT_PATH, NULL, 0U);
    if(xml_scratch.vault == NULL || kdbx_vault_storage_failed(xml_scratch.vault)) {
        furi_string_set_str(error, "FlipPass could not create the save XML scratch vault.");
        goto cleanup;
    }
    kdbx_vault_writer_reset(&xml_scratch.writer, xml_scratch.vault);
    kdbx_vault_writer_set_file_streaming(&xml_scratch.writer, true);
    if(xml_scratch.writer.failed) {
        furi_string_set_str(error, "FlipPass could not prepare the save XML scratch writer.");
        goto cleanup;
    }
    save_crypto_api = flippass_crypto_get_api();
    if(save_crypto_api == NULL) {
        furi_string_set_str(error, "FlipPass crypto plugin is unavailable.");
        goto cleanup;
    }
    serial_host_api.crypto = save_crypto_api;
    host_api.crypto = save_crypto_api;

    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_SERIAL_LOAD free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    serial_descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotSaveSerial,
        NULL,
        FLIPPASS_SAVE_SERIAL_PLUGIN_APP_ID,
        FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION,
        load_error);
    if(serial_descriptor == NULL || serial_descriptor->entry_point == NULL) {
        furi_string_printf(
            error, "FlipPass save serial is unavailable: %s", furi_string_get_cstr(load_error));
        goto cleanup;
    }
    serial_plugin = serial_descriptor->entry_point;
    if(serial_plugin->api_version != FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION ||
       serial_plugin->run == NULL) {
        furi_string_set_str(error, "FlipPass save serial has an incompatible API.");
        goto cleanup;
    }
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_SERIAL_READY free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    {
        const FlipPassSaveSerialRequestV1 serial_request = {
            .api_version = FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION,
            .root_group = app->root_group,
            .database_name = database_name,
        };
        const bool serial_ok = serial_plugin->run(&serial_request, &serial_host_api, error);
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            app,
            "SAVE_AFTER_SERIAL_RUN ok=%u free=%lu max=%lu",
            serial_ok ? 1U : 0U,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        if(!serial_ok) {
            goto cleanup;
        }
    }

    flippass_module_unload(app, FlipPassModuleSlotSaveSerial);
    serial_plugin = NULL;

    /* Finalize the vault scratch writer so the XML ref is ready for the payload. */
    if(!kdbx_vault_writer_finish(&xml_scratch.writer, &xml_scratch.xml_ref)) {
        furi_string_set_str(error, "FlipPass could not finalize the save XML scratch.");
        goto cleanup;
    }
    xml_scratch.xml_ready = true;

    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_SERIAL_DONE free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    /* ---- Stage 3: Payload writer — read from scratch, apply crypto, write KDBX ---- */

    furi_string_set_str(load_error, "");
    const FlipPassSaveRequestV1 request = {
        .api_version = FLIPPASS_SAVE_PLUGIN_API_VERSION,
        .file_path = target_path,
        .cipher_key = header_result.cipher_key,
        .cipher_key_size = sizeof(header_result.cipher_key),
        .hmac_base = header_result.hmac_base,
        .hmac_base_size = sizeof(header_result.hmac_base),
        .iv = header_result.iv,
        .iv_size = header_result.iv_size,
        .cipher = save_cipher,
        .compression = compression,
    };

    FLIPPASS_MEMORY_LOG(app, "save_before_payload_load", sizeof(request));
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_BEFORE_WRITER_LOAD profile=%s free=%lu max=%lu",
#if FLIPPASS_SAVE_WRITER_AES_NOCOMP_ONLY
        "aes_nocomp_only",
#else
        "full",
#endif
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    payload_descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotSaveWriter,
        NULL,
        FLIPPASS_SAVE_PLUGIN_APP_ID,
        FLIPPASS_SAVE_PLUGIN_API_VERSION,
        load_error);
    if(payload_descriptor == NULL || payload_descriptor->entry_point == NULL) {
        furi_string_printf(
            error, "FlipPass save writer is unavailable: %s", furi_string_get_cstr(load_error));
        goto cleanup;
    }

    payload_plugin = payload_descriptor->entry_point;
    if(payload_plugin->api_version != FLIPPASS_SAVE_PLUGIN_API_VERSION ||
       payload_plugin->run == NULL) {
        furi_string_set_str(error, "FlipPass save writer has an incompatible API.");
        goto cleanup;
    }
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_WRITER_READY free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    FLIPPASS_MEMORY_LOG(app, "save_before_payload_run", sizeof(request));
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    host_payload = malloc(sizeof(FlipPassSaveHostPayload));
    if(host_payload == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for the save payload sink.");
        if(FLIPPASS_SAVE_VERBOSE_PROBE) {
            FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
                app,
                "SAVE_HOST_PAYLOAD_ALLOC_FAIL bytes=%lu free=%lu max=%lu",
                (unsigned long)sizeof(FlipPassSaveHostPayload),
                (unsigned long)memmgr_get_free_heap(),
                (unsigned long)memmgr_heap_get_max_free_block());
        }
        goto cleanup;
    }
    memset(host_payload, 0, sizeof(FlipPassSaveHostPayload));
    host.payload = host_payload;
    if(FLIPPASS_SAVE_VERBOSE_PROBE) {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
            app,
            "SAVE_HOST_PAYLOAD_ALLOC bytes=%lu free=%lu max=%lu",
            (unsigned long)sizeof(FlipPassSaveHostPayload),
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }
#endif
    ok = payload_plugin->run(&request, &host_api, error);
    FLIPPASS_MEMORY_LOG(app, "save_after_payload_run", 0U);
    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "SAVE_AFTER_WRITER_RUN ok=%u free=%lu max=%lu",
        ok ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    flippass_module_unload(app, FlipPassModuleSlotSaveWriter);

    if(ok) {
        if(!flippass_session_store_save_material(
               app,
               save_key,
               header_result.transformed_key_ready ? header_result.transformed_key : NULL,
               header_result.transformed_key_ready ? header_result.kdf_salt : NULL,
               header_result.transformed_key_ready ? kdf_rounds : 0U)) {
            furi_string_set_str(
                error,
                "The database was saved, but the session credential could not be protected.");
            ok = false;
            goto cleanup;
        }
        memzero(save_key, sizeof(save_key));
        furi_string_set_str(app->file_path, target_path);
        app->database_cipher = cipher;
        app->database_compression = compression;
        app->database_kdf_rounds = kdf_rounds;
        flippass_db_mark_clean(app);
        FLIPPASS_MEMORY_LOG(app, "save_before_settings", 0U);
        if(!app->phantom_open) flippass_save_settings(app);
        FLIPPASS_MEMORY_LOG(app, "save_after_settings", 0U);
    }

cleanup:
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    if(host_payload != NULL) {
        flippass_save_host_payload_close(host_payload, !ok);
    }
#endif
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    if(host_payload != NULL) {
        memzero(host_payload, sizeof(FlipPassSaveHostPayload));
        free(host_payload);
        host_payload = NULL;
        host.payload = NULL;
    }
#endif
    if(!ok) {
        flippass_module_unload(app, FlipPassModuleSlotSaveHeader);
        flippass_module_unload(app, FlipPassModuleSlotSaveSerial);
        flippass_module_unload(app, FlipPassModuleSlotSaveWriter);
        if(header_temp_created) {
            flippass_save_remove_temp_file(target_path);
        }
    }
    /* Always clean up the XML scratch vault (encrypted file on /ext). */
    if(xml_scratch.writer.pending != NULL) {
        kdbx_vault_writer_abort(&xml_scratch.writer);
    }
    if(xml_scratch.vault != NULL) {
        kdbx_vault_free(xml_scratch.vault);
        xml_scratch.vault = NULL;
    }
    if(app->module_loader.storage != NULL &&
       kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        storage_simply_remove(app->module_loader.storage, KDBX_VAULT_SAVE_SCRATCH_EXT_PATH);
    }
    memzero(&header_result, sizeof(header_result));
    memzero(save_key, sizeof(save_key));
    memzero(transformed_key, sizeof(transformed_key));
    memzero(kdf_salt, sizeof(kdf_salt));
    furi_string_free(load_error);
    FLIPPASS_MEMORY_LOG(app, "save_execute_end", 0U);
    return ok;
}

#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
bool flippass_save_host_debug_after_open(App* app, FuriString* error) {
    Storage* storage = NULL;
    KDBXEntry* entry = NULL;
    bool triggered = false;
    bool ok = false;

    furi_assert(app);
    furi_assert(error);

    storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    triggered = storage_file_exists(storage, FLIPPASS_DEBUG_SAVE_TRIGGER_FILE_PATH);
    if(triggered) {
        storage_simply_remove(storage, FLIPPASS_DEBUG_SAVE_TRIGGER_FILE_PATH);
    }
    furi_record_close(RECORD_STORAGE);

    if(!triggered) {
        return true;
    }

    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(
        app,
        "DEBUG_SAVE_HOOK_BEGIN path=%s key_ready=%u",
        furi_string_get_cstr(app->file_path),
        app->database_save_key_ready ? 1U : 0U);

    if(app->root_group == NULL) {
        furi_string_set_str(error, "No opened database is available for the debug save hook.");
        goto finish;
    }
    if(app->current_group == NULL) {
        app->current_group = app->root_group;
    }

    if(!flippass_db_create_entry(
           app,
           app->current_group,
           "Debug Save Probe",
           "debug",
           "saved",
           "",
           "Low-interference save probe.",
           "",
           &entry,
           error)) {
        goto finish;
    }

    FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "DEBUG_SAVE_HOOK_MUTATION_OK");
    ok = flippass_save_host_current_database(
        app, furi_string_get_cstr(app->file_path), NULL, error);

finish:
    if(ok) {
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "DEBUG_SAVE_HOOK_OK");
    } else {
        const char* detail = !furi_string_empty(error) ? furi_string_get_cstr(error) :
                                                         "Debug save hook failed.";
        FLIPPASS_PROBE_TERMINAL_LOG_EVENT(app, "DEBUG_SAVE_HOOK_FAIL reason=%s", detail);
        UNUSED(detail);
    }
    return ok;
}
#endif
