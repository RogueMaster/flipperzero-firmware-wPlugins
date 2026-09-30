#include "../specter_i.h"

/* The boot intro's scene.
 *
 * Splash is the ROOT scene, not Start, and its scene state doubles as an
 * "already played" flag: 0 = fresh boot, 1 = the intro is behind us. That is
 * what lets one scene serve two jobs - play the animation on the way in, and
 * be the thing BACK from the menu lands on, where the right move is to quit
 * the app rather than replay it.
 */

static void specter_splash_done_cb(void* context) {
    SpecterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventSplashDone);
}

void specter_scene_splash_on_enter(void* context) {
    SpecterApp* app = context;

    if(scene_manager_get_scene_state(app->scene_manager, SpecterSceneSplash) == 1) {
        /* Arrived here via BACK from the menu - this is the app's exit. */
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    if(!app->settings.intro) {
        /* Switched off in Settings. Post the event rather than navigating from
         * here: on_enter runs inside the scene manager, and calling
         * next_scene() from it unwinds the stack underneath the call still
         * running. No view is shown in the meantime, so the menu simply comes
         * straight up. */
        view_dispatcher_send_custom_event(app->view_dispatcher, SpecterCustomEventSplashDone);
        return;
    }

    splash_view_set_done_callback(app->splash_view, specter_splash_done_cb, app);
    splash_view_reset(app->splash_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSplash);
}

bool specter_scene_splash_on_event(void* context, SceneManagerEvent event) {
    SpecterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SpecterCustomEventSplashDone) {
            scene_manager_set_scene_state(app->scene_manager, SpecterSceneSplash, 1);
            scene_manager_next_scene(app->scene_manager, SpecterSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        /* Only drive the animation while it is actually on screen. Once the
         * flag is set this scene is just the exit door, and ticking the view
         * then would re-fire SplashDone underneath the menu. */
        if(scene_manager_get_scene_state(app->scene_manager, SpecterSceneSplash) == 0 &&
           app->settings.intro) {
            if(splash_view_tick(app->splash_view)) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, SpecterCustomEventSplashDone);
            }
        }
        consumed = true;
    }
    return consumed;
}

void specter_scene_splash_on_exit(void* context) {
    UNUSED(context);
}
