/*
 * sensor_name_edit_view.c — text input for sensor name.
 * Adapted from _ref_unitemp/views/SensorNameEdit_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/text_input.h>

static TextInput* text_input;
static Sensor* editable_sensor;

#define VIEW_ID ViewSensorNameEdit

static void _sensor_name_changed_callback(void* context) {
    UNUSED(context);
    view_sensor_edit_switch(editable_sensor);
}

void view_sensor_name_edit_alloc(void) {
    text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, text_input_get_view(text_input));
    text_input_set_header_text(text_input, "Sensor name");
}

void view_sensor_name_edit_switch(Sensor* sensor) {
    editable_sensor = sensor;
    text_input_set_result_callback(
        text_input, _sensor_name_changed_callback, app, sensor->name, 11, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_sensor_name_edit_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    text_input_free(text_input);
}
