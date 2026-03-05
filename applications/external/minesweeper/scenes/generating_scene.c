#include "minesweeper.h"
#include "scenes/minesweeper_scene.h"
#include "helpers/mine_sweeper_config.h"
#include "helpers/mine_sweeper_led.h"

#include <furi.h>

static void
    minesweeper_scene_generating_input_callback(MineSweeperGeneratingEvent event, void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    if(event == MineSweeperGeneratingEventStartNow) {
        app->generation_user_preempted = true;
    }
}

static MineSweeperConfig minesweeper_scene_generating_build_config(const MineSweeperApp* app) {
    furi_assert(app);

    MineSweeperConfig config = {
        .width = app->settings_committed.board_width,
        .height = app->settings_committed.board_height,
        .difficulty = app->settings_committed.difficulty,
        .ensure_solvable = app->settings_committed.ensure_solvable_board,
        .wrap_enabled = app->wrap_enabled,
    };

    return config;
}

static void minesweeper_scene_generating_update_stats(MineSweeperApp* app) {
    furi_assert(app);

    uint32_t elapsed_seconds = 0;
    if(app->generation_job.start_tick != 0) {
        elapsed_seconds = (furi_get_tick() - app->generation_job.start_tick) / 1000u;
    }

    minesweeper_generating_view_set_stats(
        app->generating_view, app->generation_job.attempts_total, elapsed_seconds);
}

static void minesweeper_scene_generating_try_switch_to_game(
    MineSweeperApp* app,
    bool allow_unsolved_fallback) {
    furi_assert(app);

    if(minesweeper_engine_generation_finish(
           &app->generation_job, &app->game_state, allow_unsolved_fallback) !=
       MineSweeperResultChanged) {
        return;
    }

    mine_sweeper_game_screen_set_context(app->game_screen, &app->game_state);
    mine_sweeper_game_screen_reset_clock(app->game_screen);
    scene_manager_next_scene(app->scene_manager, MineSweeperSceneGameScreen);
}

static void minesweeper_scene_generating_cancel_and_return(MineSweeperApp* app) {
    furi_assert(app);

    minesweeper_engine_generation_cancel(&app->generation_job);

    switch(app->generation_origin) {
    case MineSweeperGenerationOriginStart:
        scene_manager_next_scene(app->scene_manager, MineSweeperSceneStartScreen);
        break;
    case MineSweeperGenerationOriginSettings:
        scene_manager_next_scene(app->scene_manager, MineSweeperSceneSettingsScreen);
        break;
    case MineSweeperGenerationOriginGame:
    default:
        scene_manager_next_scene(app->scene_manager, MineSweeperSceneGameScreen);
        break;
    }
}

void minesweeper_scene_generating_on_enter(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    mine_sweeper_led_reset(app);

    MineSweeperConfig config = minesweeper_scene_generating_build_config(app);
    if(minesweeper_engine_generation_begin(&app->generation_job, &config) ==
       MineSweeperResultInvalid) {
        FURI_LOG_E(TAG, "Failed to begin generation job");
        minesweeper_scene_generating_cancel_and_return(app);
        return;
    }

    app->generation_user_preempted = false;

    minesweeper_generating_view_set_context(app->generating_view, app);
    minesweeper_generating_view_set_input_callback(
        app->generating_view, minesweeper_scene_generating_input_callback);
    minesweeper_scene_generating_update_stats(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MineSweeperGeneratingScreenView);
}

bool minesweeper_scene_generating_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MineSweeperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Generating flow should never navigate away via Back.
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        if(app->generation_user_preempted) {
            if(!app->generation_job.has_latest_candidate) {
                minesweeper_engine_generation_step(&app->generation_job, 1);
                minesweeper_scene_generating_update_stats(app);
                if(!app->generation_job.has_latest_candidate) {
                    return true;
                }
            }

            minesweeper_scene_generating_try_switch_to_game(app, true);
            return true;
        }

        MineSweeperGenerationStatus status =
            minesweeper_engine_generation_step(&app->generation_job, 16);
        minesweeper_scene_generating_update_stats(app);

        if(status == MineSweeperGenerationStatusReady) {
            minesweeper_scene_generating_try_switch_to_game(app, false);
        }
        return true;
    }

    return false;
}

void minesweeper_scene_generating_on_exit(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;

    minesweeper_generating_view_reset(app->generating_view);
}
