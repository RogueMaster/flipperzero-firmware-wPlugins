#include "../fdxb_maker.h"

// Credit to anon3825968 on the DangerousThings forums for temperature research: https://forum.dangerousthings.com/t/6799/3

void fdxb_maker_scene_save_temperature_input_number_callback(void* context, float number) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;

    data->temperature = number;

    view_dispatcher_send_custom_event(app->view_dispatcher, FdxbMakerEventNext);
}

void fdxb_maker_scene_save_temperature_on_enter(void* context) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    FdxbTemperatureInput* temperature_input = app->fdxb_temperature_input;

    fdxb_temperature_input_set_header_text(temperature_input, "Enter the temperature in F");

    fdxb_temperature_input_set_result_callback(
        temperature_input,
        fdxb_maker_scene_save_temperature_input_number_callback,
        app,
        data->temperature);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewFdxbTemperatureInput);
}

bool fdxb_maker_scene_save_temperature_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerEventNext) {
            consumed = true;
            fdxb_maker_save_key_and_switch_scenes(app);
        }
    }

    return consumed;
}

void fdxb_maker_scene_save_temperature_on_exit(void* context) {
    FdxbMaker* app = context;

    fdxb_temperature_input_reset(app->fdxb_temperature_input);
}
