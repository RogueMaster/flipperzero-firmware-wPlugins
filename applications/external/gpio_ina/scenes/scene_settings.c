/*
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app.h"
#include "utils.h"
#include "scenes/scenes.h"

typedef enum {
    MenuIndex_SensorType,
    MenuIndex_I2CAddress,
    MenuIndex_ShuntResistor,
    MenuIndex_ShuntResistorAlt,
    MenuIndex_VoltagePrecision,
    MenuIndex_CurrentPrecision,
    MenuIndex_SensorAveraging,
    MenuIndex_LedBlinking,
    MenuIndex_WiringInfo,
} MenuIndex;

static void on_sensor_type_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensor_type_name(index));

    App* app = (App*)variable_item_get_context(item);
    app->config.sensor_type = index;
}

static void on_i2c_address_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t i2c_address = index + I2C_ADDRESS_MIN;

    FuriString* text = furi_string_alloc_printf("0x%02X", i2c_address);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));
    furi_string_free(text);

    App* app = (App*)variable_item_get_context(item);
    app->config.i2c_address = i2c_address;
}

static void on_voltage_precision_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensor_precision_name(index));

    App* app = (App*)variable_item_get_context(item);
    app->config.voltage_precision = index;
}

static void on_current_precision_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensor_precision_name(index));

    App* app = (App*)variable_item_get_context(item);
    app->config.current_precision = index;
}

static void on_sensor_averaging_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensor_averaging_name(index));

    App* app = (App*)variable_item_get_context(item);
    app->config.sensor_averaging = index;
}

static void on_led_blinking_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");

    App* app = (App*)variable_item_get_context(item);
    app->config.led_blinking = index ? true : false;
}

static void scene_settings_enter_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case MenuIndex_ShuntResistor:
        scene_manager_next_scene(app->scene_manager, SceneEditShunt);
        break;
    case MenuIndex_ShuntResistorAlt:
        scene_manager_next_scene(app->scene_manager, SceneEditShuntAlt);
        break;
    case MenuIndex_WiringInfo:
        scene_manager_next_scene(app->scene_manager, SceneWiring);
        break;
    }
}

void scene_settings_init(App* app) {
    VariableItemList* list = app->var_item_list;

    VariableItem* item;

    item =
        variable_item_list_add(list, "Sensor Type", SensorType_count, on_sensor_type_changed, app);
    variable_item_set_current_value_index(item, app->config.sensor_type);
    on_sensor_type_changed(item);

    item = variable_item_list_add(
        list, "I2C Address", I2C_ADDRESS_COUNT, on_i2c_address_changed, app);
    variable_item_set_current_value_index(item, app->config.i2c_address - I2C_ADDRESS_MIN);
    on_i2c_address_changed(item);

    item = variable_item_list_add(list, "Shunt Resistor", 1, NULL, app);
    FuriString* text = format_resistance_value(app->config.shunt_resistor);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));
    furi_string_free(text);

    item = variable_item_list_add(list, "Alt Resistor", 1, NULL, app);
    text = format_resistance_value(app->config.shunt_resistor_alt);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));
    furi_string_free(text);

    item = variable_item_list_add(
        list, "V Precision", SensorPrecision_count, on_voltage_precision_changed, app);
    variable_item_set_current_value_index(item, app->config.voltage_precision);
    on_voltage_precision_changed(item);

    item = variable_item_list_add(
        list, "I Precision", SensorPrecision_count, on_current_precision_changed, app);
    variable_item_set_current_value_index(item, app->config.current_precision);
    on_current_precision_changed(item);

    item = variable_item_list_add(
        list, "Averaging", SensorAveraging_count, on_sensor_averaging_changed, app);
    variable_item_set_current_value_index(item, app->config.sensor_averaging);
    on_sensor_averaging_changed(item);

    item = variable_item_list_add(list, "LED blinking", 2, on_led_blinking_changed, app);
    variable_item_set_current_value_index(item, app->config.led_blinking ? 1 : 0);
    on_led_blinking_changed(item);

    item = variable_item_list_add(list, "Wiring info", 0, NULL, app);

    variable_item_list_set_enter_callback(list, scene_settings_enter_callback, app);
}

void scene_settings_on_enter(void* context) {
    App* app = (App*)context;

    scene_settings_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableList);
}

bool scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_settings_on_exit(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->var_item_list);

    app_restart_sensor_driver(app);
}
