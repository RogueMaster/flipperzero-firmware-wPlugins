#include "src/core/cfs_app_i.h"

typedef enum {
    SavedActionView,
    SavedActionWrite,
    SavedActionEdit,
    SavedActionDelete,
} SavedAction;

void cfs_scene_saved_entry_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    // Header: the chosen file's basename (strip the browsed directory path).
    const char* full = furi_string_get_cstr(app->selected_path);
    const char* base = strrchr(full, '/');
    submenu_set_header(submenu, base ? base + 1 : full);
    submenu_add_item(submenu, "View", SavedActionView, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Write (clone)", SavedActionWrite, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Edit & Write", SavedActionEdit, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Delete", SavedActionDelete, cfs_submenu_send_index, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_saved_entry_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case SavedActionView:
        app->is_blank = false;
        app->result_read_only = true; // view-only; Back returns to this entry
        scene_manager_next_scene(app->scene_manager, CfsSceneReadResult);
        return true;
    case SavedActionWrite:
        cfs_app_stage_write_from_loaded(app, true); // clone: preserve the exact stored payload
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteConfirm);
        return true;
    case SavedActionEdit:
        cfs_app_stage_write_from_loaded(app, false); // user is changing fields; re-encode
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteBrand);
        return true;
    case SavedActionDelete:
        cfs_storage_delete(app->storage, furi_string_get_cstr(app->selected_path));
        notification_message(app->notifications, &sequence_success);
        scene_manager_previous_scene(app->scene_manager); // back to refreshed list
        return true;
    default:
        return false;
    }
}

void cfs_scene_saved_entry_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
