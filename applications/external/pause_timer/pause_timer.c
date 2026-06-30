#include "pause_timer.h"
#include "views.h"

#define TAG "PauseTimerApp"

// Callback from countdown view to return to numpad
void countdown_back_callback(void* context) {
    PauseTimerApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

// Callback from IR learn view when signal is learned
void ir_learn_signal_learned_callback(void* context) {
    PauseTimerApp* app = context;

    // Get the learned signal directly from the view
    IrLearnResult result = ir_learn_get_result(app->ir_learn);

    // Free previous signal if exists
    if(app->learned_ir_signal.raw_timings) {
        free(app->learned_ir_signal.raw_timings);
        app->learned_ir_signal.raw_timings = NULL;
        app->learned_ir_signal.raw_timings_size = 0;
    }

    // Copy the learned signal
    if(result.has_signal) {
        app->learned_ir_signal.has_signal = true;
        app->learned_ir_signal.is_decoded = result.is_decoded;

        if(result.is_decoded) {
            app->learned_ir_signal.decoded_message = result.decoded_message;
            FURI_LOG_D(
                TAG,
                "Decoded signal saved: protocol=%d, address=0x%lX, command=0x%lX",
                result.decoded_message.protocol,
                result.decoded_message.address,
                result.decoded_message.command);
        } else {
            if(result.raw_timings && result.raw_timings_size > 0) {
                app->learned_ir_signal.raw_timings =
                    malloc(result.raw_timings_size * sizeof(uint32_t));
                if(app->learned_ir_signal.raw_timings) {
                    memcpy(
                        app->learned_ir_signal.raw_timings,
                        result.raw_timings,
                        result.raw_timings_size * sizeof(uint32_t));
                    app->learned_ir_signal.raw_timings_size = result.raw_timings_size;
                    app->learned_ir_signal.frequency = result.frequency;
                    app->learned_ir_signal.duty_cycle = result.duty_cycle;
                    FURI_LOG_D(TAG, "Raw signal saved: %d timings", (int)result.raw_timings_size);
                }
            }
        }
    }

    scene_manager_previous_scene(app->scene_manager);
}

// Callback from IR learn view when user cancels
void ir_learn_back_callback(void* context) {
    PauseTimerApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

// Callback from numpad when START is pressed
static void numpad_start_callback(void* context, uint16_t timer_val) {
    PauseTimerApp* app = context;

    app->current_timer_val = timer_val;
    scene_manager_next_scene(app->scene_manager, PauseTimerSceneCountdown);
}

// Callback from numpad when LEARN is pressed
static void numpad_learn_callback(void* context) {
    PauseTimerApp* app = context;

    scene_manager_next_scene(app->scene_manager, PauseTimerSceneIrLearn);
}

void free_ir_signal(IrSignalStorage* signal) {
    if(signal->raw_timings) {
        free(signal->raw_timings);
        signal->raw_timings = NULL;
        signal->raw_timings_size = 0;
    }
    signal->has_signal = false;
}

bool pt_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PauseTimerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool pt_back_event_callback(void* context) {
    furi_assert(context);
    PauseTimerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

PauseTimerApp* pause_timer_app_alloc() {
    PauseTimerApp* app = malloc(sizeof(PauseTimerApp));

    app->gui = furi_record_open(RECORD_GUI);

    // dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pt_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pt_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&pause_timer_scene_handlers, app);

    // time input view
    app->time_input = time_input_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, PTViewTimeInput, time_input_get_view(app->time_input));

    // countdown view
    app->countdown = countdown_utils_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PTViewCountdown, countdown_get_view(app->countdown));

    // ir learn view
    app->ir_learn = ir_learn_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PTViewIrLearn, ir_learn_get_view(app->ir_learn));

    app->learned_ir_signal.has_signal = false;
    app->learned_ir_signal.is_decoded = false;
    app->learned_ir_signal.raw_timings = NULL;
    app->learned_ir_signal.raw_timings_size = 0;
    app->learned_ir_signal.frequency = 38000;
    app->learned_ir_signal.duty_cycle = 0.33f;

    return app;
}

void pause_timer_app_free(PauseTimerApp* app) {
    furi_assert(app);

    view_dispatcher_stop(app->view_dispatcher);

    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, PTViewTimeInput);
    view_dispatcher_remove_view(app->view_dispatcher, PTViewCountdown);
    view_dispatcher_remove_view(app->view_dispatcher, PTViewIrLearn);

    // Free view objects
    time_input_free(app->time_input);
    countdown_utils_free(app->countdown);
    ir_learn_free(app->ir_learn);

    // Free managers
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Other resources
    free_ir_signal(&app->learned_ir_signal);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pause_timer_app(void* p) {
    UNUSED(p);
    PauseTimerApp* app = pause_timer_app_alloc();

    time_input_set_start_callback(app->time_input, numpad_start_callback, app);
    time_input_set_learn_callback(app->time_input, numpad_learn_callback, app);

    scene_manager_set_scene_state(app->scene_manager, PauseTimerSceneMain, PTViewTimeInput);
    scene_manager_next_scene(app->scene_manager, PauseTimerSceneMain);

    FURI_LOG_D("PT", "Running view dispatcher");
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_D("PT", "Freeing...");
    pause_timer_app_free(app);

    return 0;
}
