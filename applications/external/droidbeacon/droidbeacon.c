#include "droidbeacon_app.h"
#include "scenes/droidbeacon_scene.h"

static bool droidbeacon_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool droidbeacon_scene_back_callback(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static DroidbeaconApp* droidbeacon_alloc(void) {
    DroidbeaconApp* app = malloc(sizeof(DroidbeaconApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&droidbeacon_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, droidbeacon_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, droidbeacon_scene_back_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->splash_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DroidbeaconViewSplash, widget_get_view(app->splash_widget));

    app->menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DroidbeaconViewMenu, submenu_get_view(app->menu));

    app->beaming_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DroidbeaconViewBeaming, widget_get_view(app->beaming_widget));

    app->result_popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DroidbeaconViewResult, popup_get_view(app->result_popup));

    app->selected_beacon = 0;
    app->is_beaming      = false;

    return app;
}

static void droidbeacon_free(DroidbeaconApp* app) {
    furi_assert(app);

    // Make sure beacon is stopped before freeing
    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    }

    view_dispatcher_remove_view(app->view_dispatcher, DroidbeaconViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, DroidbeaconViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, DroidbeaconViewBeaming);
    view_dispatcher_remove_view(app->view_dispatcher, DroidbeaconViewResult);

    widget_free(app->splash_widget);
    submenu_free(app->menu);
    widget_free(app->beaming_widget);
    popup_free(app->result_popup);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t droidbeacon_app(void* p) {
    UNUSED(p);
    DroidbeaconApp* app = droidbeacon_alloc();
    scene_manager_next_scene(app->scene_manager, DroidbeaconSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    droidbeacon_free(app);
    return 0;
}
