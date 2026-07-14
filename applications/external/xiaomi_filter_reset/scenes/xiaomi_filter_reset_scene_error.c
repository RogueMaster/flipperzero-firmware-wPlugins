// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_error.c
 * @brief Error scene: explains why a reset attempt did not complete.
 */
#include "../xiaomi_filter_reset_i.h"

static const char* xiaomi_filter_reset_error_text(XiaomiFilterWorkerResult result) {
    switch(result) {
    case XiaomiFilterWorkerResultAuthFailed:
        return "Authentication failed.\n"
               "This tag does not look\n"
               "like a Xiaomi filter, or\n"
               "uses an unknown password.";
    case XiaomiFilterWorkerResultReadFailed:
        return "Could not read the tag.\n"
               "Hold it flat against the\n"
               "back of the Flipper and\n"
               "try again.";
    case XiaomiFilterWorkerResultWriteFailed:
        return "The tag rejected the\n"
               "write. Keep it steady\n"
               "and try again.";
    case XiaomiFilterWorkerResultVerifyFailed:
        return "The reset could not be\n"
               "verified. Please move\n"
               "the tag away and retry.";
    case XiaomiFilterWorkerResultNotDetected:
    default:
        return "No filter tag detected.\n"
               "Try again.";
    }
}

static void xiaomi_filter_reset_scene_error_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    XiaomiFilterResetApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, XiaomiFilterResetCustomEventButtonRetry);
    }
}

void xiaomi_filter_reset_scene_error_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Widget* widget = app->widget;
    const XiaomiFilterWorkerResult result = xiaomi_filter_worker_get_result(app->worker);

    const char* title = app->pending_op == XiaomiFilterWorkerOpCheck ? "Check failed" :
                                                                       "Reset failed";

    widget_reset(widget);
    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_text_scroll_element(widget, 0, 16, 128, 34, xiaomi_filter_reset_error_text(result));
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Retry", xiaomi_filter_reset_scene_error_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewWidget);

    notification_message(app->notifications, &sequence_error);
}

bool xiaomi_filter_reset_scene_error_on_event(void* context, SceneManagerEvent event) {
    XiaomiFilterResetApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == XiaomiFilterResetCustomEventButtonRetry) {
        consumed = true;
        // Return to the scan scene, which restarts polling on enter.
        scene_manager_previous_scene(app->scene_manager);
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, XiaomiFilterResetSceneStart);
    }

    return consumed;
}

void xiaomi_filter_reset_scene_error_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    widget_reset(app->widget);
}
