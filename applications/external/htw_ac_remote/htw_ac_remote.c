#include "htw_ac_remote.h"
#include <furi.h>
#include <furi_hal_infrared.h>
#include <infrared_worker.h>

#define TAG "HtwAcRemote"

// Animation timer period (ms) - fast animation
#define SEND_ANIMATION_PERIOD 60
#define SEND_ANIMATION_FRAMES 12

// Forward declarations
static void htw_app_send_ir(HtwApp* app);
static void htw_app_send_toggle_ir(HtwApp* app, HtwToggle toggle);
static void htw_app_send_timer_on_ir(HtwApp* app);
static void htw_app_send_timer_off_ir(HtwApp* app);

// Callbacks
static void htw_app_on_send(void* context) {
    HtwApp* app = context;

    HtwToggle toggle;
    int cmd_type = htw_main_view_get_last_command(app->main_view, &toggle);

    if(cmd_type == 0) {
        // State command (mode changed)
        if(app->state->mode == HtwModeOff) {
            htw_app_send_toggle_ir(app, HtwTogglePowerOff);
        } else {
            htw_app_send_ir(app);
        }
    } else {
        // Toggle command
        htw_app_send_toggle_ir(app, toggle);
    }
}

static void htw_app_on_timer_navigate(void* context) {
    HtwApp* app = context;
    app->current_view = HtwViewTimer;
    view_dispatcher_switch_to_view(app->view_dispatcher, HtwViewTimer);
}

static void htw_app_on_setup_navigate(void* context) {
    HtwApp* app = context;
    app->current_view = HtwViewSetup;
    view_dispatcher_switch_to_view(app->view_dispatcher, HtwViewSetup);
}

static void htw_app_on_timer_send(HtwTimerCommand cmd, void* context) {
    HtwApp* app = context;

    if(cmd == HtwTimerCommandOn) {
        htw_app_send_timer_on_ir(app);
    } else {
        htw_app_send_timer_off_ir(app);
    }
}

static void htw_app_tx_stop_callback(void* context) {
    HtwApp* app = context;
    infrared_worker_tx_stop(app->ir_worker);
}

static void htw_app_send_animation_callback(void* context) {
    HtwApp* app = context;

    app->anim_frame++;
    htw_main_view_update_sending(app->main_view);

    if(app->anim_frame >= SEND_ANIMATION_FRAMES) {
        app->anim_frame = 0;
        furi_timer_stop(app->send_timer);
        app->is_sending = false;
        htw_main_view_stop_sending(app->main_view);
    }
}

// IR transmission helpers
static void htw_app_transmit_ir(HtwApp* app) {
    if(app->ir_timings_count == 0) return;

    // Stop any in-progress TX before starting a new one
    furi_timer_stop(app->ir_tx_stop_timer);
    infrared_worker_tx_stop(app->ir_worker);

    htw_main_view_start_sending(app->main_view);
    app->is_sending = true;
    app->anim_frame = 0;
    furi_timer_start(app->send_timer, SEND_ANIMATION_PERIOD);

    // Transmit using infrared worker
    infrared_worker_set_raw_signal(
        app->ir_worker,
        app->ir_timings,
        app->ir_timings_count,
        HTW_IR_CARRIER_FREQ,
        HTW_IR_DUTY_CYCLE);

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

static void htw_app_send_ir(HtwApp* app) {
    if(app->state->mode == HtwModeOff) return;

    if(htw_ir_encode_state(
           app->state->mode,
           app->state->fan,
           app->state->temp,
           app->ir_timings,
           &app->ir_timings_count)) {
        htw_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void htw_app_send_toggle_ir(HtwApp* app, HtwToggle toggle) {
    if(htw_ir_encode_toggle(toggle, app->ir_timings, &app->ir_timings_count)) {
        htw_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void htw_app_send_timer_on_ir(HtwApp* app) {
    if(app->state->timer_on_step == 0) return;
    if(app->state->mode == HtwModeOff) return;

    if(htw_ir_encode_timer_on(
           app->state->mode,
           app->state->fan,
           app->state->temp,
           app->state->timer_on_step,
           app->ir_timings,
           &app->ir_timings_count)) {
        htw_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void htw_app_send_timer_off_ir(HtwApp* app) {
    if(app->state->timer_off_step == 0) return;

    // Use last active mode if AC is currently off
    HtwMode mode = app->state->mode;
    if(mode == HtwModeOff) {
        mode = app->state->last_active_mode;
    }

    if(htw_ir_encode_timer_off(
           mode,
           app->state->temp,
           app->state->timer_off_step,
           app->ir_timings,
           &app->ir_timings_count)) {
        htw_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

// View dispatcher callbacks
static bool htw_app_back_callback(void* context) {
    HtwApp* app = context;

    if(app->current_view == HtwViewTimer || app->current_view == HtwViewSetup) {
        app->current_view = HtwViewMain;
        view_dispatcher_switch_to_view(app->view_dispatcher, HtwViewMain);
        return true;
    }

    return false; // Exit app
}

// App lifecycle
static HtwApp* htw_app_alloc(void) {
    HtwApp* app = malloc(sizeof(HtwApp));
    furi_assert(app);

    // State
    app->state = htw_state_alloc();
    htw_state_load(app->state);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, htw_app_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize current view
    app->current_view = HtwViewMain;

    // Main view
    app->main_view = htw_main_view_alloc();
    htw_main_view_set_state(app->main_view, app->state);
    htw_main_view_set_send_callback(app->main_view, htw_app_on_send, app);
    htw_main_view_set_timer_callback(app->main_view, htw_app_on_timer_navigate, app);
    htw_main_view_set_setup_callback(app->main_view, htw_app_on_setup_navigate, app);
    view_dispatcher_add_view(
        app->view_dispatcher, HtwViewMain, htw_main_view_get_view(app->main_view));

    // Timer view
    app->timer_view = htw_timer_view_alloc();
    htw_timer_view_set_state(app->timer_view, app->state);
    htw_timer_view_set_send_callback(app->timer_view, htw_app_on_timer_send, app);
    view_dispatcher_add_view(
        app->view_dispatcher, HtwViewTimer, htw_timer_view_get_view(app->timer_view));

    // Setup view
    app->setup_view = htw_setup_view_alloc();
    htw_setup_view_set_state(app->setup_view, app->state);
    view_dispatcher_add_view(
        app->view_dispatcher, HtwViewSetup, htw_setup_view_get_view(app->setup_view));

    // IR worker
    app->ir_worker = infrared_worker_alloc();
    furi_assert(app->ir_worker);
    app->ir_timings_count = 0;
    infrared_worker_tx_set_get_signal_callback(
        app->ir_worker, infrared_worker_tx_get_signal_steady_callback, app);

    // Animation timer
    app->send_timer =
        furi_timer_alloc(htw_app_send_animation_callback, FuriTimerTypePeriodic, app);
    app->ir_tx_stop_timer = furi_timer_alloc(htw_app_tx_stop_callback, FuriTimerTypeOnce, app);
    app->is_sending = false;

    return app;
}

static void htw_app_free(HtwApp* app) {
    // Save state
    htw_state_save(app->state);

    // Stop timer
    furi_timer_stop(app->send_timer);
    furi_timer_free(app->send_timer);
    furi_timer_stop(app->ir_tx_stop_timer);
    furi_timer_free(app->ir_tx_stop_timer);

    // IR worker
    infrared_worker_free(app->ir_worker);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, HtwViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, HtwViewTimer);
    view_dispatcher_remove_view(app->view_dispatcher, HtwViewSetup);

    htw_main_view_free(app->main_view);
    htw_timer_view_free(app->timer_view);
    htw_setup_view_free(app->setup_view);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    // State
    htw_state_free(app->state);

    free(app);
}

// Entry point
int32_t htw_ac_remote_app(void* p) {
    UNUSED(p);

    HtwApp* app = htw_app_alloc();

    FURI_LOG_I(TAG, "HTW AC Remote started");

    view_dispatcher_switch_to_view(app->view_dispatcher, HtwViewMain);
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "HTW AC Remote stopped");

    htw_app_free(app);

    return 0;
}
