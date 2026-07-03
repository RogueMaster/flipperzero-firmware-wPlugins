#include "flippass_rpc.h"

#if FLIPPASS_ENABLE_APP_RPC

#include "flippass.h"
#include "flippass_db.h"
#include "plugins/flippass_rpc_commands_plugin.h"

#include <rpc/rpc_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fp_rpc_eq(const char* a, const char* b) {
    if(a == NULL || b == NULL) {
        return false;
    }

    while(*a != '\0' && *b != '\0') {
        char ca = *a++;
        char cb = *b++;
        if(ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if(cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if(ca != cb) {
            return false;
        }
    }

    return (*a == '\0') && (*b == '\0');
}

static const char* fp_rpc_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static void fp_rpc_json(FuriString* out, const char* value) {
    furi_string_cat(out, "\"");
    if(value != NULL) {
        for(const unsigned char* p = (const unsigned char*)value; *p != '\0'; ++p) {
            if(*p == '\\') {
                furi_string_cat(out, "\\\\");
            } else if(*p == '"') {
                furi_string_cat(out, "\\\"");
            } else if(*p == '\n') {
                furi_string_cat(out, "\\n");
            } else if(*p == '\r') {
                furi_string_cat(out, "\\r");
            } else if(*p == '\t') {
                furi_string_cat(out, "\\t");
            } else {
                furi_string_cat_printf(out, "%c", *p);
            }
        }
    }
    furi_string_cat(out, "\"");
}

static void fp_rpc_set_error_json(FuriString* response, const char* message) {
    furi_string_set_str(response, "{\"ok\":false,\"error\":");
    fp_rpc_json(response, message);
    furi_string_cat(response, "}");
}

static void fp_rpc_send(App* app, const FuriString* response) {
    rpc_system_app_exchange_data(
        app->rpc, (const uint8_t*)furi_string_get_cstr(response), furi_string_size(response));
}

static void fp_rpc_set_transport_error(
    App* app,
    uint32_t error_code,
    const char* message,
    FuriString* response) {
    rpc_system_app_set_error_code(app->rpc, error_code);
    rpc_system_app_set_error_text(app->rpc, message);
    fp_rpc_set_error_json(response, message);
}

static void fp_rpc_path(FuriString* out, const KDBXGroup* group) {
    if(group == NULL) {
        return;
    }

    if(group->parent != NULL) {
        fp_rpc_path(out, group->parent);
        furi_string_cat(out, " / ");
    }

    furi_string_cat(out, fp_rpc_text(group->name, "Unnamed Group"));
}

static bool fp_rpc_loaded(App* app, FuriString* response) {
    if(app->database_loaded && app->current_group != NULL) {
        return true;
    }

    furi_string_set_str(response, "No database is unlocked.");
    return false;
}

static bool fp_rpc_entry_selected(App* app, FuriString* response) {
    if(app->active_entry != NULL) {
        return true;
    }

    furi_string_set_str(response, "No entry is selected.");
    return false;
}

static void fp_rpc_entry_json(FuriString* response, const KDBXEntry* entry) {
    furi_string_set_str(response, "{\"ok\":true,\"title\":");
    fp_rpc_json(response, fp_rpc_text(entry->title, "Untitled Entry"));
    furi_string_cat(response, ",\"username\":");
    fp_rpc_json(response, fp_rpc_text(entry->username, ""));
    furi_string_cat(response, ",\"password\":");
    fp_rpc_json(response, fp_rpc_text(entry->password, ""));
    furi_string_cat(response, ",\"url\":");
    fp_rpc_json(response, fp_rpc_text(entry->url, ""));
    furi_string_cat(response, ",\"notes\":");
    fp_rpc_json(response, fp_rpc_text(entry->notes, ""));
    furi_string_cat(response, ",\"autotype_sequence\":");
    fp_rpc_json(response, fp_rpc_text(entry->autotype_sequence, ""));
    furi_string_cat(response, ",\"uuid\":");
    fp_rpc_json(response, fp_rpc_text(entry->uuid, ""));
    furi_string_cat(response, "}");
}

static bool
    fp_rpc_prepare_entry(App* app, KDBXEntry* entry, bool load_notes, FuriString* response) {
    if(flippass_db_activate_entry(app, entry, load_notes, response)) {
        return true;
    }

    if(furi_string_empty(response)) {
        furi_string_set_str(response, "The selected entry could not be materialized.");
    }

    return false;
}

static bool fp_rpc_host_status(void* context, FuriString* response) {
    App* app = context;
    FuriString* path = furi_string_alloc();

    fp_rpc_path(path, app->current_group);
    furi_string_set_str(response, "{\"ok\":true,\"database_loaded\":");
    furi_string_cat(response, app->database_loaded ? "true" : "false");
    furi_string_cat(response, ",\"file\":");
    fp_rpc_json(response, furi_string_get_cstr(app->file_path));
    furi_string_cat(response, ",\"group_path\":");
    fp_rpc_json(response, furi_string_get_cstr(path));
    furi_string_cat_printf(
        response,
        ",\"supports_usb\":%s,\"supports_bluetooth\":%s}",
        FLIPPASS_ENABLE_TYPING_ACTIONS ? "true" : "false",
        (FLIPPASS_ENABLE_TYPING_ACTIONS && FLIPPASS_ENABLE_BLUETOOTH_OUTPUT) ? "true" : "false");

    furi_string_free(path);
    return true;
}

static bool fp_rpc_host_load_file(void* context, const char* path, FuriString* response) {
    App* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool exists = storage_file_exists(storage, path);
    furi_record_close(RECORD_STORAGE);

    if(!exists) {
        furi_string_set_str(response, "The requested KDBX file does not exist.");
        return false;
    }

    furi_string_set_str(app->file_path, path);
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);

    furi_string_set_str(response, "{\"ok\":true,\"file\":");
    fp_rpc_json(response, path);
    furi_string_cat(response, "}");
    return true;
}

static bool fp_rpc_host_unlock(
    void* context,
    const char* password,
    const char* backend,
    FuriString* response) {
    App* app = context;
    FuriString* error = furi_string_alloc();

    if(backend != NULL) {
        const KDBXVaultBackend backend_value = flippass_db_parse_backend_hint(backend);
        if(backend_value == KDBXVaultBackendNone) {
            furi_string_set_str(response, "Unknown backend. Use ram, int, or ext.");
            furi_string_free(error);
            return false;
        }
        app->requested_vault_backend = backend_value;
    }

    app->allow_ext_vault_promotion = app->always_allow_ext ||
                                     app->requested_vault_backend != KDBXVaultBackendRam;
    snprintf(app->master_password, sizeof(app->master_password), "%s", password);
    const bool ok = flippass_open_execute(app, error);
    if(ok) {
        furi_string_set_str(response, "{\"ok\":true,\"backend\":");
        fp_rpc_json(response, kdbx_vault_backend_label(app->active_vault_backend));
        furi_string_cat(response, "}");
    } else {
        furi_string_set_str(response, furi_string_get_cstr(error));
    }

    furi_string_free(error);
    return ok;
}

static bool fp_rpc_host_list(void* context, FuriString* response) {
    App* app = context;
    FuriString* path = furi_string_alloc();
    FuriString* title = furi_string_alloc();
    FuriString* error = furi_string_alloc();
    bool ok = true;

    if(!fp_rpc_loaded(app, response)) {
        ok = false;
        goto cleanup;
    }

    fp_rpc_path(path, app->current_group);
    furi_string_set_str(response, "{\"ok\":true,\"group_path\":");
    fp_rpc_json(response, furi_string_get_cstr(path));
    furi_string_cat(response, ",\"groups\":[");

    KDBXGroup* group = app->current_group->children;
    for(size_t i = 0U; group != NULL; ++i, group = group->next) {
        if(i != 0U) {
            furi_string_cat(response, ",");
        }
        furi_string_cat_printf(response, "{\"index\":%lu,\"name\":", (unsigned long)i);
        fp_rpc_json(response, fp_rpc_text(group->name, "Unnamed Group"));
        furi_string_cat(response, "}");
    }

    furi_string_cat(response, "],\"entries\":[");
    KDBXEntry* entry = app->current_group->entries;
    for(size_t i = 0U; entry != NULL; ++i, entry = entry->next) {
        if(!flippass_db_copy_entry_title(app, entry, title, error)) {
            furi_string_set_str(response, furi_string_get_cstr(error));
            ok = false;
            goto cleanup;
        }
        if(i != 0U) {
            furi_string_cat(response, ",");
        }
        furi_string_cat_printf(response, "{\"index\":%lu,\"title\":", (unsigned long)i);
        fp_rpc_json(response, fp_rpc_text(furi_string_get_cstr(title), "Untitled Entry"));
        furi_string_cat(response, "}");
    }

    furi_string_cat(response, "]}");

cleanup:
    furi_string_free(error);
    furi_string_free(title);
    furi_string_free(path);
    return ok;
}

static bool fp_rpc_host_cd_parent(void* context, FuriString* response) {
    App* app = context;

    if(!fp_rpc_loaded(app, response)) {
        return false;
    }

    if(app->current_group->parent == NULL) {
        furi_string_set_str(response, "The current group has no parent.");
        return false;
    }

    flippass_db_deactivate_entry(app);
    app->current_group = app->current_group->parent;
    app->current_entry = NULL;
    app->active_group = app->current_group;
    furi_string_set_str(response, "{\"ok\":true}");
    return true;
}

static bool fp_rpc_host_cd_index(void* context, uint32_t index, FuriString* response) {
    App* app = context;

    if(!fp_rpc_loaded(app, response)) {
        return false;
    }

    KDBXGroup* group = app->current_group->children;
    while(group != NULL && index > 0U) {
        group = group->next;
        index--;
    }

    if(group == NULL) {
        furi_string_set_str(response, "Group index is out of range.");
        return false;
    }

    flippass_db_deactivate_entry(app);
    app->current_group = group;
    app->current_entry = NULL;
    app->active_group = group;
    furi_string_set_str(response, "{\"ok\":true}");
    return true;
}

static bool fp_rpc_host_entry_index(void* context, uint32_t index, FuriString* response) {
    App* app = context;

    if(!fp_rpc_loaded(app, response)) {
        return false;
    }

    KDBXEntry* entry = app->current_group->entries;
    while(entry != NULL && index > 0U) {
        entry = entry->next;
        index--;
    }

    if(entry == NULL) {
        furi_string_set_str(response, "Entry index is out of range.");
        return false;
    }

    if(!fp_rpc_prepare_entry(app, entry, true, response)) {
        return false;
    }

    app->current_entry = entry;
    app->active_group = app->current_group;
    fp_rpc_entry_json(response, entry);
    return true;
}

static bool fp_rpc_host_show_entry(void* context, FuriString* response) {
    App* app = context;

    if(!fp_rpc_entry_selected(app, response)) {
        return false;
    }

    if(!fp_rpc_prepare_entry(app, app->active_entry, true, response)) {
        return false;
    }

    fp_rpc_entry_json(response, app->active_entry);
    return true;
}

static bool fp_rpc_host_show_field(void* context, const char* field, FuriString* response) {
    App* app = context;
    KDBXEntry* entry = app->active_entry;
    const char* value = NULL;

    if(!fp_rpc_entry_selected(app, response)) {
        return false;
    }

    if(fp_rpc_eq(field, "title")) {
        value = entry->title;
    } else if(fp_rpc_eq(field, "username")) {
        if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUsername, response)) {
            if(furi_string_empty(response)) {
                furi_string_set_str(response, "The username could not be read.");
            }
            return false;
        }
        value = entry->username;
    } else if(fp_rpc_eq(field, "password")) {
        if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldPassword, response)) {
            if(furi_string_empty(response)) {
                furi_string_set_str(response, "The password could not be read.");
            }
            return false;
        }
        value = entry->password;
    } else if(fp_rpc_eq(field, "url")) {
        if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUrl, response)) {
            if(furi_string_empty(response)) {
                furi_string_set_str(response, "The URL could not be read.");
            }
            return false;
        }
        value = entry->url;
    } else if(fp_rpc_eq(field, "notes")) {
        if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldNotes, response)) {
            if(furi_string_empty(response)) {
                furi_string_set_str(response, "The notes could not be read.");
            }
            return false;
        }
        value = entry->notes;
    } else if(fp_rpc_eq(field, "autotype")) {
        if(flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
           !flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldAutotype, response)) {
            if(furi_string_empty(response)) {
                furi_string_set_str(response, "The AutoType sequence could not be read.");
            }
            return false;
        }
        value = entry->autotype_sequence;
    } else if(fp_rpc_eq(field, "uuid")) {
        value = entry->uuid;
    } else {
        furi_string_set_str(response, "Unknown field name.");
        return false;
    }

    furi_string_set_str(response, "{\"ok\":true,\"field\":");
    fp_rpc_json(response, field);
    furi_string_cat(response, ",\"value\":");
    fp_rpc_json(response, fp_rpc_text(value, ""));
    furi_string_cat(response, "}");
    return true;
}

static bool fp_rpc_transport_map(
    FlipPassRpcTransport transport,
    FlipPassOutputTransport* output_transport) {
    if(output_transport == NULL) {
        return false;
    }

    switch(transport) {
    case FlipPassRpcTransportBluetooth:
        *output_transport = FlipPassOutputTransportBluetooth;
        return true;
    case FlipPassRpcTransportUsb:
    default:
        *output_transport = FlipPassOutputTransportUsb;
        return true;
    }
}

static bool fp_rpc_host_type_field(
    void* context,
    const char* field,
    FlipPassRpcTransport transport,
    FuriString* response) {
    App* app = context;
    KDBXEntry* entry = app->active_entry;
    FlipPassOutputTransport output_transport = FlipPassOutputTransportUsb;
    const char* log_prefix = "USB";
    bool typed = false;

    if(!fp_rpc_entry_selected(app, response)) {
        return false;
    }

    if(!fp_rpc_transport_map(transport, &output_transport)) {
        furi_string_set_str(response, "Unknown transport. Use usb, bt, ble, or bluetooth.");
        return false;
    }

    if(!fp_rpc_prepare_entry(app, entry, false, response)) {
        return false;
    }

    log_prefix = (output_transport == FlipPassOutputTransportBluetooth) ? "BT" : "USB";

    if(fp_rpc_eq(field, "username")) {
        typed = entry->username != NULL && entry->username[0] != '\0' &&
                flippass_output_type_string(app, output_transport, entry->username);
    } else if(fp_rpc_eq(field, "password")) {
        typed = entry->password != NULL && entry->password[0] != '\0' &&
                flippass_output_type_string(app, output_transport, entry->password);
    } else if(fp_rpc_eq(field, "login")) {
        typed =
            entry->username != NULL && entry->password != NULL &&
            flippass_output_type_login(app, output_transport, entry->username, entry->password);
    } else if(fp_rpc_eq(field, "autotype")) {
        typed = flippass_output_type_autotype(app, output_transport, entry);
    } else {
        furi_string_set_str(response, "Unknown type action.");
        return false;
    }

    if(typed) {
        FLIPPASS_LOG_EVENT(app, "%s_TYPE_OK field=%s", log_prefix, field);
        furi_string_set_str(response, "{\"ok\":true,\"transport\":");
        fp_rpc_json(response, flippass_output_transport_name(output_transport));
        furi_string_cat(response, ",\"action\":");
        fp_rpc_json(response, field);
        furi_string_cat(response, "}");
        return true;
    }

    FLIPPASS_LOG_EVENT(app, "%s_TYPE_FAIL field=%s", log_prefix, field);
    furi_string_set_str(
        response,
        "Typing failed because the transport was unavailable, not connected, or unsupported by the selected entry.");
    return false;
}

static const FlipPassRpcCommandsPluginV1* fp_rpc_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotRpcCommands,
        NULL,
        FLIPPASS_RPC_COMMANDS_PLUGIN_APPID,
        FLIPPASS_RPC_COMMANDS_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass RPC commands plugin is unavailable.");
        }
        return NULL;
    }

    return descriptor->entry_point;
}

static bool
    fp_rpc_execute_data(App* app, const uint8_t* data, size_t data_size, FuriString* response) {
    bool ok = false;
    uint32_t error_code = 0U;
    FuriString* plugin_error = furi_string_alloc();
    FuriString* load_error = furi_string_alloc();

    const FlipPassRpcCommandsPluginV1* plugin = fp_rpc_plugin_get(app, load_error);
    if(plugin == NULL) {
        fp_rpc_set_transport_error(
            app, FlipPassRpcCommandsErrorInvalidState, furi_string_get_cstr(load_error), response);
        goto cleanup;
    }

    const FlipPassRpcCommandsHostApiV1 host_api = {
        .api_version = FLIPPASS_RPC_COMMANDS_HOST_API_VERSION,
        .context = app,
        .status = fp_rpc_host_status,
        .load_file = fp_rpc_host_load_file,
        .unlock = fp_rpc_host_unlock,
        .list = fp_rpc_host_list,
        .cd_parent = fp_rpc_host_cd_parent,
        .cd_index = fp_rpc_host_cd_index,
        .entry_index = fp_rpc_host_entry_index,
        .show_entry = fp_rpc_host_show_entry,
        .show_field = fp_rpc_host_show_field,
        .type_field = fp_rpc_host_type_field,
    };

    ok = plugin->execute_bytes(&host_api, data, data_size, response, &error_code, plugin_error);
    if(!ok) {
        rpc_system_app_set_error_code(app->rpc, error_code);
        rpc_system_app_set_error_text(app->rpc, furi_string_get_cstr(plugin_error));
    }

cleanup:
    furi_string_free(load_error);
    furi_string_free(plugin_error);
    return ok;
}

static void fp_rpc_callback(const RpcAppSystemEvent* event, void* context) {
    App* app = context;
    FuriString* response = furi_string_alloc();
    bool ok = false;

    if(event->type == RpcAppEventTypeSessionClose) {
        rpc_system_app_set_callback(app->rpc, NULL, NULL);
        app->rpc = NULL;
        app->rpc_mode = false;
        flippass_module_unload(app, FlipPassModuleSlotRpcCommands);
        if(app->usb_expect_rpc_session_close) {
            app->usb_expect_rpc_session_close = false;
            furi_string_free(response);
            return;
        }
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        furi_string_free(response);
        return;
    }

    switch(event->type) {
    case RpcAppEventTypeAppExit:
        flippass_module_unload(app, FlipPassModuleSlotRpcCommands);
        rpc_system_app_confirm(app->rpc, true);
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        break;
    case RpcAppEventTypeLoadFile:
        ok = fp_rpc_host_load_file(app, event->data.string, response);
        if(!ok) {
            fp_rpc_set_transport_error(
                app,
                FlipPassRpcCommandsErrorOperationFailed,
                furi_string_get_cstr(response),
                response);
        }
        fp_rpc_send(app, response);
        rpc_system_app_confirm(app->rpc, ok);
        break;
    case RpcAppEventTypeDataExchange:
        ok = fp_rpc_execute_data(app, event->data.bytes.ptr, event->data.bytes.size, response);
        fp_rpc_send(app, response);
        rpc_system_app_confirm(app->rpc, ok);
        break;
    case RpcAppEventTypeButtonPress:
    case RpcAppEventTypeButtonRelease:
    case RpcAppEventTypeButtonPressRelease:
        fp_rpc_set_transport_error(
            app,
            FlipPassRpcCommandsErrorBadCommand,
            "Button-based app RPC is not implemented yet. Use data exchange commands instead.",
            response);
        fp_rpc_send(app, response);
        rpc_system_app_confirm(app->rpc, false);
        break;
    case RpcAppEventTypeInvalid:
    default:
        break;
    }

    furi_string_free(response);
}

bool flippass_rpc_init(App* app, const char* args) {
    uint32_t rpc_ctx = 0U;
    FuriString* load_error = NULL;

    app->rpc = NULL;
    app->rpc_mode = false;

    if(args == NULL || args[0] == '\0') {
        return false;
    }

    if(sscanf(args, "RPC %08lX", &rpc_ctx) != 1) {
        return false;
    }

    app->rpc = (RpcAppSystem*)rpc_ctx;
    app->rpc_mode = true;
    rpc_system_app_set_callback(app->rpc, fp_rpc_callback, app);

    load_error = furi_string_alloc();
    const FlipPassRpcCommandsPluginV1* plugin = fp_rpc_plugin_get(app, load_error);
    if(plugin == NULL) {
        FLIPPASS_LOG_EVENT(
            app, "RPC_PLUGIN_LOAD_FAIL reason=%s", furi_string_get_cstr(load_error));
    } else {
        FLIPPASS_LOG_EVENT(app, "RPC_PLUGIN_LOAD_OK");
    }
    furi_string_free(load_error);

    rpc_system_app_send_started(app->rpc);
    return true;
}

void flippass_rpc_deinit(App* app) {
    flippass_module_unload(app, FlipPassModuleSlotRpcCommands);
    if(app->rpc != NULL) {
        rpc_system_app_set_callback(app->rpc, NULL, NULL);
        rpc_system_app_send_exited(app->rpc);
        app->rpc = NULL;
    }
    app->rpc_mode = false;
}

#else

bool flippass_rpc_init(App* app, const char* args) {
    (void)app;
    (void)args;
    return false;
}

void flippass_rpc_deinit(App* app) {
    (void)app;
}

#endif
