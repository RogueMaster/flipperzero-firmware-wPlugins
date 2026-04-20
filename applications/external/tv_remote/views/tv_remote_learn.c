/**
 * @file views/tv_remote_learn.c
 * @brief IR learning view implementation.
 *
 * The learn view walks the user through recording an IR signal for each of
 * the TV_BUTTON_COUNT predefined TV-remote buttons, one at a time.
 *
 * State machine (learn_state field in the view model):
 *   LearnStateWaiting   – IR worker is listening; instructions are displayed.
 *   LearnStateReceived  – A signal arrived; prompt user to save/skip/retry.
 *   LearnStateDone      – All buttons processed; inform user and go back.
 */

#include "tv_remote_learn.h"
#include "../flipper_tv_remote.h"

#include <gui/elements.h>

/* ---- Learn sequence (skip Mute and Play/Pause) ---- */

#define TV_LEARN_COUNT 12

static const uint8_t learn_sequence[TV_LEARN_COUNT] = {
    TvButtonPower,
    TvButtonVolUp,
    TvButtonVolDn,
    TvButtonChUp,
    TvButtonChDn,
    TvButtonUp,
    TvButtonDown,
    TvButtonLeft,
    TvButtonRight,
    TvButtonOk,
    TvButtonBack,
    TvButtonHome,
};

/** Return the TvButton index for the current learn step. */
static inline uint8_t learn_current_btn(const TvRemoteApp* app) {
    return learn_sequence[app->learn_index];
}

/* ---- View model ---- */

typedef enum {
    LearnStateWaiting = 0,
    LearnStateReceived,
    LearnStateDone,
} LearnState;

typedef struct {
    LearnState state;
    uint8_t learn_step;    /**< Step counter (0-based) for progress display. */
    uint8_t button_index;  /**< Actual TvButton index for current step. */
    bool signal_is_parsed; /**< True when received signal was decoded. */
    /* Decoded signal info shown to the user. */
    char info_line1[32];
    char info_line2[32];
    char remote_name[TV_REMOTE_NAME_MAX + 1]; /**< Name of remote being learned. */
} TvRemoteLearnModel;

/* ---- IR worker callback (called from worker thread) ---- */

static void tv_remote_learn_ir_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    TvRemoteApp* app = context;

    /* Only capture one signal per learn step; ignore subsequent callbacks. */
    if(app->learn_signal_received) return;

    /* Store the received signal into the current button's slot. */
    TvRemoteIrSignal* dst = &app->buttons[learn_current_btn(app)].signal;

    if(infrared_worker_signal_is_decoded(received_signal)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received_signal);
        dst->is_raw = false;
        dst->message = *msg;
    } else {
        const uint32_t* timings;
        size_t timings_cnt;
        infrared_worker_get_raw_signal(received_signal, &timings, &timings_cnt);
        if(dst->timings) free(dst->timings);
        dst->is_raw = true;
        dst->timings = malloc(sizeof(uint32_t) * timings_cnt);
        memcpy(dst->timings, timings, sizeof(uint32_t) * timings_cnt);
        dst->timings_size = timings_cnt;
        dst->frequency = INFRARED_COMMON_CARRIER_FREQUENCY;
        dst->duty_cycle = INFRARED_COMMON_DUTY_CYCLE;
    }

    /*
     * Mark received before dispatching to ensure the UI-thread handler sees a
     * stable signal.  The flag is reset in tv_remote_learn_start_rx so the next
     * button's capture starts fresh.
     */
    app->learn_signal_received = true;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, TvRemoteCustomEventSignalReceived);
}

/* ---- Helper: start / stop IR worker ---- */

static void tv_remote_learn_stop_rx(TvRemoteApp* app);

static void tv_remote_learn_start_rx(TvRemoteApp* app) {
    /* Stop any previously active worker so we can start fresh. */
    if(app->worker_active) {
        tv_remote_learn_stop_rx(app);
    }
    app->learn_signal_received = false; /* reset for next capture */
    app->worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(
        app->worker, tv_remote_learn_ir_rx_callback, app);
    infrared_worker_rx_start(app->worker);
    app->worker_active = true;
    app->tx_active = false;
}

static void tv_remote_learn_stop_rx(TvRemoteApp* app) {
    if(!app->worker_active) return;
    infrared_worker_rx_stop(app->worker);
    infrared_worker_free(app->worker);
    app->worker = NULL;
    app->worker_active = false;
}

/* ---- Custom event handler (registered with ViewDispatcher) ---- */

static bool tv_remote_learn_custom_event_callback(void* context, uint32_t event) {
    TvRemoteApp* app = context;
    if((TvRemoteCustomEvent)event != TvRemoteCustomEventSignalReceived) return false;

    /* Update model: signal received, show info to user. */
    with_view_model(
        app->learn_view,
        TvRemoteLearnModel * model,
        {
            TvRemoteIrSignal* sig = &app->buttons[learn_current_btn(app)].signal;
            model->state = LearnStateReceived;
            model->learn_step = app->learn_index;
            model->button_index = learn_current_btn(app);

            if(sig->is_raw) {
                model->signal_is_parsed = false;
                snprintf(model->info_line1, sizeof(model->info_line1), "Type: RAW");
                snprintf(
                    model->info_line2,
                    sizeof(model->info_line2),
                    "Timings: %u",
                    (unsigned)sig->timings_size);
            } else {
                model->signal_is_parsed = true;
                snprintf(
                    model->info_line1,
                    sizeof(model->info_line1),
                    "Proto: %s",
                    infrared_get_protocol_name(sig->message.protocol));
                snprintf(
                    model->info_line2,
                    sizeof(model->info_line2),
                    "Cmd: 0x%04lX",
                    (unsigned long)sig->message.command);
            }
        },
        true);

    return true;
}

/* ---- Draw callback ---- */

static void tv_remote_learn_draw_callback(Canvas* canvas, void* model_void) {
    TvRemoteLearnModel* model = model_void;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(model->state == LearnStateDone) {
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Learning complete!");
        canvas_set_font(canvas, FontSecondary);
        char saved_line[TV_REMOTE_NAME_MAX + 8];
        snprintf(saved_line, sizeof(saved_line), "Saved: %s", model->remote_name);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, saved_line);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "[Back] to return");
        return;
    }

    /* Header */
    const char* btn_name =
        (model->button_index < TV_BUTTON_COUNT) ?
            tv_remote_button_names[model->button_index] :
            "?";

    if(model->state == LearnStateWaiting) {
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Learn Remote");
        canvas_set_font(canvas, FontSecondary);

        char header[48];
        snprintf(
            header,
            sizeof(header),
            "Button %u/%u: %s",
            (unsigned)(model->learn_step + 1),
            (unsigned)TV_LEARN_COUNT,
            btn_name);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, header);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignTop, "Point remote at Flipper");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "and press the button.");

        /* Hint bar */
        elements_button_right(canvas, "Skip");
        elements_button_left(canvas, "Stop");
    } else {
        /* LearnStateReceived */
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Signal Received!");
        canvas_set_font(canvas, FontSecondary);

        char header[32];
        snprintf(header, sizeof(header), "Button: %s", btn_name);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, header);
        canvas_draw_str_aligned(canvas, 0, 28, AlignLeft, AlignTop, model->info_line1);
        canvas_draw_str_aligned(canvas, 0, 38, AlignLeft, AlignTop, model->info_line2);

        /* Hint bar */
        elements_button_center(canvas, "Save");
        elements_button_right(canvas, "Skip");
        elements_button_left(canvas, "Retry");
    }
}

/* ---- Helper: advance to next learn step and update UI ---- */

static void learn_advance(TvRemoteApp* app) {
    app->learn_index++;
    if(app->learn_index >= TV_LEARN_COUNT) {
        tv_remote_app_save_named(app, app->current_remote_name);
        tv_remote_learn_stop_rx(app);
        with_view_model(
            app->learn_view,
            TvRemoteLearnModel * model,
            { model->state = LearnStateDone; },
            true);
    } else {
        tv_remote_learn_start_rx(app);
        with_view_model(
            app->learn_view,
            TvRemoteLearnModel * model,
            {
                model->state = LearnStateWaiting;
                model->learn_step = app->learn_index;
                model->button_index = learn_current_btn(app);
            },
            true);
    }
}

/* ---- Input callback ---- */

static bool tv_remote_learn_input_callback(InputEvent* event, void* context) {
    TvRemoteApp* app = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    LearnState current_state = LearnStateWaiting;

    with_view_model(
        app->learn_view,
        TvRemoteLearnModel * model,
        { current_state = model->state; },
        false);

    if(current_state == LearnStateDone) {
        /* Any key returns to menu when done */
        if(event->key == InputKeyBack || event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewMainMenu);
            consumed = true;
        }
        return consumed;
    }

    if(current_state == LearnStateWaiting) {
        if(event->key == InputKeyRight) {
            /* Skip this button */
            consumed = true;
            learn_advance(app);
        } else if(event->key == InputKeyLeft || event->key == InputKeyBack) {
            /* Stop learning and go back */
            consumed = true;
            tv_remote_learn_stop_rx(app);
            app->learn_index = 0;
            view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewMainMenu);
        }
    } else if(current_state == LearnStateReceived) {
        if(event->key == InputKeyOk) {
            /* Save signal for this button */
            consumed = true;
            app->buttons[learn_current_btn(app)].learned = true;
            notification_message(app->notifications, &sequence_success);
            learn_advance(app);
        } else if(event->key == InputKeyRight) {
            /* Skip: discard signal, advance */
            consumed = true;
            learn_advance(app);
        } else if(event->key == InputKeyLeft) {
            /* Retry: restart RX for the same button */
            consumed = true;
            tv_remote_learn_start_rx(app);
            with_view_model(
                app->learn_view,
                TvRemoteLearnModel * model,
                { model->state = LearnStateWaiting; },
                true);
        }
    }

    return consumed;
}

/* ---- Enter / exit callbacks ---- */

static void tv_remote_learn_enter_callback(void* context) {
    TvRemoteApp* app = context;
    /* Reset learning from the first step */
    app->learn_index = 0;
    app->learn_signal_received = false;

    with_view_model(
        app->learn_view,
        TvRemoteLearnModel * model,
        {
            model->state = LearnStateWaiting;
            model->learn_step = 0;
            model->button_index = learn_sequence[0];
            strncpy(model->remote_name, app->current_remote_name, TV_REMOTE_NAME_MAX);
            model->remote_name[TV_REMOTE_NAME_MAX] = '\0';
        },
        true);

    /* Register custom event handler */
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tv_remote_learn_custom_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    tv_remote_learn_start_rx(app);
}

static void tv_remote_learn_exit_callback(void* context) {
    TvRemoteApp* app = context;
    tv_remote_learn_stop_rx(app);
}

/* ---- Public API ---- */

View* tv_remote_learn_view_alloc(TvRemoteApp* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(TvRemoteLearnModel));
    view_set_draw_callback(view, tv_remote_learn_draw_callback);
    view_set_input_callback(view, tv_remote_learn_input_callback);
    view_set_enter_callback(view, tv_remote_learn_enter_callback);
    view_set_exit_callback(view, tv_remote_learn_exit_callback);

    with_view_model(
        view,
        TvRemoteLearnModel * model,
        {
            model->state = LearnStateWaiting;
            model->learn_step = 0;
            model->button_index = learn_sequence[0];
            model->signal_is_parsed = false;
            model->info_line1[0] = '\0';
            model->info_line2[0] = '\0';
            model->remote_name[0] = '\0';
        },
        true);

    return view;
}

void tv_remote_learn_view_free(View* view) {
    view_free(view);
}
