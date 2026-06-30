#include <furi.h>
#include "portal_of_flipper_i.h"

static bool pof_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PoFApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool pof_app_back_event_callback(void* context) {
    furi_assert(context);
    PoFApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void pof_app_tick_event_callback(void* context) {
    furi_assert(context);
    PoFApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void pof_show_loading_popup(void* context, bool show) {
    PoFApp* app = context;
    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(app->view_dispatcher, PoFViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

PoFApp* pof_app_alloc() {
    PoFApp* app = malloc(sizeof(PoFApp));

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&pof_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pof_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, pof_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, pof_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PoFViewSubmenu, submenu_get_view(app->submenu));

    // Popup
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PoFViewPopup, popup_get_view(app->popup));

    // Loading
    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PoFViewLoading, loading_get_view(app->loading));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PoFViewWidget, widget_get_view(app->widget));

    app->virtual_portal = virtual_portal_alloc(app->notifications);

    scene_manager_next_scene(app->scene_manager, PoFSceneTypeSelect);
    return app;
}

void pof_app_free(PoFApp* app) {
    furi_assert(app);

    // PoF emulation Stop
    pof_stop(app);

    // Clean up peripherals like LEDs 
    virtual_portal_cleanup(app->virtual_portal);

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, PoFViewSubmenu);
    submenu_free(app->submenu);

    // Popup
    view_dispatcher_remove_view(app->view_dispatcher, PoFViewPopup);
    popup_free(app->popup);

    // Loading
    view_dispatcher_remove_view(app->view_dispatcher, PoFViewLoading);
    loading_free(app->loading);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, PoFViewWidget);
    widget_free(app->widget);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    virtual_portal_free(app->virtual_portal);
    app->virtual_portal = NULL;

    free(app);
}

int32_t portal_of_flipper_app(void* p) {
    UNUSED(p);

    PoFApp* pof_app = pof_app_alloc();

    view_dispatcher_run(pof_app->view_dispatcher);

    pof_app_free(pof_app);

    return 0;
}
