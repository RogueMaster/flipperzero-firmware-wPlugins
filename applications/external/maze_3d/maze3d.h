#pragma once
#include <furi.h>
#include <input/input.h>
#include <gui/canvas.h>
#include <stdbool.h>
#include <stdint.h>

// Screen size
#define SCREEN_W    128
#define SCREEN_H    64
// Render columns: each column draws as a single vertical strip.
#define RENDER_COLS 32

// Max maze size
#define MAP_MAX 23

// Cell types
typedef enum {
    CELL_EMPTY = 0,
    WALL_BRICK = 1,
    CELL_EXIT = 9,
} CellType;

// Player camera
typedef struct {
    float x, y;
    float dir_x, dir_y;
    float plane_x, plane_y;
} Player;

// Game modes
typedef enum {
    MODE_MENU = 0,
    MODE_PLAY,
    MODE_CLEAR,
    MODE_ABOUT,
} GameMode;

// Pre-rendered column data (drawn directly with canvas primitives)
typedef struct {
    uint8_t wall_top; // y start of wall
    uint8_t wall_bot; // y end of wall (exclusive)
    uint8_t shade; // 0..4
    bool hit; // whether this column hit a wall
} ColData;

// Game state
typedef struct {
    GameMode mode;
    uint8_t map[MAP_MAX * MAP_MAX];
    int map_w, map_h;
    Player player;
    int level;
    bool dirty;
    ColData cols[RENDER_COLS];
    int exit_x, exit_y;
    bool exit_found;
    uint8_t tick;
    uint8_t lang;
    float turn_target;
    float move_fwd_target;
    float move_bwd_target;
    bool show_hud;
} GameState;

extern GameState g;

// ---- Render ----
// Compute column data into g.cols (called from app thread).
void engine_compute(void);
// Draw the pre-computed columns + HUD directly to canvas (called from GUI thread).
void engine_draw(Canvas* canvas);

// ---- Maze ----
void maze_generate(int w, int h, int level, unsigned int seed);
int maze_cell_index(int x, int y);
uint8_t maze_get(int x, int y);
void maze_set(int x, int y, uint8_t v);
uint32_t maze_rng_next(void);

// ---- Game ----
void game_init_level(int level);
void game_handle_input(InputKey key, InputType type);
void game_update(void);
void game_next_level(void);
bool player_move(float dx, float dy);
void player_rotate(float angle);

// ---- Sound (stub-safe: no-op if speaker unavailable) ----
typedef enum {
    SFX_NONE = 0,
    SFX_MENU_MOVE,
    SFX_MENU_OK,
    SFX_LEVEL_CLEAR,
    SFX_STEP,
    SFX_COUNT,
} SfxType;

void sfx_init(void);
void sfx_deinit(void);
void sfx_play(SfxType t);
void sfx_tick_update(void);

// ---- Storage ----
void storage_load(void);
void storage_save(void);
