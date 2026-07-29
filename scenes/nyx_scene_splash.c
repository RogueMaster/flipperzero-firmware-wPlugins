#include "../nyx_i.h"

/* Scene state doubles as an "already played" flag: 0 = first boot, 1 = the
 * intro has finished. That lets us tell the fresh boot apart from the user
 * backing out of the menu into this root scene, where the right move is to quit
 * the app rather than replay the animation. */

static void nyx_splash_done_cb(void* context) {
    NyxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NyxCustomEventSplashDone);
}

void nyx_scene_splash_on_enter(void* context) {
    NyxApp* app = context;

    if(scene_manager_get_scene_state(app->scene_manager, NyxSceneSplash) == 1) {
        /* Came back here via Back from the menu — this is the app's exit. */
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    splash_view_set_done_callback(app->splash_view, nyx_splash_done_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewSplash);
}

bool nyx_scene_splash_on_event(void* context, SceneManagerEvent event) {
    NyxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NyxCustomEventSplashDone) {
            scene_manager_set_scene_state(app->scene_manager, NyxSceneSplash, 1);
            scene_manager_next_scene(app->scene_manager, NyxSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(splash_view_tick(app->splash_view)) {
            view_dispatcher_send_custom_event(app->view_dispatcher, NyxCustomEventSplashDone);
        }
        consumed = true;
    }
    return consumed;
}

void nyx_scene_splash_on_exit(void* context) {
    UNUSED(context);
}
