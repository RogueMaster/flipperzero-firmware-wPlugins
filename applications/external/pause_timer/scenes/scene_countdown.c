#include "../pause_timer.h"
#include "../views.h"

void pause_timer_scene_countdown_on_enter(void* context) {
    PauseTimerApp* app = context;

    CountdownArgs args = {
        .timer_val = app->current_timer_val,
        .has_ir_signal = app->learned_ir_signal.has_signal,
        .app = app,
    };

    countdown_set_args(app->countdown, &args);
    view_dispatcher_switch_to_view(app->view_dispatcher, PTViewCountdown);
}

bool pause_timer_scene_countdown_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void pause_timer_scene_countdown_on_exit(void* context) {
    PauseTimerApp* app = context;

    stop_countdown(app->countdown);
}
