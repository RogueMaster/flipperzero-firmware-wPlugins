#include "run_report.h"

#include <stdio.h>

bool fr_boss_alive(FrGame* game) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active &&
           (game->actors[i].type == FR_MON_DRAGON || game->actors[i].type == FR_MON_YONDER_WARDEN)) {
            return true;
        }
    }
    for(uint8_t f = 0; f < FR_MAX_FLOORS; f++) {
        if(f + 1 == game->floor) continue;
        if(!game->floors[f].generated) continue;
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            if(game->floors[f].actors[i].active &&
               (game->floors[f].actors[i].type == FR_MON_DRAGON ||
                game->floors[f].actors[i].type == FR_MON_YONDER_WARDEN)) {
                return true;
            }
        }
    }
    return false;
}

bool fr_warden_position(const FrGame* game, uint8_t* floor, uint8_t* x, uint8_t* y) {
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game->actors[i].active && game->actors[i].type == FR_MON_YONDER_WARDEN) {
            if(floor) *floor = game->floor;
            if(x) *x = game->actors[i].x;
            if(y) *y = game->actors[i].y;
            return true;
        }
    }
    for(uint8_t f = 1; f <= FR_MAX_FLOORS; f++) {
        if(f == game->floor) continue;
        const FrFloorState* floor_state = &game->floors[f - 1];
        if(!floor_state->generated) continue;
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            if(floor_state->actors[i].active && floor_state->actors[i].type == FR_MON_YONDER_WARDEN) {
                if(floor) *floor = f;
                if(x) *x = floor_state->actors[i].x;
                if(y) *y = floor_state->actors[i].y;
                return true;
            }
        }
    }
    return false;
}

const char* fr_run_summary(const FrGame* game) {
    static char summary[FR_LOG_SIZE];
    if(game->mode == FR_MODE_VICTORY) {
        snprintf(summary, sizeof(summary), "Victory F%u L%u %ug", game->floor, game->player.level, game->player.gold);
    } else if(game->mode == FR_MODE_GAME_OVER) {
        snprintf(summary, sizeof(summary), "Death F%u L%u %ug", game->floor, game->player.level, game->player.gold);
    } else {
        snprintf(summary, sizeof(summary), "Run F%u L%u %ug", game->floor, game->player.level, game->player.gold);
    }
    return summary;
}

const char* fr_death_log(const FrGame* game) {
    return game->death_log[0] != '\0' ? game->death_log : game->log;
}
