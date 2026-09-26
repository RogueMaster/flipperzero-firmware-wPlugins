#pragma once
#include <furi.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>

// Screen size
#define SCREEN_W    128
#define SCREEN_H    64
// Render columns (performance): each column renders N pixels wide
#define RENDER_COLS 64
// Framebuffer: 1-bit XBM
#define FB_BYTES    (SCREEN_W * SCREEN_H / 8)

// Max maze size
#define MAP_MAX 31

// Cell types (minimal: only empty, wall, exit)
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
    MODE_PLAY, // in-game maze
    MODE_CLEAR, // level cleared overlay
    MODE_ABOUT, // about screen
} GameMode;

// Game state (minimal)
typedef struct {
    GameMode mode;
    uint8_t map[MAP_MAX * MAP_MAX];
    int map_w, map_h;
    Player player;
    int level;
    bool dirty; // needs re-render
    uint8_t fb[FB_BYTES];
    // Exit coordinates (cached after generation)
    int exit_x, exit_y;
    bool exit_found;
    // Blink tick counter
    uint8_t tick;
    // Language: 0=Chinese (XBM bitmap), 1=English (canvas_draw_str)
    uint8_t lang;
    // Smooth rotation/movement targets (interpolated per tick)
    float turn_target;
    float move_fwd_target;
    float move_bwd_target;
    // HUD on/off (long OK toggles)
    bool show_hud;
    // Jump (cosmetic vertical bob)
    float jump_z;
    uint8_t jump_timer;
} GameState;

extern GameState g;

// ---- Module interfaces ----
void engine_render(void);

void maze_generate(int w, int h, int level, unsigned int seed);
int maze_cell_index(int x, int y);
uint8_t maze_get(int x, int y);
void maze_set(int x, int y, uint8_t v);
uint32_t maze_rng_next(void);

void game_init_level(int level);
void game_handle_input(InputKey key, InputType type);
void game_update(void);
void game_next_level(void);
bool player_move(float dx, float dy);
void player_rotate(float angle);

// ---- Sound ----
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

// Textures
extern const uint8_t TEXTURES[][8];
#define TEX_COUNT 1

// Storage (minimal: language + cleared level)
void storage_load(void);
void storage_save(void);
