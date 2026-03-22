#include "../wi_fir.h"
#include "../wfr_storage.h"

static const char* ack_names[] = {"Off", "On"};

static void wi_fir_scene_settings_ack_change(VariableItem* item) {
    WiFirApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->ack_enabled = (index == 1);
    variable_item_set_current_value_text(item, ack_names[index]);
    wfr_storage_save_settings(app->storage, app->ack_enabled);
}

void wi_fir_scene_settings_on_enter(void* context) {
    WiFirApp* app = context;

    variable_item_list_reset(app->variable_item_list);

    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Wait for ACK",
        2,
        wi_fir_scene_settings_ack_change,
        app);
    variable_item_set_current_value_index(item, app->ack_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, ack_names[app->ack_enabled ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewVariableItemList);
}

bool wi_fir_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wi_fir_scene_settings_on_exit(void* context) {
    WiFirApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
