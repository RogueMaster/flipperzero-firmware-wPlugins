// Stack Attack game rules. Pure C, no Flipper headers: builds and runs on the host.
//
// Rules cross-checked with eaun01re/stktk, a remake built by watching the original.
// Geometry follows the Siemens C45 original 1:1 (measured from video):
// 12 columns of 8 px between 4 px brick walls, two checker rows and the crane rail on
// rows 0-4, floor from y = 62.
// Everything moves in whole pixels per tick; the original runs at 15 ticks/s.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define G_COLS             12
#define G_ROWS             7 // rows 0..5 hold boxes, row 6 is head room for the worker
#define G_BOX_ROWS         6
#define G_CELL             8
#define G_WALL             4
#define G_FIELD_W          104
#define G_FLOOR_Y          62
#define G_CEIL_Y           5 // first pixel under the rail
#define G_CRANE_BOX_Y      14 // top of a carried box, equals row 5
#define G_MAX_FALLING      24
#define G_MAX_CRANES       5
#define G_BOX_TYPES        8
// Row clear on the C45: every cell explodes for G_CLEAR_CELL_TICKS, the wave starting 1
// tick after the row fills at the right wall and reaching the left one 3 ticks later.
// A cell is removed when its own explosion ends, so the stacks above fall in the same
// right-to-left wave.
#define G_CLEAR_CELL_TICKS 12
#define G_CLEAR_TICKS      (1 + 3 + G_CLEAR_CELL_TICKS)

// Ticks into a row clear at which cell c starts exploding.
static inline int g_clear_delay(int c) {
    return 1 + (G_COLS - 1 - c) / 4;
}

#define G_KEY_LEFT  1
#define G_KEY_RIGHT 2
#define G_KEY_JUMP  4

#define G_EV_LAND  1
#define G_EV_CLEAR 2
#define G_EV_DEATH 4
#define G_EV_JUMP  8
#define G_EV_PUSH  16
#define G_EV_BREAK 32
#define G_EV_WLAND 64 // the worker touched down after a jump or a fall

#define G_DEBRIS_TICKS 6

static inline int g_col_x(int c) {
    return G_WALL + G_CELL * c;
}

static inline int g_row_y(int r) {
    return G_FLOOR_Y - G_CELL * (r + 1);
}

typedef struct {
    int16_t x, y;
    uint8_t type;
} GBox;

typedef enum {
    CraneCarry,
    CraneOpen,
    CraneLeave,
} CraneState;

typedef struct {
    bool active;
    int16_t x; // x of the carried box; the crane is drawn around it
    int8_t dir;
    int8_t target; // column, -1 = none left, carry off screen
    uint8_t type;
    uint8_t state;
    uint8_t timer;
} GCrane;

typedef enum {
    WGround,
    WRising,
    WFalling,
} WState;

typedef struct {
    int16_t x, y; // top-left of the 8x16 sprite
    int8_t face; // -1 left, 1 right
    int8_t move_dir;
    uint8_t move_left;
    uint8_t vstate;
    uint8_t vtimer;
    uint16_t idle; // ticks without moving, the worker turns to the viewer
    uint8_t anim;
} GWorker;

typedef struct {
    uint8_t cell[G_ROWS][G_COLS]; // 0 empty, 1..7 box type
    uint8_t clear_age[G_ROWS]; // ticks since the row filled (0 = not clearing)
    uint16_t clear_mask[G_ROWS]; // cells of that row still exploding

    GBox falling[G_MAX_FALLING];
    uint8_t n_falling;

    bool has_pushed;
    GBox pushed;
    int8_t pushed_dir;
    uint8_t pushed_left;
    uint8_t pushed_stall; // ticks a pushed falling box holds its height before falling on

    GCrane crane[G_MAX_CRANES];
    GWorker w;

    // a box smashed by the helmet, shown as debris for a few ticks
    int16_t debris_x, debris_y;
    uint8_t debris_timer;

    uint32_t score;
    uint32_t boxes_dropped;
    uint32_t rows_cleared;
    uint8_t cranes; // cranes in rotation: one more per cleared row, up to G_MAX_CRANES
    uint16_t spawn_timer;
    uint32_t rng;
    uint32_t tick;
    bool over;
    uint8_t events; // G_EV_* raised during the last tick
} Game;

void game_init(Game* g, uint32_t seed, uint8_t start_cranes);

// held: keys currently down, pressed: keys that went down since the last tick.
void game_tick(Game* g, uint8_t held, uint8_t pressed);

// Exposed for tests and for the renderer.
bool game_worker_blocked(const Game* g, int x, int y);
