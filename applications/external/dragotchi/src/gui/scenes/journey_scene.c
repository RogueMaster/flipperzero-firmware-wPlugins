#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>

#include "journey_scene.h"
#include "scenes.h"
#include "../../flipper_structs.h"

void scene_journey_on_enter(void *ctx) {
    struct ApplicationContext *c = ctx;
    text_box_reset(c->text_box_module);
    text_box_set_text(c->text_box_module, c->game_state->journey_log);
    view_dispatcher_switch_to_view(c->view_dispatcher, scene_journey);
}
bool scene_journey_on_event(void *ctx, SceneManagerEvent e) {
    UNUSED(ctx); UNUSED(e);
    return false; // Back pops to main
}
void scene_journey_on_exit(void *ctx) {
    struct ApplicationContext *c = ctx;
    text_box_reset(c->text_box_module);
}
