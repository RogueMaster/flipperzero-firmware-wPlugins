#include "pathing.h"

#include "game_core.h"
#include "map_state.h"
#include "placement.h"

#include <string.h>

/* Pathfinding and random reachable tile helpers. */

#define FR_PATH_QUEUE_CAP 1024

static uint16_t path_queue[FR_PATH_QUEUE_CAP];
static uint8_t path_seen[FR_EXPLORED_BYTES];
static uint8_t path_first_dir[(FR_MAP_W * FR_MAP_H + 3) / 4];

static bool fr_path_seen_get(uint16_t pos) {
    return (path_seen[pos / 8] & (uint8_t)(1u << (pos % 8))) != 0;
}

static void fr_path_seen_set(uint16_t pos) {
    path_seen[pos / 8] |= (uint8_t)(1u << (pos % 8));
}

static uint8_t fr_path_first_dir_get(uint16_t pos) {
    uint8_t shift = (uint8_t)((pos % 4) * 2);
    return (uint8_t)((path_first_dir[pos / 4] >> shift) & 0x03u);
}

static void fr_path_first_dir_set(uint16_t pos, uint8_t dir) {
    uint8_t shift = (uint8_t)((pos % 4) * 2);
    uint8_t mask = (uint8_t)(0x03u << shift);
    path_first_dir[pos / 4] = (uint8_t)((path_first_dir[pos / 4] & (uint8_t)~mask) | ((dir & 0x03u) << shift));
}

static bool fr_actor_path_can_enter(const FrActor* actor, uint8_t terrain) {
    if(actor->type == FR_MON_EEL) return terrain == FR_TERR_WATER || terrain == FR_TERR_PUDDLE;
    if(terrain == FR_TERR_WATER) return false;
    return fr_is_walkable(terrain);
}

bool fr_find_path_step_current(
    FrGame* game,
    const FrActor* actor,
    uint8_t target_x,
    uint8_t target_y,
    int8_t* out_dx,
    int8_t* out_dy) {
    memset(path_seen, 0, sizeof(path_seen));

    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t start = (uint16_t)actor->y * FR_MAP_W + actor->x;
    path_queue[tail++] = start;
    fr_path_seen_set(start);

    const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while(head < tail) {
        uint16_t pos = path_queue[head++];
        uint8_t x = (uint8_t)(pos % FR_MAP_W);
        uint8_t y = (uint8_t)(pos / FR_MAP_W);
        if(x == target_x && y == target_y) {
            if(pos == start) return false;
            uint8_t dir = fr_path_first_dir_get(pos);
            *out_dx = dirs[dir][0];
            *out_dy = dirs[dir][1];
            return true;
        }
        for(uint8_t i = 0; i < 4; i++) {
            int16_t nx_i = (int16_t)x + dirs[i][0];
            int16_t ny_i = (int16_t)y + dirs[i][1];
            if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
            uint8_t nx = (uint8_t)nx_i;
            uint8_t ny = (uint8_t)ny_i;
            uint16_t next = (uint16_t)ny * FR_MAP_W + nx;
            if(fr_path_seen_get(next)) continue;
            if(!fr_actor_path_can_enter(actor, fr_get_terrain(game, nx, ny))) continue;
            if(fr_blocking_item_at(game, nx, ny)) continue;
            FrActor* blocker = fr_actor_at(game, nx, ny);
            if(blocker && blocker != actor && (nx != target_x || ny != target_y)) continue;
            if(tail >= FR_PATH_QUEUE_CAP) return false;
            fr_path_seen_set(next);
            fr_path_first_dir_set(next, pos == start ? i : fr_path_first_dir_get(pos));
            path_queue[tail++] = next;
        }
    }
    return false;
}

bool fr_path_exists(const FrGame* game, uint8_t start_x, uint8_t start_y, uint8_t goal_x, uint8_t goal_y) {
    memset(path_seen, 0, sizeof(path_seen));

    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t start = (uint16_t)start_y * FR_MAP_W + start_x;
    path_queue[tail++] = start;
    fr_path_seen_set(start);

    while(head < tail) {
        uint16_t pos = path_queue[head++];
        uint8_t x = (uint8_t)(pos % FR_MAP_W);
        uint8_t y = (uint8_t)(pos / FR_MAP_W);
        if(x == goal_x && y == goal_y) return true;

        const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for(uint8_t i = 0; i < 4; i++) {
            int16_t nx_i = (int16_t)x + dirs[i][0];
            int16_t ny_i = (int16_t)y + dirs[i][1];
            if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
            uint8_t nx = (uint8_t)nx_i;
            uint8_t ny = (uint8_t)ny_i;
            uint16_t next = (uint16_t)ny * FR_MAP_W + nx;
            if(fr_path_seen_get(next)) continue;
            uint8_t terrain = fr_get_terrain(game, nx, ny);
            if(!fr_is_walkable(terrain) && (game->tiles[ny][nx] & FR_TILE_HIDDEN_DOOR) == 0) continue;
            if(fr_blocking_item_at(game, nx, ny)) continue;
            if(tail >= FR_PATH_QUEUE_CAP) return false;
            fr_path_seen_set(next);
            path_queue[tail++] = next;
        }
    }
    return false;
}

static bool fr_random_reachable_tile_impl(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps, bool reveal_hidden) {
    uint8_t rx = *x;
    uint8_t ry = *y;
    uint8_t best_x = rx;
    uint8_t best_y = ry;
    bool found_destination = false;
    for(uint8_t i = 0; i < steps; i++) {
        static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        uint8_t pick = fr_rand_u8(game, 4);
        int16_t nx_i = (int16_t)rx + dirs[pick][0];
        int16_t ny_i = (int16_t)ry + dirs[pick][1];
        if(nx_i <= 0 || ny_i <= 0 || nx_i >= FR_MAP_W - 1 || ny_i >= FR_MAP_H - 1) continue;
        uint8_t nx = (uint8_t)nx_i;
        uint8_t ny = (uint8_t)ny_i;
        if(!fr_is_walkable(fr_get_terrain(game, nx, ny))) {
            if(!reveal_hidden || (game->tiles[ny][nx] & FR_TILE_HIDDEN_DOOR) == 0) continue;
            game->tiles[ny][nx] &= (uint8_t)~FR_TILE_HIDDEN_DOOR;
            fr_set_terrain(game, nx, ny, FR_TERR_DOOR_OPEN);
            fr_event_secret(game, nx, ny);
        }
        if(fr_blocking_item_at(game, nx, ny)) continue;
        rx = nx;
        ry = ny;
        if(fr_actor_at(game, nx, ny) == NULL && (rx != *x || ry != *y)) {
            best_x = rx;
            best_y = ry;
            found_destination = true;
        }
    }
    if(!found_destination) return false;
    *x = best_x;
    *y = best_y;
    return true;
}

static bool fr_phase_step_is_open(FrGame* game, int16_t nx_i, int16_t ny_i, bool reveal_hidden) {
    if(nx_i <= 0 || ny_i <= 0 || nx_i >= FR_MAP_W - 1 || ny_i >= FR_MAP_H - 1) return false;
    uint8_t nx = (uint8_t)nx_i;
    uint8_t ny = (uint8_t)ny_i;
    if(!fr_is_walkable(fr_get_terrain(game, nx, ny)) &&
       (!reveal_hidden || (game->tiles[ny][nx] & FR_TILE_HIDDEN_DOOR) == 0)) {
        return false;
    }
    return !fr_blocking_item_at(game, nx, ny);
}

static void fr_phase_enter_tile(FrGame* game, uint8_t x, uint8_t y, bool reveal_hidden) {
    if(reveal_hidden && (game->tiles[y][x] & FR_TILE_HIDDEN_DOOR) != 0) {
        game->tiles[y][x] &= (uint8_t)~FR_TILE_HIDDEN_DOOR;
        fr_set_terrain(game, x, y, FR_TERR_DOOR_OPEN);
        fr_event_secret(game, x, y);
    }
}

static uint8_t fr_tile_distance(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by) {
    return (uint8_t)(fr_abs_i8((int8_t)ax - (int8_t)bx) + fr_abs_i8((int8_t)ay - (int8_t)by));
}

bool fr_random_reachable_tile(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps) {
    return fr_random_reachable_tile_impl(game, x, y, steps, false);
}

bool fr_random_reachable_tile_reveal(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps) {
    return fr_random_reachable_tile_impl(game, x, y, steps, true);
}

bool fr_phase_shift_tile_reveal(FrGame* game, uint8_t* x, uint8_t* y, uint8_t steps) {
    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    uint8_t origin_x = *x;
    uint8_t origin_y = *y;
    uint8_t rx = origin_x;
    uint8_t ry = origin_y;
    uint8_t best_x = rx;
    uint8_t best_y = ry;
    bool found_destination = false;

    for(uint8_t i = 0; i < steps; i++) {
        uint8_t pick = 0;
        bool have_pick = false;
        if(fr_rand_u8(game, 100) < 10) {
            pick = fr_rand_u8(game, 4);
            have_pick = true;
        } else {
            uint8_t start = fr_rand_u8(game, 4);
            uint8_t best_dist = 0;
            for(uint8_t j = 0; j < 4; j++) {
                uint8_t dir = (uint8_t)((start + j) & 0x03u);
                int16_t nx_i = (int16_t)rx + dirs[dir][0];
                int16_t ny_i = (int16_t)ry + dirs[dir][1];
                if(!fr_phase_step_is_open(game, nx_i, ny_i, true)) continue;
                uint8_t dist = fr_tile_distance((uint8_t)nx_i, (uint8_t)ny_i, origin_x, origin_y);
                if(!have_pick || dist > best_dist) {
                    best_dist = dist;
                    pick = dir;
                    have_pick = true;
                }
            }
        }
        if(!have_pick) continue;

        int16_t nx_i = (int16_t)rx + dirs[pick][0];
        int16_t ny_i = (int16_t)ry + dirs[pick][1];
        if(!fr_phase_step_is_open(game, nx_i, ny_i, true)) continue;
        rx = (uint8_t)nx_i;
        ry = (uint8_t)ny_i;
        fr_phase_enter_tile(game, rx, ry, true);
        if(fr_actor_at(game, rx, ry) == NULL && (rx != origin_x || ry != origin_y)) {
            best_x = rx;
            best_y = ry;
            found_destination = true;
        }
    }

    if(!found_destination) return false;
    *x = best_x;
    *y = best_y;
    return true;
}
