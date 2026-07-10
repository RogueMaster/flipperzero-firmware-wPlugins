#include "include/app/gurpil_app.h"

#include "include/persistence/best_store.h"
#include "include/platform/random_port.h"

// Fixed sim step, ms (~30 FPS); also the FuriTimer period. The timer is allocated in on_enter
// and stopped+freed in on_exit, so the endless countdown only ever runs while this scene (Play)
// is the one on screen — it never ticks while the menu, how-to-play or credits scene is up.
enum {
    GURPIL_GAME_TICK_MS = 33,
};

static void gurpil_scene_game_timer_callback(void *context) {
    GurpilApp *app = context;

    bool became_over = gurpil_game_view_tick(app->game_view, GURPIL_GAME_TICK_MS);
    if (became_over) {
        // pre_run_best is app->best as it stood before this run started: captured now, before
        // any bump below, so the game-over panel's "New best!" reflects this run's own result
        // rather than always being true once app->best has already been updated to match it.
        int32_t pre_run_best = app->best;
        bool is_new_best = gurpil_game_view_is_new_best(app->game_view, pre_run_best);
        if (is_new_best) {
            app->best = gurpil_game_view_distance(app->game_view);
            best_store_save(app->best);
        }
        gurpil_game_view_set_best(app->game_view, app->best, is_new_best);
    }
}

void gurpil_scene_game_on_enter(void *context) {
    GurpilApp *app = context;

    gurpil_game_view_start_run(app->game_view, platform_random_seed(), app->best);

    app->game_timer =
        furi_timer_alloc(gurpil_scene_game_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->game_timer, furi_ms_to_ticks(GURPIL_GAME_TICK_MS));

    view_dispatcher_switch_to_view(app->view_dispatcher, GurpilViewIdGame);
}

bool gurpil_scene_game_on_event(void *context, SceneManagerEvent event) {
    GurpilApp *app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom && event.event == GurpilGameEventReturnToMenu) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    // Back falls through unconsumed here too: SceneManager's default pops this scene back to
    // the menu, which stops the timer in on_exit below — matching Ok-after-game-over above.
    return consumed;
}

void gurpil_scene_game_on_exit(void *context) {
    GurpilApp *app = context;

    furi_timer_stop(app->game_timer);
    furi_timer_free(app->game_timer);
    app->game_timer = NULL;
}
