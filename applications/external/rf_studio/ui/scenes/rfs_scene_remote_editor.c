#include "../../rf_studio_i.h"

#define ITEM_ADD    0
#define ITEM_BUTTON 1

void rfs_scene_remote_editor_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    FuriString* label =
        furi_string_alloc_printf("Edit: %s (%u)", app->remote.name, (unsigned)app->remote.count);
    submenu_set_header(submenu, furi_string_get_cstr(label));
    submenu_add_item(submenu, "+ Add Button", ITEM_ADD, rfs_app_submenu_callback, app);

    for(size_t i = 0; i < app->remote.count; i++) {
        const RfsButton* button = &app->remote.buttons[i];
        /* Mark buttons whose signal is unassigned or missing (REM-011). */
        bool missing = !rfs_signal_find(&app->signals, button->signal_id);
        furi_string_printf(label, "%s%s", missing ? "! " : "", button->name);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), ITEM_BUTTON + i, rfs_app_submenu_callback, app);
    }
    uint32_t done = ITEM_BUTTON + app->remote.count;
    submenu_add_item(submenu, "Done", done, rfs_app_submenu_callback, app);
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneRemoteEditor));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_remote_editor_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteEditor, index);

    if(index == ITEM_ADD) {
        rfs_app_start_button_capture(app, RfsCaptureForNewButton);
    } else if(index - ITEM_BUTTON < app->remote.count) {
        app->button_index = index - ITEM_BUTTON;
        scene_manager_set_scene_state(app->scene_manager, RfsSceneButtonMenu, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneButtonMenu);
    } else {
        /* Done: changes are already saved as they are made. */
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void rfs_scene_remote_editor_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
