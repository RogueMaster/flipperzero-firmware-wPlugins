#include "src/core/cfs_app_i.h"

typedef enum {
    TagActionClone,
    TagActionEdit,
} TagAction;

void cfs_scene_tag_actions_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Write to Tag");
    submenu_add_item(submenu, "Clone (as-is)", TagActionClone, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Edit & Write", TagActionEdit, cfs_submenu_send_index, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_tag_actions_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case TagActionClone:
        cfs_app_stage_write_from_loaded(app, true); // clone: preserve the exact captured payload
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteConfirm);
        return true;
    case TagActionEdit:
        cfs_app_stage_write_from_loaded(app, false); // user is changing fields; re-encode
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteBrand);
        return true;
    default:
        return false;
    }
}

void cfs_scene_tag_actions_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}
