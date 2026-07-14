#include "../../app_user.h"

static uint8_t id_request_array[4];
static uint8_t id_response_array[4];

static uint32_t selected_item = 0;

/**
 * Scene to set the Ids for UDS services
 */

// Callback for the settings
void settings_input_callback(void* context, uint32_t index) {
    App* app = context;

    selected_item = index;

    scene_manager_set_scene_state(app->scene_manager, app_scene_uds_set_ids_option, index);

    scene_manager_next_scene(app->scene_manager, app_scene_uds_set_ids_option);
}

// Scene on enter
void app_scene_uds_settings_on_enter(void* context) {
    App* app = context;

    VariableItem* item;

    variable_item_list_reset(app->varList);

    // First Item
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "0x%lx", app->uds_send_id);

    item = variable_item_list_add(app->varList, "REQUEST ID", 0, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    // Second Item
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "0x%lx", app->uds_received_id);

    item = variable_item_list_add(app->varList, "RESPONSE ID", 0, NULL, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    variable_item_list_set_enter_callback(app->varList, settings_input_callback, app);

    variable_item_list_set_selected_item(app->varList, selected_item);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// Scene on event
bool app_scene_uds_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// Scene on exit
void app_scene_uds_settings_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/**
 * To set the id's
 */

void set_data(void* context) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_uds_set_ids_option);

    switch(state) {
    case 0:

        app->uds_send_id = (id_request_array[0] << 24) | (id_request_array[1] << 16) |
                           (id_request_array[2] << 8) | (id_request_array[3]);
        break;

    case 1:

        app->uds_received_id = (id_response_array[0] << 24) | (id_response_array[1] << 16) |
                               (id_response_array[2] << 8) | (id_response_array[3]);
        break;

    default:
        break;
    }

    scene_manager_previous_scene(app->scene_manager);
}

// Scene on enter
void app_scene_uds_set_ids_on_enter(void* context) {
    App* app = context;
    ByteInput* scene = app->input_byte_value;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_uds_set_ids_option);

    switch(state) {
    case 0:

        id_request_array[3] = app->uds_send_id;
        id_request_array[2] = app->uds_send_id >> 8;
        id_request_array[1] = app->uds_send_id >> 16;
        id_request_array[0] = app->uds_send_id >> 24;

        byte_input_set_result_callback(scene, set_data, NULL, app, id_request_array, 4);
        byte_input_set_header_text(scene, "SET REQUEST DATA");
        break;

    case 1:

        id_response_array[3] = app->uds_received_id;
        id_response_array[2] = app->uds_received_id >> 8;
        id_response_array[1] = app->uds_received_id >> 16;
        id_response_array[0] = app->uds_received_id >> 24;

        byte_input_set_result_callback(scene, set_data, NULL, app, id_response_array, 4);
        byte_input_set_header_text(scene, "SET RESPONSE DATA");
        break;

    default:
        break;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

// Scene on event
bool app_scene_uds_set_ids_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// Scene on exit
void app_scene_uds_set_ids_on_exit(void* context) {
    App* app = context;
    UNUSED(app);
}
