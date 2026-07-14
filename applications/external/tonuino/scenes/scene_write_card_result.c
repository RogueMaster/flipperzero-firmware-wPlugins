#include "../application.h"
#include "scenes.h"

void tonuino_scene_write_card_result_on_enter(void* context) {
    TonuinoApp* app = context;

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Card Written!");

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);
}

bool tonuino_scene_write_card_result_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_write_card_result_on_exit(void* context) {
    TonuinoApp* app = context;
    widget_reset(app->widget);
}
