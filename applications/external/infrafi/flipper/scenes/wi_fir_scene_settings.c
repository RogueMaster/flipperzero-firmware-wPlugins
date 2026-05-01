#include "../wi_fir.h"
#include "../wfr_storage.h"

static const char* ack_names[] = {"Off", "On"};
static const char* protocol_names[] = {"RC-6", "NEC"};

static void wi_fir_scene_settings_save(WiFirApp* app) {
    wfr_storage_save_settings(app->storage, app->ack_enabled, app->ir_protocol);
}

static void wi_fir_scene_settings_protocol_change(VariableItem* item) {
    WiFirApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->ir_protocol = (WfrIrProtocol)index;
    variable_item_set_current_value_text(item, protocol_names[index]);
    wi_fir_scene_settings_save(app);
}

static void wi_fir_scene_settings_ack_change(VariableItem* item) {
    WiFirApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->ack_enabled = (index == 1);
    variable_item_set_current_value_text(item, ack_names[index]);
    wi_fir_scene_settings_save(app);
}

void wi_fir_scene_settings_on_enter(void* context) {
    WiFirApp* app = context;

    variable_item_list_reset(app->variable_item_list);

    /* IR Protocol toggle */
    VariableItem* proto_item = variable_item_list_add(
        app->variable_item_list, "IR Protocol", 2, wi_fir_scene_settings_protocol_change, app);
    variable_item_set_current_value_index(proto_item, (uint8_t)app->ir_protocol);
    variable_item_set_current_value_text(proto_item, protocol_names[app->ir_protocol]);

    /* Wait for ACK toggle */
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Wait for ACK", 2, wi_fir_scene_settings_ack_change, app);
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
