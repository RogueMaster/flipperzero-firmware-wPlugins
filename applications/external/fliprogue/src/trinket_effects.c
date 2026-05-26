#include "trinket_effects.h"

#include "combat.h"
#include "equipment.h"
#include "game_core.h"

void fr_tick_trinkets(FrGame* game) {
    if(fr_has_equipped_trinket(game, FR_TRINKET_CINDER) && (game->turn % 10u) == 0) {
        if(game->player.hp > 1) {
            uint8_t damage = (uint8_t)(1u + fr_rand_u8(game, 2));
            uint8_t max_damage = (uint8_t)(game->player.hp - 1u);
            game->player.hp = (uint8_t)(game->player.hp - (damage > max_damage ? max_damage : damage));
        }
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            FrActor* actor = &game->actors[i];
            if(!actor->active) continue;
            int8_t dx = (int8_t)actor->x - (int8_t)game->player.x;
            int8_t dy = (int8_t)actor->y - (int8_t)game->player.y;
            if(fr_abs_i8(dx) <= 1 && fr_abs_i8(dy) <= 1) {
                fr_damage_actor_kind(game, actor, 1, "Cinder burns", FR_DAMAGE_BURST);
            }
        }
        fr_log(game, "Cinder bites.");
    }
}
