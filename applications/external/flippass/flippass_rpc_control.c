#include "flippass_rpc_control.h"

#include "flippass_db.h"
#include "scenes/flippass_scene.h"
#include "scenes/flippass_scene_db_entries.h"
#include "scenes/flippass_scene_status.h"

#include <ctype.h>
#include <string.h>

enum {
    FlipPassRpcCustomEventSessionClose = 0xF100,
    FlipPassRpcCustomEventExit,
    FlipPassRpcCustomEventLoadFile,
    FlipPassRpcCustomEventCommand,
};

typedef enum {
    FlipPassRpcVisibleItemUp,
    FlipPassRpcVisibleItemGroup,
    FlipPassRpcVisibleItemEntry,
} FlipPassRpcVisibleItemType;

static void flippass_rpc_command_callback(const RpcAppSystemEvent* event, void* context);
static bool flippass_rpc_handle_load_file_event(App* app);
static bool flippass_rpc_handle_command_event(App* app);
static bool flippass_rpc_execute_command(App* app, const char* command);
static bool flippass_rpc_command_match(const char* command, const char* keyword, const char** arg);
static const char* flippass_rpc_skip_spaces(const char* text);
static void flippass_rpc_append_json_string(FuriString* out, const char* value);
static void flippass_rpc_append_group_path(FuriString* out, const KDBXGroup* group);
static size_t flippass_rpc_count_groups(const KDBXGroup* group);
static size_t flippass_rpc_count_entries(const KDBXGroup* group);
static bool flippass_rpc_resolve_visible_index(
    App* app,
    uint32_t index,
    FlipPassRpcVisibleItemType* type,
    KDBXGroup** group,
    KDBXEntry** entry);
static bool flippass_rpc_send_response(App* app, FuriString* response);
static bool flippass_rpc_reply_error(App* app, uint32_t code, const char* message);
static bool flippass_rpc_focus_scene(App* app, uint32_t scene);
static bool flippass_rpc_handle_unlock(App* app, const char* password);
static bool flippass_rpc_handle_list(App* app);
static bool flippass_rpc_handle_open(App* app, const char* arg);
static bool flippass_rpc_handle_up(App* app);
static bool flippass_rpc_handle_status(App* app);
static bool flippass_rpc_handle_show(App* app);
static bool flippass_rpc_handle_read(App* app, const char* field);
static bool flippass_rpc_handle_type(App* app, const char* args);
static bool flippass_rpc_handle_help(App* app);

bool flippass_rpc_init(App* app, const char* args) {
    furi_assert(app);

    if(args == NULL || args[0] == '\0') {
        return false;
    }

    uint32_t rpc_ctx = 0;
    if(sscanf(args, "RPC %lX", &rpc_ctx) == 1) {
        app->rpc_ctx = (RpcAppSystem*)rpc_ctx;
        rpc_system_app_set_callback(app->rpc_ctx, flippass_rpc_command_callback, app);
        rpc_system_app_send_started(app->rpc_ctx);
        return true;
    }

    return false;
}

void flippass_rpc_deinit(App* app) {
    furi_assert(app);

    if(app->rpc_ctx != NULL) {
        rpc_system_app_set_callback(app->rpc_ctx, NULL, NULL);
        rpc_system_app_send_exited(app->rpc_ctx);
        app->rpc_ctx = NULL;
    }
}

bool flippass_rpc_handle_custom_event(App* app, uint32_t event) {
    furi_assert(app);

    switch(event) {
    case FlipPassRpcCustomEventSessionClose:
        flippass_request_exit(app);
        return true;
    case FlipPassRpcCustomEventExit:
        if(app->rpc_ctx != NULL) {
            rpc_system_app_confirm(app->rpc_ctx, true);
        }
        flippass_request_exit(app);
        return true;
    case FlipPassRpcCustomEventLoadFile:
        return flippass_rpc_handle_load_file_event(app);
    case FlipPassRpcCustomEventCommand:
        return flippass_rpc_handle_command_event(app);
    default:
        return false;
    }
}

static void flippass_rpc_command_callback(const RpcAppSystemEvent* event, void* context) {
    furi_assert(context);
    App* app = context;
    furi_assert(app);

    if(event->type == RpcAppEventTypeSessionClose) {
        if(app->rpc_ctx != NULL) {
            rpc_system_app_set_callback(app->rpc_ctx, NULL, NULL);
            app->rpc_ctx = NULL;
        }
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipPassRpcCustomEventSessionClose);
        return;
    }

    if(app->rpc_ctx == NULL) {
        return;
    }

    if(event->type == RpcAppEventTypeAppExit) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassRpcCustomEventExit);
    } else if(event->type == RpcAppEventTypeLoadFile) {
        if(event->data.type != RpcAppSystemEventDataTypeString || event->data.string == NULL) {
            rpc_system_app_set_error_code(app->rpc_ctx, RpcAppSystemErrorCodeParseFile);
            rpc_system_app_set_error_text(app->rpc_ctx, "Missing KDBX path.");
            rpc_system_app_confirm(app->rpc_ctx, false);
            return;
        }

        furi_string_set_str(app->rpc_pending_file, event->data.string);
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassRpcCustomEventLoadFile);
    } else if(event->type == RpcAppEventTypeDataExchange) {
        if(event->data.type != RpcAppSystemEventDataTypeBytes ||
           event->data.bytes.ptr == NULL || event->data.bytes.size == 0U ||
           event->data.bytes.size >= FLIPPASS_RPC_BUFFER_SIZE) {
            rpc_system_app_set_error_code(app->rpc_ctx, RpcAppSystemErrorCodeInternalParse);
            rpc_system_app_set_error_text(app->rpc_ctx, "Invalid command payload.");
            rpc_system_app_confirm(app->rpc_ctx, false);
            return;
        }

        char buffer[FLIPPASS_RPC_BUFFER_SIZE];
        memcpy(buffer, event->data.bytes.ptr, event->data.bytes.size);
        buffer[event->data.bytes.size] = '\0';
        furi_string_set_str(app->rpc_pending_command, buffer);
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassRpcCustomEventCommand);
    } else {
        rpc_system_app_confirm(app->rpc_ctx, false);
    }
}

static bool flippass_rpc_handle_load_file_event(App* app) {
    furi_assert(app);

    if(app->rpc_ctx == NULL) {
        return true;
    }

    const char* file_path = furi_string_get_cstr(app->rpc_pending_file);
    if(file_path == NULL || file_path[0] == '\0') {
        rpc_system_app_set_error_code(app->rpc_ctx, RpcAppSystemErrorCodeParseFile);
        rpc_system_app_set_error_text(app->rpc_ctx, "Missing KDBX path.");
        rpc_system_app_confirm(app->rpc_ctx, false);
        return true;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool exists = storage_file_exists(storage, file_path);
    furi_record_close(RECORD_STORAGE);

    if(!exists) {
        rpc_system_app_set_error_code(app->rpc_ctx, RpcAppSystemErrorCodeParseFile);
        rpc_system_app_set_error_text(app->rpc_ctx, "The requested KDBX file does not exist.");
        rpc_system_app_confirm(app->rpc_ctx, false);
        return true;
    }

    furi_string_set(app->file_path, app->rpc_pending_file);
    flippass_save_settings(app);
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_set_status(app, "FlipPass", "Database selected over RPC.");
    flippass_rpc_focus_scene(app, FlipPassScene_PasswordEntry);
    rpc_system_app_error_reset(app->rpc_ctx);
    rpc_system_app_confirm(app->rpc_ctx, true);
    return true;
}

static bool flippass_rpc_handle_command_event(App* app) {
    furi_assert(app);

    if(app->rpc_ctx == NULL) {
        return true;
    }

    rpc_system_app_error_reset(app->rpc_ctx);
    const bool result = flippass_rpc_execute_command(
        app, furi_string_get_cstr(app->rpc_pending_command));
    rpc_system_app_confirm(app->rpc_ctx, result);
    return true;
}

static bool flippass_rpc_execute_command(App* app, const char* command) {
    const char* arg = NULL;

    if(command == NULL) {
        return flippass_rpc_reply_error(app, RpcAppSystemErrorCodeInternalParse, "Missing command.");
    }

    command = flippass_rpc_skip_spaces(command);
    if(command[0] == '\0') {
        return flippass_rpc_reply_error(app, RpcAppSystemErrorCodeInternalParse, "Empty command.");
    }

    if(flippass_rpc_command_match(command, "HELP", &arg)) {
        return flippass_rpc_handle_help(app);
    } else if(flippass_rpc_command_match(command, "STATUS", &arg)) {
        return flippass_rpc_handle_status(app);
    } else if(flippass_rpc_command_match(command, "LOAD", &arg)) {
        furi_string_set_str(app->rpc_pending_file, flippass_rpc_skip_spaces(arg));
        return flippass_rpc_handle_load_file_event(app);
    } else if(flippass_rpc_command_match(command, "UNLOCK", &arg)) {
        return flippass_rpc_handle_unlock(app, flippass_rpc_skip_spaces(arg));
    } else if(flippass_rpc_command_match(command, "LIST", &arg)) {
        return flippass_rpc_handle_list(app);
    } else if(flippass_rpc_command_match(command, "OPEN", &arg)) {
        return flippass_rpc_handle_open(app, flippass_rpc_skip_spaces(arg));
    } else if(flippass_rpc_command_match(command, "UP", &arg)) {
        return flippass_rpc_handle_up(app);
    } else if(flippass_rpc_command_match(command, "SHOW", &arg)) {
        return flippass_rpc_handle_show(app);
    } else if(flippass_rpc_command_match(command, "READ", &arg)) {
        return flippass_rpc_handle_read(app, flippass_rpc_skip_spaces(arg));
    } else if(flippass_rpc_command_match(command, "TYPE", &arg)) {
        return flippass_rpc_handle_type(app, flippass_rpc_skip_spaces(arg));
    } else if(flippass_rpc_command_match(command, "EXIT", &arg)) {
        FuriString* response = furi_string_alloc();
        furi_string_set_str(response, "{\"ok\":true,\"message\":\"FlipPass will exit.\"}");
        flippass_rpc_send_response(app, response);
        furi_string_free(response);
        flippass_request_exit(app);
        return true;
    }

    return flippass_rpc_reply_error(
        app, RpcAppSystemErrorCodeInternalParse, "Unknown command. Use HELP.");
}

static const char* flippass_rpc_skip_spaces(const char* text) {
    while(text && *text == ' ') {
        text++;
    }
    return text;
}

static bool flippass_rpc_command_match(const char* command, const char* keyword, const char** arg) {
    size_t i = 0U;
    while(command[i] != '\0' && keyword[i] != '\0') {
        if(tolower((unsigned char)command[i]) != tolower((unsigned char)keyword[i])) {
            return false;
        }
        i++;
    }

    if(keyword[i] != '\0') {
        return false;
    }

    if(command[i] == '\0') {
        if(arg != NULL) {
            *arg = command + i;
        }
        return true;
    }

    if(command[i] == ' ') {
        if(arg != NULL) {
            *arg = command + i + 1U;
        }
        return true;
    }

    return false;
}

static void flippass_rpc_append_json_string(FuriString* out, const char* value) {
    furi_assert(out);

    furi_string_cat(out, "\"");
    if(value != NULL) {
        for(size_t i = 0; value[i] != '\0'; i++) {
            switch(value[i]) {
            case '\\':
                furi_string_cat(out, "\\\\");
                break;
            case '"':
                furi_string_cat(out, "\\\"");
                break;
            case '\n':
                furi_string_cat(out, "\\n");
                break;
            case '\r':
                furi_string_cat(out, "\\r");
                break;
            case '\t':
                furi_string_cat(out, "\\t");
                break;
            default:
                furi_string_cat_printf(out, "%c", value[i]);
                break;
            }
        }
    }
    furi_string_cat(out, "\"");
}

static void flippass_rpc_append_group_path(FuriString* out, const KDBXGroup* group) {
    if(group == NULL) {
        return;
    }

    if(group->parent != NULL) {
        flippass_rpc_append_group_path(out, group->parent);
        furi_string_cat(out, " / ");
    }

    furi_string_cat(out, (group->name && group->name[0] != '\0') ? group->name : "Unnamed Group");
}

static size_t flippass_rpc_count_groups(const KDBXGroup* group) {
    if(group == NULL) {
        return 0U;
    }

    return 1U + flippass_rpc_count_groups(group->children) + flippass_rpc_count_groups(group->next);
}

static size_t flippass_rpc_count_entries(const KDBXGroup* group) {
    if(group == NULL) {
        return 0U;
    }

    size_t count = 0U;
    const KDBXEntry* entry = group->entries;
    while(entry != NULL) {
        count++;
        entry = entry->next;
    }

    return count + flippass_rpc_count_entries(group->children) + flippass_rpc_count_entries(group->next);
}

static bool flippass_rpc_resolve_visible_index(
    App* app,
    uint32_t index,
    FlipPassRpcVisibleItemType* type,
    KDBXGroup** group,
    KDBXEntry** entry) {
    furi_assert(app);

    uint32_t current = 0U;
    if(app->current_group != NULL && app->current_group->parent != NULL) {
        if(index == current) {
            *type = FlipPassRpcVisibleItemUp;
            *group = NULL;
            *entry = NULL;
            return true;
        }
        current++;
    }

    KDBXGroup* child_group = app->current_group ? app->current_group->children : NULL;
    while(child_group != NULL) {
        if(index == current) {
            *type = FlipPassRpcVisibleItemGroup;
            *group = child_group;
            *entry = NULL;
            return true;
        }
        current++;
        child_group = child_group->next;
    }

    KDBXEntry* child_entry = app->current_group ? app->current_group->entries : NULL;
    while(child_entry != NULL) {
        if(index == current) {
            *type = FlipPassRpcVisibleItemEntry;
            *group = NULL;
            *entry = child_entry;
            return true;
        }
        current++;
        child_entry = child_entry->next;
    }

    return false;
}

static bool flippass_rpc_send_response(App* app, FuriString* response) {
    if(app->rpc_ctx == NULL || response == NULL) {
        return false;
    }

    const char* text = furi_string_get_cstr(response);
    const size_t size = strlen(text);
    rpc_system_app_exchange_data(app->rpc_ctx, (const uint8_t*)text, size);
    return true;
}

static bool flippass_rpc_reply_error(App* app, uint32_t code, const char* message) {
    if(app->rpc_ctx != NULL) {
        rpc_system_app_set_error_code(app->rpc_ctx, code);
        rpc_system_app_set_error_text(app->rpc_ctx, message);
    }

    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":false,\"error\":");
    flippass_rpc_append_json_string(response, message);
    furi_string_cat(response, "}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    return false;
}

static bool flippass_rpc_focus_scene(App* app, uint32_t scene) {
    if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, scene)) {
        scene_manager_next_scene(app->scene_manager, scene);
    }
    return true;
}

static bool flippass_rpc_handle_unlock(App* app, const char* password) {
    if(password == NULL || password[0] == '\0') {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeParseFile, "Missing master password.");
    }

    snprintf(app->master_password, sizeof(app->master_password), "%s", password);

    FuriString* error = furi_string_alloc();
    const bool loaded = flippass_db_load(app, error);
    if(!loaded) {
        const char* message = furi_string_get_cstr(error);
        const bool result = flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeParseFile, message[0] != '\0' ? message : "Unlock failed.");
        furi_string_free(error);
        return result;
    }
    furi_string_free(error);

    flippass_rpc_focus_scene(app, FlipPassScene_DbEntries);

    FuriString* group_path = furi_string_alloc();
    if(app->current_group != NULL) {
        flippass_rpc_append_group_path(group_path, app->current_group);
    }

    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"database_loaded\":true,\"groups\":");
    furi_string_cat_printf(response, "%lu", (unsigned long)flippass_rpc_count_groups(app->root_group));
    furi_string_cat(response, ",\"entries\":");
    furi_string_cat_printf(response, "%lu", (unsigned long)flippass_rpc_count_entries(app->root_group));
    furi_string_cat(response, ",\"current_group\":");
    flippass_rpc_append_json_string(response, furi_string_get_cstr(group_path));
    furi_string_cat(response, "}");

    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    furi_string_free(group_path);
    return true;
}

static bool flippass_rpc_handle_list(App* app) {
    if(!app->database_loaded || app->current_group == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeParseFile, "No unlocked database is active.");
    }

    FuriString* group_path = furi_string_alloc();
    flippass_rpc_append_group_path(group_path, app->current_group);

    FuriString* response = furi_string_alloc();
    FuriString* title = furi_string_alloc();
    FuriString* uuid = furi_string_alloc();
    FuriString* error = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"current_group\":");
    flippass_rpc_append_json_string(response, furi_string_get_cstr(group_path));
    furi_string_cat(response, ",\"items\":[");

    bool first = true;
    uint32_t index = 0U;
    if(app->current_group->parent != NULL) {
        furi_string_cat_printf(
            response,
            "%s{\"index\":%lu,\"type\":\"up\",\"title\":\"Parent\"}",
            first ? "" : ",",
            (unsigned long)index++);
        first = false;
    }

    for(KDBXGroup* group = app->current_group->children; group != NULL; group = group->next) {
        furi_string_cat_printf(
            response,
            "%s{\"index\":%lu,\"type\":\"group\",\"title\":",
            first ? "" : ",",
            (unsigned long)index++);
        flippass_rpc_append_json_string(
            response, (group->name && group->name[0] != '\0') ? group->name : "Unnamed Group");
        furi_string_cat(response, ",\"uuid\":");
        flippass_rpc_append_json_string(response, group->uuid);
        furi_string_cat(response, "}");
        first = false;
    }

    for(KDBXEntry* entry = app->current_group->entries; entry != NULL; entry = entry->next) {
        if(!flippass_db_copy_entry_title(app, entry, title, error)) {
            const bool replied = flippass_rpc_reply_error(
                app, RpcAppSystemErrorCodeParseFile, furi_string_get_cstr(error));
            furi_string_free(error);
            furi_string_free(uuid);
            furi_string_free(title);
            furi_string_free(response);
            furi_string_free(group_path);
            return replied;
        }
        if(!flippass_db_copy_entry_uuid(app, entry, uuid, error)) {
            const bool replied = flippass_rpc_reply_error(
                app, RpcAppSystemErrorCodeParseFile, furi_string_get_cstr(error));
            furi_string_free(error);
            furi_string_free(uuid);
            furi_string_free(title);
            furi_string_free(response);
            furi_string_free(group_path);
            return replied;
        }
        furi_string_cat_printf(
            response,
            "%s{\"index\":%lu,\"type\":\"entry\",\"title\":",
            first ? "" : ",",
            (unsigned long)index++);
        flippass_rpc_append_json_string(
            response,
            (furi_string_size(title) > 0U) ? furi_string_get_cstr(title) : "Untitled Entry");
        furi_string_cat(response, ",\"uuid\":");
        flippass_rpc_append_json_string(response, furi_string_get_cstr(uuid));
        furi_string_cat(response, "}");
        first = false;
    }

    furi_string_cat(response, "]}");
    flippass_rpc_send_response(app, response);
    furi_string_free(error);
    furi_string_free(uuid);
    furi_string_free(title);
    furi_string_free(response);
    furi_string_free(group_path);
    return true;
}

static bool flippass_rpc_handle_open(App* app, const char* arg) {
    if(!app->database_loaded || app->current_group == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeParseFile, "No unlocked database is active.");
    }

    if(arg == NULL || arg[0] == '\0') {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Missing visible item index.");
    }

    char* end = NULL;
    const long parsed = strtol(arg, &end, 10);
    if(end == arg || parsed < 0) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Invalid visible item index.");
    }

    FlipPassRpcVisibleItemType type;
    KDBXGroup* group = NULL;
    KDBXEntry* entry = NULL;
    if(!flippass_rpc_resolve_visible_index(app, (uint32_t)parsed, &type, &group, &entry)) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Visible item index is out of range.");
    }

    FuriString* response = furi_string_alloc();
    switch(type) {
    case FlipPassRpcVisibleItemUp:
        app->current_group = app->current_group->parent;
        app->current_entry = NULL;
        app->active_group = app->current_group;
        app->active_entry = NULL;
        furi_string_set_str(response, "{\"ok\":true,\"type\":\"up\",\"current_group\":");
        break;
    case FlipPassRpcVisibleItemGroup:
        app->current_group = group;
        app->current_entry = NULL;
        app->active_group = group;
        app->active_entry = NULL;
        furi_string_set_str(response, "{\"ok\":true,\"type\":\"group\",\"current_group\":");
        break;
    case FlipPassRpcVisibleItemEntry:
        app->current_entry = entry;
        app->active_group = app->current_group;
        app->active_entry = entry;
        furi_string_set_str(response, "{\"ok\":true,\"type\":\"entry\",\"entry_title\":");
        flippass_rpc_append_json_string(
            response, (entry->title && entry->title[0] != '\0') ? entry->title : "Untitled Entry");
        furi_string_cat(response, "}");
        flippass_rpc_send_response(app, response);
        furi_string_free(response);
        return true;
    }

    FuriString* group_path = furi_string_alloc();
    flippass_rpc_append_group_path(group_path, app->current_group);
    flippass_rpc_append_json_string(response, furi_string_get_cstr(group_path));
    furi_string_cat(response, "}");
    flippass_rpc_send_response(app, response);
    furi_string_free(group_path);
    furi_string_free(response);
    return true;
}

static bool flippass_rpc_handle_up(App* app) {
    if(!app->database_loaded || app->current_group == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeParseFile, "No unlocked database is active.");
    }

    if(app->current_group->parent == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Already at the root group.");
    }

    app->current_group = app->current_group->parent;
    app->current_entry = NULL;
    app->active_group = app->current_group;
    app->active_entry = NULL;

    FuriString* group_path = furi_string_alloc();
    flippass_rpc_append_group_path(group_path, app->current_group);
    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"current_group\":");
    flippass_rpc_append_json_string(response, furi_string_get_cstr(group_path));
    furi_string_cat(response, "}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    furi_string_free(group_path);
    return true;
}

static bool flippass_rpc_handle_status(App* app) {
    FuriString* response = furi_string_alloc();
    FuriString* group_path = furi_string_alloc();

    if(app->current_group != NULL) {
        flippass_rpc_append_group_path(group_path, app->current_group);
    }

    furi_string_set_str(response, "{\"ok\":true,\"database_loaded\":");
    furi_string_cat(response, app->database_loaded ? "true" : "false");
    furi_string_cat(response, ",\"file_path\":");
    flippass_rpc_append_json_string(response, furi_string_get_cstr(app->file_path));
    furi_string_cat(response, ",\"current_group\":");
    flippass_rpc_append_json_string(response, furi_string_get_cstr(group_path));
    furi_string_cat(response, ",\"active_entry\":");
    flippass_rpc_append_json_string(
        response,
        app->active_entry && app->active_entry->title ? app->active_entry->title : "");
    furi_string_cat(response, ",\"supports_usb\":true,\"supports_bluetooth\":true}");

    flippass_rpc_send_response(app, response);
    furi_string_free(group_path);
    furi_string_free(response);
    return true;
}

static bool flippass_rpc_handle_show(App* app) {
    KDBXEntry* entry = app->active_entry;
    if(entry == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "No active entry is selected.");
    }

    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"entry\":{");
    furi_string_cat(response, "\"uuid\":");
    flippass_rpc_append_json_string(response, entry->uuid);
    furi_string_cat(response, ",\"title\":");
    flippass_rpc_append_json_string(response, entry->title);
    furi_string_cat(response, ",\"username\":");
    flippass_rpc_append_json_string(response, entry->username);
    furi_string_cat(response, ",\"password\":");
    flippass_rpc_append_json_string(response, entry->password);
    furi_string_cat(response, ",\"url\":");
    flippass_rpc_append_json_string(response, entry->url);
    furi_string_cat(response, ",\"notes\":");
    flippass_rpc_append_json_string(response, entry->notes);
    furi_string_cat(response, "}}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    return true;
}

static bool flippass_rpc_handle_read(App* app, const char* field) {
    KDBXEntry* entry = app->active_entry;
    if(entry == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "No active entry is selected.");
    }

    if(field == NULL || field[0] == '\0') {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Missing field name.");
    }

    const char* value = NULL;
    if(strcasecmp(field, "uuid") == 0) {
        value = entry->uuid;
    } else if(strcasecmp(field, "title") == 0) {
        value = entry->title;
    } else if(strcasecmp(field, "username") == 0) {
        value = entry->username;
    } else if(strcasecmp(field, "password") == 0) {
        value = entry->password;
    } else if(strcasecmp(field, "url") == 0) {
        value = entry->url;
    } else if(strcasecmp(field, "notes") == 0) {
        value = entry->notes;
    } else if(strcasecmp(field, "autotype") == 0 || strcasecmp(field, "login") == 0) {
        FuriString* response = furi_string_alloc();
        furi_string_set_str(response, "{\"ok\":true,\"field\":\"autotype\",\"sequence\":[");
        bool first = true;
        if(entry->username && entry->username[0] != '\0') {
            furi_string_cat(response, first ? "" : ",");
            flippass_rpc_append_json_string(response, entry->username);
            first = false;
        }
        furi_string_cat(response, first ? "" : ",");
        flippass_rpc_append_json_string(response, "<TAB>");
        first = false;
        if(entry->password && entry->password[0] != '\0') {
            furi_string_cat(response, first ? "" : ",");
            flippass_rpc_append_json_string(response, entry->password);
            first = false;
        }
        furi_string_cat(response, first ? "" : ",");
        flippass_rpc_append_json_string(response, "<ENTER>");
        furi_string_cat(response, "]}");
        flippass_rpc_send_response(app, response);
        furi_string_free(response);
        return true;
    } else {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Unsupported field name.");
    }

    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"field\":");
    flippass_rpc_append_json_string(response, field);
    furi_string_cat(response, ",\"value\":");
    flippass_rpc_append_json_string(response, value);
    furi_string_cat(response, "}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    return true;
}

static bool flippass_rpc_handle_type(App* app, const char* args) {
    KDBXEntry* entry = app->active_entry;
    if(entry == NULL) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "No active entry is selected.");
    }

    if(args == NULL || args[0] == '\0') {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Missing type target.");
    }

    char field[32] = {0};
    char transport[32] = {0};
    const int parsed = sscanf(args, "%31s %31s", field, transport);
    if(parsed < 1) {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Missing type target.");
    }

    const char* selected_transport = (parsed >= 2 && transport[0] != '\0') ? transport : "usb";
    FlipPassOutputTransport output_transport = FlipPassOutputTransportUsb;
    if(strcasecmp(selected_transport, "usb") == 0 || strcasecmp(selected_transport, "auto") == 0) {
        output_transport = FlipPassOutputTransportUsb;
        selected_transport = "usb";
    } else if(
        strcasecmp(selected_transport, "bluetooth") == 0 || strcasecmp(selected_transport, "bt") == 0 ||
        strcasecmp(selected_transport, "ble") == 0) {
        output_transport = FlipPassOutputTransportBluetooth;
        selected_transport = "bluetooth";
    } else {
        return flippass_rpc_reply_error(
            app,
            RpcAppSystemErrorCodeInternalParse,
            "Unsupported transport. Use usb, bluetooth, bt, ble, or auto.");
    }

    bool typed = false;
    const char* field_name = field;
    const char* log_prefix =
        (output_transport == FlipPassOutputTransportBluetooth) ? "BT" : "USB";

    if(strcasecmp(field, "username") == 0) {
        if(entry->username == NULL || entry->username[0] == '\0') {
            return flippass_rpc_reply_error(
                app, RpcAppSystemErrorCodeParseFile, "The active entry has no username.");
        }
        flippass_log_event(
            app, "%s_TYPE_BEGIN field=username chars=%lu", log_prefix, (unsigned long)strlen(entry->username));
        typed = flippass_output_type_string(app, output_transport, entry->username);
        flippass_log_event(app, typed ? "%s_TYPE_OK field=username" : "%s_TYPE_FAIL field=username", log_prefix);
    } else if(strcasecmp(field, "password") == 0) {
        if(entry->password == NULL || entry->password[0] == '\0') {
            return flippass_rpc_reply_error(
                app, RpcAppSystemErrorCodeParseFile, "The active entry has no password.");
        }
        flippass_log_event(
            app, "%s_TYPE_BEGIN field=password chars=%lu", log_prefix, (unsigned long)strlen(entry->password));
        typed = flippass_output_type_string(app, output_transport, entry->password);
        flippass_log_event(app, typed ? "%s_TYPE_OK field=password" : "%s_TYPE_FAIL field=password", log_prefix);
    } else if(strcasecmp(field, "login") == 0) {
        if(entry->username == NULL || entry->username[0] == '\0' ||
           entry->password == NULL || entry->password[0] == '\0') {
            return flippass_rpc_reply_error(
                app,
                RpcAppSystemErrorCodeParseFile,
                "The active entry needs both username and password for login typing.");
        }
        flippass_log_event(
            app,
            "%s_TYPE_BEGIN field=login chars=%lu",
            log_prefix,
            (unsigned long)(strlen(entry->username) + strlen(entry->password) + 2U));
        typed = flippass_output_type_login(app, output_transport, entry->username, entry->password);
        field_name = "login";
        flippass_log_event(app, typed ? "%s_TYPE_OK field=login" : "%s_TYPE_FAIL field=login", log_prefix);
    } else if(strcasecmp(field, "autotype") == 0) {
        if((entry->autotype_sequence == NULL || entry->autotype_sequence[0] == '\0') &&
           (entry->username == NULL || entry->username[0] == '\0' ||
            entry->password == NULL || entry->password[0] == '\0')) {
            return flippass_rpc_reply_error(
                app,
                RpcAppSystemErrorCodeParseFile,
                "The active entry needs an AutoType sequence or both username and password.");
        }
        flippass_log_event(
            app,
            "%s_TYPE_BEGIN field=autotype chars=%lu",
            log_prefix,
            (unsigned long)(
                entry->autotype_sequence != NULL ? strlen(entry->autotype_sequence) :
                                                   ((entry->username ? strlen(entry->username) : 0U) +
                                                    (entry->password ? strlen(entry->password) : 0U) + 2U)));
        typed = flippass_output_type_autotype(app, output_transport, entry);
        field_name = "autotype";
        flippass_log_event(
            app, typed ? "%s_TYPE_OK field=autotype" : "%s_TYPE_FAIL field=autotype", log_prefix);
    } else {
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, "Unsupported type target.");
    }

    if(!typed) {
        const char* error_text =
            (output_transport == FlipPassOutputTransportBluetooth) ?
                "Typing failed because Bluetooth HID was unavailable, not connected, or the selected data uses unsupported characters or AutoType tokens." :
                "Typing failed because USB HID was unavailable or the selected data uses unsupported characters or AutoType tokens.";
        return flippass_rpc_reply_error(
            app, RpcAppSystemErrorCodeInternalParse, error_text);
    }

    FuriString* response = furi_string_alloc();
    furi_string_set_str(response, "{\"ok\":true,\"typed\":");
    flippass_rpc_append_json_string(response, field_name);
    furi_string_cat(response, ",\"transport\":");
    flippass_rpc_append_json_string(response, selected_transport);
    furi_string_cat(response, "}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    return true;
}

static bool flippass_rpc_handle_help(App* app) {
    FuriString* response = furi_string_alloc();
    furi_string_set_str(
        response,
        "{\"ok\":true,\"commands\":[\"STATUS\",\"LOAD <path>\",\"UNLOCK <password>\",\"LIST\",\"OPEN <index>\",\"UP\",\"SHOW\",\"READ <field>\",\"TYPE <field> [usb|bluetooth]\",\"EXIT\"]}");
    flippass_rpc_send_response(app, response);
    furi_string_free(response);
    return true;
}
