#include "../application.h"
#include "scenes.h"

static void tonuino_scene_set_special1_callback(void* context, int32_t number) {
    TonuinoApp* app = context;
    if(number >= 0 && number <= 255) {
        app->card_data.special1 = number;
    }
    scene_manager_previous_scene(app->scene_manager);
}

void tonuino_scene_set_special1_on_enter(void* context) {
    TonuinoApp* app = context;

    number_input_set_header_text(app->number_input, "Special 1");
    number_input_set_result_callback(
        app->number_input,
        tonuino_scene_set_special1_callback,
        app,
        app->card_data.special1,
        0,
        255);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewNumberInput);
}

bool tonuino_scene_set_special1_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_set_special1_on_exit(void* context) {
    UNUSED(context);
    // Number input will be reconfigured by next scene
}
