#include "../breach_map_i.h"

static void breach_map_scene_relation_pick_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void breach_map_scene_relation_pick_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;
    Session* session = app->session;

    submenu_reset(submenu);
    submenu_set_header(submenu, app->rel_pick_to ? "Target asset" : "Source asset");

    for(uint16_t i = 0; i < session->asset_count; i++) {
        /* when choosing the target, skip the already chosen source */
        if(app->rel_pick_to && session->assets[i].id == app->rel_from_id) continue;
        submenu_add_item(
            submenu, session->assets[i].name, i, breach_map_scene_relation_pick_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_relation_pick_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        uint16_t asset_index = event.event;
        if(asset_index >= app->session->asset_count) return true;
        uint16_t id = app->session->assets[asset_index].id;

        if(!app->rel_pick_to) {
            app->rel_from_id = id;
            app->rel_pick_to = true;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneRelationPick);
        } else {
            app->rel_to_id = id;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneRelationType);
        }
    }
    return consumed;
}

void breach_map_scene_relation_pick_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
