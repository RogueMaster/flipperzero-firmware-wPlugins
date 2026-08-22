#include "panasonic_ac_remote.h"
#include <furi.h>
#include <furi_hal_infrared.h>
#include <infrared_worker.h>

#define TAG "PanasonicAcRemote"

// Animation timer period (ms) - fast animation
#define SEND_ANIMATION_PERIOD 60
#define SEND_ANIMATION_FRAMES 12

// Forward declarations
static void panasonic_app_send_ir(PanasonicApp* app);
static void panasonic_app_send_toggle_ir(PanasonicApp* app, PanasonicToggle toggle);

// Callbacks
static void panasonic_app_on_send(void* context) {
    PanasonicApp* app = context;

    PanasonicToggle toggle;
    int cmd_type = panasonic_main_view_get_last_command(app->main_view, &toggle);

    if(cmd_type == 0) {
        // State command (mode/fan/temperature changed)
        if(app->state->mode == PanasonicModeOff) {
            panasonic_app_send_toggle_ir(app, PanasonicTogglePowerOff);
        } else {
            panasonic_app_send_ir(app);
        }
    } else {
        // One-shot button
        panasonic_app_send_toggle_ir(app, toggle);
    }
}

static void panasonic_app_on_extra_navigate(void* context) {
    PanasonicApp* app = context;
    app->current_view = PanasonicViewExtra;
    view_dispatcher_switch_to_view(app->view_dispatcher, PanasonicViewExtra);
}

static void panasonic_app_on_setup_navigate(void* context) {
    PanasonicApp* app = context;
    app->current_view = PanasonicViewSetup;
    view_dispatcher_switch_to_view(app->view_dispatcher, PanasonicViewSetup);
}

static void panasonic_app_tx_stop_callback(void* context) {
    PanasonicApp* app = context;
    infrared_worker_tx_stop(app->ir_worker);
}

static void panasonic_app_send_animation_callback(void* context) {
    PanasonicApp* app = context;

    app->anim_frame++;
    panasonic_main_view_update_sending(app->main_view);

    if(app->anim_frame >= SEND_ANIMATION_FRAMES) {
        app->anim_frame = 0;
        furi_timer_stop(app->send_timer);
        app->is_sending = false;
        panasonic_main_view_stop_sending(app->main_view);
    }
}

// IR transmission helpers
static void panasonic_app_transmit_ir(PanasonicApp* app) {
    if(app->ir_timings_count == 0) return;

    // Stop any in-progress TX before starting a new one
    furi_timer_stop(app->ir_tx_stop_timer);
    infrared_worker_tx_stop(app->ir_worker);

    panasonic_main_view_start_sending(app->main_view);
    app->is_sending = true;
    app->anim_frame = 0;
    furi_timer_start(app->send_timer, SEND_ANIMATION_PERIOD);

    // Transmit using infrared worker
    infrared_worker_set_raw_signal(
        app->ir_worker,
        app->ir_timings,
        app->ir_timings_count,
        PANASONIC_IR_CARRIER_FREQ,
        PANASONIC_IR_DUTY_CYCLE);

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

static void panasonic_app_send_ir(PanasonicApp* app) {
    if(app->state->mode == PanasonicModeOff) return;

    PanasonicRequest req = panasonic_state_request(app->state);
    if(panasonic_ir_encode_state(&req, app->ir_timings, &app->ir_timings_count)) {
        panasonic_ir_format_state(&req, app->last_sent, sizeof(app->last_sent));
        panasonic_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void panasonic_app_send_toggle_ir(PanasonicApp* app, PanasonicToggle toggle) {
    PanasonicRequest req = panasonic_state_request(app->state);
    if(panasonic_ir_encode_toggle(&req, toggle, app->ir_timings, &app->ir_timings_count)) {
        panasonic_ir_format_toggle(&req, toggle, app->last_sent, sizeof(app->last_sent));
        panasonic_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void panasonic_app_on_extra_send(PanasonicExtra extra, void* context) {
    PanasonicApp* app = context;

    PanasonicRequest req = panasonic_state_request(app->state);
    if(panasonic_ir_encode_extra(&req, extra, app->ir_timings, &app->ir_timings_count)) {
        panasonic_ir_format_extra(&req, extra, app->last_sent, sizeof(app->last_sent));
        panasonic_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

// View dispatcher callbacks
static bool panasonic_app_back_callback(void* context) {
    PanasonicApp* app = context;

    if(app->current_view == PanasonicViewExtra || app->current_view == PanasonicViewSetup) {
        app->current_view = PanasonicViewMain;
        view_dispatcher_switch_to_view(app->view_dispatcher, PanasonicViewMain);
        return true;
    }

    return false; // Exit app
}

// App lifecycle
static PanasonicApp* panasonic_app_alloc(void) {
    PanasonicApp* app = malloc(sizeof(PanasonicApp));
    furi_assert(app);

    // State
    app->state = panasonic_state_alloc();
    panasonic_state_load(app->state);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, panasonic_app_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize current view
    app->current_view = PanasonicViewMain;

    // Main view
    app->main_view = panasonic_main_view_alloc();
    panasonic_main_view_set_state(app->main_view, app->state);
    panasonic_main_view_set_send_callback(app->main_view, panasonic_app_on_send, app);
    panasonic_main_view_set_extra_callback(app->main_view, panasonic_app_on_extra_navigate, app);
    panasonic_main_view_set_setup_callback(app->main_view, panasonic_app_on_setup_navigate, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PanasonicViewMain, panasonic_main_view_get_view(app->main_view));

    // Extra view
    snprintf(app->last_sent, sizeof(app->last_sent), "--");
    app->extra_view = panasonic_extra_view_alloc();
    panasonic_extra_view_set_state(app->extra_view, app->state);
    panasonic_extra_view_set_last_sent(app->extra_view, app->last_sent);
    panasonic_extra_view_set_send_callback(app->extra_view, panasonic_app_on_extra_send, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PanasonicViewExtra, panasonic_extra_view_get_view(app->extra_view));

    // Setup view
    app->setup_view = panasonic_setup_view_alloc();
    panasonic_setup_view_set_state(app->setup_view, app->state);
    view_dispatcher_add_view(
        app->view_dispatcher, PanasonicViewSetup, panasonic_setup_view_get_view(app->setup_view));

    // IR worker
    app->ir_worker = infrared_worker_alloc();
    furi_assert(app->ir_worker);
    app->ir_timings_count = 0;
    infrared_worker_tx_set_get_signal_callback(
        app->ir_worker, infrared_worker_tx_get_signal_steady_callback, app);

    // Animation timer
    app->send_timer =
        furi_timer_alloc(panasonic_app_send_animation_callback, FuriTimerTypePeriodic, app);
    app->ir_tx_stop_timer =
        furi_timer_alloc(panasonic_app_tx_stop_callback, FuriTimerTypeOnce, app);
    app->is_sending = false;

    return app;
}

static void panasonic_app_free(PanasonicApp* app) {
    // Save state
    panasonic_state_save(app->state);

    // Stop timers
    furi_timer_stop(app->send_timer);
    furi_timer_free(app->send_timer);
    furi_timer_stop(app->ir_tx_stop_timer);
    furi_timer_free(app->ir_tx_stop_timer);

    // IR worker
    infrared_worker_free(app->ir_worker);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, PanasonicViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, PanasonicViewExtra);
    view_dispatcher_remove_view(app->view_dispatcher, PanasonicViewSetup);

    panasonic_main_view_free(app->main_view);
    panasonic_extra_view_free(app->extra_view);
    panasonic_setup_view_free(app->setup_view);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    // State
    panasonic_state_free(app->state);

    free(app);
}

// Entry point
int32_t panasonic_ac_remote_app(void* p) {
    UNUSED(p);

    PanasonicApp* app = panasonic_app_alloc();

    FURI_LOG_I(TAG, "Proto AC Remote started");

    view_dispatcher_switch_to_view(app->view_dispatcher, PanasonicViewMain);
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "Proto AC Remote stopped");

    panasonic_app_free(app);

    return 0;
}
