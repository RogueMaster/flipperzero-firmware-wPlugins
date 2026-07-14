#include "shrine_actions.h"

#include "game_core.h"
#include "map_state.h"
#include "perception.h"

#include <stddef.h>

bool fr_place_shrine(FrGame* game, uint8_t x, uint8_t y) {
    if(fr_get_terrain(game, x, y) != FR_TERR_FLOOR) return false;
    if(fr_actor_at(game, x, y) != NULL || fr_trap_at(game, x, y) != NULL) return false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game->items[i].active && game->items[i].x == x && game->items[i].y == y) return false;
    }
    fr_set_terrain(game, x, y, FR_TERR_SHRINE);
    return true;
}

FrActionResult fr_use_shrine_at(FrGame* game, uint8_t x, uint8_t y) {
    game->log[0] = '\0';
    if(fr_get_terrain(game, x, y) != FR_TERR_SHRINE) {
        return (FrActionResult){FR_ACTION_BLOCKED};
    }
    if(fr_reveal_secrets(game, x, y, 3)) {
        fr_log(game, "An old god winks.");
    } else if(game->player.hp < game->player.max_hp && ((game->seed + game->floor) & 1u) == 0) {
        game->player.hp++;
        fr_log(game, "An old god patches you.");
    } else {
        fr_log(game, "An old god says maybe.");
    }
    return (FrActionResult){FR_ACTION_USE};
}

FrActionResult fr_use_shrine(FrGame* game) {
    return fr_use_shrine_at(game, game->player.x, game->player.y);
}
