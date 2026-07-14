#include "../tpms_app_i.h"

#define SPLASH_TIMEOUT_TICKS 20 // 2 seconds at 100ms tick rate

static uint8_t splash_tick_count;

void tpms_scene_splash_on_enter(void* context) {
    TPMSApp* app = context;
    Widget* widget = app->widget;

    splash_tick_count = 0;

    widget_reset(widget);

    // Add centered title
    widget_add_string_element(widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "TPMS Reader");

    // Add ASCII art tire
    widget_add_string_multiline_element(
        widget,
        64,
        22,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "  .-\"\"\"\"-.\n"
        " /  .--.  \\\n"
        "|  /    \\  |\n"
        "|  \\    /  |\n"
        " \\  `--'  /\n"
        "  `-....-'");

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

bool tpms_scene_splash_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        splash_tick_count++;
        if(splash_tick_count >= SPLASH_TIMEOUT_TICKS) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom || event.type == SceneManagerEventTypeBack) {
        // Skip splash on any button press
        scene_manager_next_scene(app->scene_manager, TPMSSceneStart);
        consumed = true;
    }

    return consumed;
}

void tpms_scene_splash_on_exit(void* context) {
    TPMSApp* app = context;
    widget_reset(app->widget);
}
