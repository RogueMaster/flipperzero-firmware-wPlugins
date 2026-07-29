#include "../partition_panic_game.h"

#include <assert.h>
#include <stdio.h>

static void tick_until_wall_finishes(PPGame* game) {
    for(size_t tick = 0; tick < 1000 && game->growing_wall.active; tick++) {
        pp_game_tick(game);
    }
    assert(!game->growing_wall.active);
}

static void test_initial_state(void) {
    PPGame game;
    pp_game_init(&game, 1234);
    assert(game.screen == PPScreenTitle);
    assert(game.level == 1);
    assert(game.ball_count == 2);
    assert(game.lives == 3);
    assert(pp_game_captured_percent(&game) == 0);
    for(uint8_t x = 0; x < PP_GRID_WIDTH; x++) {
        assert(game.cells[0][x] == PPCellWall);
        assert(game.cells[PP_GRID_HEIGHT - 1][x] == PPCellWall);
    }
}

static void test_completed_wall_captures_ball_free_region(void) {
    PPGame game;
    pp_game_init(&game, 7);
    pp_game_start(&game);
    game.ball_count = 1;
    game.balls[0].x = (10 << 8) + 128;
    game.balls[0].y = (10 << 8) + 128;
    game.balls[0].vx = 0;
    game.balls[0].vy = 0;
    game.cursor_x = 30;
    game.cursor_y = 13;
    game.cursor_horizontal = false;

    assert(pp_game_start_wall(&game));
    tick_until_wall_finishes(&game);
    assert(game.cells[10][30] == PPCellWall);
    assert(game.cells[10][45] == PPCellFilled);
    assert(game.cells[10][10] == PPCellEmpty);
    assert(pp_game_captured_percent(&game) >= 45);
}

static void test_ball_breaks_growing_wall(void) {
    PPGame game;
    pp_game_init(&game, 99);
    pp_game_start(&game);
    game.ball_count = 1;
    game.cursor_x = 20;
    game.cursor_y = 12;
    game.cursor_horizontal = true;
    game.balls[0].x = (20 << 8) + 128;
    game.balls[0].y = (12 << 8) + 128;
    game.balls[0].vx = 0;
    game.balls[0].vy = 0;

    assert(pp_game_start_wall(&game));
    pp_game_tick(&game);
    assert(game.lives == 2);
    assert(!game.growing_wall.active);
    assert(game.cells[12][20] == PPCellEmpty);
}

static void test_boundary_collision_reverses_ball(void) {
    PPGame game;
    pp_game_init(&game, 4);
    pp_game_start(&game);
    game.ball_count = 1;
    game.balls[0].x = (1 << 8) + 150;
    game.balls[0].y = (8 << 8) + 128;
    game.balls[0].vx = -80;
    game.balls[0].vy = 0;

    for(size_t tick = 0; tick < 8; tick++)
        pp_game_tick(&game);
    assert(game.balls[0].vx > 0);
}

static void test_pause_freezes_timer(void) {
    PPGame game;
    pp_game_init(&game, 12);
    pp_game_start(&game);
    uint32_t before = game.time_remaining_ticks;
    pp_game_toggle_pause(&game);
    pp_game_tick(&game);
    assert(game.time_remaining_ticks == before);
    pp_game_toggle_pause(&game);
    pp_game_tick(&game);
    assert(game.time_remaining_ticks == before - 1);
}

static void test_target_capture_completes_level(void) {
    PPGame game;
    pp_game_init(&game, 42);
    pp_game_start(&game);
    game.ball_count = 1;
    game.balls[0].x = (8 << 8) + 128;
    game.balls[0].y = (8 << 8) + 128;
    game.balls[0].vx = 0;
    game.balls[0].vy = 0;
    game.cursor_x = 15;
    game.cursor_y = 12;
    game.cursor_horizontal = false;

    assert(pp_game_start_wall(&game));
    tick_until_wall_finishes(&game);
    assert(pp_game_captured_percent(&game) >= PP_TARGET_PERCENT);
    assert(game.screen == PPScreenLevelClear);
}

static void test_timeout_ends_game(void) {
    PPGame game;
    pp_game_init(&game, 15);
    pp_game_start(&game);
    game.time_remaining_ticks = 1;
    pp_game_tick(&game);
    assert(game.screen == PPScreenGameOver);
}

static void test_level_progression_and_victory(void) {
    PPGame game;
    pp_game_init(&game, 64);
    pp_game_start(&game);

    for(uint8_t level = 1; level < PP_FINAL_LEVEL; level++) {
        game.screen = PPScreenLevelClear;
        pp_game_advance_level(&game);
        assert(game.level == level + 1);
        assert(game.screen == PPScreenPlaying);
        assert(game.ball_count == game.level + 1);
    }

    game.screen = PPScreenLevelClear;
    pp_game_advance_level(&game);
    assert(game.screen == PPScreenVictory);
}

int main(void) {
    test_initial_state();
    test_completed_wall_captures_ball_free_region();
    test_ball_breaks_growing_wall();
    test_boundary_collision_reverses_ball();
    test_pause_freezes_timer();
    test_target_capture_completes_level();
    test_timeout_ends_game();
    test_level_progression_and_victory();
    puts("All Partition Panic game tests passed.");
    return 0;
}
