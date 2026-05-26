#include "tile_sprites.h"

#include "terrain_effects.h"

static const FrTileSprite shallow_water_sprite = {
    4,
    {1, 3, 2, 2, 3, 4, 4, 2},
};

static const FrTileSprite sand_sprite = {
    3,
    {2, 2, 4, 3, 1, 5},
};

static const FrTileSprite ice_sprite = {
    4,
    {1, 2, 2, 1, 4, 4, 5, 3},
};

static const FrTileSprite deep_water_frames[4] = {
    {7, {1, 1, 2, 1, 4, 1, 1, 3, 3, 3, 4, 4}},
    {8, {1, 2, 2, 1, 3, 2, 5, 2, 1, 4, 3, 4}},
    {9, {1, 1, 3, 1, 4, 2, 2, 3, 5, 3, 1, 5}},
    {8, {2, 1, 4, 1, 1, 2, 3, 3, 5, 4, 2, 5}},
};

static const FrTileSprite fire_spray_frames[4] = {
    {3, {2, 4, 3, 2, 4, 4}},
    {5, {1, 4, 2, 2, 3, 3, 4, 1, 5, 4}},
    {1, {3, 3}},
    {6, {1, 5, 2, 3, 3, 1, 4, 2, 5, 5, 3, 4}},
};

static void fr_draw_dot_sprite(Canvas* canvas, uint8_t sx, uint8_t sy, const FrTileSprite* sprite) {
    for(uint8_t i = 0; i < sprite->count && i < sizeof(sprite->xy) / 2; i++) {
        canvas_draw_dot(canvas, (uint8_t)(sx + sprite->xy[i * 2]), (uint8_t)(sy + sprite->xy[i * 2 + 1]));
    }
}

static uint8_t fr_wall_variant(const FrGame* game, uint8_t x, uint8_t y) {
    uint32_t v = game->run_seed ? game->run_seed : game->seed;
    v ^= (uint32_t)x * 37u;
    v ^= (uint32_t)y * 73u;
    v ^= v >> 7;
    return (uint8_t)(v & 3u);
}

static void fr_draw_wall_sprite(Canvas* canvas, const FrGame* game, uint8_t x, uint8_t y, uint8_t sx, uint8_t sy) {
    uint8_t variant = fr_wall_variant(game, x, y);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, sx, sy, TILE_PX, TILE_PX);
    canvas_set_color(canvas, ColorWhite);
    if(variant == 0) {
        canvas_draw_dot(canvas, (uint8_t)(sx + 2), (uint8_t)(sy + 2));
        canvas_draw_dot(canvas, (uint8_t)(sx + 4), (uint8_t)(sy + 4));
    } else if(variant == 1) {
        canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 1), (uint8_t)(sx + 4), (uint8_t)(sy + 1));
        canvas_draw_dot(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 3));
    } else if(variant == 2) {
        canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 4), (uint8_t)(sx + 4), (uint8_t)(sy + 2));
    } else {
        canvas_draw_dot(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 1));
        canvas_draw_dot(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 2));
        canvas_draw_dot(canvas, (uint8_t)(sx + 4), (uint8_t)(sy + 4));
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool fr_door_has_side_walls(const FrGame* game, uint8_t x, uint8_t y) {
    if(x == 0 || x + 1 >= FR_MAP_W) return false;
    return fr_get_terrain(game, (uint8_t)(x - 1), y) == FR_TERR_WALL &&
           fr_get_terrain(game, (uint8_t)(x + 1), y) == FR_TERR_WALL;
}

static void fr_draw_closed_door_sprite(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t sx,
    uint8_t sy) {
    if(fr_door_has_side_walls(game, map_x, map_y)) {
        canvas_draw_frame(canvas, sx, (uint8_t)(sy + 1), TILE_PX, (uint8_t)(TILE_PX - 2));
        canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 3), (uint8_t)(sx + 5), (uint8_t)(sy + 3));
        canvas_draw_dot(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 4));
        return;
    }
    canvas_draw_frame(canvas, (uint8_t)(sx + 1), sy, (uint8_t)(TILE_PX - 2), TILE_PX);
    canvas_draw_line(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 1), (uint8_t)(sx + 3), (uint8_t)(sy + 5));
    canvas_draw_dot(canvas, (uint8_t)(sx + 4), (uint8_t)(sy + 3));
}

static void fr_draw_open_door_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_line(canvas, sx, (uint8_t)(sy + 3), (uint8_t)(sx + 5), (uint8_t)(sy + 3));
    canvas_draw_dot(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 2));
    canvas_draw_dot(canvas, (uint8_t)(sx + 4), (uint8_t)(sy + 4));
}

static void fr_draw_grate_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_frame(canvas, sx, sy, TILE_PX, TILE_PX);
    canvas_draw_line(canvas, (uint8_t)(sx + 2), sy, (uint8_t)(sx + 2), (uint8_t)(sy + 5));
    canvas_draw_line(canvas, (uint8_t)(sx + 4), sy, (uint8_t)(sx + 4), (uint8_t)(sy + 5));
    canvas_draw_line(canvas, sx, (uint8_t)(sy + 3), (uint8_t)(sx + 5), (uint8_t)(sy + 3));
}

static bool fr_hidden_lurker_at(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        const FrActor* actor = &game->actors[i];
        if(actor->active && actor->x == x && actor->y == y && actor->type == FR_MON_LURKER &&
           (actor->flags & FR_ACTOR_HIDDEN) != 0) {
            return true;
        }
    }
    return false;
}

static void fr_draw_grass_sprite(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t sx,
    uint8_t sy,
    uint8_t tick) {
    int8_t offset = 0;
    if(!fr_hidden_lurker_at(game, map_x, map_y)) {
        uint8_t wind = (uint8_t)((tick / 10u) + map_x * 3u + map_y * 5u);
        if((wind & 1u) != 0) offset = (wind & 2u) != 0 ? 1 : -1;
    }
    uint8_t x1 = (uint8_t)((int16_t)sx + 2 + offset);
    uint8_t x2 = (uint8_t)((int16_t)sx + 4 + offset);
    canvas_draw_line(canvas, x1, (uint8_t)(sy + 2), x1, (uint8_t)(sy + 4));
    canvas_draw_line(canvas, x2, (uint8_t)(sy + 2), x2, (uint8_t)(sy + 4));
}

static void fr_draw_puddle_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    fr_draw_dot_sprite(canvas, sx, sy, &shallow_water_sprite);
}

static void fr_draw_deep_water_sprite(Canvas* canvas, uint8_t sx, uint8_t sy, uint8_t x, uint8_t y, uint8_t tick) {
    uint8_t frame = (uint8_t)(((tick / 3u) + x + y) & 3u);
    fr_draw_dot_sprite(canvas, sx, sy, &deep_water_frames[frame]);
}

static void fr_draw_sand_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    fr_draw_dot_sprite(canvas, sx, sy, &sand_sprite);
}

static void fr_draw_ice_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    fr_draw_dot_sprite(canvas, sx, sy, &ice_sprite);
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 5), (uint8_t)(sx + 5), (uint8_t)(sy + 1));
}

static void fr_draw_stairs_down_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_line(canvas, sx, sy, (uint8_t)(sx + 5), sy);
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 1), (uint8_t)(sx + 4), (uint8_t)(sy + 1));
    canvas_draw_line(canvas, (uint8_t)(sx + 2), (uint8_t)(sy + 2), (uint8_t)(sx + 3), (uint8_t)(sy + 2));
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 4), (uint8_t)(sx + 5), (uint8_t)(sy + 4));
    canvas_draw_line(canvas, sx, (uint8_t)(sy + 5), (uint8_t)(sx + 5), (uint8_t)(sy + 5));
}

static void fr_draw_stairs_up_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_line(canvas, (uint8_t)(sx + 2), sy, (uint8_t)(sx + 3), sy);
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 1), (uint8_t)(sx + 4), (uint8_t)(sy + 1));
    canvas_draw_line(canvas, sx, (uint8_t)(sy + 2), (uint8_t)(sx + 5), (uint8_t)(sy + 2));
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 4), (uint8_t)(sx + 5), (uint8_t)(sy + 4));
    canvas_draw_line(canvas, sx, (uint8_t)(sy + 5), (uint8_t)(sx + 5), (uint8_t)(sy + 5));
}

static void fr_draw_shrine_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_line(canvas, (uint8_t)(sx + 2), sy, (uint8_t)(sx + 4), sy);
    canvas_draw_line(canvas, (uint8_t)(sx + 3), sy, (uint8_t)(sx + 3), (uint8_t)(sy + 4));
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 4), (uint8_t)(sx + 5), (uint8_t)(sy + 4));
    canvas_draw_line(canvas, (uint8_t)(sx + 1), (uint8_t)(sy + 5), (uint8_t)(sx + 5), (uint8_t)(sy + 5));
    canvas_draw_dot(canvas, (uint8_t)(sx + 2), (uint8_t)(sy + 2));
    canvas_draw_dot(canvas, (uint8_t)(sx + 4), (uint8_t)(sy + 2));
}

static void fr_draw_chest_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    uint8_t left = sx > 0 ? (uint8_t)(sx - 1) : sx;
    uint8_t width = sx > 0 ? 8 : 7;
    uint8_t top = (uint8_t)(sy + 1);
    canvas_draw_frame(canvas, left, top, width, 6);
    canvas_draw_line(canvas, left, (uint8_t)(top + 2), (uint8_t)(left + width - 1), (uint8_t)(top + 2));
    canvas_draw_line(canvas, (uint8_t)(left + 2), top, (uint8_t)(left + 3), (uint8_t)(top - 1));
    canvas_draw_line(canvas, (uint8_t)(left + 4), (uint8_t)(top - 1), (uint8_t)(left + 5), top);
    canvas_draw_dot(canvas, (uint8_t)(left + width / 2), (uint8_t)(top + 4));
}

static void fr_draw_arrows_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_line(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 1), (uint8_t)(sx + 3), (uint8_t)(sy + 5));
    canvas_draw_line(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 1), (uint8_t)(sx + 1), (uint8_t)(sy + 3));
    canvas_draw_line(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 1), (uint8_t)(sx + 5), (uint8_t)(sy + 3));
}

static void fr_draw_dart_sprite(Canvas* canvas, uint8_t sx, uint8_t sy) {
    canvas_draw_dot(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 2));
    canvas_draw_line(canvas, (uint8_t)(sx + 3), (uint8_t)(sy + 4), (uint8_t)(sx + 2), (uint8_t)(sy + 5));
}

bool fr_draw_terrain_sprite(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t screen_x,
    uint8_t screen_y,
    uint8_t anim_tick) {
    if(fr_player_detects_trap(game, map_x, map_y)) return false;

    switch(fr_get_terrain(game, map_x, map_y)) {
    case FR_TERR_WALL:
        fr_draw_wall_sprite(canvas, game, map_x, map_y, screen_x, screen_y);
        return true;
    case FR_TERR_DOOR_CLOSED:
        fr_draw_closed_door_sprite(canvas, game, map_x, map_y, screen_x, screen_y);
        return true;
    case FR_TERR_DOOR_OPEN:
        fr_draw_open_door_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_GRATE:
        fr_draw_grate_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_GRASS:
        fr_draw_grass_sprite(canvas, game, map_x, map_y, screen_x, screen_y, anim_tick);
        return true;
    case FR_TERR_PUDDLE:
        fr_draw_puddle_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_WATER:
        fr_draw_deep_water_sprite(canvas, screen_x, screen_y, map_x, map_y, anim_tick);
        return true;
    case FR_TERR_SAND:
        fr_draw_sand_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_ICE:
        fr_draw_ice_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_STAIRS_DOWN:
        fr_draw_stairs_down_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_STAIRS_UP:
        fr_draw_stairs_up_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_TERR_SHRINE:
        fr_draw_shrine_sprite(canvas, screen_x, screen_y);
        return true;
    default:
        return false;
    }
}

bool fr_draw_item_sprite(Canvas* canvas, const FrItem* item, uint8_t screen_x, uint8_t screen_y) {
    if(!item || !item->active) return false;
    switch(item->type) {
    case FR_ITEM_CHEST:
        fr_draw_chest_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_ITEM_ARROWS:
        fr_draw_arrows_sprite(canvas, screen_x, screen_y);
        return true;
    case FR_ITEM_THROWABLE:
        if(item->subtype == FR_THROW_DART) {
            fr_draw_dart_sprite(canvas, screen_x, screen_y);
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool fr_draw_fire_overlay(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t screen_x,
    uint8_t screen_y,
    uint8_t anim_tick) {
    if(!fr_terrain_fire_at(game, game->floor, map_x, map_y)) return false;
    uint8_t frame = (uint8_t)((anim_tick + map_x * 3u + map_y) & 3u);
    fr_draw_dot_sprite(canvas, screen_x, screen_y, &fire_spray_frames[frame]);
    return true;
}
