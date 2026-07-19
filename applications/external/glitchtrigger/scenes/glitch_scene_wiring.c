#include "../glitch_trigger_i.h"

void glitch_scene_wiring_on_enter(void* context) {
    GlitchApp* app = context;
    const GlitchPinInfo* out = &glitch_pins[app->params.out_pin % glitch_pins_len];
    const GlitchPinInfo* in = &glitch_pins[app->params.in_pin % glitch_pins_len];
    bool show_in = (app->params.trigger == GlitchTriggerExternal);

    wiring_view_set_pins(app->wiring_view, out->header, out->name, in->header, in->name, show_in);
    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewWiring);
}

bool glitch_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        wiring_view_tick(app->wiring_view);
        return true;
    }
    return false;
}

void glitch_scene_wiring_on_exit(void* context) {
    UNUSED(context);
}
