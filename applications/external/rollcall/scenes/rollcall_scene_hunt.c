#include "../rollcall_i.h"

/* Scene state remembers whether we have already announced a winner, so the
 * "found it" blip fires once instead of on every tick. */
typedef enum {
    HuntStateSearching = 0,
    HuntStateFound,
} HuntState;

static void rollcall_scene_hunt_view_cb(void* context, HuntEvent event) {
    RollCallApp* app = context;
    if(event == HuntEventAdopt) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventAdoptBand);
    }
}

void rollcall_scene_hunt_on_enter(void* context) {
    RollCallApp* app = context;

    scene_manager_set_scene_state(app->scene_manager, RollCallSceneHunt, HuntStateSearching);

    hunt_view_reset(app->hunt_view);
    hunt_view_set_callback(app->hunt_view, rollcall_scene_hunt_view_cb, app);

    rc_radio_hunt_start(app->radio);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewHunt);
}

bool rollcall_scene_hunt_on_event(void* context, SceneManagerEvent event) {
    RollCallApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RollCallCustomEventAdoptBand) {
            int8_t best = rc_radio_hunt_best(app->radio);
            if(best >= 0) {
                /* Adopt the band for good - the whole point is not having to
                 * find it again next time. */
                app->settings.band_idx = (uint8_t)best;
                rc_settings_save(&app->settings);
                scene_manager_next_scene(app->scene_manager, RollCallSceneCapture);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        RcHuntBand bands[RC_BAND_COUNT];
        uint8_t n = rc_radio_hunt_snapshot(app->radio, bands, RC_BAND_COUNT);
        int8_t best = rc_radio_hunt_best(app->radio);

        hunt_view_set_data(app->hunt_view, bands, n, best, rc_radio_hunt_sweeps(app->radio));
        hunt_view_tick(app->hunt_view);

        uint32_t state = scene_manager_get_scene_state(app->scene_manager, RollCallSceneHunt);
        if(best >= 0 && state == HuntStateSearching) {
            scene_manager_set_scene_state(app->scene_manager, RollCallSceneHunt, HuntStateFound);
            rollcall_notify_capture(app);
        }
        consumed = true;
    }

    return consumed;
}

void rollcall_scene_hunt_on_exit(void* context) {
    RollCallApp* app = context;
    rc_radio_hunt_stop(app->radio);
}
