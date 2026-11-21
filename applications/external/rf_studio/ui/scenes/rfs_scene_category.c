#include "../../rf_studio_i.h"

void rfs_scene_category_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Category");
    for(uint32_t i = 0; i < RfsCategoryCount; i++) {
        submenu_add_item(submenu, rfs_category_name(i), i, rfs_app_submenu_callback, app);
    }
    submenu_set_selected_item(submenu, app->remote.category);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_category_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    if(index < RfsCategoryCount) {
        app->remote.category = index;
        if(!rfs_app_save_remote(app)) {
            rfs_app_message(app, "Not Saved", "Could not save the remote.", RfsSceneNum);
            return true;
        }
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void rfs_scene_category_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
