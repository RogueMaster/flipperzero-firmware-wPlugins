/*
 * co2_settings_view.c — CO2 sensor type selection (PWM / UART).
 */
#include "../air_stats_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;
static VariableItem* co2_type_item;

static const char co2_type_names[2][5] = {"PWM", "UART"};

#define VIEW_ID ViewCO2Settings

/* ---- CO2 hotswap ---- */

static void _co2_hotswap(Co2SensorType new_type) {
    /* Mark all CO2 sensors inactive */
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i]->type == &MHZ19C || app->sensors[i]->type == &MHZ19C_UART) {
            app->sensors[i]->status = UT_SENSORSTATUS_INACTIVE;
        }
    }
    /* Alloc new CO2 sensor */
    const SensorType* stype = (new_type == CO2_TYPE_UART) ? &MHZ19C_UART : &MHZ19C;
    char sname[11];
    snprintf(sname, sizeof(sname), "MH-Z19C");
    Sensor* s = unitemp_sensor_alloc(sname, stype, "");
    if(s) {
        unitemp_sensors_add(s);
    }
    unitemp_sensors_save();
    unitemp_sensors_reload();
}

/* ---- Exit callback ---- */

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    Co2SensorType selected =
        (Co2SensorType)variable_item_get_current_value_index(co2_type_item);
    if(selected != app->settings.co2_type) {
        app->settings.co2_type = selected;
        unitemp_saveSettings();
        _co2_hotswap(selected);
        return ViewMainMenu; /* sensor ptrs invalidated by reload */
    }
    return ViewSensorActions;
}

/* ---- Change callback ---- */

static void _co2_type_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, co2_type_names[variable_item_get_current_value_index(item)]);
}

/* ---- Lifecycle ---- */

void view_co2_settings_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    co2_type_item = variable_item_list_add(
        variable_item_list, "CO2 Type", 2, _co2_type_change, app);

    VariableItem* cal_item =
        variable_item_list_add(variable_item_list, "Calibration", 1, NULL, NULL);
    variable_item_set_current_value_text(cal_item, "N/A");

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_co2_settings_switch(void) {
    variable_item_set_current_value_index(co2_type_item, (uint8_t)app->settings.co2_type);
    variable_item_set_current_value_text(co2_type_item, co2_type_names[app->settings.co2_type]);
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_co2_settings_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
