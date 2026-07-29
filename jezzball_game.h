#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JZ_GRID_WIDTH       60
#define JZ_GRID_HEIGHT      26
#define JZ_GRID_CELLS       (JZ_GRID_WIDTH * JZ_GRID_HEIGHT)
#define JZ_MAX_BALLS        8
#define JZ_FINAL_LEVEL      6
#define JZ_TARGET_PERCENT   75
#define JZ_TICKS_PER_SECOND 25

typedef enum {
    JzCellEmpty = 0,
    JzCellWall,
    JzCellFilled,
    JzCellGrowing,
} JzCell;

typedef enum {
    JzScreenTitle = 0,
    JzScreenPlaying,
    JzScreenPaused,
    JzScreenLevelClear,
    JzScreenGameOver,
    JzScreenVictory,
} JzScreen;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
} JzBall;

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
} JzGrowingWall;

typedef struct {
    JzCell cells[JZ_GRID_HEIGHT][JZ_GRID_WIDTH];
    JzBall balls[JZ_MAX_BALLS];
    bool reachable[JZ_GRID_HEIGHT][JZ_GRID_WIDTH];
    uint16_t flood_queue[JZ_GRID_CELLS];
    JzGrowingWall growing_wall;
    JzScreen screen;
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
} JzGame;

void jz_game_init(JzGame* game, uint32_t seed);
void jz_game_start(JzGame* game);
void jz_game_advance_level(JzGame* game);
void jz_game_toggle_pause(JzGame* game);
void jz_game_move_cursor(JzGame* game, int8_t dx, int8_t dy, bool horizontal);
bool jz_game_start_wall(JzGame* game);
void jz_game_tick(JzGame* game);

uint8_t jz_game_captured_percent(const JzGame* game);
uint32_t jz_game_seconds_remaining(const JzGame* game);
