#include "delonghi_ac_remote.h"
#include <furi.h>
#include <furi_hal_infrared.h>
#include <infrared_worker.h>

#define TAG "DelonghiAcRemote"

// Animation timer period (ms) - fast animation
#define SEND_ANIMATION_PERIOD 60
#define SEND_ANIMATION_FRAMES 12

// Forward declarations
static void delonghi_app_send_ir(DelonghiApp* app);
static void delonghi_app_send_toggle_ir(DelonghiApp* app, DelonghiToggle toggle);

// Callbacks
static void delonghi_app_on_send(void* context) {
    DelonghiApp* app = context;

    DelonghiToggle toggle;
    int cmd_type = delonghi_main_view_get_last_command(app->main_view, &toggle);

    if(cmd_type == 0) {
        // State command (mode/fan/temperature changed)
        if(app->state->mode == DelonghiModeOff) {
            delonghi_app_send_toggle_ir(app, DelonghiTogglePowerOff);
        } else {
            delonghi_app_send_ir(app);
        }
    } else {
        // One-shot button
        delonghi_app_send_toggle_ir(app, toggle);
    }
}

static void delonghi_app_on_extra_navigate(void* context) {
    DelonghiApp* app = context;
    app->current_view = DelonghiViewExtra;
    view_dispatcher_switch_to_view(app->view_dispatcher, DelonghiViewExtra);
}

static void delonghi_app_on_setup_navigate(void* context) {
    DelonghiApp* app = context;
    app->current_view = DelonghiViewSetup;
    view_dispatcher_switch_to_view(app->view_dispatcher, DelonghiViewSetup);
}

static void delonghi_app_tx_stop_callback(void* context) {
    DelonghiApp* app = context;
    infrared_worker_tx_stop(app->ir_worker);
}

static void delonghi_app_send_animation_callback(void* context) {
    DelonghiApp* app = context;

    app->anim_frame++;
    delonghi_main_view_update_sending(app->main_view);

    if(app->anim_frame >= SEND_ANIMATION_FRAMES) {
        app->anim_frame = 0;
        furi_timer_stop(app->send_timer);
        app->is_sending = false;
        delonghi_main_view_stop_sending(app->main_view);
    }
}

// IR transmission helpers
static void delonghi_app_transmit_ir(DelonghiApp* app) {
    if(app->ir_timings_count == 0) return;

    // Stop any in-progress TX before starting a new one
    furi_timer_stop(app->ir_tx_stop_timer);
    infrared_worker_tx_stop(app->ir_worker);

    delonghi_main_view_start_sending(app->main_view);
    app->is_sending = true;
    app->anim_frame = 0;
    furi_timer_start(app->send_timer, SEND_ANIMATION_PERIOD);

    // Transmit using infrared worker
    infrared_worker_set_raw_signal(
        app->ir_worker,
        app->ir_timings,
        app->ir_timings_count,
        DELONGHI_IR_CARRIER_FREQ,
        DELONGHI_IR_DUTY_CYCLE);

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

static void delonghi_app_send_ir(DelonghiApp* app) {
    if(app->state->mode == DelonghiModeOff) return;

    DelonghiRequest req = delonghi_state_request(app->state);
    if(delonghi_ir_encode_state(&req, app->ir_timings, &app->ir_timings_count)) {
        delonghi_ir_format_state(&req, app->last_sent, sizeof(app->last_sent));
        delonghi_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void delonghi_app_send_toggle_ir(DelonghiApp* app, DelonghiToggle toggle) {
    DelonghiRequest req = delonghi_state_request(app->state);
    if(delonghi_ir_encode_toggle(&req, toggle, app->ir_timings, &app->ir_timings_count)) {
        delonghi_ir_format_toggle(&req, toggle, app->last_sent, sizeof(app->last_sent));
        delonghi_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

static void delonghi_app_on_extra_send(DelonghiExtra extra, void* context) {
    DelonghiApp* app = context;

    DelonghiRequest req = delonghi_state_request(app->state);
    if(delonghi_ir_encode_extra(&req, extra, app->ir_timings, &app->ir_timings_count)) {
        delonghi_ir_format_extra(&req, extra, app->last_sent, sizeof(app->last_sent));
        delonghi_app_transmit_ir(app);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

// View dispatcher callbacks
static bool delonghi_app_back_callback(void* context) {
    DelonghiApp* app = context;

    if(app->current_view == DelonghiViewExtra || app->current_view == DelonghiViewSetup) {
        app->current_view = DelonghiViewMain;
        view_dispatcher_switch_to_view(app->view_dispatcher, DelonghiViewMain);
        return true;
    }

    return false; // Exit app
}

// App lifecycle
static DelonghiApp* delonghi_app_alloc(void) {
    DelonghiApp* app = malloc(sizeof(DelonghiApp));
    furi_assert(app);

    // State
    app->state = delonghi_state_alloc();
    delonghi_state_load(app->state);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, delonghi_app_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize current view
    app->current_view = DelonghiViewMain;

    // Main view
    app->main_view = delonghi_main_view_alloc();
    delonghi_main_view_set_state(app->main_view, app->state);
    delonghi_main_view_set_send_callback(app->main_view, delonghi_app_on_send, app);
    delonghi_main_view_set_extra_callback(app->main_view, delonghi_app_on_extra_navigate, app);
    delonghi_main_view_set_setup_callback(app->main_view, delonghi_app_on_setup_navigate, app);
    view_dispatcher_add_view(
        app->view_dispatcher, DelonghiViewMain, delonghi_main_view_get_view(app->main_view));

    // Extra view
    snprintf(app->last_sent, sizeof(app->last_sent), "--");
    app->extra_view = delonghi_extra_view_alloc();
    delonghi_extra_view_set_state(app->extra_view, app->state);
    delonghi_extra_view_set_last_sent(app->extra_view, app->last_sent);
    delonghi_extra_view_set_send_callback(app->extra_view, delonghi_app_on_extra_send, app);
    view_dispatcher_add_view(
        app->view_dispatcher, DelonghiViewExtra, delonghi_extra_view_get_view(app->extra_view));

    // Setup view
    app->setup_view = delonghi_setup_view_alloc();
    delonghi_setup_view_set_state(app->setup_view, app->state);
    view_dispatcher_add_view(
        app->view_dispatcher, DelonghiViewSetup, delonghi_setup_view_get_view(app->setup_view));

    // IR worker
    app->ir_worker = infrared_worker_alloc();
    furi_assert(app->ir_worker);
    app->ir_timings_count = 0;
    infrared_worker_tx_set_get_signal_callback(
        app->ir_worker, infrared_worker_tx_get_signal_steady_callback, app);

    // Animation timer
    app->send_timer =
        furi_timer_alloc(delonghi_app_send_animation_callback, FuriTimerTypePeriodic, app);
    app->ir_tx_stop_timer =
        furi_timer_alloc(delonghi_app_tx_stop_callback, FuriTimerTypeOnce, app);
    app->is_sending = false;

    return app;
}

static void delonghi_app_free(DelonghiApp* app) {
    // Save state
    delonghi_state_save(app->state);

    // Stop timers
    furi_timer_stop(app->send_timer);
    furi_timer_free(app->send_timer);
    furi_timer_stop(app->ir_tx_stop_timer);
    furi_timer_free(app->ir_tx_stop_timer);

    // IR worker
    infrared_worker_free(app->ir_worker);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, DelonghiViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, DelonghiViewExtra);
    view_dispatcher_remove_view(app->view_dispatcher, DelonghiViewSetup);

    delonghi_main_view_free(app->main_view);
    delonghi_extra_view_free(app->extra_view);
    delonghi_setup_view_free(app->setup_view);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    // State
    delonghi_state_free(app->state);

    free(app);
}

// Entry point
int32_t delonghi_ac_remote_app(void* p) {
    UNUSED(p);

    DelonghiApp* app = delonghi_app_alloc();

    FURI_LOG_I(TAG, "Proto AC Remote started");

    view_dispatcher_switch_to_view(app->view_dispatcher, DelonghiViewMain);
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "Proto AC Remote stopped");

    delonghi_app_free(app);

    return 0;
}
