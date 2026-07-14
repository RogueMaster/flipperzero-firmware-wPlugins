#include "unitemp.h"

static void unitemp_scene_sensor_edit_name_text_input_callback(void* context) {
    UnitempApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventTextEditResult);
}

void unitemp_scene_sensor_edit_name_on_enter(void* context) {
    UnitempApp* app = context;
    TextInput* text_input = app->text_input;
    app->editable_sensor->name[10] = 0;

    text_input_set_header_text(text_input, "Enter the sensor name:");
    text_input_set_result_callback(
        text_input,
        unitemp_scene_sensor_edit_name_text_input_callback,
        app,
        app->editable_sensor->name,
        11,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewTextInput);
}

bool unitemp_scene_sensor_edit_name_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CustomEventTextEditResult) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void unitemp_scene_sensor_edit_name_on_exit(void* context) {
    UnitempApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
}
