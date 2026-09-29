#include "../ghosttag_i.h"

static void ghosttag_scene_splash_done_cb(void* context) {
    GhostTagApp* app = context;
    /* on_enter must never navigate, and neither may a view callback: post the
     * event and let the next dispatch unwind it. */
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventSplashDone);
}

void ghosttag_scene_splash_on_enter(void* context) {
    GhostTagApp* app = context;
    splash_view_set_done_callback(app->splash_view, ghosttag_scene_splash_done_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewSplash);
}

bool ghosttag_scene_splash_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        splash_view_tick(app->splash_view);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == GhostTagCustomEventSplashDone) {
        /* Pop, do not push. The main menu is already underneath us, so popping
         * lands on it and leaves the stack one deep - meaning Back from the
         * menu exits the app instead of replaying the intro. */
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void ghosttag_scene_splash_on_exit(void* context) {
    GhostTagApp* app = context;
    /* The intro is over for good; a stale callback here could fire a second
     * navigation out of a tick that is already in flight. */
    splash_view_set_done_callback(app->splash_view, NULL, NULL);
}
