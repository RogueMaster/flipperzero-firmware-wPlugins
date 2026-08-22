#include "tcl_ac_remote.h"
#include <furi.h>
#include <furi_hal_infrared.h>
#include <infrared_worker.h>

#define TAG "TclAcRemote"

// Animation timer period (ms) - fast animation
#define SEND_ANIMATION_PERIOD 60
#define SEND_ANIMATION_FRAMES 12

// Forward declarations
static void tcl_app_send_ir(TclApp* app);
static void tcl_app_send_toggle_ir(TclApp* app, TclToggle toggle);

// Callbacks
static void tcl_app_on_send(void* context) {
    TclApp* app = context;

    TclToggle toggle;
    int cmd_type = tcl_main_view_get_last_command(app->main_view, &toggle);

    if(cmd_type == 0) {
        // State command (mode/fan/temperature changed)
        if(app->state->mode == TclModeOff) {
            tcl_app_send_toggle_ir(app, TclTogglePowerOff);
        } else {
            tcl_app_send_ir(app);
        }
    } else {
        // One-shot button
        tcl_app_send_toggle_ir(app, toggle);
    }
}

static void tcl_app_on_extra_navigate(void* context) {
    TclApp* app = context;
    app->current_view = TclViewExtra;
    view_dispatcher_switch_to_view(app->view_dispatcher, TclViewExtra);
}

static void tcl_app_on_setup_navigate(void* context) {
    TclApp* app = context;
    app->current_view = TclViewSetup;
    view_dispatcher_switch_to_view(app->view_dispatcher, TclViewSetup);
}

static void tcl_app_tx_stop_callback(void* context) {
    TclApp* app = context;
    infrared_worker_tx_stop(app->ir_worker);
}

static void tcl_app_send_animation_callback(void* context) {
    TclApp* app = context;

    app->anim_frame++;
    tcl_main_view_update_sending(app->main_view);

    if(app->anim_frame >= SEND_ANIMATION_FRAMES) {
        app->anim_frame = 0;
        furi_timer_stop(app->send_timer);
        app->is_sending = false;
        tcl_main_view_stop_sending(app->main_view);
    }
}

// IR transmission helpers
static void tcl_app_transmit_ir(TclApp* app) {
    if(app->ir_timings_count == 0) return;

    // Stop any in-progress TX before starting a new one
    furi_timer_stop(app->ir_tx_stop_timer);
    infrared_worker_tx_stop(app->ir_worker);

    tcl_main_view_start_sending(app->main_view);
    app->is_sending = true;
    app->anim_frame = 0;
    furi_timer_start(app->send_timer, SEND_ANIMATION_PERIOD);

    // Transmit using infrared worker
    infrared_worker_set_raw_signal(
        app->ir_worker,
        app->ir_timings,
        app->ir_timings_count,
        TCL_IR_CARRIER_FREQ,
        TCL_IR_DUTY_CYCLE);

    infrared_worker_tx_start(app->ir_worker);

    uint32_t total_us = 0;
    for(size_t i = 0; i < app->ir_timings_count; i++) {
        total_us += app->ir_timings[i];
    }
    uint32_t delay_ms = (total_us + 999) / 1000 + 20;
    furi_timer_start(app->ir_tx_stop_timer, delay_ms);

    // Notification
    notification_message(app->notifications, &sequence_blink_cyan_100);

    FURI_LOG_I(TAG, "IR transmitted, %zu timings", app->ir_timings_count);
}

static void tcl_app_send_ir(TclApp* app) {
    if(app->state->mode == TclModeOff) return;

    TclRequest req = tcl_state_request(app->state);
    if(tcl_ir_encode_state(&req, app->ir_timings, &app->ir_timings_count)) {
        tcl_ir_format_state(&req, app->last_sent, sizeof(app->last_sent));
        tcl_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void tcl_app_send_toggle_ir(TclApp* app, TclToggle toggle) {
    TclRequest req = tcl_state_request(app->state);
    if(tcl_ir_encode_toggle(&req, toggle, app->ir_timings, &app->ir_timings_count)) {
        tcl_ir_format_toggle(&req, toggle, app->last_sent, sizeof(app->last_sent));
        tcl_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void tcl_app_on_extra_send(TclExtra extra, void* context) {
    TclApp* app = context;

    TclRequest req = tcl_state_request(app->state);
    if(tcl_ir_encode_extra(&req, extra, app->ir_timings, &app->ir_timings_count)) {
        tcl_ir_format_extra(&req, extra, app->last_sent, sizeof(app->last_sent));
        tcl_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

// View dispatcher callbacks
static bool tcl_app_back_callback(void* context) {
    TclApp* app = context;

    if(app->current_view == TclViewExtra || app->current_view == TclViewSetup) {
        app->current_view = TclViewMain;
        view_dispatcher_switch_to_view(app->view_dispatcher, TclViewMain);
        return true;
    }

    return false; // Exit app
}

// App lifecycle
static TclApp* tcl_app_alloc(void) {
    TclApp* app = malloc(sizeof(TclApp));
    furi_assert(app);

    // State
    app->state = tcl_state_alloc();
    tcl_state_load(app->state);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, tcl_app_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize current view
    app->current_view = TclViewMain;

    // Main view
    app->main_view = tcl_main_view_alloc();
    tcl_main_view_set_state(app->main_view, app->state);
    tcl_main_view_set_send_callback(app->main_view, tcl_app_on_send, app);
    tcl_main_view_set_extra_callback(app->main_view, tcl_app_on_extra_navigate, app);
    tcl_main_view_set_setup_callback(app->main_view, tcl_app_on_setup_navigate, app);
    view_dispatcher_add_view(
        app->view_dispatcher, TclViewMain, tcl_main_view_get_view(app->main_view));

    // Extra view
    snprintf(app->last_sent, sizeof(app->last_sent), "--");
    app->extra_view = tcl_extra_view_alloc();
    tcl_extra_view_set_state(app->extra_view, app->state);
    tcl_extra_view_set_last_sent(app->extra_view, app->last_sent);
    tcl_extra_view_set_send_callback(app->extra_view, tcl_app_on_extra_send, app);
    view_dispatcher_add_view(
        app->view_dispatcher, TclViewExtra, tcl_extra_view_get_view(app->extra_view));

    // Setup view
    app->setup_view = tcl_setup_view_alloc();
    tcl_setup_view_set_state(app->setup_view, app->state);
    view_dispatcher_add_view(
        app->view_dispatcher, TclViewSetup, tcl_setup_view_get_view(app->setup_view));

    // IR worker
    app->ir_worker = infrared_worker_alloc();
    furi_assert(app->ir_worker);
    app->ir_timings_count = 0;
    infrared_worker_tx_set_get_signal_callback(
        app->ir_worker, infrared_worker_tx_get_signal_steady_callback, app);

    // Animation timer
    app->send_timer =
        furi_timer_alloc(tcl_app_send_animation_callback, FuriTimerTypePeriodic, app);
    app->ir_tx_stop_timer = furi_timer_alloc(tcl_app_tx_stop_callback, FuriTimerTypeOnce, app);
    app->is_sending = false;

    return app;
}

static void tcl_app_free(TclApp* app) {
    // Save state
    tcl_state_save(app->state);

    // Stop timers
    furi_timer_stop(app->send_timer);
    furi_timer_free(app->send_timer);
    furi_timer_stop(app->ir_tx_stop_timer);
    furi_timer_free(app->ir_tx_stop_timer);

    // IR worker
    infrared_worker_free(app->ir_worker);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, TclViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, TclViewExtra);
    view_dispatcher_remove_view(app->view_dispatcher, TclViewSetup);

    tcl_main_view_free(app->main_view);
    tcl_extra_view_free(app->extra_view);
    tcl_setup_view_free(app->setup_view);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    // State
    tcl_state_free(app->state);

    free(app);
}

// Entry point
int32_t tcl_ac_remote_app(void* p) {
    UNUSED(p);

    TclApp* app = tcl_app_alloc();

    FURI_LOG_I(TAG, "Proto AC Remote started");

    view_dispatcher_switch_to_view(app->view_dispatcher, TclViewMain);
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "Proto AC Remote stopped");

    tcl_app_free(app);

    return 0;
}
