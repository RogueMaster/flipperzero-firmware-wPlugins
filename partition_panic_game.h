#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PP_GRID_WIDTH       60
#define PP_GRID_HEIGHT      26
#define PP_GRID_CELLS       (PP_GRID_WIDTH * PP_GRID_HEIGHT)
#define PP_MAX_BALLS        8
#define PP_FINAL_LEVEL      6
#define PP_TARGET_PERCENT   75
#define PP_TICKS_PER_SECOND 25

typedef enum {
    PPCellEmpty = 0,
    PPCellWall,
    PPCellFilled,
    PPCellGrowing,
} PPCell;

typedef enum {
    PPScreenTitle = 0,
    PPScreenPlaying,
    PPScreenPaused,
    PPScreenLevelClear,
    PPScreenGameOver,
    PPScreenVictory,
} PPScreen;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
} PPBall;

typedef struct {
    bool active;
    bool horizontal;
    bool negative_done;
    bool positive_done;
    uint8_t origin_x;
    uint8_t origin_y;
    int16_t negative;
    int16_t positive;
    uint8_t step_delay;
} PPGrowingWall;

typedef struct {
    PPCell cells[PP_GRID_HEIGHT][PP_GRID_WIDTH];
    PPBall balls[PP_MAX_BALLS];
    bool reachable[PP_GRID_HEIGHT][PP_GRID_WIDTH];
    uint16_t flood_queue[PP_GRID_CELLS];
    PPGrowingWall growing_wall;
    PPScreen screen;
    uint32_t random_state;
    uint32_t score;
    uint32_t time_remaining_ticks;
    uint8_t level;
    uint8_t lives;
    uint8_t ball_count;
    uint8_t cursor_x;
    uint8_t cursor_y;
    bool cursor_horizontal;
    uint8_t invalid_action_ticks;
} PPGame;

void pp_game_init(PPGame* game, uint32_t seed);
void pp_game_start(PPGame* game);
void pp_game_advance_level(PPGame* game);
void pp_game_toggle_pause(PPGame* game);
void pp_game_move_cursor(PPGame* game, int8_t dx, int8_t dy, bool horizontal);
bool pp_game_start_wall(PPGame* game);
void pp_game_tick(PPGame* game);

uint8_t pp_game_captured_percent(const PPGame* game);
uint32_t pp_game_seconds_remaining(const PPGame* game);
