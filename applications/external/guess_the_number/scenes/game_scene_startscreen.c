#include "../guess_the_number.h"
#include "../helpers/game_custom_event.h"
#include "../helpers/game_haptic.h"
#include "../helpers/game_led.h"
#include "../views/game_startscreen.h"

void game_scene_startscreen_callback(GameCustomEvent event, void* context) {
    furi_assert(context);
    GameApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void game_scene_startscreen_on_enter(void* context) {
    furi_assert(context);
    GameApp* app = context;
    game_startscreen_set_callback(app->game_startscreen, game_scene_startscreen_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GameViewIdStartscreen);
}

bool game_scene_startscreen_on_event(void* context, SceneManagerEvent event) {
    GameApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GameCustomEventStartscreenLeft:
        case GameCustomEventStartscreenRight:
            break;
        case GameCustomEventStartscreenUp:
        case GameCustomEventStartscreenDown:
            break;
        case GameCustomEventStartscreenOk:
            game_play_button_press(app);
            scene_manager_next_scene(app->scene_manager, GameSceneMenu);
            consumed = true;
            break;
        case GameCustomEventStartscreenBack:
            game_play_button_press(app);
            game_led_reset(app);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, GameSceneStartscreen)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;
        }
    }

    return consumed;
}

void game_scene_startscreen_on_exit(void* context) {
    GameApp* app = context;
    UNUSED(app);
}
