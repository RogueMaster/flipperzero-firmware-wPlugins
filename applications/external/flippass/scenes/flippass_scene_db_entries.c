#include "flippass_scene_db_entries.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_db_browser_view.h"
#include "flippass_scene.h"
#include "flippass_scene_editor.h"
#include "flippass_scene_other_fields.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    FlipPassDbBrowserItemType type;
    KDBXGroup* group;
    KDBXEntry* entry;
} FlipPassBrowserItem;

typedef enum {
    FlipPassBrowserDialogNone = 0,
    FlipPassBrowserDialogClose,
} FlipPassBrowserDialogState;

static FlipPassBrowserItem* flippass_browser_items = NULL;
static size_t flippass_browser_item_count = 0U;

static bool flippass_browser_back_filter(void* context) {
    return flippass_typing_consume_pending_back(context);
}

static bool flippass_browser_items_ensure(void) {
    if(flippass_browser_items != NULL) {
        return true;
    }

    flippass_browser_items = malloc(sizeof(FlipPassBrowserItem) * FLIPPASS_DB_BROWSER_MAX_ITEMS);
    return flippass_browser_items != NULL;
}

static void flippass_browser_items_free(void) {
    free(flippass_browser_items);
    flippass_browser_items = NULL;
    flippass_browser_item_count = 0U;
}

static const char* flippass_browser_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static bool flippass_browser_entry_has_visible_custom_field(const KDBXEntry* entry) {
    for(const KDBXCustomField* field = entry != NULL ? entry->custom_fields : NULL; field != NULL;
        field = field->next) {
        if(!flippass_otp_custom_field_is_reserved(field->key)) {
            return true;
        }
    }
    return false;
}

static bool flippass_browser_entry_has_other_fields(const KDBXEntry* entry) {
    return entry != NULL && (flippass_db_entry_has_field(entry, KDBXEntryFieldUrl) ||
                             flippass_db_entry_has_field(entry, KDBXEntryFieldNotes) ||
                             flippass_otp_entry_has_any_config(entry) ||
                             flippass_browser_entry_has_visible_custom_field(entry));
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
    if(flippass_browser_items == NULL ||
       flippass_browser_item_count >= FLIPPASS_DB_BROWSER_MAX_ITEMS) {
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
    case FlipPassEntryActionTypeOtherBluetooth:
        return FlipPassOutputTransportBluetooth;
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeOtherUsb:
    default:
        return FlipPassOutputTransportUsb;
    }
}

static FlipPassOutputTransport
    flippass_browser_opposite_transport(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FlipPassOutputTransportUsb :
                                                             FlipPassOutputTransportBluetooth;
}

static void
    flippass_browser_trim_for_layout_selection(App* app, FlipPassOutputTransport transport) {
    flippass_output_release_all(app);
    flippass_output_cleanup_transport(app, flippass_browser_opposite_transport(transport));
}

static void flippass_browser_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static bool flippass_browser_at_root(const App* app) {
    return app->current_group == NULL || app->current_group->parent == NULL;
}

static void flippass_browser_show_close_dialog(App* app) {
    furi_assert(app);

    flippass_browser_sync_selection_from_view(app);
    app->close_db_dialog_open = true;
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_DbEntries, FlipPassBrowserDialogClose);
    dialog_ex_reset(app->dialog_ex);
    if(app->database_dirty) {
        dialog_ex_set_header(app->dialog_ex, "Unsaved Changes", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            "Save changes before closing the current database?",
            4,
            18,
            AlignLeft,
            AlignTop);
        dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
        dialog_ex_set_center_button_text(app->dialog_ex, "Discard");
        dialog_ex_set_right_button_text(app->dialog_ex, "Save");
    } else {
        dialog_ex_set_header(app->dialog_ex, "Close Database?", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex, "Return to the database browser list.", 4, 18, AlignLeft, AlignTop);
        dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
        dialog_ex_set_right_button_text(app->dialog_ex, "Close");
    }
    dialog_ex_set_result_callback(app->dialog_ex, flippass_browser_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

static void flippass_browser_hide_dialog(App* app, bool restore_browser) {
    furi_assert(app);

    if(!app->close_db_dialog_open &&
       scene_manager_get_scene_state(app->scene_manager, FlipPassScene_DbEntries) ==
           FlipPassBrowserDialogNone) {
        return;
    }

    app->close_db_dialog_open = false;
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_DbEntries, FlipPassBrowserDialogNone);
    dialog_ex_reset(app->dialog_ex);
    submenu_reset(app->submenu);
    if(restore_browser) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
    }
}

static bool flippass_browser_confirm_close_database(App* app) {
    furi_assert(app);

    flippass_browser_hide_dialog(app, false);
    flippass_close_database(app);
    return scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, FlipPassScene_FileBrowser);
}

static void flippass_browser_open_create_menu(App* app) {
    furi_assert(app);

    flippass_browser_sync_selection_from_view(app);
    app->action_selected_index = 0U;
    flippass_db_browser_view_set_show_other_action(app->db_browser, false);
    flippass_db_browser_view_set_action_selected(app->db_browser, 0U);
    flippass_db_browser_view_set_action_menu_open(app->db_browser, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

static void flippass_browser_prepare_group_editor(
    App* app,
    FlipPassEditorMode mode,
    KDBXGroup* group,
    const char* initial_name) {
    app->editor_mode = mode;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = group;
    app->editor_entry = NULL;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_DbEntries;
    app->editor_close_after_commit = false;
    snprintf(
        app->editor_group_name,
        sizeof(app->editor_group_name),
        "%s",
        initial_name != NULL ? initial_name : "");
}

static bool flippass_browser_prepare_entry_editor(
    App* app,
    FlipPassEditorMode mode,
    KDBXEntry* entry,
    FuriString* error) {
    const KDBXEntry* source = entry;

    app->editor_mode = mode;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = app->current_group;
    app->editor_entry = entry;
    app->editor_selected_index =
        (mode == FlipPassEditorModeEditEntry) ? FlipPassEditorEntryRowCommit : 0U;
    app->editor_return_scene = FlipPassScene_DbEntries;
    app->editor_close_after_commit = false;
    if(mode == FlipPassEditorModeAddEntry || mode == FlipPassEditorModeEditEntry) {
        flippass_editor_clear_custom_field_drafts(app);
    }

    if(source != NULL && !flippass_db_activate_entry(app, entry, true, error)) {
        return false;
    }

    snprintf(
        app->editor_entry_title,
        sizeof(app->editor_entry_title),
        "%s",
        source != NULL && source->title != NULL ? source->title : "");
    snprintf(
        app->editor_entry_username,
        sizeof(app->editor_entry_username),
        "%s",
        source != NULL && source->username != NULL ? source->username : "");
    snprintf(
        app->editor_entry_password,
        sizeof(app->editor_entry_password),
        "%s",
        source != NULL && source->password != NULL ? source->password : "");
    snprintf(
        app->editor_entry_url,
        sizeof(app->editor_entry_url),
        "%s",
        source != NULL && source->url != NULL ? source->url : "");
    snprintf(
        app->editor_entry_notes,
        sizeof(app->editor_entry_notes),
        "%s",
        source != NULL && source->notes != NULL ? source->notes : "");
    snprintf(
        app->editor_entry_autotype,
        sizeof(app->editor_entry_autotype),
        "%s",
        source != NULL && source->autotype_sequence != NULL ? source->autotype_sequence : "");
    return true;
}

static void flippass_browser_prepare_save_editor(App* app, bool close_after_commit) {
    app->editor_mode = FlipPassEditorModeModifyDatabase;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_selected_index = 3U;
    app->editor_return_scene = FlipPassScene_DbEntries;
    app->editor_close_after_commit = close_after_commit;
    app->editor_database_password[0] = '\0';
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
    flippass_db_browser_view_set_add_menu_kind(app->db_browser, FlipPassDbBrowserAddMenuKindItem);
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

    /*     if(flippass_browser_item_count == 0U) {
        flippass_browser_add_item(app, "Empty Group", FlipPassDbBrowserItemTypeInfo, NULL, NULL);
    } */

    flippass_browser_add_item(app, "New Item", FlipPassDbBrowserItemTypeAdd, NULL, NULL);

    if(app->browser_selected_index >= flippass_browser_item_count) {
        app->browser_selected_index = 0U;
    }

    flippass_db_browser_view_set_selected_item(app->db_browser, app->browser_selected_index);
    flippass_db_browser_view_set_show_other_action(app->db_browser, false);
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
    case FlipPassDbBrowserEventTypeUsbLong:
        custom_event = FlipPassSceneDbEntriesEventSelectUsbLayout;
        break;
    case FlipPassDbBrowserEventTypeBluetoothLong:
        custom_event = FlipPassSceneDbEntriesEventSelectBluetoothLayout;
        break;
    case FlipPassDbBrowserEventOpenOther:
        custom_event = FlipPassSceneDbEntriesEventOpenOtherFields;
        break;
    case FlipPassDbBrowserEventLongOk:
        custom_event = FlipPassSceneDbEntriesEventEditSelected;
        break;
    case FlipPassDbBrowserEventSelectAction:
        app->action_selected_index = flippass_db_browser_view_get_action_selected(app->db_browser);
        custom_event = (app->action_selected_index == 0U) ?
                           FlipPassSceneDbEntriesEventCreateGroup :
                           FlipPassSceneDbEntriesEventCreateEntry;
        break;
    case FlipPassDbBrowserEventCloseActionMenu:
        custom_event = FlipPassSceneDbEntriesEventCloseActionMenu;
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
    flippass_db_browser_view_set_show_other_action(
        app->db_browser, flippass_browser_entry_has_other_fields(item->entry));
    flippass_entry_action_prepare_type_menu(app);
}

static void flippass_browser_enter_selected_group(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL) {
        return;
    }

    if(item->type == FlipPassDbBrowserItemTypeAdd) {
        flippass_browser_open_create_menu(app);
        return;
    }

    if(item->type != FlipPassDbBrowserItemTypeGroup || item->group == NULL) {
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

    flippass_typing_begin(app);
    flippass_progress_begin(
        app, flippass_browser_progress_title(app->action_selected_index), "Connecting", 5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FlipPassSceneDbEntriesEventRunPendingAction);
}

static void flippass_browser_begin_layout_selection(App* app, FlipPassOutputTransport transport) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);

    if(item == NULL || item->type != FlipPassDbBrowserItemTypeEntry || item->entry == NULL) {
        return;
    }

    app->current_entry = item->entry;
    app->active_entry = item->entry;
    app->active_group = app->current_group;
    app->pending_entry_action =
        flippass_browser_map_pending_action(app->action_selected_index, transport);
    app->keyboard_layout_return_scene = FlipPassScene_DbEntries;
    flippass_browser_trim_for_layout_selection(app, transport);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_KeyboardLayout);
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
    app->pending_other_otp_kind = FlipPassOtpKindNone;
    app->pending_other_field_name[0] = '\0';
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeType);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_OtherFields);
}

static bool flippass_browser_edit_selected(App* app) {
    FlipPassBrowserItem* item = flippass_browser_get_selected_item(app);
    FuriString* error = furi_string_alloc();
    bool handled = false;

    if(item == NULL) {
        furi_string_free(error);
        return false;
    }

    if(item->type == FlipPassDbBrowserItemTypeGroup && item->group != NULL) {
        flippass_browser_prepare_group_editor(
            app, FlipPassEditorModeEditGroup, item->group, item->group->name);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
        handled = true;
    } else if(item->type == FlipPassDbBrowserItemTypeEntry && item->entry != NULL) {
        if(flippass_browser_prepare_entry_editor(
               app, FlipPassEditorModeEditEntry, item->entry, error)) {
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
            handled = true;
        } else {
            flippass_scene_status_show(
                app, "Edit Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            handled = true;
        }
    } else if(item->type == FlipPassDbBrowserItemTypeAdd) {
        flippass_browser_open_create_menu(app);
        handled = true;
    }

    furi_string_free(error);
    return handled;
}

void flippass_scene_db_entries_on_enter(void* context) {
    App* app = context;
    FLIPPASS_BENCH_LOG(app, "SCENE db_entries");
    flippass_db_browser_view_set_callback(app->db_browser, flippass_browser_view_callback, app);
    flippass_db_browser_view_set_back_filter(app->db_browser, flippass_browser_back_filter);
    if(!flippass_browser_items_ensure()) {
        flippass_scene_status_show(
            app,
            "Open Failed",
            "Not enough RAM is available to list entries.",
            FlipPassScene_FileBrowser);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        return;
    }
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_DbEntries, FlipPassBrowserDialogNone);

    if(app->database_loaded && app->root_group != NULL) {
        if(app->current_group == NULL) {
            app->current_group = app->root_group;
        }
        if(app->editor_mode == FlipPassEditorModeModifyDatabase &&
           app->editor_return_scene == FlipPassScene_FileBrowser) {
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
            return;
        }
        flippass_browser_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
        return;
    }

    const bool resuming_ext_continuation = app->pending_gzip_scratch_vault != NULL &&
                                           app->allow_ext_vault_promotion;
    flippass_progress_begin(
        app,
        "Opening Database",
        resuming_ext_continuation ? "Continuing on /ext" : "Preparing",
        resuming_ext_continuation ? 80U : 0U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FlipPassSceneDbEntriesEventLoadDatabase);
}

bool flippass_scene_db_entries_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    const uint32_t dialog_state =
        scene_manager_get_scene_state(app->scene_manager, FlipPassScene_DbEntries);

    if(dialog_state != FlipPassBrowserDialogNone) {
        if(event.type == SceneManagerEventTypeBack) {
            flippass_browser_hide_dialog(app, true);
            return true;
        }

        if(event.type == SceneManagerEventTypeCustom) {
            if(dialog_state == FlipPassBrowserDialogClose) {
                if(event.event == DialogExResultLeft) {
                    flippass_browser_hide_dialog(app, true);
                    return true;
                }

                if(app->database_dirty && event.event == DialogExResultCenter) {
                    return flippass_browser_confirm_close_database(app);
                }

                if(app->database_dirty && event.event == DialogExResultRight) {
                    flippass_browser_hide_dialog(app, false);
                    flippass_browser_prepare_save_editor(app, true);
                    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
                    return true;
                }

                if(!app->database_dirty && event.event == DialogExResultRight) {
                    return flippass_browser_confirm_close_database(app);
                }
            }

            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlipPassSceneDbEntriesEventLoadDatabase) {
            FuriString* error = furi_string_alloc();

            if(flippass_open_execute(app, error)) {
                flippass_progress_update(app, "Ready", "", 100U);
                FLIPPASS_BENCH_LOG(app, "LOAD_EVENT_OK");
                if(app->current_group == NULL) {
                    app->current_group = app->root_group;
                }
                flippass_progress_reset(app);
                if(app->editor_mode == FlipPassEditorModeModifyDatabase &&
                   app->editor_return_scene == FlipPassScene_FileBrowser) {
                    scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
                } else {
                    FLIPPASS_BENCH_LOG(app, "BROWSER_RENDER_BEGIN");
                    flippass_browser_render(app);
                    FLIPPASS_BENCH_LOG(app, "BROWSER_RENDER_OK");
                    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
                    FLIPPASS_BENCH_LOG(app, "DB_VIEW_READY");
                }
            } else if(strcmp(furi_string_get_cstr(error), "Unlock canceled.") == 0) {
                flippass_progress_reset(app);
                FLIPPASS_BENCH_LOG(app, "LOAD_EVENT_CANCEL");
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FlipPassScene_PasswordEntry);
            } else if(app->pending_vault_fallback && !app->rpc_mode) {
                flippass_progress_reset(app);
                FLIPPASS_BENCH_LOG(app, "LOAD_EVENT_FALLBACK");
                scene_manager_next_scene(app->scene_manager, FlipPassScene_VaultFallback);
            } else {
                flippass_progress_reset(app);
                FLIPPASS_BENCH_LOG(app, "LOAD_EVENT_FAIL");
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

        if(event.event == FlipPassSceneDbEntriesEventCloseActionMenu) {
            flippass_entry_action_cleanup_type_menu(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventCreateGroup) {
            flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
            flippass_browser_prepare_group_editor(
                app, FlipPassEditorModeAddGroup, app->current_group, "");
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventCreateEntry) {
            FuriString* error = furi_string_alloc();
            flippass_db_browser_view_set_action_menu_open(app->db_browser, false);
            if(flippass_browser_prepare_entry_editor(
                   app, FlipPassEditorModeAddEntry, NULL, error)) {
                scene_manager_next_scene(app->scene_manager, FlipPassScene_Editor);
            } else {
                flippass_scene_status_show(
                    app, "Create Failed", furi_string_get_cstr(error), FlipPassScene_DbEntries);
                scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
            }
            furi_string_free(error);
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

        if(event.event == FlipPassSceneDbEntriesEventSelectUsbLayout) {
            flippass_browser_begin_layout_selection(app, FlipPassOutputTransportUsb);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventSelectBluetoothLayout) {
            flippass_browser_begin_layout_selection(app, FlipPassOutputTransportBluetooth);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventOpenOtherFields) {
            flippass_browser_open_other_fields(app);
            return true;
        }

        if(event.event == FlipPassSceneDbEntriesEventEditSelected) {
            return flippass_browser_edit_selected(app);
        }

        if(event.event == FlipPassSceneDbEntriesEventRunPendingAction) {
            FuriString* error = furi_string_alloc();
            const bool ok = flippass_entry_action_execute_pending(app, error);
            const bool canceled = !ok && flippass_typing_should_cancel(app);

            flippass_typing_end(app);

            if(ok) {
                flippass_progress_update(app, "Done", "Field sent.", 100U);
                flippass_browser_render(app);
                flippass_progress_reset(app);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
            } else if(canceled) {
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
        if(flippass_typing_consume_pending_back(app)) {
            return true;
        }

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
    flippass_db_browser_view_set_back_filter(app->db_browser, NULL);
    flippass_entry_action_cleanup_type_menu(app);
    flippass_browser_items_free();
    flippass_browser_hide_dialog(app, false);
}
