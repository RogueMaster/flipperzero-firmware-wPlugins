#include <stdlib.h>
#include <string.h>

#include "scenes/timed_remote_scene.h"
#include "timed_remote.h"

extern const SceneManagerHandlers scene_handlers;

static bool nav_cb(void* context) {
    TimedRemoteApp* app;

    app = context;
    return scene_manager_handle_back_event(app->sm);
}

static bool evt_cb(void* context, uint32_t evt) {
    TimedRemoteApp* app;

    app = context;
    return scene_manager_handle_custom_event(app->sm, evt);
}

TimedRemoteApp* timed_remote_app_alloc(void) {
    TimedRemoteApp* app;

    app = malloc(sizeof(TimedRemoteApp));
    if(app == NULL) return NULL;
    memset(app, 0, sizeof(TimedRemoteApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, nav_cb);
    view_dispatcher_set_custom_event_callback(app->vd, evt_cb);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    app->sm = scene_manager_alloc(&scene_handlers, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->vd, VIEW_MENU, submenu_get_view(app->submenu));

    app->vlist = variable_item_list_alloc();
    view_dispatcher_add_view(app->vd, VIEW_LIST, variable_item_list_get_view(app->vlist));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->vd, VIEW_RUN, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->vd, VIEW_POP, popup_get_view(app->popup));

    return app;
}

void timed_remote_app_free(TimedRemoteApp* app) {
    if(app == NULL) return;

    if(app->timer != NULL) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }

    if(app->ir != NULL) infrared_signal_free(app->ir);

    view_dispatcher_remove_view(app->vd, VIEW_MENU);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->vd, VIEW_LIST);
    variable_item_list_free(app->vlist);

    view_dispatcher_remove_view(app->vd, VIEW_RUN);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->vd, VIEW_POP);
    popup_free(app->popup);

    scene_manager_free(app->sm);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t timed_remote_app(void* p) {
    TimedRemoteApp* app;

    UNUSED(p);

    app = timed_remote_app_alloc();
    if(app == NULL) return -1;

    scene_manager_next_scene(app->sm, SCENE_BROWSE);
    view_dispatcher_run(app->vd);
    timed_remote_app_free(app);

    return 0;
}
