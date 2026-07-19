#include "../nyx_i.h"

/* The setup screen keeps its own ADC handle — the sweep worker is stopped
 * whenever this scene is on top, so there is no contention. */
static IrProbeMeter* meter;
static uint8_t tick_counter;
static bool detected;

void nyx_scene_probe_on_enter(void* context) {
    NyxApp* app = context;

    tick_counter = 0;
    detected = ir_sense_probe_detect(app->settings.probe_pin_index);
    meter = ir_probe_meter_alloc(app->settings.probe_pin_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewProbe);
}

bool nyx_scene_probe_on_event(void* context, SceneManagerEvent event) {
    NyxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        tick_counter++;

        /* Re-run the pull-up test about once a second, so the screen follows
         * along while you are actually wiring the thing up. It briefly takes
         * the pin out of analog mode, which is why it is not on every tick. */
        if(tick_counter % 10u == 0u) {
            detected = ir_sense_probe_detect(app->settings.probe_pin_index);
        }

        const IrProbePin* pins = ir_sense_probe_pins();
        uint8_t idx = app->settings.probe_pin_index;
        if(idx >= ir_sense_probe_pin_count()) idx = 0;

        probe_view_update(
            app->probe_view,
            pins[idx].name,
            pins[idx].header_number,
            detected,
            ir_probe_meter_read_mv(meter));
        probe_view_tick(app->probe_view);
        consumed = true;
    }
    return consumed;
}

void nyx_scene_probe_on_exit(void* context) {
    UNUSED(context);
    if(meter) {
        ir_probe_meter_free(meter);
        meter = NULL;
    }
}
