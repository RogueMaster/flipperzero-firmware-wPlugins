#include "map_gen.h"

#include "actor_state.h"
#include "ai.h"
#include "equipment.h"
#include "floor_state.h"
#include "game_core.h"
#include "map_state.h"
#include "monster_defs.h"
#include "perception.h"
#include "placement.h"
#include "room_decorators.h"
#include "room_templates.h"
#include "special_floors.h"

#include <string.h>

static void fr_carve_room(FrGame* game, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for(uint8_t yy = y; yy < y + h && yy < FR_MAP_H - 1; yy++) {
        for(uint8_t xx = x; xx < x + w && xx < FR_MAP_W - 1; xx++) {
            fr_set_terrain(game, xx, yy, FR_TERR_FLOOR);
        }
    }
}

static void fr_carve_h(FrGame* game, uint8_t x1, uint8_t x2, uint8_t y) {
    uint8_t start = x1 < x2 ? x1 : x2;
    uint8_t end = x1 < x2 ? x2 : x1;
    for(uint8_t x = start; x <= end; x++) fr_set_terrain(game, x, y, FR_TERR_FLOOR);
}

static void fr_carve_v(FrGame* game, uint8_t x, uint8_t y1, uint8_t y2) {
    uint8_t start = y1 < y2 ? y1 : y2;
    uint8_t end = y1 < y2 ? y2 : y1;
    for(uint8_t y = start; y <= end; y++) fr_set_terrain(game, x, y, FR_TERR_FLOOR);
}

static uint8_t fr_room_center_x(const FrRoom* room) {
    return (uint8_t)(room->x + room->w / 2);
}

static uint8_t fr_room_center_y(const FrRoom* room) {
    return (uint8_t)(room->y + room->h / 2);
}

static bool fr_room_contains(const FrRoom* room, uint8_t x, uint8_t y) {
    return x >= room->x && x < room->x + room->w && y >= room->y && y < room->y + room->h;
}

static bool fr_in_any_room(const FrRoom* rooms, uint8_t room_count, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < room_count; i++) {
        if(fr_room_contains(&rooms[i], x, y)) return true;
    }
    return false;
}

static void fr_place_item(FrGame* game, uint8_t type, uint8_t subtype, uint8_t x, uint8_t y, uint8_t amount) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(!game->items[i].active) {
            game->items[i].active = true;
            game->items[i].type = type;
            game->items[i].subtype = subtype;
            game->items[i].x = x;
            game->items[i].y = y;
            game->items[i].amount = amount;
            return;
        }
    }
}

static void fr_place_item_near(
    FrGame* game,
    uint8_t type,
    uint8_t subtype,
    uint8_t x,
    uint8_t y,
    uint8_t amount) {
    if(fr_find_free_near(game, &x, &y)) fr_place_item(game, type, subtype, x, y, amount);
}

static FrActor* fr_spawn_actor_near(FrGame* game, uint8_t type, uint8_t x, uint8_t y) {
    if(!fr_find_free_near(game, &x, &y)) return NULL;
    return fr_spawn_actor(game, type, x, y);
}

static void fr_place_door_if_choke(
    FrGame* game,
    const FrRoom* rooms,
    uint8_t room_count,
    uint8_t x,
    uint8_t y) {
    if(!fr_in_bounds(x, y)) return;
    if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR) return;
    if(fr_in_any_room(rooms, room_count, x, y)) return;

    bool west = x > 0 && fr_is_walkable(fr_get_terrain(game, (uint8_t)(x - 1), y));
    bool east = x + 1 < FR_MAP_W && fr_is_walkable(fr_get_terrain(game, (uint8_t)(x + 1), y));
    bool north = y > 0 && fr_is_walkable(fr_get_terrain(game, x, (uint8_t)(y - 1)));
    bool south = y + 1 < FR_MAP_H && fr_is_walkable(fr_get_terrain(game, x, (uint8_t)(y + 1)));
    bool horizontal = west && east && !north && !south;
    bool vertical = north && south && !west && !east;
    if(!horizontal && !vertical) return;

    uint8_t room_sides = 0;
    if(horizontal) {
        if(fr_in_any_room(rooms, room_count, (uint8_t)(x - 1), y)) room_sides++;
        if(fr_in_any_room(rooms, room_count, (uint8_t)(x + 1), y)) room_sides++;
    } else {
        if(fr_in_any_room(rooms, room_count, x, (uint8_t)(y - 1))) room_sides++;
        if(fr_in_any_room(rooms, room_count, x, (uint8_t)(y + 1))) room_sides++;
    }
    if(room_sides == 1) fr_set_terrain(game, x, y, FR_TERR_DOOR_CLOSED);
}

static bool fr_adjacent_to_any_room(const FrRoom* rooms, uint8_t room_count, uint8_t x, uint8_t y) {
    if(fr_in_any_room(rooms, room_count, x, y)) return false;
    if(x > 0 && fr_in_any_room(rooms, room_count, (uint8_t)(x - 1), y)) return true;
    if(x + 1 < FR_MAP_W && fr_in_any_room(rooms, room_count, (uint8_t)(x + 1), y)) return true;
    if(y > 0 && fr_in_any_room(rooms, room_count, x, (uint8_t)(y - 1))) return true;
    if(y + 1 < FR_MAP_H && fr_in_any_room(rooms, room_count, x, (uint8_t)(y + 1))) return true;
    return false;
}

static void fr_connect_rooms(FrGame* game, const FrRoom* a, const FrRoom* b, bool horizontal_first) {
    uint8_t ax = fr_room_center_x(a);
    uint8_t ay = fr_room_center_y(a);
    uint8_t bx = fr_room_center_x(b);
    uint8_t by = fr_room_center_y(b);
    if(horizontal_first) {
        fr_carve_h(game, ax, bx, ay);
        fr_carve_v(game, bx, ay, by);
    } else {
        fr_carve_v(game, ax, ay, by);
        fr_carve_h(game, ax, bx, by);
    }
}

static void fr_place_corridor_doors(FrGame* game, const FrRoom* rooms, uint8_t room_count) {
    uint8_t placed = 0;
    for(uint8_t y = 1; y < FR_MAP_H - 1 && placed < 5; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1 && placed < 5; x++) {
            if(!fr_adjacent_to_any_room(rooms, room_count, x, y)) continue;
            uint8_t before = fr_get_terrain(game, x, y);
            fr_place_door_if_choke(game, rooms, room_count, x, y);
            if(before != fr_get_terrain(game, x, y)) placed++;
        }
    }
}

static bool fr_secret_area_clear(FrGame* game, int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if(x <= 0 || y <= 0 || x + w >= FR_MAP_W - 1 || y + h >= FR_MAP_H - 1) return false;
    for(uint8_t oy = 0; oy < h; oy++) {
        for(uint8_t ox = 0; ox < w; ox++) {
            uint8_t tx = (uint8_t)(x + ox);
            uint8_t ty = (uint8_t)(y + oy);
            if(fr_get_terrain(game, tx, ty) != FR_TERR_WALL) return false;
            if((game->tiles[ty][tx] & FR_TILE_HIDDEN_DOOR) != 0) return false;
        }
    }
    return true;
}

static void fr_secret_room_loot(FrGame* game, const FrRoom* room, uint8_t floor, bool allow_guard) {
    uint8_t cx = fr_room_center_x(room);
    uint8_t cy = fr_room_center_y(room);
    fr_place_item_near(game, FR_ITEM_GOLD, 0, cx, cy, (uint8_t)(8 + floor));
    uint8_t magic_type = fr_rand_u8(game, 2) == 0 ? FR_ITEM_POTION : FR_ITEM_SCROLL;
    uint8_t magic_subtype = magic_type == FR_ITEM_POTION ? (uint8_t)(1 + fr_rand_u8(game, FR_POTION_MAX - 1)) :
                                                           (uint8_t)(1 + fr_rand_u8(game, FR_SCROLL_MAX - 1));
    fr_place_item_near(
        game,
        magic_type,
        magic_subtype,
        (uint8_t)(cx + 1),
        cy,
        1);
    if(fr_rand_u8(game, 2) == 0) {
        uint8_t rare_type = fr_rand_u8(game, 2) == 0 ? FR_ITEM_WAND : FR_ITEM_TRINKET;
        uint8_t rare_subtype = rare_type == FR_ITEM_WAND ? (uint8_t)(1 + fr_rand_u8(game, FR_WAND_MAX - 1)) :
                                                           (uint8_t)(1 + fr_rand_u8(game, FR_TRINKET_MAX - 1));
        fr_place_item_near(
            game,
            rare_type,
            rare_subtype,
            cx,
            (uint8_t)(cy + 1),
            1);
    }
    if(allow_guard && floor > 3 && fr_rand_u8(game, 4) == 0) {
        uint8_t type = floor < 7 ? FR_MON_GOBLIN : (floor < 13 ? FR_MON_ARCHER : FR_MON_OGRE);
        FrActor* guard = fr_spawn_actor_near(game, type, (uint8_t)(cx - 1), cy);
        if(guard) fr_maybe_actor_starts_asleep(game, guard);
    }
}

static bool fr_can_turn_to_water(uint8_t terrain) {
    return terrain == FR_TERR_FLOOR || terrain == FR_TERR_GRASS ||
           terrain == FR_TERR_GRASS_TRAMPLED || terrain == FR_TERR_PUDDLE ||
           terrain == FR_TERR_SAND;
}

static void fr_set_secret_water_border(FrGame* game, uint8_t x, uint8_t y) {
    if(!fr_in_bounds(x, y)) return;
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(fr_can_turn_to_water(terrain)) fr_set_terrain(game, x, y, FR_TERR_PUDDLE);
}

static bool fr_add_secret_water_gate(FrGame* game, const FrRoom* base, uint8_t dir, uint8_t door_x, uint8_t door_y) {
    uint8_t xs[3] = {door_x, door_x, door_x};
    uint8_t ys[3] = {door_y, door_y, door_y};
    uint8_t count = 0;
    if(dir == 0) {
        count = 3;
        for(uint8_t i = 0; i < count; i++) xs[i] = (uint8_t)(base->x + i);
    } else if(dir == 1) {
        count = 3;
        for(uint8_t i = 0; i < count; i++) xs[i] = (uint8_t)(base->x + base->w - 1 - i);
    } else if(dir == 2) {
        count = 3;
        for(uint8_t i = 0; i < count; i++) ys[i] = (uint8_t)(base->y + i);
    } else {
        count = 3;
        for(uint8_t i = 0; i < count; i++) ys[i] = (uint8_t)(base->y + base->h - 1 - i);
    }

    for(uint8_t i = 0; i < count; i++) {
        if(!fr_can_turn_to_water(fr_get_terrain(game, xs[i], ys[i]))) return false;
    }
    for(uint8_t i = 0; i < count; i++) {
        fr_set_terrain(game, xs[i], ys[i], FR_TERR_WATER);
        fr_set_secret_water_border(game, (uint8_t)(xs[i] + 1), ys[i]);
        fr_set_secret_water_border(game, (uint8_t)(xs[i] - 1), ys[i]);
        fr_set_secret_water_border(game, xs[i], (uint8_t)(ys[i] + 1));
        fr_set_secret_water_border(game, xs[i], (uint8_t)(ys[i] - 1));
    }
    return true;
}

static bool fr_try_secret_room_from(FrGame* game, const FrRoom* base, uint8_t dir, uint8_t floor, bool water_gate) {
    uint8_t w = (uint8_t)(4 + fr_rand_u8(game, 3));
    uint8_t h = (uint8_t)(3 + fr_rand_u8(game, 3));
    FrRoom secret = {0};
    int16_t door_x = 0;
    int16_t door_y = 0;
    int16_t area_x = 0;
    int16_t area_y = 0;
    uint8_t area_w = 0;
    uint8_t area_h = 0;

    if(dir == 0) {
        door_x = (int16_t)base->x - 1;
        door_y = fr_room_center_y(base);
        secret = (FrRoom){(uint8_t)(door_x - w - 1), (uint8_t)(door_y - h / 2), w, h};
        area_x = secret.x;
        area_y = secret.y;
        area_w = (uint8_t)(w + 1);
        area_h = h;
    } else if(dir == 1) {
        door_x = (int16_t)base->x + base->w;
        door_y = fr_room_center_y(base);
        secret = (FrRoom){(uint8_t)(door_x + 2), (uint8_t)(door_y - h / 2), w, h};
        area_x = (int16_t)door_x + 1;
        area_y = secret.y;
        area_w = (uint8_t)(w + 1);
        area_h = h;
    } else if(dir == 2) {
        door_x = fr_room_center_x(base);
        door_y = (int16_t)base->y - 1;
        secret = (FrRoom){(uint8_t)(door_x - w / 2), (uint8_t)(door_y - h - 1), w, h};
        area_x = secret.x;
        area_y = secret.y;
        area_w = w;
        area_h = (uint8_t)(h + 1);
    } else {
        door_x = fr_room_center_x(base);
        door_y = (int16_t)base->y + base->h;
        secret = (FrRoom){(uint8_t)(door_x - w / 2), (uint8_t)(door_y + 2), w, h};
        area_x = secret.x;
        area_y = (int16_t)door_y + 1;
        area_w = w;
        area_h = (uint8_t)(h + 1);
    }

    if(door_x <= 0 || door_y <= 0 || door_x >= FR_MAP_W - 1 || door_y >= FR_MAP_H - 1) return false;
    if(fr_get_terrain(game, (uint8_t)door_x, (uint8_t)door_y) != FR_TERR_WALL) return false;
    if(!fr_secret_area_clear(game, area_x, area_y, area_w, area_h)) return false;

    fr_carve_room(game, secret.x, secret.y, secret.w, secret.h);
    if(dir == 0) {
        fr_carve_h(game, (uint8_t)(secret.x + secret.w - 1), (uint8_t)(door_x - 1), (uint8_t)door_y);
    } else if(dir == 1) {
        fr_carve_h(game, (uint8_t)(door_x + 1), secret.x, (uint8_t)door_y);
    } else if(dir == 2) {
        fr_carve_v(game, (uint8_t)door_x, (uint8_t)(secret.y + secret.h - 1), (uint8_t)(door_y - 1));
    } else {
        fr_carve_v(game, (uint8_t)door_x, (uint8_t)(door_y + 1), secret.y);
    }
    fr_mark_hidden_door(game, (uint8_t)door_x, (uint8_t)door_y);
    if(water_gate && !fr_add_secret_water_gate(game, base, dir, (uint8_t)door_x, (uint8_t)door_y)) return false;
    fr_secret_room_loot(game, &secret, floor, !water_gate);
    return true;
}

static bool fr_place_secret_room(FrGame* game, const FrRoom* rooms, uint8_t room_count, uint8_t floor) {
    if(room_count < 2) return false;
    uint8_t start_room = (uint8_t)(1 + fr_rand_u8(game, (uint8_t)(room_count - 1)));
    uint8_t start_dir = fr_rand_u8(game, 4);
    if(floor >= 4 && fr_rand_u8(game, 3) == 0) {
        for(uint8_t ri = 0; ri < room_count - 1; ri++) {
            uint8_t room_index = (uint8_t)(1 + ((start_room - 1 + ri) % (room_count - 1)));
            if(room_index == 4 || room_index == 5) continue;
            for(uint8_t di = 0; di < 4; di++) {
                if(fr_try_secret_room_from(game, &rooms[room_index], (uint8_t)((start_dir + di) % 4), floor, true)) {
                    return true;
                }
            }
        }
    }
    for(uint8_t ri = 0; ri < room_count - 1; ri++) {
        uint8_t room_index = (uint8_t)(1 + ((start_room - 1 + ri) % (room_count - 1)));
        for(uint8_t di = 0; di < 4; di++) {
            if(fr_try_secret_room_from(game, &rooms[room_index], (uint8_t)((start_dir + di) % 4), floor, false)) {
                return true;
            }
        }
    }
    return false;
}

static void fr_connect_room_template(
    FrGame* game,
    const FrRoom* rooms,
    uint8_t room_count,
    uint8_t template_id) {
    if(template_id == 1) {
        fr_connect_rooms(game, &rooms[0], &rooms[3], true);
        fr_connect_rooms(game, &rooms[3], &rooms[1], false);
        fr_connect_rooms(game, &rooms[3], &rooms[2], false);
        fr_connect_rooms(game, &rooms[3], &rooms[4], true);
        fr_connect_rooms(game, &rooms[3], &rooms[5], true);
    } else if(template_id == 2) {
        fr_connect_rooms(game, &rooms[0], &rooms[1], true);
        fr_connect_rooms(game, &rooms[0], &rooms[2], true);
        fr_connect_rooms(game, &rooms[1], &rooms[3], false);
        fr_connect_rooms(game, &rooms[2], &rooms[3], false);
        fr_connect_rooms(game, &rooms[3], &rooms[4], true);
        fr_connect_rooms(game, &rooms[3], &rooms[5], true);
    } else if(template_id == 3) {
        fr_connect_rooms(game, &rooms[0], &rooms[1], true);
        fr_connect_rooms(game, &rooms[1], &rooms[3], false);
        fr_connect_rooms(game, &rooms[3], &rooms[4], true);
        fr_connect_rooms(game, &rooms[4], &rooms[5], false);
        fr_connect_rooms(game, &rooms[2], &rooms[3], true);
    } else if(template_id == FR_ROOM_TEMPLATE_MAZE) {
        fr_connect_rooms(game, &rooms[0], &rooms[1], true);
        fr_connect_rooms(game, &rooms[1], &rooms[2], false);
        fr_connect_rooms(game, &rooms[2], &rooms[3], true);
        fr_connect_rooms(game, &rooms[3], &rooms[4], false);
        fr_connect_rooms(game, &rooms[4], &rooms[5], true);
        fr_connect_rooms(game, &rooms[2], &rooms[6], false);
    } else {
        uint8_t topology = fr_rand_u8(game, 3);
        if(topology == 0) {
            fr_connect_rooms(game, &rooms[0], &rooms[1], true);
            fr_connect_rooms(game, &rooms[1], &rooms[3], false);
            fr_connect_rooms(game, &rooms[3], &rooms[5], true);
            fr_connect_rooms(game, &rooms[0], &rooms[2], false);
            fr_connect_rooms(game, &rooms[2], &rooms[4], true);
        } else if(topology == 1) {
            fr_connect_rooms(game, &rooms[0], &rooms[2], true);
            fr_connect_rooms(game, &rooms[2], &rooms[3], false);
            fr_connect_rooms(game, &rooms[3], &rooms[4], true);
            fr_connect_rooms(game, &rooms[3], &rooms[5], true);
            fr_connect_rooms(game, &rooms[1], &rooms[4], false);
        } else {
            fr_connect_rooms(game, &rooms[0], &rooms[3], true);
            fr_connect_rooms(game, &rooms[3], &rooms[1], false);
            fr_connect_rooms(game, &rooms[3], &rooms[2], false);
            fr_connect_rooms(game, &rooms[1], &rooms[4], true);
            fr_connect_rooms(game, &rooms[2], &rooms[5], true);
        }
    }

    if(room_count > 6) {
        fr_connect_rooms(game, &rooms[6], &rooms[3], (fr_rand(game) & 1u) != 0);
        fr_connect_rooms(game, &rooms[6], &rooms[fr_rand_u8(game, 6)], (fr_rand(game) & 1u) != 0);
    }
}

void fr_generate_floor(FrGame* game, uint8_t floor) {
    uint32_t run_seed = game->run_seed ? game->run_seed : game->seed;
    game->run_seed = run_seed ? run_seed : 1u;
    game->seed = game->run_seed ^ (uint32_t)floor * 0x9E3779B9u;
    game->mode = FR_MODE_PLAYING;
    game->floor = floor;
    game->death_cause = FR_DEATH_NONE;
    game->last_event = FR_EVENT_NONE;
    uint8_t old_suppress = game->suppress_deltas;
    game->suppress_deltas = 1;
    memset(game->tiles, 0, sizeof(game->tiles));
    memset(game->actors, 0, sizeof(game->actors));
    memset(game->items, 0, sizeof(game->items));
    memset(game->traps, 0, sizeof(game->traps));

    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(game, x, y, FR_TERR_WALL);
    }

    FrRoom rooms[FR_ROOM_MAX];
    uint8_t room_count = 0;
    uint8_t template_id = 0;
    uint8_t special_type = fr_special_floor_type(run_seed, floor);
    fr_room_template_build(game, floor, rooms, &room_count, &template_id);

    for(uint8_t i = 0; i < room_count; i++) fr_carve_room(game, rooms[i].x, rooms[i].y, rooms[i].w, rooms[i].h);

    fr_connect_room_template(game, rooms, room_count, template_id);
    if(fr_rand_u8(game, 4) != 0) fr_place_secret_room(game, rooms, room_count, floor);
    fr_place_corridor_doors(game, rooms, room_count);

    uint8_t up_x = (uint8_t)(rooms[0].x + 1 + fr_rand_u8(game, (uint8_t)(rooms[0].w - 2)));
    uint8_t up_y = (uint8_t)(rooms[0].y + 1 + fr_rand_u8(game, (uint8_t)(rooms[0].h - 2)));
    fr_set_terrain(game, up_x, up_y, FR_TERR_STAIRS_UP);
    game->player.x = up_x;
    game->player.y = up_y;

    FrRoom* far_room = &rooms[(fr_rand(game) & 1u) ? 4 : 5];
    uint8_t down_x = (uint8_t)(far_room->x + far_room->w - 2 - fr_rand_u8(game, 3));
    uint8_t down_y = (uint8_t)(far_room->y + far_room->h - 2 - fr_rand_u8(game, 3));
    if(floor < FR_MAX_FLOORS) fr_set_terrain(game, down_x, down_y, FR_TERR_STAIRS_DOWN);

    fr_apply_room_decorators(game, rooms, room_count, floor, template_id, special_type);

    uint8_t tier = fr_monster_tier_for_floor(floor);
    uint8_t monster_count = (uint8_t)(5 + tier + fr_rand_u8(game, 4));
    for(uint8_t i = 0; i < monster_count; i++) {
        uint8_t room_index = (uint8_t)(1 + fr_rand_u8(game, (uint8_t)(room_count - 1)));
        uint8_t type = fr_monster_from_tier(tier, fr_rand_u8(game, 8));
        uint8_t mx = (uint8_t)(rooms[room_index].x + 1 + fr_rand_u8(game, (uint8_t)(rooms[room_index].w - 2)));
        uint8_t my = (uint8_t)(rooms[room_index].y + 1 + fr_rand_u8(game, (uint8_t)(rooms[room_index].h - 2)));
        uint8_t pack_id = (uint8_t)(i + 1);
        FrActor* leader = fr_spawn_actor_near(game, type, mx, my);
        if(leader) {
            leader->pack_id = pack_id;
            fr_maybe_actor_starts_asleep(game, leader);
        }
        bool can_pack = fr_monster_can_pack(type, floor);
        uint8_t pack_roll = fr_monster_pack_roll(type);
        if(can_pack && fr_rand_u8(game, pack_roll) == 0) {
            uint8_t extra_count =
                fr_monster_pack_extra_count(type, fr_rand_u8(game, 3), fr_rand_u8(game, 2));
            for(uint8_t extra = 0; extra < extra_count; extra++) {
                uint8_t ox = extra == 0 ? 1 : (extra == 1 ? 0 : 1);
                uint8_t oy = extra == 0 ? 0 : (extra == 1 ? 1 : 1);
                FrActor* pack = fr_spawn_actor_near(game, type, (uint8_t)(mx + ox), (uint8_t)(my + oy));
                if(pack) {
                    pack->pack_id = pack_id;
                    fr_maybe_actor_starts_asleep(game, pack);
                }
            }
        }
    }

    fr_place_item_near(game, FR_ITEM_POTION, FR_POTION_HEALING, (uint8_t)(rooms[1].x + 2), (uint8_t)(rooms[1].y + 2), 1);
    uint8_t food_roll = fr_rand_u8(game, 10);
    bool place_food = floor <= 6 || (floor <= 12 ? food_roll < 6 : food_roll < 3);
    if(place_food) fr_place_item_near(game, FR_ITEM_FOOD, 0, fr_room_center_x(&rooms[1]), fr_room_center_y(&rooms[1]), 1);
    if(fr_rand_u8(game, 3) != 0) {
        fr_place_item_near(game, FR_ITEM_SCROLL, (uint8_t)(FR_SCROLL_IDENTIFY + fr_rand_u8(game, FR_SCROLL_MAX - 1)), (uint8_t)(rooms[3].x + 2), (uint8_t)(rooms[3].y + 2), 1);
    }
    if(fr_rand_u8(game, 2) == 0) fr_place_item_near(game, FR_ITEM_POTION, (uint8_t)(1 + fr_rand_u8(game, FR_POTION_MAX - 1)), fr_room_center_x(&rooms[2]), fr_room_center_y(&rooms[2]), 1);
    if(fr_rand_u8(game, 5) == 0) {
        uint8_t wand = (uint8_t)(1 + fr_rand_u8(game, FR_WAND_MAX - 1));
        uint8_t charges = (uint8_t)(1 + fr_rand_u8(game, fr_wand_max_charges(wand)));
        fr_place_item_near(game, FR_ITEM_WAND, wand, fr_room_center_x(&rooms[4]), fr_room_center_y(&rooms[4]), charges);
    }
    if(fr_rand_u8(game, 3) != 0) {
        fr_place_item_near(
            game,
            FR_ITEM_THROWABLE,
            fr_rand_u8(game, 2) == 0 ? FR_THROW_STONE : FR_THROW_DART,
            fr_room_center_x(&rooms[2]),
            (uint8_t)(fr_room_center_y(&rooms[2]) + 1),
            (uint8_t)(3 + fr_rand_u8(game, 6)));
    }
    if(fr_rand_u8(game, 6) == 0) {
        fr_place_item_near(
            game,
            FR_ITEM_TRINKET,
            (uint8_t)(1 + fr_rand_u8(game, FR_TRINKET_MAX - 1)),
            fr_room_center_x(&rooms[4]),
            (uint8_t)(fr_room_center_y(&rooms[4]) + 1),
            1);
    }
    if(fr_rand_u8(game, 3) != 0) fr_place_item_near(game, FR_ITEM_GOLD, 0, down_x, down_y, (uint8_t)(4 + floor));
    if(game->player.class_id == FR_CLASS_RANGER) {
        uint8_t arrow_roll = fr_rand_u8(game, 10);
        bool place_arrows = floor <= 6 ? arrow_roll < 8 : (floor <= 12 ? arrow_roll < 4 : arrow_roll < 2);
        if(place_arrows) {
            fr_place_item_near(
                game,
                FR_ITEM_ARROWS,
                0,
                fr_room_center_x(&rooms[2]),
                fr_room_center_y(&rooms[2]),
                floor <= 6 ? 4 : 3);
        }
    }

    if(floor == FR_MAX_FLOORS) {
        uint8_t orb_x = fr_room_center_x(far_room);
        uint8_t orb_y = fr_room_center_y(far_room);
        fr_place_item_near(game, FR_ITEM_ORB, 0, orb_x, orb_y, 1);
        uint8_t sand_x = (uint8_t)(orb_x > 2 ? orb_x - 2 : orb_x);
        if(fr_actor_at(game, sand_x, orb_y) == NULL) fr_set_terrain(game, sand_x, orb_y, FR_TERR_SAND);
    }

    fr_update_fov(game);
    game->suppress_deltas = old_suppress;
    if(!game->suppress_floor_state_save) {
        fr_init_floor_state(game, floor, up_x, up_y, floor < FR_MAX_FLOORS ? down_x : 0, floor < FR_MAX_FLOORS ? down_y : 0);
    }
}
