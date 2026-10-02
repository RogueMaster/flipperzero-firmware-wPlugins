#include "maze3d.h"
#include <math.h>

// Initialize a level: generate maze, place player at (1.5, 1.5) facing east.
void game_init_level(int level) {
    g.mode = MODE_PLAY;
    g.level = level;
    g.tick = 0;
    g.turn_target = 0.0f;
    g.move_fwd_target = 0.0f;
    g.move_bwd_target = 0.0f;
    g.show_hud = false;

    // Maze size grows slowly with level, capped at 21.
    int sz = 11 + (level - 1) * 2;
    if(sz > 21) sz = 21;
    unsigned seed = 0x12345678u ^ (unsigned)furi_get_tick();
    maze_generate(sz, sz, level, seed);

    g.player.x = 1.5f;
    g.player.y = 1.5f;
    float angle = 0.0f; // face east
    g.player.dir_x = cosf(angle);
    g.player.dir_y = sinf(angle);
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y = g.player.dir_x * 0.66f;

    g.dirty = true;
}

// Move player with simple collision (axis-separated).
bool player_move(float dx, float dy) {
    float pad = 0.2f;
    float nx = g.player.x + dx;
    float ny = g.player.y + dy;
    uint8_t cx = maze_get((int)(nx + (dx > 0 ? pad : -pad)), (int)g.player.y);
    uint8_t cy = maze_get((int)g.player.x, (int)(ny + (dy > 0 ? pad : -pad)));
    bool moved = false;
    if(cx != WALL_BRICK) {
        g.player.x = nx;
        moved = true;
    }
    if(cy != WALL_BRICK) {
        g.player.y = ny;
        moved = true;
    }
    return moved;
}

void player_rotate(float angle) {
    float cs = cosf(angle), sn = sinf(angle);
    float ndx = g.player.dir_x * cs - g.player.dir_y * sn;
    float ndy = g.player.dir_x * sn + g.player.dir_y * cs;
    g.player.dir_x = ndx;
    g.player.dir_y = ndy;
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y = g.player.dir_x * 0.66f;
}

// In-game input handling.
void game_handle_input(InputKey key, InputType type) {
    if(type == InputTypeShort) {
        if(key == InputKeyUp) {
            g.move_fwd_target += 0.12f;
            sfx_play(SFX_STEP);
        } else if(key == InputKeyDown) {
            g.move_bwd_target += 0.10f;
            sfx_play(SFX_STEP);
        } else if(key == InputKeyLeft) {
            g.turn_target += 0.18f;
        } else if(key == InputKeyRight) {
            g.turn_target -= 0.18f;
        } else if(key == InputKeyOk) {
            // Long-press handled separately; short OK is unused.
        } else if(key == InputKeyBack) {
            g.mode = MODE_MENU;
            sfx_play(SFX_MENU_OK);
        }
    } else if(type == InputTypeLong) {
        if(key == InputKeyOk) {
            // Long OK toggles the HUD.
            g.show_hud = !g.show_hud;
            sfx_play(SFX_MENU_OK);
        } else if(key == InputKeyUp) {
            g.move_fwd_target += 0.4f;
            sfx_play(SFX_STEP);
        } else if(key == InputKeyDown) {
            g.move_bwd_target += 0.3f;
            sfx_play(SFX_STEP);
        } else if(key == InputKeyLeft) {
            g.turn_target += 0.5f;
        } else if(key == InputKeyRight) {
            g.turn_target -= 0.5f;
        }
    }
    g.dirty = true;
}

// Per-tick world update: interpolate movement/rotation, check exit.
void game_update(void) {
    g.tick++;

    // Rotation interpolation.
    if(g.turn_target != 0.0f) {
        float max_step = 0.05f;
        float step = g.turn_target;
        if(step > max_step) step = max_step;
        if(step < -max_step) step = -max_step;
        player_rotate(step);
        g.turn_target -= step;
        if(fabsf(g.turn_target) < 0.001f) g.turn_target = 0.0f;
    }

    // Forward movement.
    if(g.move_fwd_target != 0.0f) {
        float s = g.move_fwd_target > 0.15f ? 0.15f : g.move_fwd_target;
        player_move(g.player.dir_x * s, g.player.dir_y * s);
        g.move_fwd_target -= s;
        if(g.move_fwd_target < 0.0f) g.move_fwd_target = 0.0f;
    }
    // Backward movement.
    if(g.move_bwd_target != 0.0f) {
        float s = g.move_bwd_target > 0.12f ? 0.12f : g.move_bwd_target;
        player_move(-g.player.dir_x * s, -g.player.dir_y * s);
        g.move_bwd_target -= s;
        if(g.move_bwd_target < 0.0f) g.move_bwd_target = 0.0f;
    }

    // Check exit reached.
    int cx = (int)g.player.x;
    int cy = (int)g.player.y;
    if(cx == g.exit_x && cy == g.exit_y) {
        g.mode = MODE_CLEAR;
        sfx_play(SFX_LEVEL_CLEAR);
    }

    g.dirty = true;
}

void game_next_level(void) {
    game_init_level(g.level + 1);
}
