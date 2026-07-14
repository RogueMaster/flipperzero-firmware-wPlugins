#include "../guess_the_number.h"
#include "../helpers/game_haptic.h"

enum SubmenuIndex {
    SubmenuIndexPlay = 10,
    SubmenuIndexInstructions,
    SubmenuIndexSettings,
};

void game_scene_menu_submenu_callback(void* context, uint32_t index) {
    GameApp* app = context;
    game_play_button_press(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void game_scene_menu_on_enter(void* context) {
    GameApp* app = context;

    submenu_add_item(
        app->submenu, "🎮 Start Game", SubmenuIndexPlay, game_scene_menu_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "📖 How to Play",
        SubmenuIndexInstructions,
        game_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "⚙️ Settings", SubmenuIndexSettings, game_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, GameSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, GameViewIdMenu);
}

bool game_scene_menu_on_event(void* context, SceneManagerEvent event) {
    GameApp* app = context;
    UNUSED(app);
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexInstructions) {
            scene_manager_set_scene_state(
                app->scene_manager, GameSceneMenu, SubmenuIndexInstructions);
            scene_manager_next_scene(app->scene_manager, GameSceneInstructions);
            return true;
        } else if(event.event == SubmenuIndexPlay) {
            scene_manager_set_scene_state(app->scene_manager, GameSceneMenu, SubmenuIndexPlay);
            scene_manager_next_scene(app->scene_manager, GameScenePlay);
            return true;
        } else if(event.event == SubmenuIndexSettings) {
            scene_manager_set_scene_state(app->scene_manager, GameSceneMenu, SubmenuIndexSettings);
            scene_manager_next_scene(app->scene_manager, GameSceneSettings);
            return true;
        }
    }
    return false;
}

void game_scene_menu_on_exit(void* context) {
    GameApp* app = context;
    submenu_reset(app->submenu);
}
