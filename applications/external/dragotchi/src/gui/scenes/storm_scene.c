#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "storm_scene.h"
#include "scenes.h"
#include "../storm_view.h"
#include "../../flipper_structs.h"

/* Drives the radar animation while the Signal Storm screen is open. */
static void storm_timer_cb(void *ctx) {
    storm_view_tick((View *)ctx);
}

void scene_storm_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    storm_view_set(c->storm_view, c->game_state);
    if(!c->storm_timer) {
        c->storm_timer = furi_timer_alloc(storm_timer_cb, FuriTimerTypePeriodic, c->storm_view);
    }
    furi_timer_start(c->storm_timer, furi_ms_to_ticks(80)); /* ~12 fps */
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_storm);
}

bool scene_storm_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type == SceneManagerEventTypeCustom && e.event == STORM_EVT_DONE) {
        scene_manager_search_and_switch_to_previous_scene(c->scene_manager, scene_main);
        return true;
    }
    return false; /* Back also pops to main via navigation */
}

void scene_storm_on_exit(void *ctx) {
    struct ApplicationContext *c = ctx;
    if(c->storm_timer) furi_timer_stop(c->storm_timer);
}
