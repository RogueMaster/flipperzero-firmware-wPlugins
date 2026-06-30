#include "uv_meter_app_i.hpp"
#include "uv_meter_event.hpp"

static const char* i2c_addresses[] = {
    [UVMeterI2CAddressAuto] = "Auto",
    [UVMeterI2CAddress74] = "0x74",
    [UVMeterI2CAddress75] = "0x75",
    [UVMeterI2CAddress76] = "0x76",
    [UVMeterI2CAddress77] = "0x77",
};

static const char* units[] = {
    [UVMeterUnituW_cm_2] = "uW/cm2",
    [UVMeterUnitW_m_2] = "W/m2",
    [UVMeterUnitmW_m_2] = "mW/m2",
};

static void i2c_address_change_callback(VariableItem* item) {
    UVMeterApp* app = static_cast<UVMeterApp*>(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, i2c_addresses[index]);

    app->app_state->i2c_address = static_cast<UVMeterI2CAddress>(index);
}

static void unit_change_callback(VariableItem* item) {
    UVMeterApp* app = static_cast<UVMeterApp*>(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, units[index]);

    app->app_state->unit = static_cast<UVMeterUnit>(index);
}

static void enter_callback(void* context, uint32_t index) {
    auto* app = static_cast<UVMeterApp*>(context);
    switch(index) {
    // Update indices when adding new setting items
    case 2:
        scene_manager_handle_custom_event(app->scene_manager, UVMeterCustomEventSceneEnterHelp);
        break;
    case 3:
        scene_manager_handle_custom_event(app->scene_manager, UVMeterCustomEventSceneEnterAbout);
        break;
    default:
        break;
    }
}

void uv_meter_scene_settings_on_enter(void* context) {
    auto* app = static_cast<UVMeterApp*>(context);
    variable_item_list_reset(app->variable_item_list);
    VariableItem* item;

    item = variable_item_list_add(
        app->variable_item_list,
        "I2C Address",
        COUNT_OF(i2c_addresses),
        i2c_address_change_callback,
        app);
    variable_item_set_current_value_index(item, app->app_state->i2c_address);
    variable_item_set_current_value_text(item, i2c_addresses[app->app_state->i2c_address]);

    item = variable_item_list_add(
        app->variable_item_list, "Unit", COUNT_OF(units), unit_change_callback, app);
    variable_item_set_current_value_index(item, app->app_state->unit);
    variable_item_set_current_value_text(item, units[app->app_state->unit]);

    // Be aware when adding new items before "Help" to change index in `enter_callback()`
    variable_item_list_add(app->variable_item_list, "Help", 0, NULL, NULL);
    variable_item_list_add(app->variable_item_list, "About", 0, NULL, NULL);

    variable_item_list_set_enter_callback(app->variable_item_list, enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, UVMeterViewVariableItemList);
}

bool uv_meter_scene_settings_on_event(void* context, SceneManagerEvent event) {
    auto* app = static_cast<UVMeterApp*>(context);
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case UVMeterCustomEventSceneEnterHelp:
            scene_manager_next_scene(app->scene_manager, UVMeterSceneHelp);
            consumed = true;
            break;
        case UVMeterCustomEventSceneEnterAbout:
            scene_manager_next_scene(app->scene_manager, UVMeterSceneAbout);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void uv_meter_scene_settings_on_exit(void* context) {
    auto* app = static_cast<UVMeterApp*>(context);
    variable_item_list_reset(app->variable_item_list);
}