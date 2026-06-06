#include "../disn3y_toolbox_app.h"

static uint32_t loc_broadcast_item_index;

static void loc_config_location_changed(VariableItem* item) {
    Disn3yToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->selected_droid_location = (DroidLocation)index;
    variable_item_set_current_value_text(item, droid_location_info[index].name);
}

static void loc_config_interval_changed(VariableItem* item) {
    Disn3yToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->droid_loc_interval_idx = index;
    variable_item_set_current_value_text(item, loc_interval_info[index].name);
}

static void loc_config_rssi_changed(VariableItem* item) {
    Disn3yToolboxApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->droid_loc_rssi_idx = index;
    variable_item_set_current_value_text(item, loc_rssi_info[index].name);
}

static void loc_config_enter_callback(void* context, uint32_t index) {
    Disn3yToolboxApp* app = context;
    UNUSED(index);
    app->beacon_data_len = droid_location_beacon_generate(
        app->selected_droid_location,
        loc_interval_info[app->droid_loc_interval_idx].value,
        loc_rssi_info[app->droid_loc_rssi_idx].value,
        app->beacon_data);
    scene_manager_set_scene_state(app->scene_manager, Disn3yToolboxAppSceneDroidLocation, index);
    scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneDroidLocationBroadcast);
}

void disn3y_toolbox_app_scene_droid_location_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    uint32_t item_count = 0;

    item = variable_item_list_add(
        list, "Location", DroidLocationCount, loc_config_location_changed, app);
    variable_item_set_current_value_index(item, app->selected_droid_location);
    variable_item_set_current_value_text(
        item, droid_location_info[app->selected_droid_location].name);
    item_count++;

    item = variable_item_list_add(
        list, "Interval", LOC_INTERVAL_COUNT, loc_config_interval_changed, app);
    variable_item_set_current_value_index(item, app->droid_loc_interval_idx);
    variable_item_set_current_value_text(
        item, loc_interval_info[app->droid_loc_interval_idx].name);
    item_count++;

    item = variable_item_list_add(list, "Distance", LOC_RSSI_COUNT, loc_config_rssi_changed, app);
    variable_item_set_current_value_index(item, app->droid_loc_rssi_idx);
    variable_item_set_current_value_text(item, loc_rssi_info[app->droid_loc_rssi_idx].name);
    item_count++;

    variable_item_list_add(list, "Start Broadcast", 0, NULL, app);
    loc_broadcast_item_index = item_count;

    uint32_t selected =
        scene_manager_get_scene_state(app->scene_manager, Disn3yToolboxAppSceneDroidLocation);
    variable_item_list_set_selected_item(list, selected);

    variable_item_list_set_enter_callback(list, loc_config_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewConfig);
}

bool disn3y_toolbox_app_scene_droid_location_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void disn3y_toolbox_app_scene_droid_location_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
