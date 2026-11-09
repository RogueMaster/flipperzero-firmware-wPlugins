#include "scenes.h"

typedef enum {
    MenuNewGame,
    MenuContinue,
    MenuAllCards,
    MenuHistory,
    MenuStats,
    MenuOurStory,
    MenuSettings,
    MenuAbout,
} MenuIndex;

static void loteria_scene_main_menu_callback(void* context, uint32_t index) {
    LoteriaApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void loteria_scene_main_menu_on_enter(void* context) {
    LoteriaApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Loteria Zero");

    submenu_add_item(app->submenu, "New game", MenuNewGame, loteria_scene_main_menu_callback, app);

    if(loteria_game_has_save(&app->game) || app->persist.has_saved_game) {
        submenu_add_item(
            app->submenu, "Continue game", MenuContinue, loteria_scene_main_menu_callback, app);
    }

    submenu_add_item(
        app->submenu, "All cards", MenuAllCards, loteria_scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "History", MenuHistory, loteria_scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "Statistics", MenuStats, loteria_scene_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Our story", MenuOurStory, loteria_scene_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", MenuSettings, loteria_scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuAbout, loteria_scene_main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewSubmenu);
}

bool loteria_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case MenuNewGame:
            if(loteria_game_has_save(&app->game)) {
                app->persist.stats.games_interrupted++;
            }
            loteria_game_start_new(&app->game);
            app->persist.stats.games_played++;
            app->persist.stats.cards_sung++;
            app->persist.has_saved_game = true;
            app->persist.saved_game = app->game;
            app->persist.stats.last_played_epoch = furi_hal_rtc_get_timestamp();
            loteria_app_save(app);
            app->viewing_previous = false;
            app->history_offset = 0;
            scene_manager_next_scene(app->scene_manager, LoteriaSceneGame);
            break;
        case MenuContinue:
            if(!loteria_game_has_save(&app->game) && app->persist.has_saved_game) {
                app->game = app->persist.saved_game;
            }
            app->viewing_previous = false;
            app->history_offset = 0;
            scene_manager_next_scene(app->scene_manager, LoteriaSceneGame);
            break;
        case MenuAllCards:
            app->all_cards_index = 0;
            scene_manager_next_scene(app->scene_manager, LoteriaSceneAllCards);
            break;
        case MenuHistory:
            app->history_offset = 0;
            scene_manager_next_scene(app->scene_manager, LoteriaSceneHistory);
            break;
        case MenuStats:
            scene_manager_next_scene(app->scene_manager, LoteriaSceneStats);
            break;
        case MenuOurStory:
            scene_manager_next_scene(app->scene_manager, LoteriaSceneOurStory);
            break;
        case MenuSettings:
            scene_manager_next_scene(app->scene_manager, LoteriaSceneSettings);
            break;
        case MenuAbout:
            scene_manager_next_scene(app->scene_manager, LoteriaSceneAbout);
            break;
        default:
            break;
        }
    }
    return consumed;
}

void loteria_scene_main_menu_on_exit(void* context) {
    LoteriaApp* app = context;
    submenu_reset(app->submenu);
}
