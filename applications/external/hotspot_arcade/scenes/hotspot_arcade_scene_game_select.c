#include "../hotspot_arcade_i.h"
#include "../helpers/ha_session.h"

// Pure selector: pick which game is live, tell the ESP, then return to the
// dashboard. Hosting a round is a separate action on the dashboard, so choosing
// a game never jumps you into a screen you didn't ask for.
typedef enum {
    GameTrivia,
    GameConnect4,
    GameTicTacToe,
    GameDots,
    GameReversi,
    GameDraw,
    GamePong,
    GameReact,
    GameWyr,
    GameScramble,
    GameGuessColor,
    GameBattleship,
    GameSpectrum,
    GameKmk,
    GameFillBlank,
    GameChess,
    GameSecrets,
    GameWerewolf,
    GameSpyfall,
    GameFrankendraw,
    GameNone,
} GameIndex;

static void ha_game_cb(void* context, uint32_t index) {
    HotspotArcadeApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hotspot_arcade_scene_game_select_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Game");
    submenu_add_item(app->submenu, "Trivia", GameTrivia, ha_game_cb, app);
    submenu_add_item(app->submenu, "Would You Rather", GameWyr, ha_game_cb, app);
    submenu_add_item(app->submenu, "Word Scramble", GameScramble, ha_game_cb, app);
    submenu_add_item(app->submenu, "Spectrum", GameSpectrum, ha_game_cb, app);
    submenu_add_item(app->submenu, "Kiss Marry Kill", GameKmk, ha_game_cb, app);
    submenu_add_item(app->submenu, "Secrets", GameSecrets, ha_game_cb, app);
    submenu_add_item(app->submenu, "Fill the Blank", GameFillBlank, ha_game_cb, app);
    submenu_add_item(app->submenu, "Werewolf", GameWerewolf, ha_game_cb, app);
    submenu_add_item(app->submenu, "Spyfall", GameSpyfall, ha_game_cb, app);
    submenu_add_item(app->submenu, "Draw a Monster", GameFrankendraw, ha_game_cb, app);
    submenu_add_item(app->submenu, "Reaction Duel", GameReact, ha_game_cb, app);
    submenu_add_item(app->submenu, "Connect Four", GameConnect4, ha_game_cb, app);
    submenu_add_item(app->submenu, "Tic-Tac-Toe", GameTicTacToe, ha_game_cb, app);
    submenu_add_item(app->submenu, "Dots & Boxes", GameDots, ha_game_cb, app);
    submenu_add_item(app->submenu, "Reversi", GameReversi, ha_game_cb, app);
    submenu_add_item(app->submenu, "Drawing", GameDraw, ha_game_cb, app);
    submenu_add_item(app->submenu, "Pong", GamePong, ha_game_cb, app);
    submenu_add_item(app->submenu, "Guess the Color", GameGuessColor, ha_game_cb, app);
    submenu_add_item(app->submenu, "Battleship", GameBattleship, ha_game_cb, app);
    submenu_add_item(app->submenu, "Chess", GameChess, ha_game_cb, app);
    submenu_add_item(app->submenu, "None (lobby)", GameNone, ha_game_cb, app);
    uint32_t sel = app->active_game == HA_GAME_TRIVIA      ? GameTrivia :
                   app->active_game == HA_GAME_WYR         ? GameWyr :
                   app->active_game == HA_GAME_SCRAMBLE    ? GameScramble :
                   app->active_game == HA_GAME_SPECTRUM    ? GameSpectrum :
                   app->active_game == HA_GAME_KMK         ? GameKmk :
                   app->active_game == HA_GAME_FILLBLANK   ? GameFillBlank :
                   app->active_game == HA_GAME_SPYFALL     ? GameSpyfall :
                   app->active_game == HA_GAME_REACT       ? GameReact :
                   app->active_game == HA_GAME_CONNECT4    ? GameConnect4 :
                   app->active_game == HA_GAME_TICTACTOE   ? GameTicTacToe :
                   app->active_game == HA_GAME_DOTS        ? GameDots :
                   app->active_game == HA_GAME_REVERSI     ? GameReversi :
                   app->active_game == HA_GAME_DRAW        ? GameDraw :
                   app->active_game == HA_GAME_PONG        ? GamePong :
                   app->active_game == HA_GAME_GUESSCOLOR  ? GameGuessColor :
                   app->active_game == HA_GAME_BATTLESHIP  ? GameBattleship :
                   app->active_game == HA_GAME_CHESS       ? GameChess :
                   app->active_game == HA_GAME_SECRETS     ? GameSecrets :
                   app->active_game == HA_GAME_WEREWOLF    ? GameWerewolf :
                   app->active_game == HA_GAME_FRANKENDRAW ? GameFrankendraw :
                                                             GameNone;
    submenu_set_selected_item(app->submenu, sel);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
}

bool hotspot_arcade_scene_game_select_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case GameTrivia:
        ha_select_game(app, HA_GAME_TRIVIA);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameConnect4:
        ha_select_game(app, HA_GAME_CONNECT4);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameTicTacToe:
        ha_select_game(app, HA_GAME_TICTACTOE);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameDots:
        ha_select_game(app, HA_GAME_DOTS);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameReversi:
        ha_select_game(app, HA_GAME_REVERSI);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameWyr:
        ha_select_game(app, HA_GAME_WYR);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameScramble:
        ha_select_game(app, HA_GAME_SCRAMBLE);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameSpectrum:
        ha_select_game(app, HA_GAME_SPECTRUM);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameKmk:
        ha_select_game(app, HA_GAME_KMK);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameFillBlank:
        ha_select_game(app, HA_GAME_FILLBLANK);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameReact:
        ha_select_game(app, HA_GAME_REACT);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameDraw:
        ha_select_game(app, HA_GAME_DRAW);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GamePong:
        ha_select_game(app, HA_GAME_PONG);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameGuessColor:
        ha_select_game(app, HA_GAME_GUESSCOLOR);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameBattleship:
        ha_select_game(app, HA_GAME_BATTLESHIP);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameChess:
        ha_select_game(app, HA_GAME_CHESS);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameSecrets:
        ha_select_game(app, HA_GAME_SECRETS);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameWerewolf:
        ha_select_game(app, HA_GAME_WEREWOLF);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameSpyfall:
        ha_select_game(app, HA_GAME_SPYFALL);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameFrankendraw:
        ha_select_game(app, HA_GAME_FRANKENDRAW);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    case GameNone:
        ha_select_game(app, HA_GAME_NONE);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    default:
        return false;
    }
}

void hotspot_arcade_scene_game_select_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    submenu_reset(app->submenu);
}
