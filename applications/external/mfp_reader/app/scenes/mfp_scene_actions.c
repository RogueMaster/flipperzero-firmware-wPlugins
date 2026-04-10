#include "../mfp_app.h"
#include "../mfp_storage.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <notification/notification_messages.h>

typedef enum {
    ActionsMenuSave = 0,
    ActionsMenuEmulate,
    ActionsMenuViewDump,
    ActionsMenuCardInfo,
    ActionsMenuDelete,
} ActionsMenuItem;

static void actions_submenu_cb(void* ctx, uint32_t index) {
    MfpApp* app = ctx;
    if(index == ActionsMenuSave) {
        /* Go through the name editor first so the user can review or
         * change the suggested file name before the dump hits disk. */
        scene_manager_next_scene(app->scene_manager, MfpSceneSaveName);
    } else if(index == ActionsMenuEmulate) {
        scene_manager_next_scene(app->scene_manager, MfpSceneEmulateSetup);
    } else if(index == ActionsMenuViewDump) {
        scene_manager_next_scene(app->scene_manager, MfpSceneDumpView);
    } else if(index == ActionsMenuCardInfo) {
        scene_manager_next_scene(app->scene_manager, MfpSceneCardInfo);
    } else if(index == ActionsMenuDelete) {
        scene_manager_next_scene(app->scene_manager, MfpSceneDeleteConfirm);
    }
}

void mfp_scene_actions_on_enter(void* ctx) {
    MfpApp* app = ctx;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Actions");
    /* Save is only meaningful for fresh scans — loaded files are
     * already on disk and Save would just overwrite the same bytes. */
    if(!app->loaded_from_file) {
        submenu_add_item(
            app->submenu, "Save", ActionsMenuSave, actions_submenu_cb, app);
    }
    submenu_add_item(
        app->submenu, "Emulate", ActionsMenuEmulate, actions_submenu_cb, app);
    submenu_add_item(
        app->submenu, "View dump", ActionsMenuViewDump, actions_submenu_cb, app);
    submenu_add_item(
        app->submenu, "Card Info", ActionsMenuCardInfo, actions_submenu_cb, app);
    /* Delete only makes sense for files already on disk. */
    if(app->loaded_from_file) {
        submenu_add_item(
            app->submenu, "Delete", ActionsMenuDelete, actions_submenu_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewSubmenu);
}

bool mfp_scene_actions_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void mfp_scene_actions_on_exit(void* ctx) {
    MfpApp* app = ctx;
    submenu_reset(app->submenu);
}
