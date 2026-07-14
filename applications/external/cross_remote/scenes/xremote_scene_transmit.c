#include "../xremote.h"
#include "../views/xremote_transmit.h"
#include "../models/infrared/xremote_ir_signal.h"
#include "../helpers/subghz/subghz.h"

static const NotificationSequence* xremote_notification_sequences[] = {
    &sequence_success,
    &sequence_set_only_green_255,
    &sequence_reset_green,
    &sequence_solid_yellow,
    &sequence_reset_rgb,
    &sequence_blink_start_cyan,
    &sequence_blink_start_magenta,
    &sequence_blink_stop,
    &sequence_blink_start_yellow,
    &sequence_blink_stop,
    &sequence_blink_start_blue,
    &sequence_blink_stop,
};

void xremote_scene_transmit_callback(XRemoteCustomEvent event, void* context) {
    furi_assert(context);
    XRemote* app = context;
    FURI_LOG_D(TAG, "trigger xremote_transmit_callback");
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void xremote_scene_ir_notification_message(XRemote* app, uint32_t message) {
    if(app->led == 1) {
        notification_message(app->notification, xremote_notification_sequences[message]);
    }
}

bool xremote_scene_ir_signal_is_raw(InfraredSignal* signal) {
    if(signal->is_raw) {
        return true;
    }
    return false;
}

void xremote_scene_transmit_stop_ir_signal(XRemote* app) {
    if(!app->transmitting) {
        return;
    }
    app->transmitting = false;
    infrared_worker_tx_stop(app->ir_worker);
    infrared_worker_tx_set_get_signal_callback(app->ir_worker, NULL, NULL);
    xremote_scene_ir_notification_message(app, InfraredNotificationMessageBlinkStop);
}

void xremote_scene_transmit_send_ir_signal(XRemote* app, CrossRemoteItem* item) {
    InfraredSignal* signal = xremote_cross_remote_item_get_ir_signal(item);
    dolphin_deed(DolphinDeedIrSend);
    xremote_scene_ir_notification_message(app, InfraredNotificationMessageBlinkStartSend);
    if(xremote_scene_ir_signal_is_raw(signal)) {
        InfraredRawSignal* raw = xremote_ir_signal_get_raw_signal(signal);
        infrared_worker_set_raw_signal(
            app->ir_worker, raw->timings, raw->timings_size, raw->frequency, raw->duty_cycle);
    } else {
        InfraredMessage* message = xremote_ir_signal_get_message(signal);
        infrared_worker_set_decoded_signal(app->ir_worker, message);
    }
    infrared_worker_tx_set_get_signal_callback(
        app->ir_worker, infrared_worker_tx_get_signal_steady_callback, app);
    infrared_worker_tx_start(app->ir_worker);
    app->transmitting = true;
    uint32_t time = app->ir_timing;
    if(item->time > 0) {
        time = item->time;
    }
    furi_thread_flags_wait(0, FuriFlagWaitAny, time);
    xremote_scene_transmit_stop_ir_signal(app);
}

void xremote_scene_transmit_send_pause(XRemote* app, CrossRemoteItem* item) {
    // Non-blocking: arm a deadline and let the scene's 100ms tick count it down.
    // This keeps the event loop free so the app stays responsive (Back cancels
    // immediately) and can render a live countdown, even for a 60-minute pause.
    // The old code blocked the tick handler here with furi_thread_flags_wait(),
    // which froze the whole app for the entire pause.
    app->transmitting = true;
    xremote_scene_ir_notification_message(app, PauseNotificationMessageBlinkStartSend);
    uint32_t freq = furi_kernel_get_tick_frequency();
    app->pause_deadline = furi_get_tick() + (uint32_t)item->time * freq;
    app->pause_active = true;
    xremote_transmit_model_set_remaining(app->xremote_transmit, item->time);
}

// Called each tick while a pause is counting down. Updates the on-screen
// countdown and, once the deadline passes, ends the pause and advances the chain.
static void xremote_scene_transmit_handle_pause_tick(XRemote* app) {
    if((int32_t)(furi_get_tick() - app->pause_deadline) >= 0) {
        app->pause_active = false;
        app->transmitting = false;
        xremote_scene_ir_notification_message(app, PauseNotificationMessageBlinkStop);
        xremote_transmit_model_set_remaining(app->xremote_transmit, 0);
        xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingStop);
    } else {
        uint32_t freq = furi_kernel_get_tick_frequency();
        uint32_t remaining_ticks = app->pause_deadline - furi_get_tick();
        int remaining_s = (int)((remaining_ticks + freq - 1) / freq); // ceil to seconds
        xremote_transmit_model_set_remaining(app->xremote_transmit, remaining_s);
    }
}

void xremote_scene_transmit_send_subghz(XRemote* app, CrossRemoteItem* item) {
    app->transmitting = true;
    xremote_scene_ir_notification_message(app, SubGhzNotificationMessageBlinkStartSend);
    if(furi_string_utf8_length(item->filename) < 3) {
        xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingStop);
        app->transmitting = false;
        return;
    }
    subghz_send(app, furi_string_get_cstr(item->filename));
    //furi_thread_flags_wait(0, FuriFlagWaitAny, 2000);
}

void xremote_scene_transmit_send_signal(void* context, CrossRemoteItem* item) {
    furi_assert(context);
    XRemote* app = context;
    CrossRemote* remote = app->cross_remote;

    if(app->transmitting) {
        return;
    }

    xremote_transmit_model_set_name(
        app->xremote_transmit, xremote_cross_remote_item_get_name(item));
    xremote_transmit_model_set_type(app->xremote_transmit, item->type);
    if(item->type == XRemoteRemoteItemTypeInfrared) {
        xremote_scene_transmit_send_ir_signal(app, item);
        xremote_cross_remote_set_transmitting(remote, XRemoteTransmittingStop);
    } else if(item->type == XRemoteRemoteItemTypePause) {
        // Pause arms a deadline and completes later in handle_pause_tick(); do
        // not mark it Stop here or it would end after a single tick.
        xremote_scene_transmit_send_pause(app, item);
    } else if(item->type == XRemoteRemoteItemTypeSubGhz) {
        xremote_scene_transmit_send_subghz(app, item);
    }
}

static void xremote_scene_transmit_end_scene(XRemote* app) {
    xremote_scene_ir_notification_message(app, InfraredNotificationMessageBlinkStop);
    if(app->loadFavorite) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

static void xremote_scene_transmit_run_single_transmit(XRemote* app) {
    CrossRemote* remote = app->cross_remote;
    if(app->pause_active) {
        xremote_scene_transmit_handle_pause_tick(app);
        return;
    }
    if(xremote_cross_remote_get_transmitting(remote) == XRemoteTransmittingIdle) {
        xremote_cross_remote_set_transmitting(remote, XRemoteTransmittingStart);
        CrossRemoteItem* item = xremote_cross_remote_get_item(remote, app->transmit_item);
        xremote_scene_transmit_send_signal(app, item);
    } else if(xremote_cross_remote_get_transmitting(remote) == XRemoteTransmittingStopSubghz) {
        app->transmit_item++;
        app->state_notifications = SubGhzNotificationStateIDLE;
        app->transmitting = false;
        subghz_txrx_stop(app->subghz->txrx);
        xremote_scene_ir_notification_message(app, SubGhzNotificationMessageBlinkStop);
        xremote_cross_remote_set_transmitting(remote, XRemoteTransmittingIdle);
    } else if(xremote_cross_remote_get_transmitting(remote) == XRemoteTransmittingStop) {
        app->transmit_item++;
        xremote_cross_remote_set_transmitting(remote, XRemoteTransmittingIdle);
    }
}

static void xremote_scene_transmit_run_next_transmission(XRemote* app) {
    CrossRemote* remote = app->cross_remote;
    size_t item_count = xremote_cross_remote_get_item_count(remote);
    if(app->transmit_item < item_count) {
        xremote_scene_transmit_run_single_transmit(app);
        return;
    }
    if(app->loop_transmit && !app->stop_transmit) {
        app->transmit_item = 0;
        return;
    }
    xremote_scene_transmit_end_scene(app);
}

void xremote_scene_transmit_on_enter(void* context) {
    furi_assert(context);
    XRemote* app = context;
    app->transmit_item = 0;
    app->pause_active = false;
    app->stop_transmit = false;
    app->transmitting = false;
    xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingIdle);
    xremote_transmit_model_set_type(app->xremote_transmit, XRemoteRemoteItemTypeInfrared);
    xremote_transmit_model_set_name(app->xremote_transmit, "");
    xremote_transmit_model_set_remaining(app->xremote_transmit, 0);
    xremote_transmit_set_callback(app->xremote_transmit, xremote_scene_transmit_callback, app);

    // Backlight is left to its normal auto-dim behaviour: the pause is now
    // non-blocking and cancellable, so a dimmed screen is fine (any keypress
    // wakes it to show the countdown) and long waits don't drain the battery.
    view_dispatcher_switch_to_view(app->view_dispatcher, XRemoteViewIdTransmit);
}

bool xremote_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    XRemote* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        FURI_LOG_D(TAG, "Custom Event");
        switch(event.event) {
        case XRemoteCustomEventViewTransmitterSendStop:
            app->stop_transmit = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        FURI_LOG_D(TAG, "Tick Event");
        xremote_scene_transmit_run_next_transmission(app);
        with_view_model(
            xremote_transmit_get_view(app->xremote_transmit),
            void* model,
            { UNUSED(model); },
            true);
        if(app->state_notifications == SubGhzNotificationStateTx && app->led == 1) {
            //blink for subghz
        }
        if(app->stop_transmit == true) {
            app->stop_transmit = false;
            xremote_scene_transmit_end_scene(app);
        }
    }

    return consumed;
}

void xremote_scene_transmit_on_exit(void* context) {
    XRemote* app = context;
    app->transmitting = false;
    app->pause_active = false;
    app->state_notifications = SubGhzNotificationStateIDLE;
}
