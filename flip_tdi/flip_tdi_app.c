#include <furi.h>
#include "flip_tdi_app_i.h"

static bool flip_tdi_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipTDIApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flip_tdi_app_back_event_callback(void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void flip_tdi_app_tick_event_callback(void* context) {
    furi_assert(context);
    FlipTDIApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

FlipTDIApp* flip_tdi_app_alloc() {
    FlipTDIApp* app = malloc(sizeof(FlipTDIApp));

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&flip_tdi_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flip_tdi_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flip_tdi_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flip_tdi_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipTDIViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipTDIViewWidget, widget_get_view(app->widget));

    // Field Presence
    app->flip_tdi_view_main_instance = flip_tdi_view_main_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipTDIViewMain,
        flip_tdi_view_main_get_view(app->flip_tdi_view_main_instance));

    // FTDI emulation Start
    flip_tdi_start(app);

    scene_manager_next_scene(app->scene_manager, FlipTDISceneMain);

    return app;
}

void flip_tdi_app_free(FlipTDIApp* app) {
    furi_assert(app);

    // FTDI emulation Stop
    flip_tdi_stop(app);

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, FlipTDIViewSubmenu);
    submenu_free(app->submenu);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, FlipTDIViewWidget);
    widget_free(app->widget);

    // FlipTDIViewMain
    view_dispatcher_remove_view(app->view_dispatcher, FlipTDIViewMain);
    flip_tdi_view_main_free(app->flip_tdi_view_main_instance);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t flip_tdi_app(void* p) {
    UNUSED(p);
    FlipTDIApp* flip_tdi_app = flip_tdi_app_alloc();

    view_dispatcher_run(flip_tdi_app->view_dispatcher);

    flip_tdi_app_free(flip_tdi_app);

    return 0;
}