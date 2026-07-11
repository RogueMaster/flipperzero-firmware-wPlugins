#include "../flipper_recon_i.h"

#define RELATION_LIST_ADD_INDEX 0
#define RELATION_LIST_OFFSET    1

static void flipper_recon_scene_relation_list_callback(void* context, uint32_t index) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_recon_scene_relation_list_on_enter(void* context) {
    FlipperReconApp* app = context;
    Submenu* submenu = app->submenu;
    Session* session = app->session;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Relations");
    submenu_add_item(
        submenu,
        "[+] Add relation",
        RELATION_LIST_ADD_INDEX,
        flipper_recon_scene_relation_list_callback,
        app);

    FuriString* label = furi_string_alloc();
    for(uint16_t i = 0; i < session->relation_count; i++) {
        const Relation* r = &session->relations[i];
        uint16_t fi = asset_manager_index_by_id(session, r->from_id);
        uint16_t ti = asset_manager_index_by_id(session, r->to_id);
        furi_string_printf(
            label,
            "%s %s %s",
            (fi != RECON_INVALID_INDEX) ? session->assets[fi].name : "?",
            relation_type_name(r->type),
            (ti != RECON_INVALID_INDEX) ? session->assets[ti].name : "?");
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            i + RELATION_LIST_OFFSET,
            flipper_recon_scene_relation_list_callback,
            app);
    }
    furi_string_free(label);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FlipperReconSceneRelationList));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool flipper_recon_scene_relation_list_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, FlipperReconSceneRelationList, event.event);
        consumed = true;
        if(event.event == RELATION_LIST_ADD_INDEX) {
            if(app->session->asset_count < 2) {
                app->message_mode = ReconMessageInfo;
                furi_string_set(app->message_text, "Need at least 2 assets");
                scene_manager_next_scene(app->scene_manager, FlipperReconSceneMessage);
            } else {
                app->rel_pick_to = false;
                scene_manager_next_scene(app->scene_manager, FlipperReconSceneRelationPick);
            }
        } else {
            app->selected_relation = event.event - RELATION_LIST_OFFSET;
            app->message_mode = ReconMessageConfirmDeleteRelation;
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneMessage);
        }
    }
    return consumed;
}

void flipper_recon_scene_relation_list_on_exit(void* context) {
    FlipperReconApp* app = context;
    submenu_reset(app->submenu);
}
