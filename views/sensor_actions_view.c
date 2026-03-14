/*
 * sensor_actions_view.c — actions for a selected sensor.
 * Adapted from _ref_unitemp/views/SensorActions_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;
static Sensor* current_sensor;

#define VIEW_ID ViewSensorActions

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewMain;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    switch(index) {
    case 0: /* Info — go to main to see readings */
        view_main_switch();
        return;
    case 1: /* Edit */
        view_sensor_edit_switch(current_sensor);
        break;
    case 2: /* Delete */
        view_widget_delete_switch(current_sensor);
        break;
    case 3: /* Add new sensor */
        view_sensors_list_switch();
        break;
    case 4: /* Settings */
        view_settings_switch();
        break;
    case 5: /* About */
        view_widget_about_switch();
        break;
    }
}

void view_sensor_actions_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    variable_item_list_add(variable_item_list, "Info", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "Edit", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "Delete", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "Add new sensor", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "Settings", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "About", 1, NULL, NULL);

    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);
    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_sensor_actions_switch(Sensor* sensor) {
    current_sensor = sensor;
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_sensor_actions_free(void) {
    variable_item_list_free(variable_item_list);
    view_free(view);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
}
