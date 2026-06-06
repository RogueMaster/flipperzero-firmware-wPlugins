#include "grate_actions.h"

#include "game_core.h"
#include "map_state.h"

#include <string.h>

static bool fr_tile_has_object(const FrGame* game, uint8_t x, uint8_t y) {
    if(game->player.x == x && game->player.y == y) return true;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active && game->actors[i].x == x && game->actors[i].y == y) return true;
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active && game->items[i].x == x && game->items[i].y == y) return true;
    }
    for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
        if(game->traps[i].active && game->traps[i].x == x && game->traps[i].y == y) return true;
    }
    return false;
}

bool fr_place_grate(FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(terrain != FR_TERR_FLOOR && terrain != FR_TERR_WALL) return false;
    if(fr_tile_has_object(game, x, y)) return false;
    fr_set_terrain(game, x, y, FR_TERR_GRATE);
    return true;
}

bool fr_place_key(FrGame* game, uint8_t x, uint8_t y) {
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(!fr_is_walkable(terrain) || terrain == FR_TERR_WATER || terrain == FR_TERR_STAIRS_DOWN ||
       terrain == FR_TERR_STAIRS_UP) {
        return false;
    }
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        FrItem* item = &game->items[i];
        if(item->active) continue;
        memset(item, 0, sizeof(*item));
        item->active = true;
        item->type = FR_ITEM_KEY;
        item->x = x;
        item->y = y;
        item->amount = 1;
        return true;
    }
    return false;
}

bool fr_place_button(FrGame* game, uint8_t x, uint8_t y) {
    if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR) return false;
    fr_set_terrain(game, x, y, FR_TERR_BUTTON);
    return true;
}

bool fr_open_grates_on_floor(FrGame* game) {
    bool opened = false;
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_GRATE) continue;
            fr_set_terrain(game, x, y, FR_TERR_FLOOR);
            opened = true;
        }
    }
    if(opened) fr_log(game, "Grate opens.");
    return opened;
}
