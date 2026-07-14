#include "../timed_remote.h"
#include "timed_remote_scene.h"

static void on_popup_done(void* context) {
    TimedRemoteApp* app;

    app = context;
    view_dispatcher_send_custom_event(app->vd, EVENT_DONE);
}

void scene_done_enter(void* context) {
    TimedRemoteApp* app;

    app = context;
    popup_reset(app->popup);
    popup_set_header(app->popup, "Signal Sent!", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(app->popup, app->sig, 64, 35, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 2000);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, on_popup_done);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->vd, VIEW_POP);
}

bool scene_done_event(void* context, SceneManagerEvent event) {
    TimedRemoteApp* app;

    app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != EVENT_DONE) return false;

    scene_manager_search_and_switch_to_previous_scene(app->sm, SCENE_BROWSE);
    return true;
}

void scene_done_exit(void* context) {
    TimedRemoteApp* app;

    app = context;
    popup_reset(app->popup);
}
