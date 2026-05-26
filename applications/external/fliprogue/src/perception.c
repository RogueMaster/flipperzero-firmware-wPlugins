#include "perception.h"

#include "actor_state.h"
#include "equipment.h"
#include "game_core.h"
#include "map_state.h"
#include "status_effects.h"

bool fr_player_sees_traps(const FrGame* game) {
    return game->player.class_id == FR_CLASS_RANGER;
}

bool fr_player_knows_scrolls(const FrGame* game) {
    return game->player.class_id == FR_CLASS_MAGE;
}

bool fr_player_detects_trap(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
        if(!game->traps[i].active || game->traps[i].x != x || game->traps[i].y != y) continue;
        return fr_player_sees_traps(game) || game->traps[i].hidden == 0;
    }
    return false;
}

uint8_t fr_hidden_trap_detection_chance(const FrGame* game) {
    uint8_t chance = (uint8_t)(20 + game->player.dex * 5);
    if(fr_player_sees_traps(game)) chance = (uint8_t)(chance + 40);
    if(fr_has_equipped_trinket(game, FR_TRINKET_SCOUT)) chance = (uint8_t)(chance + 25);
    return chance > 95 ? 95 : chance;
}

bool fr_search_nearby(FrGame* game) {
    bool found = false;
    bool secret_event = false;
    uint8_t secret_x = game->player.x;
    uint8_t secret_y = game->player.y;
    uint8_t chance = fr_hidden_trap_detection_chance(game);
    for(int8_t oy = -2; oy <= 2; oy++) {
        for(int8_t ox = -2; ox <= 2; ox++) {
            int16_t nx_i = (int16_t)game->player.x + ox;
            int16_t ny_i = (int16_t)game->player.y + oy;
            if(nx_i < 0 || ny_i < 0 || nx_i >= FR_MAP_W || ny_i >= FR_MAP_H) continue;
            uint8_t nx = (uint8_t)nx_i;
            uint8_t ny = (uint8_t)ny_i;
            if((game->tiles[ny][nx] & FR_TILE_HIDDEN_DOOR) != 0 && game->player.dex >= 5) {
                fr_reveal_hidden_door_at(game, nx, ny);
                fr_log(game, "Hidden door.");
                found = true;
                if(!secret_event) {
                    secret_x = nx;
                    secret_y = ny;
                    secret_event = true;
                }
            }
            FrActor* actor = fr_actor_at(game, nx, ny);
            if(actor && (actor->flags & FR_ACTOR_HIDDEN) != 0 && fr_rand_u8(game, 100) < chance) {
                fr_reveal_actor(actor);
                fr_log(game, "A shape appears.");
                found = true;
            }
            FrTrap* trap = fr_trap_at(game, nx, ny);
            if(trap && trap->hidden && (fr_rand_u8(game, 100) < chance || game->player.dex >= 6)) {
                trap->hidden = 0;
                game->last_event = FR_EVENT_TRAP_SPOTTED;
                game->event_x = nx;
                game->event_y = ny;
                fr_log(game, "Trap spotted.");
                found = true;
            }
        }
    }
    if(secret_event) fr_event_secret(game, secret_x, secret_y);
    return found;
}

bool fr_reveal_secrets(FrGame* game, uint8_t cx, uint8_t cy, uint8_t radius) {
    bool found = false;
    bool secret_event = false;
    uint8_t secret_x = cx;
    uint8_t secret_y = cy;
    for(int8_t oy = -(int8_t)radius; oy <= (int8_t)radius; oy++) {
        for(int8_t ox = -(int8_t)radius; ox <= (int8_t)radius; ox++) {
            int16_t x_i = (int16_t)cx + ox;
            int16_t y_i = (int16_t)cy + oy;
            if(x_i < 0 || y_i < 0 || x_i >= FR_MAP_W || y_i >= FR_MAP_H) continue;
            uint8_t x = (uint8_t)x_i;
            uint8_t y = (uint8_t)y_i;
            if((game->tiles[y][x] & FR_TILE_HIDDEN_DOOR) != 0) {
                fr_reveal_hidden_door_at(game, x, y);
                found = true;
                if(!secret_event) {
                    secret_x = x;
                    secret_y = y;
                    secret_event = true;
                }
            }
            FrTrap* trap = fr_trap_at(game, x, y);
            if(trap && trap->hidden) {
                trap->hidden = 0;
                game->last_event = FR_EVENT_TRAP_SPOTTED;
                game->event_x = x;
                game->event_y = y;
                found = true;
            }
            FrActor* actor = fr_actor_at(game, x, y);
            if(actor && (actor->flags & FR_ACTOR_HIDDEN) != 0) {
                fr_reveal_actor(actor);
                found = true;
            }
        }
    }
    if(secret_event) fr_event_secret(game, secret_x, secret_y);
    return found;
}

void fr_auto_close_doors(FrGame* game) {
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            if(fr_get_terrain(game, x, y) != FR_TERR_DOOR_OPEN) continue;
            bool actor_near = false;
            for(int8_t oy = -1; oy <= 1; oy++) {
                for(int8_t ox = -1; ox <= 1; ox++) {
                    int16_t nx = (int16_t)x + ox;
                    int16_t ny = (int16_t)y + oy;
                    if(nx < 0 || ny < 0 || nx >= FR_MAP_W || ny >= FR_MAP_H) continue;
                    if(game->player.x == (uint8_t)nx && game->player.y == (uint8_t)ny) actor_near = true;
                    if(fr_actor_at(game, (uint8_t)nx, (uint8_t)ny)) actor_near = true;
                }
            }
            if(!actor_near) fr_set_terrain(game, x, y, FR_TERR_DOOR_CLOSED);
        }
    }
}

void fr_update_fov(FrGame* game) {
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) game->tiles[y][x] &= (uint8_t)~FR_TILE_VISIBLE;
    }
    uint8_t radius = (game->player.effects & FR_FX_BLIND) != 0 ? 2 : 8;
    uint8_t radius_sq = (uint8_t)(radius * radius);
    bool glass_peek = fr_has_equipped_trinket(game, FR_TRINKET_GLASS);
    for(int8_t oy = -8; oy <= 8; oy++) {
        for(int8_t ox = -8; ox <= 8; ox++) {
            int16_t x = (int16_t)game->player.x + ox;
            int16_t y = (int16_t)game->player.y + oy;
            if(x < 0 || y < 0 || x >= FR_MAP_W || y >= FR_MAP_H) continue;
            if((ox * ox + oy * oy) > radius_sq) continue;
            int16_t cx = game->player.x;
            int16_t cy = game->player.y;
            int16_t dx = x > cx ? 1 : (x < cx ? -1 : 0);
            int16_t dy = y > cy ? 1 : (y < cy ? -1 : 0);
            bool visible = true;
            while(cx != x || cy != y) {
                if(cx != x) cx += dx;
                if(cy != y) cy += dy;
                if(cx == x && cy == y) break;
                if(fr_blocks_sight(fr_get_terrain(game, (uint8_t)cx, (uint8_t)cy))) {
                    if(glass_peek && cx + dx == x && cy + dy == y) break;
                    visible = false;
                    break;
                }
            }
            if(visible) game->tiles[y][x] |= FR_TILE_VISIBLE | FR_TILE_EXPLORED;
        }
    }
    game->tiles[game->player.y][game->player.x] |= FR_TILE_VISIBLE | FR_TILE_EXPLORED;
}
