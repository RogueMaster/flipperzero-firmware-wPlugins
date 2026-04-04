/**
 * @file flippass_scene_entry_actions.c
 * @brief Implementation of the entry action menu scene.
 */
#include "flippass_scene_entry_actions.h"
#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"
#include "flippass_scene_text_view.h"

static const char* flippass_scene_entry_actions_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static bool flippass_scene_entry_actions_has_other_fields(const KDBXEntry* entry) {
    return entry != NULL &&
           (flippass_db_entry_has_field(entry, KDBXEntryFieldUrl) ||
            flippass_db_entry_has_field(entry, KDBXEntryFieldNotes) ||
            flippass_db_entry_get_custom_fields(entry) != NULL);
}

static void flippass_scene_entry_actions_show_text(App* app, const char* title, const char* body) {
    flippass_scene_status_show(app, title, body, FlipPassScene_EntryActions);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

static bool flippass_scene_entry_actions_execute(App* app, uint32_t index, FuriString* error) {
    const KDBXEntry* entry = app->active_entry;

    app->action_selected_index = index;
    app->pending_entry_action = (FlipPassEntryAction)index;

    if(entry == NULL) {
        furi_string_set_str(error, "Return to the browser and pick an entry first.");
        return false;
    }

    switch((FlipPassEntryAction)index) {
    case FlipPassEntryActionShowDetails:
        scene_manager_next_scene(app->scene_manager, FlipPassScene_EntryDetail);
        return true;
    case FlipPassEntryActionBrowseOtherFields:
        scene_manager_next_scene(app->scene_manager, FlipPassScene_OtherFields);
        return true;
    case FlipPassEntryActionRevealUsername:
        if(!flippass_db_ensure_entry_field(app, app->active_entry, KDBXEntryFieldUsername, error)) {
            return false;
        }
        flippass_scene_entry_actions_show_text(
            app,
            "Username",
            flippass_scene_entry_actions_safe_text(entry->username, "Not set"));
        return true;
    case FlipPassEntryActionRevealPassword:
        if(!flippass_db_ensure_entry_field(app, app->active_entry, KDBXEntryFieldPassword, error)) {
            return false;
        }
        flippass_scene_entry_actions_show_text(
            app,
            "Password",
            flippass_scene_entry_actions_safe_text(entry->password, "Not set"));
        return true;
    case FlipPassEntryActionRevealUrl:
        if(!flippass_db_ensure_entry_field(app, app->active_entry, KDBXEntryFieldUrl, error)) {
            return false;
        }
        flippass_scene_entry_actions_show_text(
            app, "URL", flippass_scene_entry_actions_safe_text(entry->url, "Not set"));
        return true;
    case FlipPassEntryActionRevealNotes:
        if(!flippass_db_ensure_entry_field(app, app->active_entry, KDBXEntryFieldNotes, error)) {
            return false;
        }
        flippass_scene_text_view_show(
            app,
            "Notes",
            flippass_scene_entry_actions_safe_text(entry->notes, "Not set"),
            FlipPassScene_EntryActions);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_TextView);
        return true;
    case FlipPassEntryActionRevealAutoType:
        if(flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
           !flippass_db_ensure_entry_field(app, app->active_entry, KDBXEntryFieldAutotype, error)) {
            return false;
        }
        flippass_scene_entry_actions_show_text(
            app,
            "AutoType",
            flippass_scene_entry_actions_safe_text(
                entry->autotype_sequence, "{USERNAME}{TAB}{PASSWORD}{ENTER}"));
        return true;
    case FlipPassEntryActionTypeUsernameUsb:
    case FlipPassEntryActionTypePasswordUsb:
    case FlipPassEntryActionTypeAutoTypeUsb:
    case FlipPassEntryActionTypeLoginUsb:
    case FlipPassEntryActionTypeOtherUsb:
    case FlipPassEntryActionTypeUsernameBluetooth:
    case FlipPassEntryActionTypePasswordBluetooth:
    case FlipPassEntryActionTypeAutoTypeBluetooth:
    case FlipPassEntryActionTypeLoginBluetooth:
    case FlipPassEntryActionTypeOtherBluetooth:
        scene_manager_next_scene(app->scene_manager, FlipPassScene_SendConfirm);
        return true;
    case FlipPassEntryActionNone:
    default:
        furi_string_set_str(error, "Unsupported entry action.");
        return false;
    }
}

static void flippass_scene_entry_actions_callback(void* context, uint32_t index) {
    App* app = context;
    FuriString* error = furi_string_alloc();
    if(!flippass_scene_entry_actions_execute(app, index, error)) {
        flippass_scene_status_show(
            app,
            "Entry Action Failed",
            furi_string_get_cstr(error),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }
    furi_string_free(error);
}

void flippass_scene_entry_actions_on_enter(void* context) {
    App* app = context;
    const KDBXEntry* entry = app->active_entry;
    FuriString* error = furi_string_alloc();

    if(entry == NULL) {
        flippass_scene_status_show(
            app,
            "No Entry Selected",
            "Return to the browser and pick an entry first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    if(!flippass_db_activate_entry(app, app->active_entry, false, error)) {
        flippass_scene_status_show(
            app,
            "Entry Load Failed",
            furi_string_get_cstr(error),
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }
    furi_string_free(error);

    submenu_reset(app->submenu);
    submenu_set_header(
        app->submenu,
        flippass_scene_entry_actions_safe_text(entry->title, "Untitled Entry"));

    submenu_add_item(
        app->submenu,
        "View Details",
        FlipPassEntryActionShowDetails,
        flippass_scene_entry_actions_callback,
        app);

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername)) {
        submenu_add_item(
            app->submenu,
            "Show Username",
            FlipPassEntryActionRevealUsername,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Username (USB)",
            FlipPassEntryActionTypeUsernameUsb,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Username (BT)",
            FlipPassEntryActionTypeUsernameBluetooth,
            flippass_scene_entry_actions_callback,
            app);
    }

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldPassword)) {
        submenu_add_item(
            app->submenu,
            "Show Password",
            FlipPassEntryActionRevealPassword,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Password (USB)",
            FlipPassEntryActionTypePasswordUsb,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Password (BT)",
            FlipPassEntryActionTypePasswordBluetooth,
            flippass_scene_entry_actions_callback,
            app);
    }

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUrl)) {
        submenu_add_item(
            app->submenu,
            "Show URL",
            FlipPassEntryActionRevealUrl,
            flippass_scene_entry_actions_callback,
            app);
    }

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldNotes)) {
        submenu_add_item(
            app->submenu,
            "Show Notes",
            FlipPassEntryActionRevealNotes,
            flippass_scene_entry_actions_callback,
            app);
    }

    if(flippass_scene_entry_actions_has_other_fields(entry)) {
        submenu_add_item(
            app->submenu,
            "Other Fields",
            FlipPassEntryActionBrowseOtherFields,
            flippass_scene_entry_actions_callback,
            app);
    }

    submenu_add_item(
        app->submenu,
        "Show AutoType",
        FlipPassEntryActionRevealAutoType,
        flippass_scene_entry_actions_callback,
        app);

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) &&
       flippass_db_entry_has_field(entry, KDBXEntryFieldPassword)) {
        submenu_add_item(
            app->submenu,
            "Type AutoType (USB)",
            FlipPassEntryActionTypeAutoTypeUsb,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Login (USB)",
            FlipPassEntryActionTypeLoginUsb,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type AutoType (BT)",
            FlipPassEntryActionTypeAutoTypeBluetooth,
            flippass_scene_entry_actions_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Type Login (BT)",
            FlipPassEntryActionTypeLoginBluetooth,
            flippass_scene_entry_actions_callback,
            app);
    }

    submenu_set_selected_item(app->submenu, app->action_selected_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool flippass_scene_entry_actions_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void flippass_scene_entry_actions_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

bool flippass_entry_actions_trigger(App* app, uint32_t index, FuriString* error) {
    return flippass_scene_entry_actions_execute(app, index, error);
}
