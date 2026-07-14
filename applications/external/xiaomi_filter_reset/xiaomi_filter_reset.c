// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset.c
 * @brief Application entry point and lifecycle.
 *
 * Xiaomi Filter Reset clears the filter-life counter stored on the NTAG213 tag of
 * Xiaomi air purifier filters, restoring the reported filter life to 100%.
 */
#include "xiaomi_filter_reset_i.h"

static bool xiaomi_filter_reset_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    XiaomiFilterResetApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool xiaomi_filter_reset_back_event_callback(void* context) {
    furi_assert(context);
    XiaomiFilterResetApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static XiaomiFilterResetApp* xiaomi_filter_reset_app_alloc(void) {
    XiaomiFilterResetApp* app = malloc(sizeof(XiaomiFilterResetApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&xiaomi_filter_reset_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, xiaomi_filter_reset_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, xiaomi_filter_reset_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, XiaomiFilterResetViewSubmenu, submenu_get_view(app->submenu));
    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, XiaomiFilterResetViewPopup, popup_get_view(app->popup));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, XiaomiFilterResetViewWidget, widget_get_view(app->widget));

    app->nfc = nfc_alloc();
    app->worker = xiaomi_filter_worker_alloc();
    app->pending_op = XiaomiFilterWorkerOpReset;

    return app;
}

static void xiaomi_filter_reset_app_free(XiaomiFilterResetApp* app) {
    furi_assert(app);

    xiaomi_filter_worker_stop(app->worker);
    xiaomi_filter_worker_free(app->worker);
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, XiaomiFilterResetViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, XiaomiFilterResetViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, XiaomiFilterResetViewWidget);
    submenu_free(app->submenu);
    popup_free(app->popup);
    widget_free(app->widget);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t xiaomi_filter_reset_app(void* p) {
    UNUSED(p);

    XiaomiFilterResetApp* app = xiaomi_filter_reset_app_alloc();

    scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    xiaomi_filter_reset_app_free(app);
    return 0;
}
