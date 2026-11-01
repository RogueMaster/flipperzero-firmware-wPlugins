#include "src/core/cfs_app_i.h"

// Pick a template to delete; the confirm step is cfs_scene_template_delete_confirm.
// Scene state of the confirm scene holds the picked index.

void cfs_scene_template_delete_on_enter(void* context) {
    CfsApp* app = context;
    app->template_count =
        cfs_storage_list(app->storage, CFS_TEMPLATE_DIR, app->template_names, CFS_TEMPLATES_MAX);
    if(app->template_count == 0) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Delete which?");
    for(size_t i = 0; i < app->template_count; i++) {
        submenu_add_item(
            submenu,
            app->template_names[i],
            CfsCustomEventMenuBase + i,
            cfs_submenu_send_index,
            app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_template_delete_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < CfsCustomEventMenuBase ||
       event.event >= CfsCustomEventMenuBase + app->template_count) {
        return false;
    }
    scene_manager_set_scene_state(
        app->scene_manager, CfsSceneTemplateDeleteConfirm, event.event - CfsCustomEventMenuBase);
    scene_manager_next_scene(app->scene_manager, CfsSceneTemplateDeleteConfirm);
    return true;
}

void cfs_scene_template_delete_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
