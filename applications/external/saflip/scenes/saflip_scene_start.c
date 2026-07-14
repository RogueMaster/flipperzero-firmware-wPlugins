#include "../saflip.h"
#include <furi_hal.h>

enum SubmenuIndex {
    SubmenuIndexReadCard,
    SubmenuIndexSaved,
    SubmenuIndexCreate,
};

void saflip_scene_start_submenu_callback(void* context, uint32_t index) {
    SaflipApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void saflip_scene_start_on_enter(void* context) {
    SaflipApp* app = context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Read Card",
        SubmenuIndexReadCard,
        saflip_scene_start_submenu_callback,
        context);

    submenu_add_item(
        app->submenu, "Saved", SubmenuIndexSaved, saflip_scene_start_submenu_callback, context);

    submenu_add_item(
        app->submenu, "Create", SubmenuIndexCreate, saflip_scene_start_submenu_callback, context);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, SaflipSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewSubmenu);
}

bool saflip_scene_start_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexReadCard) {
            scene_manager_set_scene_state(
                app->scene_manager, SaflipSceneDetectCard, SaflipDetectCardModeRead);
            scene_manager_next_scene(app->scene_manager, SaflipSceneDetectCard);
            consumed = true;
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_next_scene(app->scene_manager, SaflipSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexCreate) {
            // Reset data before entering Edit scene
            saflip_reset_data(app);
            scene_manager_set_scene_state(app->scene_manager, SaflipSceneEdit, 0);
            scene_manager_next_scene(app->scene_manager, SaflipSceneEdit);
            consumed = true;
        }

        if(consumed) {
            scene_manager_set_scene_state(app->scene_manager, SaflipSceneStart, event.event);
        }
    }

    return consumed;
}

void saflip_scene_start_on_exit(void* context) {
    SaflipApp* app = context;
    submenu_reset(app->submenu);
}
