#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"
#include <gui/scene_manager.h>

typedef enum {
    StartMenuRead = 0,
    StartMenuSaved,
} StartMenuItem;

static void start_submenu_cb(void* ctx, uint32_t index) {
    MfpApp* app = ctx;
    if(index == StartMenuRead) {
        app->loaded_from_file = false;
        scene_manager_next_scene(app->scene_manager, MfpSceneRead);
    } else if(index == StartMenuSaved) {
        scene_manager_next_scene(app->scene_manager, MfpSceneSaved);
    }
}

void mfp_scene_start_on_enter(void* ctx) {
    MfpApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "MFP Reader");
    submenu_add_item(app->submenu, "Read card",    StartMenuRead,  start_submenu_cb, app);
    submenu_add_item(app->submenu, "Saved cards",  StartMenuSaved, start_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewSubmenu);
}

bool mfp_scene_start_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void mfp_scene_start_on_exit(void* ctx) {
    MfpApp* app = ctx;
    submenu_reset(app->submenu);
}
