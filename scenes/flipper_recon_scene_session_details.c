#include "../flipper_recon_i.h"

typedef enum {
    DetailIndexName,
    DetailIndexClient,
    DetailIndexLocation,
} DetailIndex;

static void flipper_recon_scene_session_details_enter_callback(void* context, uint32_t index) {
    FlipperReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void add_field(VariableItemList* list, const char* label, const char* value) {
    VariableItem* item = variable_item_list_add(list, label, 1, NULL, NULL);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, (value && value[0]) ? value : "-");
}

void flipper_recon_scene_session_details_on_enter(void* context) {
    FlipperReconApp* app = context;
    VariableItemList* list = app->var_item_list;
    Session* session = app->session;

    variable_item_list_reset(list);
    add_field(list, "Name", session->name);
    add_field(list, "Client", session->client);
    add_field(list, "Location", session->location);

    variable_item_list_set_enter_callback(
        list, flipper_recon_scene_session_details_enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, FlipperReconSceneSessionDetails));

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

bool flipper_recon_scene_session_details_on_event(void* context, SceneManagerEvent event) {
    FlipperReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, FlipperReconSceneSessionDetails, event.event);
        consumed = true;
        switch(event.event) {
        case DetailIndexName:
            app->text_target = ReconTextTargetSessionName;
            break;
        case DetailIndexClient:
            app->text_target = ReconTextTargetClient;
            break;
        case DetailIndexLocation:
            app->text_target = ReconTextTargetLocation;
            break;
        default:
            consumed = false;
            break;
        }
        if(consumed) {
            scene_manager_next_scene(app->scene_manager, FlipperReconSceneTextInput);
        }
    }
    return consumed;
}

void flipper_recon_scene_session_details_on_exit(void* context) {
    FlipperReconApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
