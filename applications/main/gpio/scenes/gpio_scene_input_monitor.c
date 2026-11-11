#include "../gpio_app_i.h"

void gpio_scene_input_monitor_on_enter(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_input(app->gpio_items, GpioPullDown);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioAppViewInputMonitor);
}

bool gpio_scene_input_monitor_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    GpioApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        gpio_input_monitor_update(app->gpio_input_monitor);
        consumed = true;
    }

    return consumed;
}

void gpio_scene_input_monitor_on_exit(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_pins(app->gpio_items, GpioModeAnalog);
}
