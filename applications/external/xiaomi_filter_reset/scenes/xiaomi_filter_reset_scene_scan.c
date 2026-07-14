// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_scan.c
 * @brief Active scene: polls for a filter tag and performs the pending action.
 *
 * Shared by both menu actions. app->pending_op decides whether the worker resets
 * the counter or only reads it; the prompt and the follow-up scene adapt to match.
 */
#include "../xiaomi_filter_reset_i.h"

// Invoked from the NFC worker thread; only posts an event to the UI thread.
static void xiaomi_filter_reset_scene_scan_worker_callback(void* context) {
    XiaomiFilterResetApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, XiaomiFilterResetCustomEventWorkerDone);
}

void xiaomi_filter_reset_scene_scan_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Popup* popup = app->popup;
    const bool checking = app->pending_op == XiaomiFilterWorkerOpCheck;

    popup_reset(popup);
    popup_set_header(
        popup, checking ? "Check Filter" : "Reset Filter", 64, 8, AlignCenter, AlignTop);
    popup_set_text(
        popup,
        "Hold the filter's\nNFC tag against the\nback of your Flipper",
        64,
        26,
        AlignCenter,
        AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewPopup);

    notification_message(app->notifications, &sequence_blink_start_cyan);
    xiaomi_filter_worker_start(
        app->worker,
        app->nfc,
        app->pending_op,
        xiaomi_filter_reset_scene_scan_worker_callback,
        app);
}

bool xiaomi_filter_reset_scene_scan_on_event(void* context, SceneManagerEvent event) {
    XiaomiFilterResetApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == XiaomiFilterResetCustomEventWorkerDone) {
        consumed = true;
        const XiaomiFilterWorkerResult result = xiaomi_filter_worker_get_result(app->worker);
        if(result != XiaomiFilterWorkerResultSuccess) {
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneError);
        } else if(app->pending_op == XiaomiFilterWorkerOpCheck) {
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneChecked);
        } else {
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneSuccess);
        }
    }

    return consumed;
}

void xiaomi_filter_reset_scene_scan_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    xiaomi_filter_worker_stop(app->worker);
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
