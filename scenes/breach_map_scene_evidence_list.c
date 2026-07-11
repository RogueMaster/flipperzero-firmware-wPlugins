#include "../breach_map_i.h"

#define EVIDENCE_LIST_ADD_INDEX 0
#define EVIDENCE_LIST_OFFSET    1

static void breach_map_scene_evidence_list_callback(void* context, uint32_t index) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static uint16_t current_asset_id(BreachMapApp* app) {
    if(app->selected_asset >= app->session->asset_count) return RECON_INVALID_INDEX;
    return app->session->assets[app->selected_asset].id;
}

void breach_map_scene_evidence_list_on_enter(void* context) {
    BreachMapApp* app = context;
    Submenu* submenu = app->submenu;
    Session* session = app->session;
    uint16_t asset_id = current_asset_id(app);

    submenu_reset(submenu);
    submenu_set_header(submenu, "Evidence");
    submenu_add_item(
        submenu,
        "[+] Add evidence",
        EVIDENCE_LIST_ADD_INDEX,
        breach_map_scene_evidence_list_callback,
        app);

    FuriString* label = furi_string_alloc();
    for(uint16_t i = 0; i < session->evidence_count; i++) {
        const Evidence* e = &session->evidence[i];
        if(e->asset_id != asset_id) continue;
        furi_string_printf(label, "%s [%s]", e->label, evidence_type_name(e->type));
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            i + EVIDENCE_LIST_OFFSET,
            breach_map_scene_evidence_list_callback,
            app);
    }
    furi_string_free(label);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool breach_map_scene_evidence_list_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == EVIDENCE_LIST_ADD_INDEX) {
            uint16_t asset_id = current_asset_id(app);
            uint16_t idx =
                asset_manager_add_evidence(app->session, asset_id, EvidenceNote, "Evidence", "");
            if(idx == RECON_INVALID_INDEX) {
                app->message_mode = ReconMessageInfo;
                furi_string_set(app->message_text, "Evidence limit reached");
                scene_manager_next_scene(app->scene_manager, BreachMapSceneMessage);
            } else {
                app->selected_evidence = idx;
                scene_manager_next_scene(app->scene_manager, BreachMapSceneEvidenceEdit);
            }
        } else {
            app->selected_evidence = event.event - EVIDENCE_LIST_OFFSET;
            scene_manager_next_scene(app->scene_manager, BreachMapSceneEvidenceEdit);
        }
    }
    return consumed;
}

void breach_map_scene_evidence_list_on_exit(void* context) {
    BreachMapApp* app = context;
    submenu_reset(app->submenu);
}
