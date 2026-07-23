#include "../ear_trainer_i.h"
#include "ear_scene.h"

/* Free-play sandbox: walk every interval and hear it on demand, with the tune
 * that opens with it. Useful before a session and as a cheat sheet after a
 * wrong answer. */

#define REF_ROOT 60 /* C4 */

static void reference_callback(void* context, uint32_t index) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void reference_build(EarTrainerApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Play an interval");

    for(uint8_t s = 0; s < IntervalCount; s++) {
        const IntervalInfo* info = interval_get(s);
        char label[40];
        snprintf(label, sizeof(label), "%-2s %s", info->shortname, info->name);
        submenu_add_item(submenu, label, s, reference_callback, app);
    }
    submenu_set_selected_item(submenu, app->reference_index);
}

void ear_scene_reference_on_enter(void* context) {
    EarTrainerApp* app = context;
    reference_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewSubmenu);
}

bool ear_scene_reference_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= IntervalCount) return false;

    app->reference_index = event.event;
    tone_player_play_interval(app->player, REF_ROOT, (uint8_t)(REF_ROOT + event.event));
    return true;
}

void ear_scene_reference_on_exit(void* context) {
    EarTrainerApp* app = context;
    tone_player_stop(app->player);
    submenu_reset(app->submenu);
}
