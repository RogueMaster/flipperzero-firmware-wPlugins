#include <furi.h>
#include <gui/view_dispatcher.h>
#include "inventory_scene.h"
#include "scenes.h"
#include "../inventory_view.h"
#include "../../flipper_structs.h"

void scene_inventory_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    inventory_view_update(c->inventory_view, c->game_state);
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_inventory);
}
bool scene_inventory_on_event(void *ctx, SceneManagerEvent e) {
    struct ApplicationContext *c = ctx;
    if(e.type == SceneManagerEventTypeTick) {
        inventory_view_update(c->inventory_view, c->game_state);
        return true;
    }
    return false;
}
void scene_inventory_on_exit(void *ctx) { UNUSED(ctx); }
