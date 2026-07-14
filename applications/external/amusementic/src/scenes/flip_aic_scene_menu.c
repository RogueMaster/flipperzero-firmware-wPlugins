#include "../flip_aic.h"
#include "flip_aic_scene.h"

enum SubmenuIndex {
    SubmenuIndexRead,
    SubmenuIndexSaved,
};

static void flip_aic_scene_menu_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    FlipAIC* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flip_aic_scene_menu_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    submenu_add_item(
        app->submenu, "Read", SubmenuIndexRead, flip_aic_scene_menu_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Saved", SubmenuIndexSaved, flip_aic_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FlipAICSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewSubmenu);
}

bool flip_aic_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, FlipAICSceneMenu, event.event);
        switch(event.event) {
        case SubmenuIndexRead:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneRead);
            return true;
        case SubmenuIndexSaved:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneSaved);
            return true;
        }
    }

    return false;
}

void flip_aic_scene_menu_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    submenu_reset(app->submenu);
}
