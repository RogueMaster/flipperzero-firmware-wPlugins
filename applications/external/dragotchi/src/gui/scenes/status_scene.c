#include <furi.h>
#include <gui/view_dispatcher.h>
#include "status_scene.h"
#include "scenes.h"
#include "../stats_view.h"
#include "../../flipper_structs.h"
#include "../../constants.h"
#include "../../clock.h"

static uint32_t age_days(struct ApplicationContext *c) {
    uint32_t now = game_now(), birth = c->game_state->persistent.birth_timestamp;
    return (now > birth) ? (now - birth) / 86400u : 0;
}

void scene_status_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    stats_view_update(c->stats_view, c->game_state, age_days(c));
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_status);
}

bool scene_status_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type == SceneManagerEventTypeTick) {
        stats_view_update(c->stats_view, c->game_state, age_days(c));
        return true;
    }
    return false;
}

void scene_status_on_exit(void *ctx) { UNUSED(ctx); }
