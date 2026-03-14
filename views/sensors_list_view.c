/*
 * sensors_list_view.c — list of sensor types to add.
 * Adapted from _ref_unitemp/views/SensorsList_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>
#include <stdio.h>

static View* view;
static VariableItemList* variable_item_list;

#define VIEW_ID ViewSensorsList

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewMain;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);

    const SensorType* type = unitemp_sensors_getTypes()[index];
    uint8_t sensor_type_count = 0;

    for(uint8_t i = 0; i < unitemp_sensors_getActiveCount(); i++) {
        if(unitemp_sensor_getActive(i)->type == type) {
            sensor_type_count++;
        }
    }

    char sensor_name[11];
    if(sensor_type_count == 0)
        snprintf(sensor_name, 11, "%s", type->typename);
    else
        snprintf(sensor_name, 11, "%s_%d", type->typename, sensor_type_count);

    char args[22] = {0};

    if(type->interface == &SINGLE_WIRE || type->interface == &ONE_WIRE ||
       type->interface == &I2C || type->interface == &SPI) {
        if(unitemp_gpio_getAviablePort(type->interface, 0, NULL) == NULL) {
            if(type->interface == &SINGLE_WIRE || type->interface == &ONE_WIRE) {
                view_popup(NULL, "Unavailable", "All GPIOs\nare busy", VIEW_ID);
            }
            if(type->interface == &I2C) {
                view_popup(NULL, "Unavailable", "GPIOs 15/16\nare busy", VIEW_ID);
            }
            return;
        }
    }

    if(type->interface == &SINGLE_WIRE || type->interface == &SPI) {
        snprintf(
            args,
            4,
            "%d",
            unitemp_gpio_toInt(unitemp_gpio_getAviablePort(type->interface, 0, NULL)));
    }
    if(type->interface == &ONE_WIRE) {
        snprintf(
            args,
            21,
            "%d %02X%02X%02X%02X%02X%02X%02X%02X",
            unitemp_gpio_toInt(unitemp_gpio_getAviablePort(type->interface, 0, NULL)),
            0, 0, 0, 0, 0, 0, 0, 0);
    }

    Sensor* new_sensor = unitemp_sensor_alloc(sensor_name, type, args);
    if(!new_sensor) return;
    view_sensor_edit_switch(new_sensor);
}

void view_sensors_list_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    for(uint8_t i = 0; i < unitemp_sensors_getTypesCount(); i++) {
        if(unitemp_sensors_getTypes()[i]->altname == NULL) {
            variable_item_list_add(
                variable_item_list, unitemp_sensors_getTypes()[i]->typename, 1, NULL, app);
        } else {
            variable_item_list_add(
                variable_item_list, unitemp_sensors_getTypes()[i]->altname, 1, NULL, app);
        }
    }

    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_sensors_list_switch(void) {
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_sensors_list_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
