#include "countdown.h"
#include "../pause_timer.h"
#include <gui/elements.h>
#include <furi.h>
#include <furi_hal.h>
#include <infrared_transmit.h>
#include <gui/scene_manager.h>

#define TAG "PT"

struct CountdownUtils {
    View* view;
    FuriTimer* timer;
    PauseTimerApp* app;
};

typedef enum {
    CountdownState_Running,
    CountdownState_Complete
} CountdownState;

typedef struct {
    uint16_t total_seconds;
    uint16_t remaining_seconds;
    CountdownState state;
    bool has_ir_signal;
    bool ir_sent;
} CountdownModel;

static void transmit_ir_signal(PauseTimerApp* app) {
    if(!app || !app->learned_ir_signal.has_signal) {
        FURI_LOG_W(TAG, "No IR signal to transmit");
        return;
    }

    FURI_LOG_I(TAG, "Transmitting IR signal...");

    // Figure out if it's raw or decoded and send it accordingly
    if(app->learned_ir_signal.is_decoded) {
        infrared_send(&app->learned_ir_signal.decoded_message, 1);
    } else if(app->learned_ir_signal.raw_timings) {
        infrared_send_raw(
            app->learned_ir_signal.raw_timings, app->learned_ir_signal.raw_timings_size, true);
    }
    furi_delay_ms(100);
}

static void countdown_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    CountdownModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(model->state == CountdownState_Running) {
        elements_multiline_text_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Countdown");

        // Calculate from the time value minutes and seconds
        // Technically we let you do more than 60 seconds in minute and seconds but it
        // just makes more sense that way. Like a microwave (mmmmmmmvvvvvmmmmmmm)
        uint8_t minutes = model->remaining_seconds / 60;
        uint8_t seconds = model->remaining_seconds % 60;

        canvas_set_font(canvas, FontBigNumbers);
        char timer_str[10];
        snprintf(timer_str, sizeof(timer_str), "%02d:%02d", minutes, seconds);
        elements_multiline_text_aligned(canvas, 64, 35, AlignCenter, AlignCenter, timer_str);

        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas, 64, 55, AlignCenter, AlignBottom, "Press Back to cancel");
    } else if(model->state == CountdownState_Complete) {
        elements_multiline_text_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Complete!");

        if(model->has_ir_signal && model->ir_sent) {
            elements_multiline_text_aligned(
                canvas, 64, 30, AlignCenter, AlignTop, "IR Signal Sent");
        }

        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas, 64, 50, AlignCenter, AlignBottom, "Press any key to return");
    }
}

static void countdown_timer_callback(void* context) {
    furi_assert(context);
    CountdownUtils* countdown = context;

    bool completed = false;
    bool send_ir = false;

    with_view_model(
        countdown->view,
        CountdownModel * model,
        {
            if(model->remaining_seconds > 0) {
                model->remaining_seconds--;
                if(model->remaining_seconds == 0) {
                    model->state = CountdownState_Complete;
                    completed = true;
                    send_ir = model->has_ir_signal;
                    model->ir_sent = send_ir;
                }
            }
        },
        true);

    if(completed) {
        if(send_ir) transmit_ir_signal(countdown->app);

        furi_hal_vibro_on(true);
        furi_delay_ms(100);
        furi_hal_vibro_on(false);

        furi_timer_stop(countdown->timer);
    }
}

static bool countdown_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    CountdownUtils* countdown = context;
    bool consumed = false;

    with_view_model(
        countdown->view,
        CountdownModel * model,
        {
            if(event->type == InputTypeShort) {
                if(model->state == CountdownState_Running) {
                    // Cancel countdown on Back press
                    if(event->key == InputKeyBack) {
                        model->state = CountdownState_Complete;
                        furi_timer_stop(countdown->timer);
                        consumed = true;
                    }
                } else if(model->state == CountdownState_Complete) {
                    // Any key to return
                    if(countdown->app && countdown->app->scene_manager) {
                        scene_manager_previous_scene(countdown->app->scene_manager);
                    }
                    consumed = true;
                }
            }
        },
        true);

    return consumed;
}

CountdownUtils* countdown_utils_alloc() {
    CountdownUtils* countdown = malloc(sizeof(CountdownUtils));
    countdown->view = view_alloc();
    view_set_context(countdown->view, countdown);
    view_allocate_model(countdown->view, ViewModelTypeLocking, sizeof(CountdownModel));
    view_set_draw_callback(countdown->view, countdown_draw_callback);
    view_set_input_callback(countdown->view, countdown_input_callback);

    countdown->timer =
        furi_timer_alloc(countdown_timer_callback, FuriTimerTypePeriodic, countdown);
    countdown->app = NULL;

    with_view_model(
        countdown->view,
        CountdownModel * model,
        {
            model->total_seconds = 0;
            model->remaining_seconds = 0;
            model->state = CountdownState_Running;
            model->has_ir_signal = false;
            model->ir_sent = false;
        },
        true);

    return countdown;
}

void countdown_utils_free(CountdownUtils* countdown) {
    furi_assert(countdown);

    if(countdown->timer) {
        furi_timer_stop(countdown->timer);
        furi_timer_free(countdown->timer);
        countdown->timer = NULL;
    }
    view_free(countdown->view);
    free(countdown);
}

View* countdown_get_view(CountdownUtils* countdown) {
    furi_assert(countdown);
    return countdown->view;
}

void countdown_set_args(CountdownUtils* countdown, CountdownArgs* args) {
    furi_assert(countdown);
    furi_assert(args);

    countdown->app = args->app;

    with_view_model(
        countdown->view,
        CountdownModel * model,
        {
            // Convert from MMSS format to total seconds
            uint8_t minutes = args->timer_val / 100;
            uint8_t seconds = args->timer_val % 100;
            model->total_seconds = minutes * 60 + seconds;
            model->remaining_seconds = model->total_seconds;
            model->has_ir_signal = args->has_ir_signal;
            model->state = CountdownState_Running;
            model->ir_sent = false;

            if(model->total_seconds > 0) {
                furi_timer_start(countdown->timer, 1000);
            } else {
                // Edge case where if timer is 0, complete immediately
                model->state = CountdownState_Complete;
                if(model->has_ir_signal) {
                    transmit_ir_signal(countdown->app);
                    model->ir_sent = true;
                }
            }
        },
        true);
}

void countdown_start(CountdownUtils* countdown) {
    furi_assert(countdown);

    with_view_model(
        countdown->view,
        CountdownModel * model,
        {
            model->state = CountdownState_Running;
            model->remaining_seconds = model->total_seconds;
            if(model->total_seconds > 0) {
                furi_timer_start(countdown->timer, 1000);
            }
        },
        true);
}

void stop_countdown(CountdownUtils* countdown) {
    furi_assert(countdown);
    furi_timer_stop(countdown->timer);
}
