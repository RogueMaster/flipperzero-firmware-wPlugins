#include "flippass_rpc.h"

#if FLIPPASS_ENABLE_APP_RPC

#include "flippass.h"
#include "flippass_db.h"

#include <rpc/rpc_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FlipPassRpcErrorBadCommand = 1,
    FlipPassRpcErrorMissingArgument,
    FlipPassRpcErrorInvalidState,
    FlipPassRpcErrorInvalidIndex,
    FlipPassRpcErrorUnsupportedTransport,
    FlipPassRpcErrorOperationFailed,
};

typedef struct {
    char* raw;
    size_t raw_size;
    char* part[4];
    size_t count;
} FlipPassRpcCommand;

static bool fp_rpc_set_file(App* app, const char* path, FuriString* response);
static bool fp_rpc_eq(const char* a, const char* b) {
    if(a == NULL || b == NULL) return false;
    while(*a != '\0' && *b != '\0') {
        char ca = *a++;
        char cb = *b++;
        if(ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if(cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if(ca != cb) return false;
    }
    return (*a == '\0') && (*b == '\0');
}

static const char* fp_rpc_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static void fp_rpc_trim(char* text) {
    if(text == NULL) return;
    size_t len = strlen(text);
    while(len > 0U && (text[len - 1U] == '\r' || text[len - 1U] == '\n' || text[len - 1U] == ' ')) {
        text[--len] = '\0';
    }
}

static bool fp_rpc_parse(const uint8_t* data, size_t data_size, FlipPassRpcCommand* command) {
    if(data == NULL || data_size == 0U || command == NULL) return false;
    memset(command, 0, sizeof(*command));
    command->raw = malloc(data_size + 1U);
    if(command->raw == NULL) return false;
    command->raw_size = data_size + 1U;
    memcpy(command->raw, data, data_size);
    command->raw[data_size] = '\0';
    char* cursor = command->raw;
    while(cursor != NULL && *cursor != '\0' && command->count < COUNT_OF(command->part)) {
        char* token = cursor;
        char* newline = strchr(cursor, '\n');
        if(newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = NULL;
        }
        fp_rpc_trim(token);
        command->part[command->count++] = token;
    }
    if(command->count == 0U || command->part[0][0] == '\0') {
        free(command->raw);
        memset(command, 0, sizeof(*command));
        return false;
    }
    return true;
}

static void fp_rpc_free(FlipPassRpcCommand* command) {
    if(command == NULL) return;
    if(command->raw != NULL) {
        memzero(command->raw, command->raw_size);
        free(command->raw);
    }
    memset(command, 0, sizeof(*command));
}

static void fp_rpc_json(FuriString* out, const char* value) {
    furi_string_cat(out, "\"");
    if(value != NULL) {
        for(const unsigned char* p = (const unsigned char*)value; *p != '\0'; ++p) {
            if(*p == '\\') furi_string_cat(out, "\\\\");
            else if(*p == '"') furi_string_cat(out, "\\\"");
            else if(*p == '\n') furi_string_cat(out, "\\n");
            else if(*p == '\r') furi_string_cat(out, "\\r");
            else if(*p == '\t') furi_string_cat(out, "\\t");
            else furi_string_cat_printf(out, "%c", *p);
        }
    }
    furi_string_cat(out, "\"");
}

static void fp_rpc_send(App* app, const FuriString* response) {
    rpc_system_app_exchange_data(
        app->rpc, (const uint8_t*)furi_string_get_cstr(response), furi_string_size(response));
}

static void fp_rpc_fail(App* app, FuriString* response, uint32_t code, const char* message) {
    furi_string_set_str(response, "{\"ok\":false,\"error\":");
    fp_rpc_json(response, message);
    furi_string_cat(response, "}");
    rpc_system_app_set_error_code(app->rpc, code);
    rpc_system_app_set_error_text(app->rpc, message);
}

static void fp_rpc_path(FuriString* out, const KDBXGroup* group) {
    if(group == NULL) return;
    if(group->parent != NULL) {
        fp_rpc_path(out, group->parent);
        furi_string_cat(out, " / ");
    }
    furi_string_cat(out, fp_rpc_text(group->name, "Unnamed Group"));
}

static bool fp_rpc_transport(const char* text, FlipPassOutputTransport* transport) {
    if(text == NULL || text[0] == '\0' || fp_rpc_eq(text, "usb")) {
        *transport = FlipPassOutputTransportUsb;
        return true;
    }
    if(fp_rpc_eq(text, "bt") || fp_rpc_eq(text, "ble") || fp_rpc_eq(text, "bluetooth")) {
        *transport = FlipPassOutputTransportBluetooth;
        return true;
    }
    return false;
}

static bool fp_rpc_loaded(App* app, FuriString* response) {
    if(app->database_loaded && app->current_group != NULL) return true;
    fp_rpc_fail(app, response, FlipPassRpcErrorInvalidState, "No database is unlocked.");
    return false;
}

static bool fp_rpc_entry(App* app, FuriString* response) {
    if(app->active_entry != NULL) return true;
    fp_rpc_fail(app, response, FlipPassRpcErrorInvalidState, "No entry is selected.");
    return false;
}

static bool fp_rpc_set_file(App* app, const char* path, FuriString* response) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool exists = storage_file_exists(storage, path);
    furi_record_close(RECORD_STORAGE);

    if(!exists) {
        fp_rpc_fail(
            app,
            response,
            FlipPassRpcErrorOperationFailed,
            "The requested KDBX file does not exist.");
        return false;
    }

    furi_string_set_str(app->file_path, path);
    flippass_save_settings(app);
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);

    furi_string_set_str(response, "{\"ok\":true,\"file\":");
    fp_rpc_json(response, path);
    furi_string_cat(response, "}");
    return true;
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

static bool fp_rpc_prepare_entry(
    App* app,
    KDBXEntry* entry,
    bool load_notes,
    FuriString* response) {
    if(flippass_db_activate_entry(app, entry, load_notes, response)) {
        return true;
    }

    fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
    return false;
}

static bool fp_rpc_exec(App* app, const uint8_t* data, size_t data_size, FuriString* response) {
    FlipPassRpcCommand cmd;
    bool ok = false;
    if(!fp_rpc_parse(data, data_size, &cmd)) {
        fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Empty RPC command.");
        return false;
    }

    if(fp_rpc_eq(cmd.part[0], "help")) {
        furi_string_set_str(response, "{\"ok\":true,\"commands\":[\"status\",\"load\",\"unlock\",\"ls\",\"cd\",\"entry\",\"show\",\"type\"]}");
        ok = true;
    } else if(fp_rpc_eq(cmd.part[0], "status")) {
        FuriString* path = furi_string_alloc();
        fp_rpc_path(path, app->current_group);
        furi_string_set_str(response, "{\"ok\":true,\"database_loaded\":");
        furi_string_cat(response, app->database_loaded ? "true" : "false");
        furi_string_cat(response, ",\"file\":");
        fp_rpc_json(response, furi_string_get_cstr(app->file_path));
        furi_string_cat(response, ",\"group_path\":");
        fp_rpc_json(response, furi_string_get_cstr(path));
        furi_string_cat(response, ",\"supports_usb\":true,\"supports_bluetooth\":true}");
        furi_string_free(path);
        ok = true;
    } else if(fp_rpc_eq(cmd.part[0], "load")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The load command requires a file path.");
        else ok = true, furi_string_set_str(app->file_path, cmd.part[1]), flippass_save_settings(app), flippass_reset_database(app), flippass_clear_text_buffer(app), flippass_clear_master_password(app), furi_string_set_str(response, "{\"ok\":true,\"file\":"), fp_rpc_json(response, cmd.part[1]), furi_string_cat(response, "}");
    } else if(fp_rpc_eq(cmd.part[0], "unlock")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The unlock command requires a password.");
        else {
            FuriString* error = furi_string_alloc();
            if(cmd.count >= 3U) {
                const KDBXVaultBackend backend = flippass_db_parse_backend_hint(cmd.part[2]);
                if(backend == KDBXVaultBackendNone) {
                    fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Unknown backend. Use ram, int, or ext.");
                    furi_string_free(error);
                    fp_rpc_free(&cmd);
                    return false;
                }
                app->requested_vault_backend = backend;
            }
            snprintf(app->master_password, sizeof(app->master_password), "%s", cmd.part[1]);
            ok = flippass_db_load(app, error);
            if(ok) {
                furi_string_set_str(response, "{\"ok\":true,\"backend\":");
                fp_rpc_json(response, kdbx_vault_backend_label(app->active_vault_backend));
                furi_string_cat(response, "}");
            } else {
                fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(error));
            }
            furi_string_free(error);
        }
    } else if(fp_rpc_eq(cmd.part[0], "ls")) {
        if(fp_rpc_loaded(app, response)) {
            FuriString* path = furi_string_alloc();
            FuriString* title = furi_string_alloc();
            FuriString* error = furi_string_alloc();
            bool list_ok = true;
            fp_rpc_path(path, app->current_group);
            furi_string_set_str(response, "{\"ok\":true,\"group_path\":");
            fp_rpc_json(response, furi_string_get_cstr(path));
            furi_string_cat(response, ",\"groups\":[");
            KDBXGroup* group = app->current_group->children;
            for(size_t i = 0U; group != NULL; ++i, group = group->next) {
                if(i != 0U) furi_string_cat(response, ",");
                furi_string_cat_printf(response, "{\"index\":%lu,\"name\":", (unsigned long)i);
                fp_rpc_json(response, fp_rpc_text(group->name, "Unnamed Group"));
                furi_string_cat(response, "}");
            }
            furi_string_cat(response, "],\"entries\":[");
            KDBXEntry* entry = app->current_group->entries;
            for(size_t i = 0U; entry != NULL; ++i, entry = entry->next) {
                if(!flippass_db_copy_entry_title(app, entry, title, error)) {
                    fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(error));
                    list_ok = false;
                    break;
                }
                if(i != 0U) furi_string_cat(response, ",");
                furi_string_cat_printf(response, "{\"index\":%lu,\"title\":", (unsigned long)i);
                fp_rpc_json(response, fp_rpc_text(furi_string_get_cstr(title), "Untitled Entry"));
                furi_string_cat(response, "}");
            }
            if(list_ok) {
                furi_string_cat(response, "]}");
            }
            furi_string_free(error);
            furi_string_free(title);
            furi_string_free(path);
            ok = list_ok;
        }
    } else if(fp_rpc_eq(cmd.part[0], "cd")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The cd command requires a group index or '..'.");
        else if(fp_rpc_loaded(app, response)) {
            if(fp_rpc_eq(cmd.part[1], "..")) {
                if(app->current_group->parent == NULL) fp_rpc_fail(app, response, FlipPassRpcErrorInvalidIndex, "The current group has no parent.");
                else ok = true, flippass_db_deactivate_entry(app), app->current_group = app->current_group->parent, app->current_entry = NULL, app->active_group = app->current_group, furi_string_set_str(response, "{\"ok\":true}");
            } else {
                char* end = NULL;
                unsigned long index = strtoul(cmd.part[1], &end, 10);
                if(end == NULL || *end != '\0') fp_rpc_fail(app, response, FlipPassRpcErrorInvalidIndex, "Group index must be numeric.");
                else {
                    KDBXGroup* group = app->current_group->children;
                    while(group != NULL && index > 0U) group = group->next, index--;
                    if(group == NULL) fp_rpc_fail(app, response, FlipPassRpcErrorInvalidIndex, "Group index is out of range.");
                    else ok = true, flippass_db_deactivate_entry(app), app->current_group = group, app->current_entry = NULL, app->active_group = group, furi_string_set_str(response, "{\"ok\":true}");
                }
            }
        }
    } else if(fp_rpc_eq(cmd.part[0], "entry")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The entry command requires an entry index.");
        else if(fp_rpc_loaded(app, response)) {
            char* end = NULL;
            unsigned long index = strtoul(cmd.part[1], &end, 10);
            if(end == NULL || *end != '\0') fp_rpc_fail(app, response, FlipPassRpcErrorInvalidIndex, "Entry index must be numeric.");
            else {
                KDBXEntry* entry = app->current_group->entries;
                while(entry != NULL && index > 0U) entry = entry->next, index--;
                if(entry == NULL) fp_rpc_fail(app, response, FlipPassRpcErrorInvalidIndex, "Entry index is out of range.");
                else if(!fp_rpc_prepare_entry(app, entry, true, response)) {
                } else {
                    ok = true;
                    app->current_entry = entry;
                    app->active_group = app->current_group;
                    fp_rpc_entry_json(response, entry);
                }
            }
        }
    } else if(fp_rpc_eq(cmd.part[0], "show")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The show command requires a field name.");
        else if(fp_rpc_entry(app, response)) {
            KDBXEntry* entry = app->active_entry;
            const char* value = NULL;
            bool field_known = true;
            if(fp_rpc_eq(cmd.part[1], "entry")) {
                if(fp_rpc_prepare_entry(app, entry, true, response)) {
                    fp_rpc_entry_json(response, entry);
                    ok = true;
                }
            }
            else {
                if(fp_rpc_eq(cmd.part[1], "title")) value = entry->title;
                else if(fp_rpc_eq(cmd.part[1], "username")) {
                    if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUsername, response)) {
                        fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
                    } else value = entry->username;
                } else if(fp_rpc_eq(cmd.part[1], "password")) {
                    if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldPassword, response)) {
                        fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
                    } else value = entry->password;
                } else if(fp_rpc_eq(cmd.part[1], "url")) {
                    if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUrl, response)) {
                        fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
                    } else value = entry->url;
                } else if(fp_rpc_eq(cmd.part[1], "notes")) {
                    if(!flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldNotes, response)) {
                        fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
                    } else value = entry->notes;
                } else if(fp_rpc_eq(cmd.part[1], "autotype")) {
                    if(flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
                       !flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldAutotype, response)) {
                        fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, furi_string_get_cstr(response));
                    } else value = entry->autotype_sequence;
                }
                else if(fp_rpc_eq(cmd.part[1], "uuid")) value = entry->uuid;
                else field_known = false;

                if(!field_known) {
                    fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Unknown field name.");
                } else if(furi_string_empty(response)) {
                    ok = true;
                    furi_string_set_str(response, "{\"ok\":true,\"field\":");
                    fp_rpc_json(response, cmd.part[1]);
                    furi_string_cat(response, ",\"value\":");
                    fp_rpc_json(response, fp_rpc_text(value, ""));
                    furi_string_cat(response, "}");
                }
            }
        }
    } else if(fp_rpc_eq(cmd.part[0], "type")) {
        if(cmd.count < 2U) fp_rpc_fail(app, response, FlipPassRpcErrorMissingArgument, "The type command requires a field name.");
        else if(fp_rpc_entry(app, response)) {
            FlipPassOutputTransport transport;
            KDBXEntry* entry = app->active_entry;
            const char* log_prefix = "USB";
            if(!fp_rpc_transport(cmd.count >= 3U ? cmd.part[2] : "usb", &transport)) {
                fp_rpc_fail(app, response, FlipPassRpcErrorUnsupportedTransport, "Unknown transport. Use usb, bt, ble, or bluetooth.");
            } else {
                bool typed = false;
                if(!fp_rpc_prepare_entry(app, entry, false, response)) {
                    typed = false;
                } else {
                    log_prefix = (transport == FlipPassOutputTransportBluetooth) ? "BT" : "USB";
                    if(fp_rpc_eq(cmd.part[1], "username")) typed = entry->username && entry->username[0] != '\0' && flippass_output_type_string(app, transport, entry->username);
                    else if(fp_rpc_eq(cmd.part[1], "password")) typed = entry->password && entry->password[0] != '\0' && flippass_output_type_string(app, transport, entry->password);
                    else if(fp_rpc_eq(cmd.part[1], "login")) typed = entry->username && entry->password && flippass_output_type_login(app, transport, entry->username, entry->password);
                    else if(fp_rpc_eq(cmd.part[1], "autotype")) typed = flippass_output_type_autotype(app, transport, entry);
                    else fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Unknown type action."), typed = false;
                }
                if(typed) {
                    flippass_log_event(app, "%s_TYPE_OK field=%s", log_prefix, cmd.part[1]);
                    ok = true;
                    furi_string_set_str(response, "{\"ok\":true,\"transport\":");
                    fp_rpc_json(response, flippass_output_transport_name(transport));
                    furi_string_cat(response, ",\"action\":");
                    fp_rpc_json(response, cmd.part[1]);
                    furi_string_cat(response, "}");
                } else if(furi_string_empty(response)) {
                    flippass_log_event(app, "%s_TYPE_FAIL field=%s", log_prefix, cmd.part[1]);
                    fp_rpc_fail(app, response, FlipPassRpcErrorOperationFailed, "Typing failed because the transport was unavailable, not connected, or unsupported by the selected entry.");
                }
            }
        }
    } else {
        fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Unknown RPC command.");
    }

    fp_rpc_free(&cmd);
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
        rpc_system_app_confirm(app->rpc, true);
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        break;
    case RpcAppEventTypeLoadFile:
        ok = fp_rpc_set_file(app, event->data.string, response);
        fp_rpc_send(app, response);
        rpc_system_app_confirm(app->rpc, ok);
        break;
    case RpcAppEventTypeDataExchange:
        ok = fp_rpc_exec(app, event->data.bytes.ptr, event->data.bytes.size, response);
        fp_rpc_send(app, response);
        rpc_system_app_confirm(app->rpc, ok);
        break;
    case RpcAppEventTypeButtonPress:
    case RpcAppEventTypeButtonRelease:
    case RpcAppEventTypeButtonPressRelease:
        fp_rpc_fail(app, response, FlipPassRpcErrorBadCommand, "Button-based app RPC is not implemented yet. Use data exchange commands instead.");
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
    app->rpc = NULL;
    app->rpc_mode = false;
    if(args == NULL || args[0] == '\0') return false;
    if(sscanf(args, "RPC %08lX", &rpc_ctx) != 1) return false;
    app->rpc = (RpcAppSystem*)rpc_ctx;
    app->rpc_mode = true;
    rpc_system_app_set_callback(app->rpc, fp_rpc_callback, app);
    rpc_system_app_send_started(app->rpc);
    return true;
}

void flippass_rpc_deinit(App* app) {
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
