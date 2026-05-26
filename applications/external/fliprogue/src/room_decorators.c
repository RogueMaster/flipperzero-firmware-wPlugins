#include "room_decorators.h"

#include "chest_actions.h"
#include "game_core.h"
#include "grate_actions.h"
#include "map_state.h"
#include "placement.h"
#include "shrine_actions.h"
#include "special_floors.h"

#include <stddef.h>

static uint8_t fr_pick_decorator_room(FrGame* game, uint8_t room_count, bool* used) {
    if(room_count <= 1) return 0;
    uint8_t start = (uint8_t)(1 + fr_rand_u8(game, (uint8_t)(room_count - 1)));
    for(uint8_t i = 0; i < room_count - 1; i++) {
        uint8_t room = (uint8_t)(1 + ((start - 1 + i) % (room_count - 1)));
        if(!used[room]) {
            used[room] = true;
            return room;
        }
    }
    return start;
}

static uint8_t fr_pick_flood_room(FrGame* game, uint8_t room_count, bool* used) {
    if(room_count <= 1) return 0;
    uint8_t start = (uint8_t)(1 + fr_rand_u8(game, (uint8_t)(room_count - 1)));
    for(uint8_t i = 0; i < room_count - 1; i++) {
        uint8_t room = (uint8_t)(1 + ((start - 1 + i) % (room_count - 1)));
        if(room == 4 || room == 5) continue;
        if(!used[room]) {
            used[room] = true;
            return room;
        }
    }
    return room_count > 6 && !used[6] ? 6 : 3;
}

static void fr_place_puddle_patch(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_find_floor_near(game, &x, &y, 4)) return;
    fr_set_terrain(game, x, y, FR_TERR_PUDDLE);
    uint8_t target = (uint8_t)(1 + fr_rand_u8(game, 5));
    for(uint8_t i = 1; i < target; i++) {
        static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        uint8_t start = fr_rand_u8(game, 4);
        bool placed = false;
        for(uint8_t tries = 0; tries < 4; tries++) {
            uint8_t pick = (uint8_t)((start + tries) % 4);
            int16_t nx_i = (int16_t)x + dirs[pick][0];
            int16_t ny_i = (int16_t)y + dirs[pick][1];
            if(nx_i <= 0 || ny_i <= 0 || nx_i >= FR_MAP_W - 1 || ny_i >= FR_MAP_H - 1) continue;
            uint8_t nx = (uint8_t)nx_i;
            uint8_t ny = (uint8_t)ny_i;
            uint8_t terrain = fr_get_terrain(game, nx, ny);
            if(terrain == FR_TERR_FLOOR || terrain == FR_TERR_PUDDLE) {
                if(terrain == FR_TERR_FLOOR) {
                    fr_set_terrain(game, nx, ny, FR_TERR_PUDDLE);
                    placed = true;
                }
                x = nx;
                y = ny;
                break;
            }
        }
        if(!placed && fr_rand_u8(game, 2) == 0) break;
    }
}

static void fr_place_trap_near(FrGame* game, const FrRoom* room, uint8_t type) {
    uint8_t x = 0;
    uint8_t y = 0;
    if(type == FR_TRAP_ARROW || type == FR_TRAP_FIRE) {
        uint8_t wall_x = 0;
        uint8_t wall_y = 0;
        if(fr_find_wall_opposed_floor_in_room(game, room, &x, &y, &wall_x, &wall_y)) {
            fr_place_trap(game, x, y, type);
        }
        return;
    }
    if(fr_find_room_center_floor(game, room, &x, &y) && fr_find_free_near(game, &x, &y)) {
        fr_place_trap(game, x, y, type);
    }
}

static void fr_decorate_grass(FrGame* game, const FrRoom* room) {
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t gx = (uint8_t)(room->x + 1 + fr_rand_u8(game, (uint8_t)(room->w - 2)));
        uint8_t gy = (uint8_t)(room->y + 1 + fr_rand_u8(game, (uint8_t)(room->h - 2)));
        if(fr_get_terrain(game, gx, gy) == FR_TERR_FLOOR) fr_set_terrain(game, gx, gy, FR_TERR_GRASS);
    }
}

static void fr_decorate_puddle(FrGame* game, const FrRoom* room) {
    uint8_t x = (uint8_t)(room->x + 1 + fr_rand_u8(game, (uint8_t)(room->w - 2)));
    uint8_t y = (uint8_t)(room->y + 1 + fr_rand_u8(game, (uint8_t)(room->h - 2)));
    fr_place_puddle_patch(game, x, y);
}

static void fr_decorate_sand(FrGame* game, const FrRoom* room) {
    uint8_t x = (uint8_t)(room->x + 1 + fr_rand_u8(game, (uint8_t)(room->w - 2)));
    uint8_t y = (uint8_t)(room->y + 1 + fr_rand_u8(game, (uint8_t)(room->h - 2)));
    if(!fr_find_floor_near(game, &x, &y, 4)) return;
    uint8_t target = (uint8_t)(3 + fr_rand_u8(game, 5));
    fr_set_terrain(game, x, y, FR_TERR_SAND);
    uint8_t placed = 1;
    for(uint8_t i = 1; i < target; i++) {
        static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        uint8_t start = fr_rand_u8(game, 4);
        for(uint8_t tries = 0; tries < 4; tries++) {
            uint8_t pick = (uint8_t)((start + tries) % 4);
            int16_t nx_i = (int16_t)x + dirs[pick][0];
            int16_t ny_i = (int16_t)y + dirs[pick][1];
            if(nx_i <= 0 || ny_i <= 0 || nx_i >= FR_MAP_W - 1 || ny_i >= FR_MAP_H - 1) continue;
            uint8_t nx = (uint8_t)nx_i;
            uint8_t ny = (uint8_t)ny_i;
            uint8_t terrain = fr_get_terrain(game, nx, ny);
            if(terrain != FR_TERR_FLOOR && terrain != FR_TERR_SAND) continue;
            if(terrain == FR_TERR_FLOOR) {
                fr_set_terrain(game, nx, ny, FR_TERR_SAND);
                placed++;
            }
            x = nx;
            y = ny;
            break;
        }
    }
    for(uint8_t y2 = (uint8_t)(room->y + 1); y2 + 1 < room->y + room->h && placed < target; y2++) {
        for(uint8_t x2 = (uint8_t)(room->x + 1); x2 + 1 < room->x + room->w && placed < target; x2++) {
            if(fr_get_terrain(game, x2, y2) != FR_TERR_FLOOR) continue;
            bool near_sand = false;
            static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for(uint8_t i = 0; i < 4; i++) {
                uint8_t nx = (uint8_t)(x2 + dirs[i][0]);
                uint8_t ny = (uint8_t)(y2 + dirs[i][1]);
                if(fr_get_terrain(game, nx, ny) == FR_TERR_SAND) near_sand = true;
            }
            if(!near_sand) continue;
            fr_set_terrain(game, x2, y2, FR_TERR_SAND);
            placed++;
        }
    }
}

static void fr_decorate_chest(FrGame* game, const FrRoom* room, bool mimic) {
    uint8_t x = 0;
    uint8_t y = 0;
    if(!fr_find_room_center_floor(game, room, &x, &y)) return;
    if(!fr_find_free_near(game, &x, &y)) return;
    fr_place_chest(game, x, y, mimic);
}

static void fr_decorate_shrine(FrGame* game, const FrRoom* room) {
    uint8_t x = 0;
    uint8_t y = 0;
    if(!fr_find_room_center_floor(game, room, &x, &y)) return;
    if(!fr_find_free_near(game, &x, &y)) return;
    fr_place_shrine(game, x, y);
}

static bool fr_pocket_area_is_solid(const FrGame* game, uint8_t x0, uint8_t y0, uint8_t w, uint8_t h) {
    if(x0 == 0 || y0 == 0 || x0 + w >= FR_MAP_W || y0 + h >= FR_MAP_H) return false;
    for(uint8_t y = y0; y < y0 + h; y++) {
        for(uint8_t x = x0; x < x0 + w; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_WALL) return false;
        }
    }
    return true;
}

static bool fr_decorate_grate_reward(FrGame* game, const FrRoom* room, const FrRoom* key_room) {
    if(!fr_room_is_large_enough(room, 5, 5)) return false;
    uint8_t cy = (uint8_t)(room->y + room->h / 2);
    if(cy == 0 || cy + 1 >= FR_MAP_H) return false;

    uint8_t grate_x = 0;
    uint8_t grate_y = cy;
    uint8_t pocket_x = 0;
    uint8_t pocket_y = (uint8_t)(cy - 1);
    if(room->x + room->w + 3 < FR_MAP_W &&
       fr_pocket_area_is_solid(game, (uint8_t)(room->x + room->w + 1), pocket_y, 3, 3)) {
        grate_x = (uint8_t)(room->x + room->w);
        pocket_x = (uint8_t)(grate_x + 1);
    } else if(room->x > 4 && fr_pocket_area_is_solid(game, (uint8_t)(room->x - 4), pocket_y, 3, 3)) {
        grate_x = (uint8_t)(room->x - 1);
        pocket_x = (uint8_t)(room->x - 4);
    } else {
        return false;
    }

    for(uint8_t y = pocket_y; y < pocket_y + 3; y++) {
        for(uint8_t x = pocket_x; x < pocket_x + 3; x++) fr_set_terrain(game, x, y, FR_TERR_FLOOR);
    }
    if(!fr_place_grate(game, grate_x, grate_y)) return false;
    fr_place_chest(game, (uint8_t)(pocket_x + 1), cy, false);
    if(fr_rand_u8(game, 2) == 0) {
        uint8_t guard_y = fr_rand_u8(game, 2) == 0 ? pocket_y : (uint8_t)(pocket_y + 2);
        if(fr_actor_at(game, (uint8_t)(pocket_x + 1), guard_y) == NULL) {
            fr_spawn_actor(game, fr_rand_u8(game, 2) == 0 ? FR_MON_RAT : FR_MON_GOBLIN, (uint8_t)(pocket_x + 1), guard_y);
        }
    }

    uint8_t sx = 0;
    uint8_t sy = 0;
    if(!fr_find_room_center_floor(game, key_room, &sx, &sy)) return true;
    if(!fr_find_free_near(game, &sx, &sy)) return true;
    if(((game->run_seed + game->floor + room->x) & 1u) == 0) {
        fr_place_key(game, sx, sy);
    } else {
        fr_place_button(game, sx, sy);
    }
    return true;
}

static void fr_maybe_place_lurker(FrGame* game, const FrRoom* room, uint8_t floor) {
    uint32_t roll = game->run_seed ^ ((uint32_t)floor * 37u) ^ ((uint32_t)room->x * 3u) ^ room->y;
    if(floor < 3 || (roll % 17u) != 0) return;
    for(uint8_t y = (uint8_t)(room->y + 1); y + 1 < room->y + room->h; y++) {
        for(uint8_t x = (uint8_t)(room->x + 1); x + 1 < room->x + room->w; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_GRASS && fr_get_terrain(game, x, y) != FR_TERR_PUDDLE) {
                continue;
            }
            if(fr_actor_at(game, x, y) != NULL) continue;
            FrActor* lurker = fr_spawn_actor(game, FR_MON_LURKER, x, y);
            if(lurker) return;
        }
    }
}

static void fr_set_shallow_border_if_floor(FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(terrain == FR_TERR_FLOOR || terrain == FR_TERR_GRASS || terrain == FR_TERR_GRASS_TRAMPLED ||
       terrain == FR_TERR_SAND) {
        fr_set_terrain(game, x, y, FR_TERR_PUDDLE);
    }
}

static bool fr_can_flood_center_tile(uint8_t terrain) {
    return terrain == FR_TERR_FLOOR || terrain == FR_TERR_GRASS ||
           terrain == FR_TERR_GRASS_TRAMPLED || terrain == FR_TERR_PUDDLE ||
           terrain == FR_TERR_SAND;
}

static bool fr_can_border_flooded_pool(uint8_t terrain) {
    return terrain == FR_TERR_WALL || terrain == FR_TERR_DOOR_CLOSED ||
           terrain == FR_TERR_DOOR_OPEN || fr_can_flood_center_tile(terrain);
}

static bool fr_decorate_flooded_pool(FrGame* game, const FrRoom* room) {
    if(!fr_room_is_large_enough(room, 6, 5)) return false;
    uint8_t x0 = (uint8_t)(room->x + 2);
    uint8_t y0 = (uint8_t)(room->y + 2);
    uint8_t x1 = (uint8_t)(room->x + room->w - 3);
    uint8_t y1 = (uint8_t)(room->y + room->h - 3);
    if(x0 > x1 || y0 > y1) return false;

    for(uint8_t y = y0; y <= y1; y++) {
        for(uint8_t x = x0; x <= x1; x++) {
            if(!fr_can_flood_center_tile(fr_get_terrain(game, x, y))) return false;
        }
    }

    for(uint8_t y = (uint8_t)(y0 - 1); y <= y1 + 1; y++) {
        for(uint8_t x = (uint8_t)(x0 - 1); x <= x1 + 1; x++) {
            if(x >= x0 && x <= x1 && y >= y0 && y <= y1) continue;
            if(x == 0 || y == 0 || x >= FR_MAP_W - 1 || y >= FR_MAP_H - 1) return false;
            if(!fr_can_border_flooded_pool(fr_get_terrain(game, x, y))) return false;
        }
    }

    for(uint8_t y = y0; y <= y1; y++) {
        for(uint8_t x = x0; x <= x1; x++) {
            fr_set_terrain(game, x, y, FR_TERR_WATER);
        }
    }

    for(uint8_t y = (uint8_t)(y0 - 1); y <= y1 + 1; y++) {
        for(uint8_t x = (uint8_t)(x0 - 1); x <= x1 + 1; x++) {
            if(x >= x0 && x <= x1 && y >= y0 && y <= y1) continue;
            if(x == 0 || y == 0 || x >= FR_MAP_W - 1 || y >= FR_MAP_H - 1) continue;
            uint8_t terrain = fr_get_terrain(game, x, y);
            if(terrain == FR_TERR_WALL || terrain == FR_TERR_DOOR_CLOSED || terrain == FR_TERR_DOOR_OPEN) continue;
            fr_set_shallow_border_if_floor(game, x, y);
        }
    }
    return true;
}

static bool fr_find_eel_spawn_tile(
    FrGame* game,
    const FrRoom* room,
    uint8_t start,
    uint8_t* out_x,
    uint8_t* out_y) {
    uint8_t inner_w = (uint8_t)(room->w - 2);
    uint8_t inner_h = (uint8_t)(room->h - 2);
    uint16_t slots = (uint16_t)inner_w * inner_h;
    for(uint16_t i = 0; i < slots; i++) {
        uint16_t pick = (uint16_t)((start + i) % slots);
        uint8_t x = (uint8_t)(room->x + 1 + (pick % inner_w));
        uint8_t y = (uint8_t)(room->y + 1 + (pick / inner_w));
        uint8_t terrain = fr_get_terrain(game, x, y);
        if(terrain != FR_TERR_WATER && terrain != FR_TERR_PUDDLE) continue;
        if(game->player.x == x && game->player.y == y) continue;
        if(fr_actor_at(game, x, y) != NULL) continue;
        *out_x = x;
        *out_y = y;
        return true;
    }
    return false;
}

static void fr_maybe_spawn_eel_pack(FrGame* game, const FrRoom* room) {
    if(fr_rand_u8(game, 2) != 0) return;
    uint8_t count = (uint8_t)(3 + fr_rand_u8(game, 2));
    uint8_t pack_id = (uint8_t)(180 + fr_rand_u8(game, 60));
    uint8_t placed = 0;
    uint8_t start = fr_rand_u8(game, (uint8_t)((room->w - 2) * (room->h - 2)));
    for(uint8_t i = 0; i < count; i++) {
        uint8_t x = 0;
        uint8_t y = 0;
        if(!fr_find_eel_spawn_tile(game, room, (uint8_t)(start + i * 3), &x, &y)) break;
        FrActor* eel = fr_spawn_actor(game, FR_MON_EEL, x, y);
        if(!eel) break;
        eel->pack_id = pack_id;
        placed++;
    }
    if(placed < 3) {
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            if(game->actors[i].active && game->actors[i].type == FR_MON_EEL &&
               game->actors[i].pack_id == pack_id) {
                game->actors[i].active = false;
            }
        }
    }
}

void fr_apply_room_decorators(
    FrGame* game,
    const FrRoom* rooms,
    uint8_t room_count,
    uint8_t floor,
    uint8_t template_id,
    uint8_t special_type) {
    (void)template_id;
    bool used[FR_ROOM_MAX] = {false};
    used[0] = true;

    uint8_t grass_room = fr_pick_decorator_room(game, room_count, used);
    fr_decorate_grass(game, &rooms[grass_room]);

    uint8_t puddle_room = fr_pick_decorator_room(game, room_count, used);
    fr_decorate_puddle(game, &rooms[puddle_room]);

    uint8_t sand_room = room_count > 3 && !used[3] ? 3 : fr_pick_decorator_room(game, room_count, used);
    used[sand_room] = true;
    fr_decorate_sand(game, &rooms[sand_room]);

    if(floor >= 4 && (special_type == FR_SPECIAL_FLOOR_FLOODED || fr_rand_u8(game, 3) == 0)) {
        uint8_t flood_room = fr_pick_flood_room(game, room_count, used);
        if(fr_decorate_flooded_pool(game, &rooms[flood_room])) fr_maybe_spawn_eel_pack(game, &rooms[flood_room]);
    }

    if(floor >= 2 && room_count > 4) {
        uint8_t chest_room = fr_pick_decorator_room(game, room_count, used);
        fr_decorate_chest(game, &rooms[chest_room], floor >= 5 && fr_rand_u8(game, 8) == 0);
    }

    if(floor >= 3 && room_count > 5) {
        uint8_t shrine_room = fr_pick_decorator_room(game, room_count, used);
        fr_decorate_shrine(game, &rooms[shrine_room]);
        if(special_type == FR_SPECIAL_FLOOR_SHRINE && room_count > 6) {
            fr_decorate_shrine(game, &rooms[6]);
        }
    }

    if(floor >= 5 && room_count > 4 && fr_rand_u8(game, 4) == 0) {
        uint8_t reward_room = room_count > 6 ? 6 : (uint8_t)(room_count - 1);
        if(!used[reward_room]) {
            used[reward_room] = true;
            fr_decorate_grate_reward(game, &rooms[reward_room], &rooms[1]);
        }
    }

    fr_place_trap_near(game, &rooms[1], FR_TRAP_ARROW);
    if(room_count > 2 && floor > 3) fr_place_trap_near(game, &rooms[2], FR_TRAP_SNARE);
    if(floor > 6) fr_place_trap_near(game, &rooms[5], FR_TRAP_FIRE);
    if(special_type == FR_SPECIAL_FLOOR_TRAPWORKS || special_type == FR_SPECIAL_FLOOR_MAZE) {
        if(room_count > 3) fr_place_trap_near(game, &rooms[3], FR_TRAP_ARROW);
        if(room_count > 4) fr_place_trap_near(game, &rooms[4], FR_TRAP_SNARE);
    }
    if(special_type == FR_SPECIAL_FLOOR_NEST && floor >= 7 && room_count > 2) {
        uint8_t pack_id = (uint8_t)(140 + floor);
        for(uint8_t i = 0; i < 3; i++) {
            uint8_t x = (uint8_t)(rooms[2].x + 1 + i);
            uint8_t y = (uint8_t)(rooms[2].y + rooms[2].h / 2);
            if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR || fr_actor_at(game, x, y) ||
               fr_trap_at(game, x, y) != NULL) {
                continue;
            }
            FrActor* bat = fr_spawn_actor(game, FR_MON_BAT, x, y);
            if(bat) bat->pack_id = pack_id;
        }
    }

    fr_maybe_place_lurker(game, &rooms[grass_room], floor);
    fr_maybe_place_lurker(game, &rooms[puddle_room], floor);
}
