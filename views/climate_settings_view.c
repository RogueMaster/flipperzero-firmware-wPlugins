/*
 * climate_settings_view.c — climate sensor selection + unit settings.
 * Sensor list: all types without CO2 data bit.
 */
#include "../air_stats_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

static VariableItem* sensor_item;
static VariableItem* temp_unit_item;
static VariableItem* pressure_unit_item;
static VariableItem* humidity_unit_item;
static VariableItem* heat_index_item;

static const char temp_units[UT_TEMP_COUNT][3]          = {"*C", "*F"};
static const char humidity_units[UT_HUMIDITY_COUNT][12] = {"Relative", "Dewpoint"};
static const char pressure_units[UT_PRESSURE_COUNT][6]  = {"mmHg", "inHg", "kPa", "hPa"};
static const char heat_index_bool[2][4]                 = {"OFF", "ON"};

#define MAX_CLIMATE_TYPES 32
static const SensorType* climate_types[MAX_CLIMATE_TYPES];
static uint8_t climate_types_count = 0;

#define VIEW_ID ViewClimateSettings

/* ---- Climate hotswap ---- */

static void _climate_hotswap(uint8_t new_idx, uint8_t old_idx) {
    if(new_idx >= climate_types_count) return;
    const SensorType* new_type = climate_types[new_idx];

    /* Mark all non-CO2 sensors inactive */
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(!(app->sensors[i]->type->datatype & UT_CO2)) {
            app->sensors[i]->status = UT_SENSORSTATUS_INACTIVE;
        }
    }

    /* Build args for new sensor */
    char args[22] = {0};
    if(new_type->interface == &SINGLE_WIRE || new_type->interface == &SPI) {
        const GPIO* gpio = unitemp_gpio_getAviablePort(new_type->interface, 0, NULL);
        if(gpio) snprintf(args, sizeof(args), "%d", unitemp_gpio_toInt(gpio));
    }
    /* I2C and custom interfaces: empty args (allocator uses defaults) */

    char sname[11];
    snprintf(sname, sizeof(sname), "%s", new_type->typename);
    Sensor* s = unitemp_sensor_alloc(sname, new_type, args);
    if(!s) {
        /* Alloc failed — restore old sensors and setting */
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(!(app->sensors[i]->type->datatype & UT_CO2)) {
                app->sensors[i]->status = UT_SENSORSTATUS_ERROR;
            }
        }
        app->settings.climate_type_idx = old_idx;
        unitemp_saveSettings();
        return;
    }

    app->settings.climate_type_idx = new_idx;
    unitemp_sensors_add(s);
    unitemp_sensors_save();
    unitemp_sensors_reload();
}

/* ---- Exit callback ---- */

static uint32_t _exit_callback(void* context) {
    UNUSED(context);

    uint8_t new_sensor_idx = (uint8_t)variable_item_get_current_value_index(sensor_item);
    uint8_t old_sensor_idx = app->settings.climate_type_idx;
    bool sensor_changed    = (new_sensor_idx != old_sensor_idx);

    app->settings.temp_unit     = (tempMeasureUnit)variable_item_get_current_value_index(temp_unit_item);
    app->settings.pressure_unit = (pressureMeasureUnit)variable_item_get_current_value_index(pressure_unit_item);
    app->settings.humidity_unit = (humidityUnit)variable_item_get_current_value_index(humidity_unit_item);
    app->settings.heat_index    = (bool)variable_item_get_current_value_index(heat_index_item);
    unitemp_saveSettings();
    unitemp_loadSettings();

    if(sensor_changed) {
        _climate_hotswap(new_sensor_idx, old_sensor_idx);
        return ViewMainMenu; /* sensor ptrs invalidated by reload */
    }

    return ViewSensorActions;
}

/* ---- Change callback ---- */

static void _setting_change(VariableItem* item) {
    if(item == temp_unit_item) {
        variable_item_set_current_value_text(
            item, temp_units[variable_item_get_current_value_index(item)]);
    } else if(item == pressure_unit_item) {
        variable_item_set_current_value_text(
            item, pressure_units[variable_item_get_current_value_index(item)]);
    } else if(item == humidity_unit_item) {
        variable_item_set_current_value_text(
            item, humidity_units[variable_item_get_current_value_index(item)]);
    } else if(item == heat_index_item) {
        variable_item_set_current_value_text(
            item, heat_index_bool[variable_item_get_current_value_index(item)]);
    } else if(item == sensor_item) {
        uint8_t idx = variable_item_get_current_value_index(item);
        if(idx < climate_types_count) {
            const char* name = climate_types[idx]->altname
                                   ? climate_types[idx]->altname
                                   : climate_types[idx]->typename;
            variable_item_set_current_value_text(item, name);
        }
    }
}

/* ---- Lifecycle ---- */

void view_climate_settings_alloc(void) {
    /* Build filtered list: all sensor types without CO2 data */
    climate_types_count = 0;
    uint8_t total            = unitemp_sensors_getTypesCount();
    const SensorType** all   = unitemp_sensors_getTypes();
    for(uint8_t i = 0; i < total && climate_types_count < MAX_CLIMATE_TYPES; i++) {
        if(!(all[i]->datatype & UT_CO2)) {
            climate_types[climate_types_count++] = all[i];
        }
    }

    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    sensor_item = variable_item_list_add(
        variable_item_list, "Change sensor", climate_types_count, _setting_change, app);
    temp_unit_item = variable_item_list_add(
        variable_item_list, "Temperature", UT_TEMP_COUNT, _setting_change, app);
    pressure_unit_item = variable_item_list_add(
        variable_item_list, "Pressure", UT_PRESSURE_COUNT, _setting_change, app);
    humidity_unit_item = variable_item_list_add(
        variable_item_list, "Humidity", UT_HUMIDITY_COUNT, _setting_change, app);
    heat_index_item = variable_item_list_add(
        variable_item_list, "Heat index", 2, _setting_change, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_climate_settings_switch(void) {
    /* Sync idx with the actual active climate sensor */
    uint8_t idx = app->settings.climate_type_idx;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i] && !(app->sensors[i]->type->datatype & UT_CO2)) {
            for(uint8_t j = 0; j < climate_types_count; j++) {
                if(climate_types[j] == app->sensors[i]->type) {
                    idx = j;
                    break;
                }
            }
            break;
        }
    }
    if(idx >= climate_types_count) idx = 0;
    app->settings.climate_type_idx = idx;

    variable_item_set_current_value_index(sensor_item, idx);
    const char* sname = climate_types[idx]->altname
                            ? climate_types[idx]->altname
                            : climate_types[idx]->typename;
    variable_item_set_current_value_text(sensor_item, sname);

    variable_item_set_current_value_index(temp_unit_item, (uint8_t)app->settings.temp_unit);
    variable_item_set_current_value_text(temp_unit_item, temp_units[app->settings.temp_unit]);

    variable_item_set_current_value_index(pressure_unit_item, (uint8_t)app->settings.pressure_unit);
    variable_item_set_current_value_text(
        pressure_unit_item, pressure_units[app->settings.pressure_unit]);

    variable_item_set_current_value_index(humidity_unit_item, (uint8_t)app->settings.humidity_unit);
    variable_item_set_current_value_text(
        humidity_unit_item, humidity_units[app->settings.humidity_unit]);

    variable_item_set_current_value_index(heat_index_item, app->settings.heat_index ? 1 : 0);
    variable_item_set_current_value_text(
        heat_index_item, heat_index_bool[app->settings.heat_index ? 1 : 0]);

    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_climate_settings_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
