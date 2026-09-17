#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "main_scene.h"
#include "scenes.h"
#include "../pet_view.h"
#include "../../flipper_structs.h"
#include "../../constants.h"
#include "../../state_management.h"

void scene_main_on_enter(void *ctx) {
    struct ApplicationContext *context = (struct ApplicationContext *)ctx;
    pet_view_update(context->pet_view, context->game_state, state_is_night_now(), state_expedition_remaining(context->game_state));
    view_dispatcher_switch_to_view(context->view_dispatcher, scene_main);
}

bool scene_main_on_event(void *ctx, SceneManagerEvent event) {
    struct ApplicationContext *context = (struct ApplicationContext *)ctx;
    switch(event.type) {
        case SceneManagerEventTypeBack:
            scene_manager_stop(context->scene_manager);
            view_dispatcher_stop(context->view_dispatcher);
            return true;
        case SceneManagerEventTypeTick:
            if(context->game_state->storm_ready) {
                context->game_state->storm_ready = 0;
                scene_manager_next_scene(context->scene_manager, scene_storm);
                return true;
            }
            if(context->game_state->journey_ready) {
                context->game_state->journey_ready = 0;
                scene_manager_next_scene(context->scene_manager, scene_journey);
                return true;
            }
            pet_view_update(context->pet_view, context->game_state, state_is_night_now(), state_expedition_remaining(context->game_state));
            return true;
        case SceneManagerEventTypeCustom:
            if(event.event == PET_EVT_MENU) {
                scene_manager_next_scene(context->scene_manager, scene_menu);
            }
            return true;
        default:
            break;
    }
    return false;
}

void scene_main_on_exit(void *ctx) { UNUSED(ctx); }
