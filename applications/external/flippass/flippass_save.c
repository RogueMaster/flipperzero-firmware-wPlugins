#include "flippass.h"
#include "flippass_db.h"
#include "kdbx/kdbx_constants.h"
#include "kdbx/memzero.h"
#include "plugins/flippass_save_plugin.h"
#include "scenes/flippass_scene.h"
#include "scenes/flippass_scene_db_entries.h"
#include "scenes/flippass_scene_editor.h"

#include <storage/storage.h>

#include <string.h>

#define FLIPPASS_SAVE_DATABASE_NAME_SIZE 64U

typedef struct {
    App* app;
} FlipPassSaveHostContext;

static uint64_t flippass_save_normalize_kdf_rounds(uint64_t rounds) {
    if(rounds < FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS) {
        return FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    }

    return rounds;
}

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
        FLIPPASS_LOG_EVENT(host->app, "%s", message);
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
    flippass_db_deactivate_entry(app);
    FLIPPASS_MEMORY_LOG(app, "save_after_entry_deactivate", 0U);
    flippass_output_cleanup(app);
    flippass_module_unload(app, FlipPassModuleSlotOutputUsb);
    flippass_module_unload(app, FlipPassModuleSlotOutputBle);
    flippass_module_unload(app, FlipPassModuleSlotOutputAction);
    flippass_module_unload(app, FlipPassModuleSlotOtherFields);
    flippass_module_unload(app, FlipPassModuleSlotFileOps);
    flippass_module_unload(app, FlipPassModuleSlotEditorCrud);
    flippass_module_unload(app, FlipPassModuleSlotRpcCommands);
    flippass_module_unload(app, FlipPassModuleSlotKeyboardLayout);
    flippass_module_unload(app, FlipPassModuleSlotOtp);
    flippass_module_unload(app, FlipPassModuleSlotPasswordGen);
    flippass_module_unload(app, FlipPassModuleSlotOpenAcquire);
    flippass_module_unload(app, FlipPassModuleSlotOpenStream);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflateNonPaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenInflatePaged);
    flippass_module_unload(app, FlipPassModuleSlotOpenModel);
    flippass_module_unload(app, FlipPassModuleSlotSaveHeader);
    flippass_module_unload(app, FlipPassModuleSlotSaveWriter);
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

bool flippass_save_current_database(
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
    flippass_save_trim_ui_for_save(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    const bool ok = flippass_save_execute(
        app,
        target_path,
        password,
        app->database_cipher,
        app->database_compression,
        app->database_kdf_rounds,
        error);
    flippass_progress_reset(app);
    return ok;
}

bool flippass_save_execute(
    App* app,
    const char* target_path,
    const char* password,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    uint64_t kdf_rounds,
    FuriString* error) {
    const FlipperAppPluginDescriptor* header_descriptor = NULL;
    const FlipperAppPluginDescriptor* payload_descriptor = NULL;
    const FlipPassSaveHeaderPluginV1* header_plugin = NULL;
    const FlipPassSavePluginV1* payload_plugin = NULL;
    uint8_t save_key[32];
    uint8_t transformed_key[32];
    uint8_t kdf_salt[32];
    uint64_t transformed_kdf_rounds = 0U;
    bool transformed_key_ready = false;
    FlipPassSaveHeaderResultV1 header_result;
    char database_name[FLIPPASS_SAVE_DATABASE_NAME_SIZE];
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
    memzero(save_key, sizeof(save_key));
    memzero(transformed_key, sizeof(transformed_key));
    memzero(kdf_salt, sizeof(kdf_salt));
    memzero(&header_result, sizeof(header_result));
    memzero(database_name, sizeof(database_name));

    kdf_rounds = flippass_save_normalize_kdf_rounds(kdf_rounds);
    if(password != NULL && password[0] != '\0') {
        FLIPPASS_LOG_EVENT(app, "SAVE_KEY_SOURCE password");
        FLIPPASS_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
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
        } else {
            transformed_key_ready = false;
            memzero(transformed_key, sizeof(transformed_key));
            memzero(kdf_salt, sizeof(kdf_salt));
            transformed_kdf_rounds = 0U;
            FLIPPASS_LOG_EVENT(app, "SAVE_KDF_SOURCE derive");
        }
    }

    if(app->root_group == NULL) {
        furi_string_set_str(error, "No editable database is available to save.");
        goto cleanup;
    }

    flippass_save_extract_database_name(target_path, database_name, sizeof(database_name));

    flippass_save_unload_runtime_modules(app);

    const FlipPassSaveCipher save_cipher = (cipher == FlipPassKdbxCipherChaCha20) ?
                                               FlipPassSaveCipherChaCha20 :
                                               FlipPassSaveCipherAes256;
    FlipPassSaveHostContext host = {
        .app = app,
    };
    const FlipPassSaveStageHostApiV1 stage_host_api = {
        .api_version = FLIPPASS_SAVE_STAGE_HOST_API_VERSION,
        .context = &host,
        .progress = flippass_save_host_progress,
        .log = flippass_save_host_log,
    };
    const FlipPassSaveHostApiV1 host_api = {
        .api_version = FLIPPASS_SAVE_HOST_API_VERSION,
        .context = &host,
        .progress = flippass_save_host_progress,
        .log = flippass_save_host_log,
        .copy_group_uuid = flippass_save_host_copy_group_uuid,
        .copy_entry_uuid = flippass_save_host_copy_entry_uuid,
        .entry_has_field = flippass_save_host_entry_has_field,
        .stream_ref = flippass_save_host_stream_ref,
    };
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

    FLIPPASS_MEMORY_LOG(app, "save_before_header_run", sizeof(header_request));
    if(!header_plugin->run(&header_request, &stage_host_api, &header_result, error)) {
        goto cleanup;
    }
    header_temp_created = true;
    FLIPPASS_MEMORY_LOG(app, "save_after_header_run", sizeof(header_result));
    flippass_module_unload(app, FlipPassModuleSlotSaveHeader);
    FLIPPASS_MEMORY_LOG(app, "save_after_header_unload", sizeof(header_result));

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
        .root_group = app->root_group,
        .database_name = database_name,
        .cipher = save_cipher,
        .compression = compression,
    };

    FLIPPASS_MEMORY_LOG(app, "save_before_payload_load", sizeof(request));
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

    FLIPPASS_MEMORY_LOG(app, "save_before_payload_run", sizeof(request));
    ok = payload_plugin->run(&request, &host_api, error);
    FLIPPASS_MEMORY_LOG(app, "save_after_payload_run", 0U);

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
        flippass_save_settings(app);
        FLIPPASS_MEMORY_LOG(app, "save_after_settings", 0U);
    }

cleanup:
    if(!ok) {
        flippass_module_unload(app, FlipPassModuleSlotSaveHeader);
        flippass_module_unload(app, FlipPassModuleSlotSaveWriter);
        if(header_temp_created) {
            flippass_save_remove_temp_file(target_path);
        }
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
bool flippass_debug_save_after_open(App* app, FuriString* error) {
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

    FLIPPASS_LOG_EVENT(
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

    FLIPPASS_LOG_EVENT(app, "DEBUG_SAVE_HOOK_MUTATION_OK");
    ok = flippass_save_current_database(app, furi_string_get_cstr(app->file_path), NULL, error);

finish:
    if(ok) {
        FLIPPASS_LOG_EVENT(app, "DEBUG_SAVE_HOOK_OK");
    } else {
        const char* detail = !furi_string_empty(error) ? furi_string_get_cstr(error) :
                                                         "Debug save hook failed.";
        FLIPPASS_LOG_EVENT(app, "DEBUG_SAVE_HOOK_FAIL reason=%s", detail);
        UNUSED(detail);
    }
    return ok;
}
#endif
