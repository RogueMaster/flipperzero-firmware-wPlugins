#include "../../rf_studio_i.h"

#define ITEM_IMPORT  0
#define ITEM_CAPTURE 1
#define ITEM_EMPTY   99
#define ITEM_SIGNAL  100

void rfs_scene_signal_list_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    FuriString* label = furi_string_alloc();
    bool browse = app->signal_list_mode == RfsSignalListBrowse;

    if(browse) {
        furi_string_printf(label, "Signal Library (%u)", (unsigned)app->signals.count);
        submenu_set_header(submenu, furi_string_get_cstr(label));
        submenu_add_item(
            submenu, "+ Import .sub File", ITEM_IMPORT, rfs_app_submenu_callback, app);
        submenu_add_item(submenu, "+ Capture Signal", ITEM_CAPTURE, rfs_app_submenu_callback, app);
    } else {
        submenu_set_header(submenu, "Choose a Signal");
    }

    /* [F] fixed code, [R] raw, [D] rolling code, [?] unknown (SIG-007). */
    for(size_t i = 0; i < app->signals.count; i++) {
        const RfsSignalMeta* meta = &app->signals.items[i];
        furi_string_printf(label, "[%s] %s", rfs_kind_tag(meta->kind), meta->name);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), ITEM_SIGNAL + i, rfs_app_submenu_callback, app);
    }
    if(app->signals.count == 0) {
        submenu_add_item(submenu, "(library is empty)", ITEM_EMPTY, rfs_app_submenu_callback, app);
    }
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneSignalList));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

/* After picking from inside the capture flow, return to the remote; otherwise go back. */
static void rfs_signal_list_return(RfStudioApp* app) {
    if(scene_manager_has_previous_scene(app->scene_manager, RfsSceneCapture)) {
        rfs_app_exit_capture(app);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool rfs_scene_signal_list_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalList, index);

    if(index == ITEM_IMPORT) {
        app->import_kind = RfsImportSignal;
        scene_manager_next_scene(app->scene_manager, RfsSceneFileImport);
        return true;
    }
    if(index == ITEM_CAPTURE) {
        rfs_app_start_library_capture(app);
        return true;
    }
    if(index < ITEM_SIGNAL || index - ITEM_SIGNAL >= app->signals.count) return true;

    const RfsSignalMeta* meta = &app->signals.items[index - ITEM_SIGNAL];
    switch(app->signal_list_mode) {
    case RfsSignalListBrowse:
        strlcpy(app->signal_id, meta->id, sizeof(app->signal_id));
        scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalMenu, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneSignalMenu);
        break;
    case RfsSignalListPickForButton:
        if(app->button_index >= 0 && (size_t)app->button_index < app->remote.count) {
            RfsButton* button = &app->remote.buttons[app->button_index];
            strlcpy(button->signal_id, meta->id, sizeof(button->signal_id));
            button->updated = rfs_storage_now();
            if(!rfs_app_save_remote(app)) {
                rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
                break;
            }
        }
        rfs_signal_list_return(app);
        break;
    case RfsSignalListPickNewButton: {
        /* A signal can back many buttons without being copied (SIG-008). */
        RfsButton* button =
            rfs_remote_add_button(app->storage, &app->remote, meta->name, meta->id);
        if(!button) {
            rfs_app_message(
                app, "Low Memory", "Not enough memory to\nadd another button.", RfsSceneNum);
            break;
        }
        app->button_index = app->remote.count - 1;
        rfs_app_remember_frequency(app, meta->frequency, meta->preset);
        if(!rfs_app_save_remote(app)) {
            rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
            break;
        }
        rfs_signal_list_return(app);
        break;
    }
    }
    return true;
}

void rfs_scene_signal_list_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
