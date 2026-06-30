#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexUseDefaults,
    VarItemListIndexStaticFreqs,
    VarItemListIndexHopperFreqs,
};

void cfw_app_scene_protocols_freqs_var_item_list_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_protocols_freqs_use_defaults_changed(VariableItem* item) {
    CFWApp* app = variable_item_get_context(item);
    bool value = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    app->subghz_use_defaults = value;
    app->save_subghz_freqs = true;
}

void cfw_app_scene_protocols_freqs_on_enter(void* context) {
    CFWApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        var_item_list, "Use Defaults", 2, cfw_app_scene_protocols_freqs_use_defaults_changed, app);
    variable_item_set_current_value_index(item, app->subghz_use_defaults);
    variable_item_set_current_value_text(item, app->subghz_use_defaults ? "ON" : "OFF");

    item = variable_item_list_add(var_item_list, "Static Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    item = variable_item_list_add(var_item_list, "Hopper Freqs", 0, NULL, app);
    variable_item_set_current_value_text(item, ">");

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_protocols_freqs_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneProtocolsFreqs));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewVarItemList);
}

bool cfw_app_scene_protocols_freqs_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CFWAppSceneProtocolsFreqs, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexStaticFreqs:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneProtocolsFreqsStatic, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneProtocolsFreqsStatic);
            break;
        case VarItemListIndexHopperFreqs:
            scene_manager_set_scene_state(app->scene_manager, CFWAppSceneProtocolsFreqsHopper, 0);
            scene_manager_next_scene(app->scene_manager, CFWAppSceneProtocolsFreqsHopper);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_protocols_freqs_on_exit(void* context) {
    CFWApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
