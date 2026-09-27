#include "../ghosttag_i.h"

static void ghosttag_scene_air_help_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventScanOpenHelp);
}

void ghosttag_scene_air_on_enter(void* context) {
    GhostTagApp* app = context;
    air_view_set_help_callback(app->air_view, ghosttag_scene_air_help_cb, app);
    air_check_start(app->air);
    ghosttag_backlight_hold(app, app->settings.keep_lit);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewAir);
}

bool ghosttag_scene_air_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        AirSnapshot snap;
        air_check_snapshot(app->air, &snap);
        air_view_set_snapshot(app->air_view, &snap);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == GhostTagCustomEventScanOpenHelp) {
        scene_manager_next_scene(app->scene_manager, GhostTagSceneAbout);
        return true;
    }
    return false;
}

void ghosttag_scene_air_on_exit(void* context) {
    GhostTagApp* app = context;
    /* Stopping matters more here than anywhere else in the app: the worker
     * borrows the Flipper's Bluetooth radio, and it is air_check_stop() that
     * hands it back. Leave it running and the device's own Bluetooth stays
     * dead until it is rebooted. */
    air_check_stop(app->air);
    air_view_set_help_callback(app->air_view, NULL, NULL);
    ghosttag_backlight_hold(app, false);
}
