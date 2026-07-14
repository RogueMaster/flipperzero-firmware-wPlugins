#include "../pause_timer.h"
#include "../views.h"

void pause_timer_scene_ir_learn_on_enter(void* context) {
    PauseTimerApp* app = context;

    FURI_LOG_D("PauseTimer", "Starting IR learn scene");

    ir_learn_set_callbacks(
        app->ir_learn, ir_learn_signal_learned_callback, ir_learn_back_callback, app);

    // Start receiving immediately
    ir_learn_start_receiving(app->ir_learn);

    view_dispatcher_switch_to_view(app->view_dispatcher, PTViewIrLearn);
}

bool pause_timer_scene_ir_learn_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pause_timer_scene_ir_learn_on_exit(void* context) {
    UNUSED(context);
}
