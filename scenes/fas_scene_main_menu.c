#include "../animation_switcher.h"
#include "fas_scene.h"
#include "animation_switcher_icons.h"

typedef enum {
    FasMainIdxCreate = 0,
    FasMainIdxImport,
    FasMainIdxChoose,
    FasMainIdxDelete,
    FasMainIdxAbout,
} FasMainMenuIdx;

static void fas_main_menu_cb(void* context, uint32_t index) {
    FasApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)index);
}

void fas_scene_main_menu_on_enter(void* context) {
    FasApp* app = context;
    /* Returning to the main menu is the canonical "back to neutral state"
     * point; clear any transient flags here. */
    app->import_mode = false;

    menu_reset(app->menu);
    menu_add_item(app->menu, "Create Playlist", &I_create, FasMainIdxCreate, fas_main_menu_cb, app);
    menu_add_item(app->menu, "Import Manifest", &I_create, FasMainIdxImport, fas_main_menu_cb, app);
    menu_add_item(app->menu, "Choose Playlist", &I_choose, FasMainIdxChoose, fas_main_menu_cb, app);
    menu_add_item(app->menu, "Delete Playlist", &I_delete, FasMainIdxDelete, fas_main_menu_cb, app);
    menu_add_item(app->menu, "About / Help",    &I_about,  FasMainIdxAbout,  fas_main_menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewMenu);
}

bool fas_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    FasApp* app      = context;
    bool    consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasMainIdxCreate:
            scene_manager_next_scene(app->scene_manager, FasSceneAnimList);
            consumed = true;
            break;
        case FasMainIdxImport:
            /* Skip the flow entirely if there is nothing to import — leaves
             * the user on the main menu rather than asking for a name and
             * silently failing. */
            if(fas_manifest_exists(app)) {
                app->import_mode = true;
                scene_manager_next_scene(app->scene_manager, FasScenePlaylistName);
            }
            consumed = true;
            break;
        case FasMainIdxChoose:
            scene_manager_next_scene(app->scene_manager, FasSceneChoosePlaylist);
            consumed = true;
            break;
        case FasMainIdxDelete:
            scene_manager_next_scene(app->scene_manager, FasSceneDeletePlaylist);
            consumed = true;
            break;
        case FasMainIdxAbout:
            scene_manager_next_scene(app->scene_manager, FasSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_main_menu_on_exit(void* context) {
    FasApp* app = context;
    menu_reset(app->menu);
}