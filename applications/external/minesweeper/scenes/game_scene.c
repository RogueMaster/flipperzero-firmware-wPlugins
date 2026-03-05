#include "gui/scene_manager.h"
#include "helpers/mine_sweeper_haptic.h"
#include "helpers/mine_sweeper_led.h"
#include "helpers/mine_sweeper_speaker.h"
#include "minesweeper.h"
#include "scenes/minesweeper_scene.h"
#include "engine/mine_sweeper_engine.h"
#include "views/minesweeper_game_screen.h"
#include <input/input.h>

static void mine_sweeper_game_screen_action_callback(MineSweeperEvent event, void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void mine_sweeper_short_ok_effect(MineSweeperApp* app) {
    furi_assert(app);
    mine_sweeper_led_blink_magenta(app);
    mine_sweeper_play_ok_sound(app);
    mine_sweeper_play_haptic_short(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_long_ok_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_blink_magenta(app);
    mine_sweeper_play_ok_sound(app);
    mine_sweeper_play_haptic_double_short(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_flag_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_blink_cyan(app);
    mine_sweeper_play_flag_sound(app);
    mine_sweeper_play_haptic_short(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_move_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_play_haptic_short(app);
}

static void mine_sweeper_wrap_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_blink_yellow(app);
    mine_sweeper_play_wrap_sound(app);
    mine_sweeper_play_haptic_short(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_oob_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_blink_red(app);
    mine_sweeper_play_oob_sound(app);
    mine_sweeper_play_haptic_short(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_lose_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_set_rgb(app, 255, 0, 000);
    mine_sweeper_play_lose_sound(app);
    mine_sweeper_play_haptic_lose(app);
    mine_sweeper_stop_all_sound(app);
}

static void mine_sweeper_win_effect(MineSweeperApp* app) {
    furi_assert(app);

    mine_sweeper_led_set_rgb(app, 0, 0, 255);
    mine_sweeper_play_win_sound(app);
    mine_sweeper_play_haptic_win(app);
    mine_sweeper_stop_all_sound(app);
}

static void process_feedback(
    MineSweeperApp* app,
    MineSweeperActionType action_type,
    MineSweeperActionResult result) {
    if(result.result == MineSweeperResultNoop && action_type != MineSweeperActionMove) {
        return;
    }

    if(result.result == MineSweeperResultLose) {
        mine_sweeper_lose_effect(app);
        return;
    } else if(result.result == MineSweeperResultWin) {
        mine_sweeper_win_effect(app);
        return;
    }

    switch(action_type) {
    case MineSweeperActionMove:
        if(result.move_outcome == MineSweeperMoveOutcomeBlocked) {
            mine_sweeper_oob_effect(app);
        } else if(result.move_outcome == MineSweeperMoveOutcomeWrapped) {
            mine_sweeper_wrap_effect(app);
        } else {
            mine_sweeper_move_effect(app);
        }
        break;
    case MineSweeperActionReveal:
        mine_sweeper_short_ok_effect(app);
        break;
    case MineSweeperActionFlag:
        mine_sweeper_flag_effect(app);
        break;
    case MineSweeperActionChord:
        mine_sweeper_long_ok_effect(app);
        break;
    default:
        break;
    }
}

static bool handle_playing_inputs(MineSweeperApp* app, SceneManagerEvent event) {
    MineSweeperAction action = {0};

    switch(event.event) {
    case MineSweeperEventMoveUp:
        action.type = MineSweeperActionMove;
        action.dy = -1;
        break;
    case MineSweeperEventMoveDown:
        action.type = MineSweeperActionMove;
        action.dy = 1;
        break;
    case MineSweeperEventMoveLeft:
        action.type = MineSweeperActionMove;
        action.dx = -1;
        break;
    case MineSweeperEventMoveRight:
        action.type = MineSweeperActionMove;
        action.dx = 1;
        break;
    case MineSweeperEventShortOkPress:
        action.type = MineSweeperActionReveal;
        break;
    case MineSweeperEventLongOkPress:
        action.type = MineSweeperActionChord;
        break;
    case MineSweeperEventBackLong:
        action.type = MineSweeperActionFlag;
        break;
    default:
        return false;
    }

    MineSweeperActionResult result = minesweeper_engine_apply_action(&app->game_state, action);

    process_feedback(app, action.type, result);

    if(result.result != MineSweeperResultNoop) {
        // force redraw necessary??
    }

    return true;
}

static bool handle_gameover_inputs(MineSweeperApp* app, SceneManagerEvent event) {
    MineSweeperAction action = {0};
    bool is_move_input = true;

    switch(event.event) {
    case MineSweeperEventMoveUp:
        action.type = MineSweeperActionMove;
        action.dy = -1;
        break;
    case MineSweeperEventMoveDown:
        action.type = MineSweeperActionMove;
        action.dy = 1;
        break;
    case MineSweeperEventMoveLeft:
        action.type = MineSweeperActionMove;
        action.dx = -1;
        break;
    case MineSweeperEventMoveRight:
        action.type = MineSweeperActionMove;
        action.dx = 1;
        break;
    case MineSweeperEventShortOkPress:
    case MineSweeperEventLongOkPress:
    case MineSweeperEventBackLong:
        is_move_input = false;
        break;
    default:
        return false;
    }

    if(!is_move_input) {
        app->generation_origin = MineSweeperGenerationOriginGame;
        scene_manager_next_scene(app->scene_manager, MineSweeperSceneGenerating);
        return true;
    }

    MineSweeperActionResult result = minesweeper_engine_apply_action(&app->game_state, action);

    if(result.result != MineSweeperResultNoop) {
        // force redraw necessary??
    }

    return true;
}

void minesweeper_scene_game_screen_on_enter(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    furi_assert(app->game_screen);

    // Keep existing view projection across transient scene hops (e.g. menu back).
    // Context is bound when a new board is committed in the generating scene.
    mine_sweeper_game_screen_set_input_callback(
        app->game_screen, mine_sweeper_game_screen_action_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MineSweeperGameScreenView);
}

bool minesweeper_scene_game_screen_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    MineSweeperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, MineSweeperSceneMenuScreen);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        return app->game_state.rt.phase == MineSweeperPhasePlaying ?
                   handle_playing_inputs(app, event) :
                   handle_gameover_inputs(app, event);
    }

    if(event.type == SceneManagerEventTypeTick) {
        mine_sweeper_game_screen_update_clock(app->game_screen);
        return true;
    }

    return false;
}

void minesweeper_scene_game_screen_on_exit(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    // Keep the game-state context bound to avoid transient NULL deref during scene handoff.
    // Detach only the input callback owned by this scene.
    mine_sweeper_game_screen_set_input_callback(app->game_screen, NULL, NULL);
}
