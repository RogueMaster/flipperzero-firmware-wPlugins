#include "terrain_effects.h"

#include "actor_state.h"
#include "combat.h"
#include "game_core.h"
#include "hazards.h"
#include "ice_effects.h"
#include "map_state.h"
#include "placement.h"
#include "status_effects.h"

#include <string.h>

static uint16_t fr_cell_index(uint8_t x, uint8_t y) {
    return (uint16_t)y * FR_MAP_W + x;
}

static bool fr_cell_get(const uint8_t cells[FR_EXPLORED_BYTES], uint8_t x, uint8_t y) {
    uint16_t index = fr_cell_index(x, y);
    return (cells[index / 8] & (uint8_t)(1u << (index & 7u))) != 0;
}

static void fr_cell_set(uint8_t cells[FR_EXPLORED_BYTES], uint8_t x, uint8_t y) {
    uint16_t index = fr_cell_index(x, y);
    cells[index / 8] |= (uint8_t)(1u << (index & 7u));
}

static void fr_cell_clear(uint8_t cells[FR_EXPLORED_BYTES], uint8_t x, uint8_t y) {
    uint16_t index = fr_cell_index(x, y);
    cells[index / 8] &= (uint8_t)~(1u << (index & 7u));
}

bool fr_terrain_field_cell_get(const FrTerrainField* field, uint8_t x, uint8_t y) {
    return field && fr_cell_get(field->cells, x, y);
}

void fr_terrain_field_cell_set(FrTerrainField* field, uint8_t x, uint8_t y) {
    if(field) fr_cell_set(field->cells, x, y);
}

void fr_terrain_field_cell_clear(FrTerrainField* field, uint8_t x, uint8_t y) {
    if(field) fr_cell_clear(field->cells, x, y);
}

bool fr_terrain_field_has_any_cell(const FrTerrainField* field) {
    if(!field) return false;
    for(uint16_t i = 0; i < FR_EXPLORED_BYTES; i++) {
        if(field->cells[i] != 0) return true;
    }
    return false;
}

static bool fr_fire_can_occupy_at(const FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(fr_blocking_item_at(game, x, y)) return false;
    return terrain == FR_TERR_FLOOR || terrain == FR_TERR_GRASS || terrain == FR_TERR_GRASS_TRAMPLED;
}

FrTerrainField* fr_alloc_terrain_field(FrGame* game) {
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        if(!game->terrain_fields[i].active) return &game->terrain_fields[i];
    }
    uint8_t oldest = 0;
    for(uint8_t i = 1; i < FR_TERRAIN_FIELD_CAP; i++) {
        if(game->terrain_fields[i].age > game->terrain_fields[oldest].age) oldest = i;
    }
    game->terrain_fields[oldest].active = false;
    return &game->terrain_fields[oldest];
}

void fr_ignite_fire_field(FrGame* game, uint8_t floor, uint8_t x, uint8_t y, uint8_t radius) {
    FrTerrainField* field = fr_alloc_terrain_field(game);
    memset(field, 0, sizeof(*field));
    field->active = true;
    field->type = FR_TERRAIN_FIELD_FIRE;
    field->floor = floor;
    field->ttl = 5;
    int8_t r = (int8_t)radius;
    for(int8_t oy = -r; oy <= r; oy++) {
        for(int8_t ox = -r; ox <= r; ox++) {
            int16_t tx_i = (int16_t)x + ox;
            int16_t ty_i = (int16_t)y + oy;
            if(tx_i < 0 || ty_i < 0 || tx_i >= FR_MAP_W || ty_i >= FR_MAP_H) continue;
            uint8_t tx = (uint8_t)tx_i;
            uint8_t ty = (uint8_t)ty_i;
            if(!fr_fire_can_occupy_at(game, tx, ty)) continue;
            uint8_t terrain = fr_get_terrain(game, tx, ty);
            if(terrain == FR_TERR_GRASS) fr_set_terrain(game, tx, ty, FR_TERR_GRASS_TRAMPLED);
            fr_cell_set(field->cells, tx, ty);
        }
    }
}

static FrTerrainField* fr_find_fire_field_near(
    FrGame* game,
    uint8_t floor,
    uint8_t cx,
    uint8_t cy,
    uint8_t radius) {
    int8_t r = (int8_t)radius;
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        FrTerrainField* field = &game->terrain_fields[i];
        if(!field->active || field->type != FR_TERRAIN_FIELD_FIRE || field->floor != floor) continue;
        for(int8_t oy = -r; oy <= r; oy++) {
            for(int8_t ox = -r; ox <= r; ox++) {
                int16_t x_i = (int16_t)cx + ox;
                int16_t y_i = (int16_t)cy + oy;
                if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
                if(fr_cell_get(field->cells, (uint8_t)x_i, (uint8_t)y_i)) return field;
            }
        }
    }
    return NULL;
}

void fr_refresh_or_expand_fire_field(FrGame* game, uint8_t floor, uint8_t x, uint8_t y, uint8_t base_radius) {
    FrTerrainField* field = fr_find_fire_field_near(game, floor, x, y, (uint8_t)(base_radius + 1));
    uint8_t radius = field ? (uint8_t)(base_radius + 1) : base_radius;
    if(!field) {
        field = fr_alloc_terrain_field(game);
        memset(field, 0, sizeof(*field));
        field->active = true;
        field->type = FR_TERRAIN_FIELD_FIRE;
        field->floor = floor;
    }
    field->age = 0;
    field->ttl = 5;
    int8_t r = (int8_t)radius;
    for(int8_t oy = -r; oy <= r; oy++) {
        for(int8_t ox = -r; ox <= r; ox++) {
            int16_t tx_i = (int16_t)x + ox;
            int16_t ty_i = (int16_t)y + oy;
            if(tx_i < 0 || ty_i < 0 || tx_i >= FR_MAP_W || ty_i >= FR_MAP_H) continue;
            uint8_t tx = (uint8_t)tx_i;
            uint8_t ty = (uint8_t)ty_i;
            if(!fr_fire_can_occupy_at(game, tx, ty)) continue;
            if(fr_get_terrain(game, tx, ty) == FR_TERR_GRASS) {
                fr_set_terrain(game, tx, ty, FR_TERR_GRASS_TRAMPLED);
            }
            fr_cell_set(field->cells, tx, ty);
        }
    }
}

bool fr_terrain_fire_at(const FrGame* game, uint8_t floor, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        const FrTerrainField* field = &game->terrain_fields[i];
        if(!field->active || field->type != FR_TERRAIN_FIELD_FIRE || field->floor != floor) continue;
        if(fr_cell_get(field->cells, x, y)) return true;
    }
    return false;
}

uint8_t fr_active_terrain_field_count(const FrGame* game) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        if(game->terrain_fields[i].active) count++;
    }
    return count;
}

bool fr_fire_area_has_fire(const FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius) {
    int8_t r = (int8_t)radius;
    for(int8_t oy = -r; oy <= r; oy++) {
        for(int8_t ox = -r; ox <= r; ox++) {
            int16_t x_i = (int16_t)cx + ox;
            int16_t y_i = (int16_t)cy + oy;
            if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
            if(fr_terrain_fire_at(game, floor, (uint8_t)x_i, (uint8_t)y_i)) return true;
        }
    }
    return false;
}

bool fr_extinguish_fire_area(FrGame* game, uint8_t floor, uint8_t cx, uint8_t cy, uint8_t radius) {
    bool cleared = false;
    int8_t r = (int8_t)radius;
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        FrTerrainField* field = &game->terrain_fields[i];
        if(!field->active || field->type != FR_TERRAIN_FIELD_FIRE || field->floor != floor) continue;
        for(int8_t oy = -r; oy <= r; oy++) {
            for(int8_t ox = -r; ox <= r; ox++) {
                int16_t x_i = (int16_t)cx + ox;
                int16_t y_i = (int16_t)cy + oy;
                if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
                uint8_t x = (uint8_t)x_i;
                uint8_t y = (uint8_t)y_i;
                if(fr_cell_get(field->cells, x, y)) {
                    fr_cell_clear(field->cells, x, y);
                    cleared = true;
                }
            }
        }
        if(!fr_terrain_field_has_any_cell(field)) field->active = false;
    }
    return cleared;
}

static void fr_fire_damage_cell(FrGame* game, uint8_t floor, uint8_t x, uint8_t y) {
    if(floor != game->floor) return;
    if(game->player.x == x && game->player.y == y && game->player.hp > 0) {
        uint8_t turns = ((game->player.effects & FR_FX_BURNING) == 0 ||
                         game->player.fx_timer[FR_FX_BURNING_INDEX] < 4) ?
                            4 :
                            0;
        fr_apply_fire_to_player(game, fr_fire_damage_roll(game), turns);
        if(game->player.hp == 0) {
            fr_set_game_over(game, FR_DEATH_BURNED, "Burned.");
        }
    }
    FrActor* actor = fr_actor_at(game, x, y);
    if(actor) {
        fr_damage_actor_kind(game, actor, fr_fire_damage_roll(game), "Fire sears", FR_DAMAGE_DOT);
        if(actor->active &&
           ((actor->effects & FR_FX_BURNING) == 0 || actor->fx_timer[FR_FX_BURNING_INDEX] < 4)) {
            fr_apply_effect_to_actor(actor, FR_FX_BURNING, FR_FX_BURNING_INDEX, 4);
        }
    }
}

static void fr_tick_fire_field(FrGame* game, FrTerrainField* field) {
    uint8_t next_cells[FR_EXPLORED_BYTES];
    memcpy(next_cells, field->cells, sizeof(next_cells));
    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
        for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
            if(!fr_cell_get(field->cells, x, y)) continue;
            fr_fire_damage_cell(game, field->floor, x, y);
            for(uint8_t dir = 0; dir < 4; dir++) {
                uint8_t nx = (uint8_t)(x + dirs[dir][0]);
                uint8_t ny = (uint8_t)(y + dirs[dir][1]);
                if(!fr_fire_can_occupy_at(game, nx, ny)) continue;
                uint8_t terrain = fr_get_terrain(game, nx, ny);
                if(((uint8_t)(x * 13u + y * 7u + field->age + dir) % 3u) != 0) continue;
                if(terrain == FR_TERR_GRASS) fr_set_terrain(game, nx, ny, FR_TERR_GRASS_TRAMPLED);
                fr_cell_set(next_cells, nx, ny);
            }
        }
    }
    memcpy(field->cells, next_cells, sizeof(field->cells));
}

void fr_tick_terrain_effects(FrGame* game) {
    for(uint8_t i = 0; i < FR_TERRAIN_FIELD_CAP; i++) {
        FrTerrainField* field = &game->terrain_fields[i];
        if(!field->active) continue;
        field->age++;
        if(field->age >= field->ttl) {
            field->active = false;
            continue;
        }
        if(field->type == FR_TERRAIN_FIELD_FIRE) fr_tick_fire_field(game, field);
        else if(field->type == FR_TERRAIN_FIELD_ICE) fr_tick_ice_field(game, field);
    }
}
