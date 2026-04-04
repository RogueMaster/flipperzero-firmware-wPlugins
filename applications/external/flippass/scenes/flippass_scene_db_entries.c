#include "flippass_scene_db_entries.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_db_browser_view.h"
#include "flippass_scene.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"

#include <stdio.h>

typedef struct {
    FlipPassDbBrowserItemType type;
    KDBXGroup* group;
    KDBXEntry* entry;
} FlipPassBrowserItem;

static FlipPassBrowserItem flippass_browser_items[FLIPPASS_DB_BROWSER_MAX_ITEMS];
static size_t flippass_browser_item_count = 0U;

static const char* flippass_browser_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static const char* flippass_browser_safe_path_segment(const char* value, const char* fallback) {
    const char* segment = value;

    if(segment == NULL) {
        return fallback;
    }

    while(*segment == '/' || *segment == '\\') {
        segment++;
    }

    return (segment[0] != '\0') ? segment : fallback;
}

static void flippass_browser_append_group_path(
    FuriString* path,
    const KDBXGroup* group,
    const KDBXGroup* root_group) {
    if(group == NULL || group == root_group) {
        return;
    }

    if(group->parent != NULL) {
        flippass_browser_append_group_path(path, group->parent, root_group);
    }

    furi_string_cat_str(path, "/");
    furi_string_cat_str(path, flippass_browser_safe_path_segment(group->name, "Unnamed Group"));
}

static void flippass_browser_build_header(App* app, FuriString* header) {
    furi_string_reset(header);
    if(app->current_group == NULL || app->current_group == app->root_group) {
        furi_string_set_str(header, "/");
        return;
    }

    flippass_browser_append_group_path(header, app->current_group, app->root_group);
}

static void flippass_browser_add_item(
    App* app,
    const char* label,
    FlipPassDbBrowserItemType type,
    KDBXGroup* group,
    KDBXEntry* entry) {
    if(flippass_browser_item_count >= FLIPPASS_DB_BROWSER_MAX_ITEMS) {
        return;
    }

    flippass_browser_items[flippass_browser_item_count] =
        (FlipPassBrowserItem){.type = type, .group = group, .entry = entry};
    flippass_db_browser_view_add_item(app->db_browser, type, label);
    flippass_browser_item_count++;
}

static void flippass_browser_sync_selection_from_view(App* app) {
    furi_assert(app);

    app->browser_selected_index = flippass_db_browser_view_get_selected_item(app->db_browser);
    app->action_selected_index = flippass_db_browser_view_get_action_selected(app->db_browser);
}

static FlipPassBrowserItem* flippass_browser_get_selected_item(App* app) {
    furi_assert(app);

    flippass_browser_sync_selection_from_view(app);
    if(app->browser_selected_index >= flippass_browser_item_count) {
        return NULL;
    }

    return &flippass_browser_items[app->browser_selected_index];
}

static uint32_t
    flippass_browser_find_group_index(const KDBXGroup* parent, const KDBXGroup* child) {
    uint32_t index = 0U;

    for(const KDBXGroup* group = parent != NULL ? parent->children : NULL; group != NULL;
        group = group->next, index++) {
        if(group == child) {
            return index;
        }
    }

    return 0U;
}

static FlipPassOutputTransport flippass_browser_action_transport(FlipPassEntryAction action) {
    switch(action) {
    case FlipPassEntryActionTypeUsernameBluetooth:
    case FlipPassEntryActionTypePasswordBluetooth:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
    case FlipPassEntryActionTypeLoginBluetooth:
        return FlipPassOutputTransportBluetooth;
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeLoginUsb:
    default:
        return FlipPassOutputTransportUsb;
    }
}

static void flippass_browser_close_dialog_callback(DialogExResult result, void* context) {
    App* app = context;

    if(result == DialogExResultCenter) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipPassSceneDbEntriesEventConfirmCloseDatabase);
    }
}

static bool flippass_browser_at_root(const App* app) {
    return app->current_group == NULL || app->current_group->parent == NULL;
}

static void flippass_browser_show_close_dialog(App* app) {
    furi_assert(app);

    flippass_browser_sync_selection_from_view(app);
    app->close_db_dialog_open = true;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Close Database?", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "Are you sure you want to close the current database and return to the file browser?",
        4,
        15,
        AlignLeft,
        AlignTop);
    dialog_ex_set_center_button_text(app->dialog_ex, "Close");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_browser_close_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

static void flippass_browser_hide_close_dialog(App* app, bool restore_browser) {
    furi_assert(app);

    if(!app->close_db_dialog_open) {
        return;
    }

    app->close_db_dialog_open = false;
    dialog_ex_reset(app->dialog_ex);
    if(restore_browser) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
    }
}

static bool flippass_browser_confirm_close_database(App* app) {
    furi_assert(app);

    flippass_browser_hide_close_dialog(app, false);
    flippass_close_database(app);
    return scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, FlipPassScene_FileBrowser);
}

static FlipPassEntryAction
    flippass_browser_map_pending_action(uint32_t action_index, FlipPassOutputTransport transport) {
    const uint32_t clamped_index = (action_index < FlipPassDbBrowserActionCount) ? action_index :
                                                                                   0U;

    switch(clamped_index) {
    case FlipPassDbBrowserActionPassword:
        return (transport == FlipPassOutputTransportBluetooth) ?
                   FlipPassEntryActionTypePasswordBluetooth :
                   FlipPassEntryActionTypePasswordUsb;
    case FlipPassDbBrowserActionUsername:
        return (transport == FlipPassOutputTransportBluetooth) ?
                   FlipPassEntryActionTypeUsernameBluetooth :
                   FlipPassEntryActionTypeUsernameUsb;
    case FlipPassDbBrowserActionAutoType:
    default:
        return (transport == FlipPassOutputTransportBluetooth) ?
                   FlipPassEntryActionTypeAutoTypeBluetooth :
                   FlipPassEntryActionTypeAutoTypeUsb;
    }
}

static const char* flippass_browser_progress_title(uint32_t action_index) {
    switch(action_index) {
    case FlipPassDbBrowserActionPassword:
        return "Typing Password";
    case FlipPassDbBrowserActionUsername:
        return "Typing Username";
    case FlipPassDbBrowserActionAutoType:
    default:
        return "Typing AutoType";
    }
}

static bool flippass_browser_show_selected_action(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);
    FuriString* error = furi_string_alloc();
    bool handled = false;

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeEntry || item->entry == NULL) {
        furi_string_free(error);
        return false;
    }

    app->current_entry = item->entry;
    app->active_entry = item->entry;
    app->active_group = app->current_group;

    if(!flippass_db_activate_entry(app, item->entry, false, error)) {
        flippass_scene_status_show(
            app, "Entry Load Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return true;
    }

    switch(app->action_selected_index) {
    case FlipPassDbBrowserActionPassword:
        if(flippass_db_entry_has_field(item->entry, KDBXEntryFieldPassword) &&
           !flippass_db_ensure_entry_field(app, item->entry, KDBXEntryFieldPassword, error)) {
            flippass_scene_status_show(
                app, "Show Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            handled = true;
            break;
        }
        flippass_scene_status_show(
            app,
            "Password",
            flippass_browser_safe_text(item->entry->password, "Not set"),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        handled = true;
        break;
    case FlipPassDbBrowserActionUsername:
        if(flippass_db_entry_has_field(item->entry, KDBXEntryFieldUsername) &&
           !flippass_db_ensure_entry_field(app, item->entry, KDBXEntryFieldUsername, error)) {
            flippass_scene_status_show(
                app, "Show Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            handled = true;
            break;
        }
        flippass_scene_status_show(
            app,
            "Username",
            flippass_browser_safe_text(item->entry->username, "Not set"),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        handled = true;
        break;
    case FlipPassDbBrowserActionAutoType:
    default:
        if(flippass_db_entry_has_field(item->entry, KDBXEntryFieldAutotype) &&
           !flippass_db_ensure_entry_field(app, item->entry, KDBXEntryFieldAutotype, error)) {
            flippass_scene_status_show(
                app, "Show Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            handled = true;
            break;
        }
        flippass_scene_status_show(
            app,
            "AutoType",
            flippass_browser_safe_text(
                item->entry->autotype_sequence, "{USERNAME}{TAB}{PASSWORD}{ENTER}"),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        handled = true;
        break;
    }

    furi_string_free(error);
    return handled;
}

static const char* flippass_browser_typing_status_title(App* app) {
    if(flippass_browser_action_transport(app->pending_entry_action) ==
       FlipPassOutputTransportBluetooth) {
        return flippass_output_bluetooth_is_advertising(app) ? "Bluetooth Waiting" :
                                                               "Bluetooth Typing Failed";
    }

    return "USB Typing Failed";
}

static void flippass_browser_render(App* app) {
    FuriString* scratch = furi_string_alloc();

    flippass_browser_item_count = 0U;
    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_has_parent(
        app->db_browser, app->current_group != NULL && app->current_group->parent != NULL);

    flippass_browser_build_header(app, scratch);
    flippass_db_browser_view_set_header(app->db_browser, furi_string_get_cstr(scratch));

    for(KDBXGroup* child = app->current_group != NULL ? app->current_group->children : NULL;
        child != NULL;
        child = child->next) {
        char label[FLIPPASS_DB_BROWSER_LABEL_SIZE];
        snprintf(
            label, sizeof(label), "%s", flippass_browser_safe_text(child->name, "Unnamed Group"));
        flippass_browser_add_item(app, label, FlipPassDbBrowserItemTypeGroup, child, NULL);
    }

    for(KDBXEntry* entry = app->current_group != NULL ? app->current_group->entries : NULL;
        entry != NULL;
        entry = entry->next) {
        char label[FLIPPASS_DB_BROWSER_LABEL_SIZE];
        const bool have_title = flippass_db_copy_entry_title(app, entry, scratch, NULL);
        snprintf(
            label,
            sizeof(label),
            "%s",
            have_title ?
                flippass_browser_safe_text(furi_string_get_cstr(scratch), "Untitled Entry") :
                "Title unavailable");
        flippass_browser_add_item(app, label, FlipPassDbBrowserItemTypeEntry, NULL, entry);
    }

    if(flippass_browser_item_count == 0U) {
        flippass_browser_add_item(app, "Empty Group", FlipPassDbBrowserItemTypeInfo, NULL, NULL);
    }

    if(app->browser_selected_index >= flippass_browser_item_count) {
        app->browser_selected_index = 0U;
    }

    flippass_db_browser_view_set_selected_item(app->db_browser, app->browser_selected_index);
    flippass_db_browser_view_set_action_selected(app->db_browser, app->action_selected_index);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    furi_string_free(scratch);
}

static void flippass_browser_view_callback(FlipPassDbBrowserEvent event, void* context) {
    App* app = context;
    uint32_t custom_event = 0U;

    switch(event) {
    case FlipPassDbBrowserEventEnter:
        custom_event = FlipPassSceneDbEntriesEventEnterSelected;
        break;
    case FlipPassDbBrowserEventBack:
        custom_event = FlipPassSceneDbEntriesEventLeaveCurrentGroup;
        break;
    case FlipPassDbBrowserEventOpenActionMenu:
        custom_event = FlipPassSceneDbEntriesEventOpenActionMenu;
        break;
    case FlipPassDbBrowserEventShow:
        custom_event = FlipPassSceneDbEntriesEventShowSelectedAction;
        break;
    case FlipPassDbBrowserEventTypeUsb:
        custom_event = FlipPassSceneDbEntriesEventExecuteUsbAction;
        break;
    case FlipPassDbBrowserEventTypeBluetooth:
        custom_event = FlipPassSceneDbEntriesEventExecuteBluetoothAction;
        break;
    case FlipPassDbBrowserEventOpenOther:
        custom_event = FlipPassSceneDbEntriesEventOpenOtherFields;
        break;
    default:
        break;
    }

    if(custom_event != 0U) {
        view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
    }
}

static void flippass_browser_open_action_menu(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeEntry || item->entry == NULL) {
        return;
    }

    app->current_entry = item->entry;
    app->active_entry = item->entry;
    app->active_group = app->current_group;
    flippass_entry_action_prepare_pending(app);
}

static void flippass_browser_enter_selected_group(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeGroup || item->group == NULL) {
        return;
    }

    flippass_db_deactivate_entry(app);
    app->current_group = item->group;
    app->current_entry = NULL;
    app->active_group = app->current_group;
    app->active_entry = NULL;
    app->browser_selected_index = 0U;
    flippass_browser_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

static void flippass_browser_leave_current_group(App* app) {
    if(app->current_group == NULL || app->current_group->parent == NULL) {
        return;
    }

    KDBXGroup* child_group = app->current_group;
    flippass_db_deactivate_entry(app);
    app->current_group = child_group->parent;
    app->current_entry = NULL;
    app->active_group = app->current_group;
    app->active_entry = NULL;
    app->browser_selected_index =
        flippass_browser_find_group_index(app->current_group, child_group);
    flippass_browser_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

static void flippass_browser_begin_execute_action(App* app, FlipPassOutputTransport transport) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeEntry || item->entry == NULL) {
        return;
    }

    app->current_entry = item->entry;
    app->active_entry = item->entry;
    app->active_group = app->current_group;
    app->pending_entry_action =
        flippass_browser_map_pending_action(app->action_selected_index, transport);

    flippass_progress_begin(
        app,
        flippass_browser_progress_title(app->action_selected_index),
        "Connecting",
        5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FlipPassSceneDbEntriesEventRunPendingAction);
}

static void flippass_browser_open_other_fields(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeEntry || item->entry == NULL) {
        return;
    }

    app->current_entry = item->entry;
    app->active_entry = item->entry;
    app->active_group = app->current_group;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_field_name[0] = '\0';
    scene_manager_next_scene(app->scene_manager, FlipPassScene_OtherFields);
}

void flippass_scene_db_entries_on_enter(void* context) {
    App* app = context;
    flippass_log_event(app, "SCENE db_entries");
    flippass_db_browser_view_set_callback(app->db_browser, flippass_browser_view_callback, app);

    if(app->database_loaded && app->root_group != NULL) {
        if(app->current_group == NULL) {
            app->current_group = app->root_group;
        }
        flippass_browser_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
        return;
    }

    flippass_progress_begin(app, "Opening Database", "Preparing", 0U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FlipPassSceneDbEntriesEventLoadDatabase);
}

bool flippass_scene_db_entries_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(app->close_db_dialog_open) {
        if(event.type == SceneManagerEventTypeBack) {
            flippass_browser_hide_close_dialog(app, true);
            return true;
        }

        if(event.type == SceneManagerEventTypeCustom &&
           event.event == FlipPassSceneDbEntriesEventConfirmCloseDatabase) {
            return flippass_browser_confirm_close_database(app);
        }
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlipPassSceneDbEntriesEventLoadDatabase) {
            FuriString* error = furi_string_alloc();

            if(flippass_db_load(app, error)) {
                flippass_progress_update(app, "Ready", "", 100U);
                flippass_log_event(app, "LOAD_EVENT_OK");
                if(app->current_group == NULL) {
                    app->current_group = app->root_group;
                }
                flippass_log_event(app, "BROWSER_RENDER_BEGIN");
                flippass_browser_render(app);
                flippass_log_event(app, "BROWSER_RENDER_OK");
                flippass_progress_reset(app);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
                flippass_log_event(app, "DB_VIEW_READY");
            } else if(app->pending_vault_fallback && !app->rpc_mode) {
                flippass_progress_reset(app);
                flippass_log_event(app, "LOAD_EVENT_FALLBACK");
                scene_manager_next_scene(app->scene_manager, FlipPassScene_VaultFallback);
            } else {
                flippass_progress_reset(app);
                flippass_log_event(app, "LOAD_EVENT_FAIL");
                flippass_scene_status_show(
                    app, "Unlock Failed", furi_string_get_cstr(error), FlipPassScene_PasswordEntry);
                scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            }

            furi_string_free(error);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventEnterSelected) {
            flippass_browser_enter_selected_group(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventLeaveCurrentGroup) {
            flippass_browser_leave_current_group(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventOpenActionMenu) {
            flippass_browser_open_action_menu(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventShowSelectedAction) {
            return flippass_browser_show_selected_action(app);
        }

        if(event.event == FlipPassSceneDbEntriesEventExecuteUsbAction) {
            flippass_browser_begin_execute_action(app, FlipPassOutputTransportUsb);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventExecuteBluetoothAction) {
            flippass_browser_begin_execute_action(app, FlipPassOutputTransportBluetooth);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventOpenOtherFields) {
            flippass_browser_open_other_fields(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventRunPendingAction) {
            FuriString* error = furi_string_alloc();

            if(flippass_entry_action_execute_pending(app, error)) {
                flippass_progress_update(app, "Done", "Field sent.", 100U);
                flippass_browser_render(app);
                flippass_progress_reset(app);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
            } else {
                flippass_progress_reset(app);
                flippass_scene_status_show(
                    app,
                    flippass_browser_typing_status_title(app),
                    furi_string_get_cstr(error),
                    FlipPassScene_DbEntries);
                scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            }

            furi_string_free(error);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(app->database_loaded && app->root_group != NULL && flippass_browser_at_root(app)) {
            flippass_browser_show_close_dialog(app);
            return true;
        }

        flippass_browser_sync_selection_from_view(app);
        flippass_clear_master_password(app);
        if(!scene_manager_previous_scene(app->scene_manager)) {
            flippass_request_exit(app);
        }
        return true;
    }

    return false;
}

void flippass_scene_db_entries_on_exit(void* context) {
    App* app = context;
    flippass_browser_sync_selection_from_view(app);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
    if(app->close_db_dialog_open) {
        flippass_browser_hide_close_dialog(app, false);
    }
}
