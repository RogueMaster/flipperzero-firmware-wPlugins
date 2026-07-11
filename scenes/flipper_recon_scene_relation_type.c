#include "../flipper_recon_i.h"

static void flipper_recon_scene_relation_type_callback(void* context, uint32_t index) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_recon_scene_relation_type_on_enter(void* context) {
    FlipperReconApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Relation type");
    for(uint16_t t = 0; t < RelTypeCount; t++) {
        submenu_add_item(
            submenu, relation_type_name(t), t, flipper_recon_scene_relation_type_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool flipper_recon_scene_relation_type_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        RelationType type = (event.event < RelTypeCount) ? event.event : RelConnectsTo;
        graph_add_relation(app->session, app->rel_from_id, app->rel_to_id, type);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipperReconSceneRelationList);
    }
    return consumed;
}

void flipper_recon_scene_relation_type_on_exit(void* context) {
    FlipperReconApp* app = context;
    submenu_reset(app->submenu);
}
