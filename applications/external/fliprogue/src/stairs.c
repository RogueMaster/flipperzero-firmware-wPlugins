#include "stairs.h"

#include "floor_state.h"
#include "game_core.h"
#include "map_state.h"

FrActionResult fr_descend(FrGame* game) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(fr_get_terrain(game, game->player.x, game->player.y) != FR_TERR_STAIRS_DOWN) {
        fr_log(game, "No stairs down.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    if(game->floor >= FR_MAX_FLOORS) {
        fr_log(game, "No deeper stairs.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    uint8_t next_floor = (uint8_t)(game->floor + 1);
    fr_save_current_floor(game);
    fr_enter_floor(game, next_floor, FR_TERR_STAIRS_UP);
    fr_log(game, "Floor %u.", next_floor);
    return (FrActionResult){FR_ACTION_DESCEND};
}

FrActionResult fr_ascend(FrGame* game) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(fr_get_terrain(game, game->player.x, game->player.y) != FR_TERR_STAIRS_UP) {
        fr_log(game, "No stairs up.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    if(game->floor == 1) {
        if(game->player.has_orb) {
            game->mode = FR_MODE_VICTORY;
            fr_log(game, "Orb reaches daylight.");
            return (FrActionResult){FR_ACTION_DESCEND};
        }
        fr_log(game, "The surface waits.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    uint8_t prev_floor = (uint8_t)(game->floor - 1);
    fr_save_current_floor(game);
    fr_enter_floor(game, prev_floor, FR_TERR_STAIRS_DOWN);
    fr_log(game, "Floor %u.", prev_floor);
    return (FrActionResult){FR_ACTION_DESCEND};
}
