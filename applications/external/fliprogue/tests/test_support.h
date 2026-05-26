#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_logic.h"
#include "ui_layout.h"

static uint8_t count_items_of_type(const FrGame* game, uint8_t type) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active && game->items[i].type == type) count++;
    }
    return count;
}

static uint8_t count_actors_of_type(const FrGame* game, uint8_t type) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active && game->actors[i].type == type) count++;
    }
    return count;
}

static uint8_t max_pack_size_of_type(const FrGame* game, uint8_t type) {
    uint8_t max = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        const FrActor* actor = &game->actors[i];
        if(!actor->active || actor->type != type) continue;
        uint8_t count = 1;
        if(actor->pack_id != 0) {
            count = 0;
            for(uint8_t j = 0; j < FR_MAX_ACTORS; j++) {
                const FrActor* other = &game->actors[j];
                if(other->active && other->type == type && other->pack_id == actor->pack_id) count++;
            }
        }
        if(count > max) max = count;
    }
    return max;
}

static uint8_t count_terrain(const FrGame* game, uint8_t terrain) {
    uint8_t count = 0;
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if(fr_get_terrain(game, x, y) == terrain) count++;
        }
    }
    return count;
}

static uint8_t count_log_phrases(const char* text) {
    uint8_t count = 0;
    for(uint8_t i = 0; text[i] != '\0'; i++) {
        if(text[i] == '.' || text[i] == '!' || text[i] == '?') count++;
    }
    return count;
}

static uint8_t count_substrings(const char* text, const char* needle) {
    uint8_t count = 0;
    size_t len = strlen(needle);
    if(len == 0) return 0;
    const char* cursor = text;
    while((cursor = strstr(cursor, needle)) != NULL) {
        count++;
        cursor += len;
    }
    return count;
}

static bool terrain_is_walkable_for_test(uint8_t terrain) {
    return terrain == FR_TERR_FLOOR || terrain == FR_TERR_DOOR_OPEN ||
           terrain == FR_TERR_DOOR_CLOSED || terrain == FR_TERR_GRASS ||
           terrain == FR_TERR_GRASS_TRAMPLED || terrain == FR_TERR_PUDDLE ||
           terrain == FR_TERR_SAND || terrain == FR_TERR_WATER || terrain == FR_TERR_STAIRS_DOWN ||
           terrain == FR_TERR_STAIRS_UP || terrain == FR_TERR_BUTTON || terrain == FR_TERR_ICE;
}

static bool terrain_maps_differ(const FrGame* a, const FrGame* b) {
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if(fr_get_terrain(a, x, y) != fr_get_terrain(b, x, y)) return true;
        }
    }
    return false;
}

static bool find_plain_floor(const FrGame* game, uint8_t* out_x, uint8_t* out_y) {
    for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
            if(fr_get_terrain(game, x, y) == FR_TERR_FLOOR) {
                *out_x = x;
                *out_y = y;
                return true;
            }
        }
    }
    return false;
}

static bool find_floor_next_to_wall(const FrGame* game, uint8_t* floor_x, uint8_t* floor_y, uint8_t* wall_x, uint8_t* wall_y) {
    const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR) continue;
            for(uint8_t i = 0; i < 4; i++) {
                uint8_t nx = (uint8_t)(x + dirs[i][0]);
                uint8_t ny = (uint8_t)(y + dirs[i][1]);
                if(fr_get_terrain(game, nx, ny) == FR_TERR_WALL) {
                    *floor_x = x;
                    *floor_y = y;
                    *wall_x = nx;
                    *wall_y = ny;
                    return true;
                }
            }
        }
    }
    return false;
}

static void assert_runtime_floor_equal(const FrGame* expected, const FrGame* actual) {
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            uint8_t expected_tile = expected->tiles[y][x] & (uint8_t)~FR_TILE_EXPLORED;
            uint8_t actual_tile = actual->tiles[y][x] & (uint8_t)~FR_TILE_EXPLORED;
            assert(expected_tile == actual_tile);
            if((expected->tiles[y][x] & FR_TILE_EXPLORED) != 0 &&
               (expected->tiles[y][x] & FR_TILE_TERRAIN_MASK) != FR_TERR_WALL) {
                assert((actual->tiles[y][x] & FR_TILE_EXPLORED) != 0);
            }
        }
    }
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        const FrActor* a = &expected->actors[i];
        const FrActor* b = &actual->actors[i];
        assert(a->active == b->active);
        if(!a->active) continue;
        assert(a->type == b->type);
        assert(a->x == b->x);
        assert(a->y == b->y);
        assert(a->hp == b->hp);
        assert(a->max_hp == b->max_hp);
        assert(a->dmg == b->dmg);
        assert(a->effects == b->effects);
        assert(memcmp(a->fx_timer, b->fx_timer, sizeof(a->fx_timer)) == 0);
        assert(a->flags == b->flags);
        assert(a->pack_id == b->pack_id);
        assert(a->cooldown == b->cooldown);
    }
    assert(memcmp(expected->items, actual->items, sizeof(expected->items)) == 0);
    assert(memcmp(expected->traps, actual->traps, sizeof(expected->traps)) == 0);
}

static void assert_doors_are_chokepoints(const FrGame* game) {
    uint8_t door_count = 0;
    for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_DOOR_CLOSED) continue;
            door_count++;
            uint8_t open_neighbors = 0;
            bool west = terrain_is_walkable_for_test(fr_get_terrain(game, (uint8_t)(x - 1), y));
            bool east = terrain_is_walkable_for_test(fr_get_terrain(game, (uint8_t)(x + 1), y));
            bool north = terrain_is_walkable_for_test(fr_get_terrain(game, x, (uint8_t)(y - 1)));
            bool south = terrain_is_walkable_for_test(fr_get_terrain(game, x, (uint8_t)(y + 1)));
            if(west) open_neighbors++;
            if(east) open_neighbors++;
            if(north) open_neighbors++;
            if(south) open_neighbors++;
            assert(open_neighbors == 2);
            assert((west && east && !north && !south) || (north && south && !west && !east));
        }
    }
    assert(door_count >= 2);
}

static void make_empty_test_room(FrGame* game) {
    memset(game, 0, sizeof(*game));
    game->mode = FR_MODE_PLAYING;
    game->floor = 1;
    game->player.class_id = FR_CLASS_WARRIOR;
    game->player.hp = 12;
    game->player.max_hp = 24;
    game->player.str = 6;
    game->player.dex = 3;
    game->player.wil = 3;
    game->player.hunger = 200;
    game->player.x = 5;
    game->player.y = 5;
    game->log[0] = '\0';

    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            fr_set_terrain(game, x, y, FR_TERR_WALL);
        }
    }

    for(uint8_t y = 4; y <= 6; y++) {
        for(uint8_t x = 4; x <= 8; x++) {
            fr_set_terrain(game, x, y, FR_TERR_FLOOR);
        }
    }
    fr_update_fov(game);
}
