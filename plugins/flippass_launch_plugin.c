#include "flippass_launch_plugin.h"
#include "flippass_launch_auto.h"
#include "../flippass.h"
#include "../flippass_launch.h"
#include "../flippass_launch_layout.h"
#include "../flippass_deferred_launch.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"
#include "../scenes/flippass_scene_file_browser.h"
#include "../scenes/flippass_scene_db_entries.h"
#include "../scenes/flippass_scene_send_confirm.h"
#include "../scenes/flippass_scene_status.h"
#include "../transport/flippass_output_transport.h"
#include <core/kernel.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FLIPPASS_LAUNCH_CONNECT_TIMEOUT_MS 30000U

static void* launch_alloc(size_t bytes) {
    const int32_t lock = furi_kernel_lock();
    void* result = memmgr_heap_get_max_free_block() >= bytes + 16U ? malloc(bytes) : NULL;
    furi_kernel_restore_lock(lock);
    return result;
}

static void launch_error(App* app, const char* message, uint32_t return_scene) {
    flippass_scene_status_show(app, "Launch Failed", message, return_scene);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

static bool launch_start(App* app, const char* arguments) {
    size_t length = 0U;
    while(length <= FLIPPASS_LAUNCH_ARGS_MAX_BYTES && arguments[length] != '\0')
        ++length;
    if(length > FLIPPASS_LAUNCH_ARGS_MAX_BYTES) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
        launch_error(app, "Launch arguments exceed 4095 bytes.", FlipPassScene_FileBrowser);
        return true;
    }
    /* Preserve existing whole raw file associations with spaces. A parameter
     * list must not become a filename merely because its password ends .kdbx.
     * Quoting the database makes parameterized calls unambiguous. */
    if(arguments[0] == '/' && length <= FLIPPASS_LAUNCH_PATH_MAX_BYTES && length >= 5U &&
       strchr(arguments, ' ') != NULL && strchr(arguments, '"') == NULL &&
       strcmp(arguments + length - 5U, ".kdbx") == 0 &&
       storage_file_exists(app->module_loader.storage, arguments)) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
        app->phantom_open = true;
        furi_string_set_str(app->file_path, arguments);
        if(flippass_file_browser_check_database(app, arguments))
            scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        return true;
    }
    char* scratch = launch_alloc(length + 1U);
    if(scratch == NULL) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
        launch_error(app, "Not enough RAM to read launch arguments.", FlipPassScene_FileBrowser);
        return true;
    }
    FlipPassLaunchArgs parsed;
    const FlipPassLaunchArgsError parse_error =
        flippass_launch_args_parse(arguments, scratch, length + 1U, &parsed);
    if(parse_error == FlipPassLaunchArgsOk && parsed.database_path == NULL) {
        flippass_launch_args_clear(&parsed, scratch, length + 1U);
        free(scratch);
        return false; /* No arguments uses the ordinary remembered-file startup. */
    }
    scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
    if(parse_error != FlipPassLaunchArgsOk) {
        launch_error(
            app, flippass_launch_args_error_message(parse_error), FlipPassScene_FileBrowser);
        goto finish;
    }
    app->phantom_open = true;
    furi_string_set_str(app->file_path, parsed.database_path);
    if(!flippass_file_browser_check_database(app, parsed.database_path)) goto finish;
    if(parsed.password == NULL) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        goto finish; /* Preserve the ordinary pre-password compression offer. */
    }
    char layout_path[FLIPPASS_KEYBOARD_LAYOUT_PATH_SIZE];
    const FlipPassLaunchLayoutResult layout_result = flippass_launch_layout_resolve(
        app->module_loader.storage, parsed.layout_id, layout_path, sizeof(layout_path));
    if(layout_result != FlipPassLaunchLayoutDefault &&
       layout_result != FlipPassLaunchLayoutOverride) {
        launch_error(app, flippass_launch_layout_error(layout_result), FlipPassScene_FileBrowser);
        goto finish;
    }
    const size_t entry_bytes = parsed.entry_path ? strlen(parsed.entry_path) + 1U : 1U;
    const size_t field_bytes = parsed.field_tag ? strlen(parsed.field_tag) + 1U : 1U;
    const size_t layout_bytes =
        layout_result == FlipPassLaunchLayoutOverride ? strlen(layout_path) + 1U : 0U;
    const size_t request_bytes =
        sizeof(FlipPassLaunchRequest) + entry_bytes + field_bytes + layout_bytes;
    FlipPassLaunchRequest* request = launch_alloc(request_bytes);
    if(request == NULL) {
        launch_error(
            app, "Not enough RAM to retain the launch target.", FlipPassScene_FileBrowser);
        goto finish;
    }
    memset(request, 0, request_bytes);
    request->allocation_bytes = request_bytes;
    request->entry_path = request->data;
    request->field_tag = request->data + entry_bytes;
    if(layout_bytes != 0U) {
        request->keyboard_layout_path = request->data + entry_bytes + field_bytes;
        memcpy((char*)request->keyboard_layout_path, layout_path, layout_bytes);
    }
    memcpy(request->data, parsed.entry_path ? parsed.entry_path : "", entry_bytes);
    memcpy(request->data + entry_bytes, parsed.field_tag ? parsed.field_tag : "", field_bytes);
    request->transport = parsed.transport;
    request->autoclose = parsed.autoclose;
    app->launch_request = request;
    /* A launch is an open/send request, never consent to rewrite the database. */
    app->open_header_gzip = false;
    app->save_uncompressed_requested = false;
    app->editor_mode = FlipPassEditorModeNone;
    scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
    flippass_reset_database(app);
    app->allow_ext_vault_promotion = app->always_allow_ext;
    snprintf(app->master_password, sizeof(app->master_password), "%s", parsed.password);
    flippass_clear_text_buffer(app);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
finish:
    flippass_launch_args_clear(&parsed, scratch, length + 1U);
    free(scratch);
    return true;
}

static bool launch_segment_equal(const char* name, const char* segment, size_t length) {
    if(name == NULL) return false;
    size_t offset = 0U;
    for(size_t i = 0U; i < length; ++i) {
        char value = segment[i];
        if(value == '~') {
            if(++i >= length || (segment[i] != '0' && segment[i] != '1')) return false;
            value = segment[i] == '0' ? '~' : '/';
        }
        if(name[offset] == '\0' || name[offset++] != value) return false;
    }
    return name[offset] == '\0';
}

static bool launch_canceled(App* app, FuriString* error) {
    if(!flippass_typing_should_cancel(app)) return false;
    furi_string_set_str(error, "Typing canceled.");
    return true;
}

/* KeePass stores UUID bytes as canonical Base64, with no GUID little-endian
 * field swapping. Accept display hex, dashed hex and {dashed hex}. Explicit
 * uuid: also accepts the stored Base64 spelling, including embedded '/'. */
static bool launch_uuid_normalize(const char* text, char out[25]) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t length = strlen(text);
    if(length == 24U && text[22] == '=' && text[23] == '=') {
        for(size_t i = 0U; i < 22U; ++i) {
            const char* digit = strchr(alphabet, text[i]);
            if(digit == NULL || (i == 21U && ((size_t)(digit - alphabet) & 15U))) return false;
        }
        memcpy(out, text, 25U);
        return true;
    }
    if(length == 38U && text[0] == '{' && text[37] == '}') {
        ++text;
        length -= 2U;
    }
    if(length != 32U && length != 36U) return false;
    uint8_t bytes[16] = {0};
    unsigned digits = 0U;
    for(size_t i = 0U; i < length; ++i) {
        if(length == 36U && (i == 8U || i == 13U || i == 18U || i == 23U)) {
            if(text[i] != '-') return false;
            continue;
        }
        const char c = text[i];
        const int value = c >= '0' && c <= '9' ? c - '0' :
                          c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                          c >= 'A' && c <= 'F' ? c - 'A' + 10 :
                                                 -1;
        if(value < 0 || digits >= 32U) return false;
        bytes[digits / 2U] |= (uint8_t)(value << ((digits & 1U) ? 0U : 4U));
        ++digits;
    }
    unsigned offset = 0U;
    for(unsigned i = 0U; i < 15U; i += 3U) {
        const uint32_t word = ((uint32_t)bytes[i] << 16U) | ((uint32_t)bytes[i + 1U] << 8U) |
                              bytes[i + 2U];
        out[offset++] = alphabet[(word >> 18U) & 63U];
        out[offset++] = alphabet[(word >> 12U) & 63U];
        out[offset++] = alphabet[(word >> 6U) & 63U];
        out[offset++] = alphabet[word & 63U];
    }
    out[20] = alphabet[bytes[15] >> 2U];
    out[21] = alphabet[(bytes[15] & 3U) << 4U];
    memcpy(out + 22U, "==", 3U);
    return true;
}

static KDBXEntry* launch_resolve_uuid(App* app, const char* uuid, FuriString* error) {
    FuriString* value = furi_string_alloc();
    KDBXEntry* selected = NULL;
    KDBXGroup* selected_group = NULL;
    uint32_t selected_index = 0U;
    for(KDBXGroup* group = app->root_group; group != NULL;
        group = kdbx_group_next_in_subtree(group, app->root_group)) {
        uint32_t index = 0U;
        for(KDBXGroup* child = group->children; child != NULL; child = child->next)
            ++index;
        for(KDBXEntry* entry = group->entries; entry != NULL; entry = entry->next, ++index) {
            if(launch_canceled(app, error)) goto failed;
            /* A UUID has 24 encoded bytes. Skip absent/oversized references
             * without allocating an unbounded imported field. */
            if(entry->uuid == NULL && entry->uuid_ref.plain_len != 24U) continue;
            if(!flippass_db_copy_entry_uuid(app, entry, value, error)) goto failed;
            if(strcmp(furi_string_get_cstr(value), uuid) != 0) continue;
            if(selected != NULL) {
                furi_string_set_str(error, "The requested UUID matches multiple entries.");
                goto failed;
            }
            selected = entry;
            selected_group = group;
            selected_index = index;
        }
    }
    furi_string_free(value);
    if(selected != NULL) {
        app->current_group = selected_group;
        app->browser_selected_index = selected_index;
    } else {
        furi_string_set_str(error, "The requested entry UUID was not found.");
    }
    return selected;
failed:
    furi_string_free(value);
    return NULL;
}

/* Resolve one root-relative path without recursion or materializing credentials.
 * Duplicate names are errors rather than a first-match credential selection. */
static KDBXEntry* launch_resolve_entry(App* app, const char* path, FuriString* error) {
    if(launch_canceled(app, error)) return NULL;
    char uuid[25];
    const bool explicit_uuid = strncmp(path, "uuid:", 5U) == 0;
    if(explicit_uuid || path[0] != '/') {
        if(launch_uuid_normalize(explicit_uuid ? path + 5U : path, uuid))
            return launch_resolve_uuid(app, uuid, error);
        if(explicit_uuid) {
            furi_string_set_str(error, "Invalid entry UUID.");
            return NULL;
        }
    }
    KDBXGroup* group = app->root_group;
    const char* segment = path[0] == '/' ? path + 1 : path;
    while(group != NULL) {
        if(launch_canceled(app, error)) return NULL;
        const char* slash = strchr(segment, '/');
        const size_t size = slash ? (size_t)(slash - segment) : strlen(segment);
        if(size == 0U) break;
        if(slash != NULL) {
            KDBXGroup* selected = NULL;
            for(KDBXGroup* child = group->children; child != NULL; child = child->next) {
                if(!launch_segment_equal(child->name, segment, size)) continue;
                if(selected != NULL) {
                    furi_string_set_str(error, "The entry path contains ambiguous group names.");
                    return NULL;
                }
                selected = child;
            }
            group = selected;
            segment = slash + 1;
            continue;
        }
        FuriString* title = furi_string_alloc();
        KDBXEntry* selected = NULL;
        uint32_t index = 0U, selected_index = 0U;
        for(KDBXGroup* child = group->children; child != NULL; child = child->next)
            ++index;
        for(KDBXEntry* entry = group->entries; entry != NULL; entry = entry->next, ++index) {
            if(launch_canceled(app, error)) {
                selected = NULL;
                break;
            }
            const char* name = entry->title;
            if(name == NULL) {
                /* A longer byte string cannot match this encoded path segment.
                 * Bound temporary title materialization by the path limit. */
                if(entry->title_ref.plain_len > size) continue;
                if(!flippass_db_copy_entry_title(app, entry, title, error)) {
                    selected = NULL;
                    break;
                }
                flippass_db_clear_entry_title_cached(entry);
                name = furi_string_get_cstr(title);
            }
            if(!launch_segment_equal(name, segment, size)) continue;
            if(selected != NULL) {
                furi_string_set_str(error, "The entry path matches multiple entry titles.");
                selected = NULL;
                break;
            }
            selected = entry;
            selected_index = index;
        }
        furi_string_free(title);
        flippass_db_browser_clear_title_cache(app);
        if(selected != NULL) {
            app->current_group = group;
            app->browser_selected_index = selected_index;
            return selected;
        }
        break;
    }
    if(furi_string_empty(error))
        furi_string_set_str(error, "The requested entry path was not found.");
    return NULL;
}

#if FLIPPASS_ENABLE_TYPING_ACTIONS
static bool launch_tag_equal(const char* text, const char* expected) {
    while(*text && *expected) {
        char a = *text++, b = *expected++;
        if(a >= 'a' && a <= 'z') a -= 'a' - 'A';
        if(b >= 'a' && b <= 'z') b -= 'a' - 'A';
        if(a != b) return false;
    }
    return *text == *expected;
}

static bool launch_prepare_field(
    App* app,
    KDBXEntry* entry,
    const char* field,
    bool bluetooth,
    FuriString* error) {
    char tag[FLIPPASS_LAUNCH_FIELD_MAX_BYTES + 1U];
    const size_t length = strlen(field);
    if(length >= 2U && field[0] == '{' && field[length - 1U] == '}') {
        memcpy(tag, field + 1U, length - 2U);
        tag[length - 2U] = '\0';
    } else {
        memcpy(tag, field, length + 1U);
    }
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_otp_kind = FlipPassOtpKindNone;
    FlipPassEntryAction action = bluetooth ? FlipPassEntryActionTypeOtherBluetooth :
                                             FlipPassEntryActionTypeOtherUsb;
    if(launch_tag_equal(tag, "USERNAME"))
        action = bluetooth ? FlipPassEntryActionTypeUsernameBluetooth :
                             FlipPassEntryActionTypeUsernameUsb;
    else if(launch_tag_equal(tag, "PASSWORD"))
        action = bluetooth ? FlipPassEntryActionTypePasswordBluetooth :
                             FlipPassEntryActionTypePasswordUsb;
    else if(launch_tag_equal(tag, "AUTOTYPE"))
        action = bluetooth ? FlipPassEntryActionTypeAutoTypeBluetooth :
                             FlipPassEntryActionTypeAutoTypeUsb;
    else if(launch_tag_equal(tag, "LOGIN"))
        action = bluetooth ? FlipPassEntryActionTypeLoginBluetooth :
                             FlipPassEntryActionTypeLoginUsb;
    else if(launch_tag_equal(tag, "TITLE"))
        app->pending_other_field_mask = KDBXEntryFieldTitle;
    else if(launch_tag_equal(tag, "UUID"))
        app->pending_other_field_mask = KDBXEntryFieldUuid;
    else if(launch_tag_equal(tag, "URL"))
        app->pending_other_field_mask = KDBXEntryFieldUrl;
    else if(launch_tag_equal(tag, "NOTES"))
        app->pending_other_field_mask = KDBXEntryFieldNotes;
    else if(launch_tag_equal(tag, "TIMEOTP") || launch_tag_equal(tag, "TOTP"))
        app->pending_other_otp_kind = FlipPassOtpKindTime;
    else if(launch_tag_equal(tag, "HMACOTP") || launch_tag_equal(tag, "HOTP"))
        app->pending_other_otp_kind = FlipPassOtpKindHmac;
    else if((tag[0] == 'S' || tag[0] == 's') && tag[1] == ':') {
        for(KDBXCustomField* custom = entry->custom_fields; custom != NULL;
            custom = custom->next) {
            if(custom->key != NULL && strcmp(custom->key, tag + 2) == 0) {
                if(app->pending_other_custom_field != NULL) {
                    furi_string_set_str(error, "The custom field name is ambiguous.");
                    return false;
                }
                app->pending_other_custom_field = custom;
            }
        }
        if(app->pending_other_custom_field == NULL) {
            furi_string_set_str(error, "The requested custom field was not found.");
            return false;
        }
    } else {
        furi_string_set_str(error, "The requested field tag is unsupported.");
        return false;
    }
    app->active_entry = entry;
    app->current_entry = entry;
    app->pending_entry_action = action;
    snprintf(
        app->pending_other_field_name,
        sizeof(app->pending_other_field_name),
        "%.*s",
        (int)sizeof(app->pending_other_field_name) - 1,
        tag);
    return true;
}
#endif

static FlipPassLaunchOutcome launch_after_open(App* app) {
    const FlipPassLaunchRequest* request = app->launch_request;
    FuriString* error = furi_string_alloc();
    KDBXEntry* entry = NULL;
#if FLIPPASS_ENABLE_TYPING_ACTIONS
    bool typing_started = request->field_tag[0] != '\0';
    if(typing_started) flippass_typing_begin(app);
#endif
    if(request->entry_path[0] != '\0') {
        /* Scene exit synchronizes selection from the old rows. Drop those
         * rebuildable rows first, so it cannot overwrite the resolved index. */
        flippass_scene_db_entries_trim_for_intermediary_plugin(app);
        entry = launch_resolve_entry(app, request->entry_path, error);
        if(entry == NULL) goto failed;
        const uint32_t selected_index = app->browser_selected_index;
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, FlipPassScene_DbEntries);
        if(scene_manager_get_current_scene(app->scene_manager) != FlipPassScene_DbEntries ||
           app->browser_selected_index != selected_index) {
            furi_string_set_str(
                error, "The requested entry cannot be selected in this browser list.");
            goto failed;
        }
    }
    if(request->field_tag[0] != '\0') {
#if FLIPPASS_ENABLE_TYPING_ACTIONS
        FlipPassOutputTransport transport = request->transport ==
                                                    FlipPassLaunchTransportBluetooth ?
                                                FlipPassOutputTransportBluetooth :
                                                FlipPassOutputTransportUsb;
#if !FLIPPASS_ENABLE_BLUETOOTH_OUTPUT
        if(request->transport == FlipPassLaunchTransportBluetooth) {
            furi_string_set_str(error, "Bluetooth output is not included in this build.");
            goto failed;
        }
#endif
        if(!launch_prepare_field(
               app,
               entry,
               request->field_tag,
               transport == FlipPassOutputTransportBluetooth,
               error))
            goto failed;
        if(launch_canceled(app, error)) goto failed;
        if(request->transport == FlipPassLaunchTransportAuto) {
            flippass_progress_begin(app, "Typing", "Waiting for USB or BT", 0U);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
            if(!flippass_launch_wait_auto(app, &transport, FLIPPASS_LAUNCH_CONNECT_TIMEOUT_MS)) {
                furi_string_set_str(
                    error,
                    flippass_typing_should_cancel(app) ?
                        "Typing canceled." :
                        "No output host connected within 30 seconds.");
                goto failed;
            }
            if(!launch_prepare_field(
                   app,
                   entry,
                   request->field_tag,
                   transport == FlipPassOutputTransportBluetooth,
                   error)) {
                goto failed;
            }
        }
        const bool sent = flippass_entry_action_execute_pending(app, error);
        flippass_typing_end(app);
        typing_started = false;
        flippass_output_cleanup(app);
        flippass_progress_reset(app);
        app->pending_entry_action = FlipPassEntryActionNone;
        app->pending_other_custom_field = NULL;
        if(!sent) goto failed;
        if(app->deferred_launch != NULL && app->deferred_launch->committed) {
            furi_string_free(error);
            return FlipPassLaunchExit;
        }
#else
        furi_string_set_str(error, "Typing is not included in this build.");
        goto failed;
#endif
    }
    if(request->autoclose) {
        if(app->database_dirty) {
            flippass_scene_status_show(
                app,
                "Launch Completed",
                "The action changed this database. Save changes before closing.",
                FlipPassScene_DbEntries);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        } else {
            flippass_request_exit(app);
            furi_string_free(error);
            return FlipPassLaunchExit;
        }
    }
    furi_string_free(error);
    if(request->field_tag[0] != '\0') return FlipPassLaunchLock;
    return entry != NULL ? FlipPassLaunchTypeMenu : FlipPassLaunchBrowse;
failed:
#if FLIPPASS_ENABLE_TYPING_ACTIONS
    if(typing_started) {
        flippass_typing_end(app);
        flippass_output_cleanup(app);
        flippass_progress_reset(app);
    }
#endif
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_custom_field = NULL;
    launch_error(
        app,
        furi_string_empty(error) ? "The launch action could not be completed." :
                                   furi_string_get_cstr(error),
        FlipPassScene_DbEntries);
    furi_string_free(error);
    return FlipPassLaunchBrowse;
}

static const FlipPassLaunchPluginV2 api = {
    .api_version = FLIPPASS_LAUNCH_PLUGIN_API_VERSION,
    .start = launch_start,
    .after_open = launch_after_open,
};
static const FlipperAppPluginDescriptor descriptor = {
    .appid = FLIPPASS_LAUNCH_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_LAUNCH_PLUGIN_API_VERSION,
    .entry_point = &api,
};
const FlipperAppPluginDescriptor* flippass_launch_plugin_ep(void) {
    return &descriptor;
}
