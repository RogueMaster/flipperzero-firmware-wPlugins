#include "../hermes_i.h"

void hermes_scene_wiring_on_enter(void* context) {
    HermesApp* app = context;

    /* Show the pins the current profile actually uses, not a generic diagram. */
    if(app->port == HermesPortLpuart) {
        wiring_view_set_pins(app->wiring_view, 16, 15);
    } else {
        wiring_view_set_pins(app->wiring_view, 14, 13);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewWiring);
}

bool hermes_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hermes_scene_wiring_on_exit(void* context) {
    UNUSED(context);
}
