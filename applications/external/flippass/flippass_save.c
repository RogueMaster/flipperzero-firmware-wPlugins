#include "flippass.h"
#include "flippass_db.h"
#include "kdbx/kdbx_constants.h"
#include "kdbx/memzero.h"
#include "plugins/flippass_save_plugin.h"

#include <storage/storage.h>
#include <toolbox/path.h>

#include <string.h>

typedef struct {
    App* app;
} FlipPassSaveHostContext;

static uint64_t flippass_save_normalize_kdf_rounds(uint64_t rounds) {
    if(rounds < FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS) {
        return FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS;
    }

    return rounds;
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

static bool flippass_save_host_activate_entry(
    void* context,
    KDBXEntry* entry,
    bool load_notes,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_db_activate_entry(host->app, entry, load_notes, error);
}

static void flippass_save_host_deactivate_entry(void* context) {
    FlipPassSaveHostContext* host = context;
    if(host != NULL && host->app != NULL) {
        flippass_db_deactivate_entry(host->app);
    }
}

static bool flippass_save_host_ensure_custom_field(
    void* context,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    FlipPassSaveHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_db_ensure_custom_field(host->app, entry, field, error);
}

static bool
    flippass_save_host_entry_has_field(void* context, const KDBXEntry* entry, uint32_t field_mask) {
    FlipPassSaveHostContext* host = context;
    UNUSED(host);
    return flippass_db_entry_has_field(entry, field_mask);
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
    flippass_db_deactivate_entry(app);
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
    FlipPassSaveHeaderResultV1 header_result;
    bool ok = false;
    bool header_temp_created = false;
    FuriString* load_error = furi_string_alloc();
    FuriString* target_path_string = furi_string_alloc();
    FuriString* database_name = furi_string_alloc();

    furi_assert(app);
    furi_assert(target_path);
    furi_assert(error);

    FLIPPASS_MEMORY_LOG(
        app,
        "save_execute_begin",
        sizeof(FlipPassSaveHostApiV1) + sizeof(FlipPassSaveStageHostApiV1) + sizeof(save_key) +
            sizeof(header_result));
    memzero(save_key, sizeof(save_key));
    memzero(&header_result, sizeof(header_result));

    kdf_rounds = flippass_save_normalize_kdf_rounds(kdf_rounds);
    if(password != NULL && password[0] != '\0') {
        flippass_make_password_composite_key(password, save_key);
    } else if(!flippass_session_copy_save_key(app, save_key)) {
        furi_string_set_str(error, "A save password is required.");
        goto cleanup;
    }

    if(app->root_group == NULL) {
        furi_string_set_str(error, "No editable database is available to save.");
        goto cleanup;
    }

    furi_string_set_str(target_path_string, target_path);
    path_extract_filename(target_path_string, database_name, true);
    if(furi_string_empty(database_name)) {
        furi_string_set_str(database_name, "Database");
    }

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
        .activate_entry = flippass_save_host_activate_entry,
        .deactivate_entry = flippass_save_host_deactivate_entry,
        .ensure_custom_field = flippass_save_host_ensure_custom_field,
        .entry_has_field = flippass_save_host_entry_has_field,
    };
    const FlipPassSaveHeaderRequestV1 header_request = {
        .api_version = FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION,
        .file_path = target_path,
        .composite_key = save_key,
        .composite_key_size = sizeof(save_key),
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
        .database_name = furi_string_get_cstr(database_name),
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
        if(!flippass_session_store_save_key(app, save_key)) {
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
    furi_string_free(load_error);
    furi_string_free(target_path_string);
    furi_string_free(database_name);
    FLIPPASS_MEMORY_LOG(app, "save_execute_end", 0U);
    return ok;
}
