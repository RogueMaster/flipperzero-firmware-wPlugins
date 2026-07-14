#include "game.h"
#include "levels/level_imu.h"

void game_start(GameManager* game_manager, void* ctx) {
    GameContext* context = ctx;
    context->imu = imu_alloc();
    context->imu_present = imu_present(context->imu);
    context->level = game_manager_add_level(game_manager, &level_imu);
    context->game_manager = game_manager;
    game_manager_show_fps_set(context->game_manager, false);
}

void game_stop(void* ctx) {
    GameContext* context = ctx;
    imu_free(context->imu);
}

const Game game = {
    .target_fps = 30,
    .show_fps = false,
    .always_backlight = true,
    .start = game_start,
    .stop = game_stop,
    .context_size = sizeof(GameContext),
};
