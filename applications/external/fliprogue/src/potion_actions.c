#include "potion_actions.h"

#include "game_core.h"
#include "hazards.h"
#include "ice_effects.h"
#include "map_state.h"
#include "status_effects.h"
#include "terrain_effects.h"

void fr_apply_potion_to_player(FrGame* game, uint8_t potion) {
    if(potion == FR_POTION_HEALING) {
        uint8_t healed = (uint8_t)(game->player.hp + 8);
        game->player.hp = healed > game->player.max_hp ? game->player.max_hp : healed;
        fr_log(game, "Warmth knits flesh.");
    } else if(potion == FR_POTION_STRENGTH) {
        game->player.str++;
        fr_log(game, "Iron wakes in you.");
    } else if(potion == FR_POTION_ANTIDOTE) {
        fr_clear_effect_from_player(&game->player, FR_FX_POISONED, FR_FX_POISONED_INDEX);
        fr_log(game, "Venom loosens.");
    } else if(potion == FR_POTION_QUICKNESS) {
        game->player.fx_timer[FR_FX_SLOWED_INDEX] = 0;
        game->player.effects &= (uint8_t)~FR_FX_SLOWED;
        fr_log(game, "Your feet spark.");
    } else if(potion == FR_POTION_FIRE_WARD) {
        fr_clear_effect_from_player(&game->player, FR_FX_BURNING, FR_FX_BURNING_INDEX);
        game->player.fire_ward_timer = 10;
        fr_log(game, "Your skin cools.");
    } else if(potion == FR_POTION_VENOM) {
        fr_apply_effect_to_player(&game->player, FR_FX_POISONED, FR_FX_POISONED_INDEX, 12);
        fr_log(game, "Bitter roots bloom.");
    } else if(potion == FR_POTION_FLAME) {
        fr_fire_burst(game, game->player.x, game->player.y);
    } else if(potion == FR_POTION_FROST) {
        fr_apply_effect_to_player(&game->player, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 10);
        fr_log(game, "Time turns thick.");
    } else if(potion == FR_POTION_BLINDNESS) {
        fr_apply_effect_to_player(&game->player, FR_FX_BLIND, FR_FX_BLIND_INDEX, 8);
        fr_log(game, "The dark leans in.");
    } else if(potion == FR_POTION_SMOKE) {
        fr_apply_effect_to_player(&game->player, FR_FX_CONFUSED, FR_FX_CONFUSED_INDEX, 8);
        fr_apply_effect_to_player(&game->player, FR_FX_BLIND, FR_FX_BLIND_INDEX, 4);
        fr_log(game, "Smoke folds you.");
    }
    fr_mark_known_potion(game, potion);
}

void fr_apply_potion_to_actor(FrGame* game, FrActor* actor, uint8_t potion) {
    if(!actor) {
        fr_mark_known_potion(game, potion);
        fr_log(game, "Potion shatters.");
        return;
    }
    if(potion == FR_POTION_HEALING) {
        uint8_t hp = (uint8_t)(actor->hp + 8);
        actor->hp = hp > actor->max_hp ? actor->max_hp : hp;
    } else if(potion == FR_POTION_STRENGTH) {
        actor->hp = actor->hp < actor->max_hp ? (uint8_t)(actor->hp + 1) : actor->hp;
    } else if(potion == FR_POTION_ANTIDOTE || potion == FR_POTION_QUICKNESS) {
        actor->effects &= (uint8_t)~FR_FX_POISONED;
        actor->fx_timer[FR_FX_POISONED_INDEX] = 0;
        actor->effects &= (uint8_t)~FR_FX_SLOWED;
        actor->fx_timer[FR_FX_SLOWED_INDEX] = 0;
    } else if(potion == FR_POTION_FIRE_WARD) {
        actor->effects &= (uint8_t)~FR_FX_BURNING;
        actor->fx_timer[FR_FX_BURNING_INDEX] = 0;
    } else if(potion == FR_POTION_FLAME) {
        fr_fire_burst(game, actor->x, actor->y);
    } else if(potion == FR_POTION_FROST) {
        fr_apply_effect_to_actor(actor, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 10);
    } else if(potion == FR_POTION_VENOM) {
        fr_apply_effect_to_actor(actor, FR_FX_POISONED, FR_FX_POISONED_INDEX, 12);
    } else if(potion == FR_POTION_BLINDNESS) {
        fr_apply_effect_to_actor(actor, FR_FX_BLIND, FR_FX_BLIND_INDEX, 8);
    } else if(potion == FR_POTION_SMOKE) {
        fr_apply_effect_to_actor(actor, FR_FX_CONFUSED, FR_FX_CONFUSED_INDEX, 8);
        fr_apply_effect_to_actor(actor, FR_FX_BLIND, FR_FX_BLIND_INDEX, 4);
    }
    fr_mark_known_potion(game, potion);
    fr_log(game, "Potion splashes.");
}

void fr_throw_potion_at(FrGame* game, uint8_t potion, uint8_t tx, uint8_t ty) {
    FrActor* actor = fr_actor_at(game, tx, ty);
    if(potion == FR_POTION_FLAME) {
        fr_mark_known_potion(game, potion);
        fr_fire_burst(game, tx, ty);
        return;
    }
    if(potion == FR_POTION_FIRE_WARD) {
        bool changed = false;
        if(actor && (actor->effects & FR_FX_BURNING) != 0) {
            actor->effects &= (uint8_t)~FR_FX_BURNING;
            actor->fx_timer[FR_FX_BURNING_INDEX] = 0;
            changed = true;
        }
        if(fr_extinguish_fire_area(game, game->floor, tx, ty, 1)) changed = true;
        fr_mark_known_potion(game, potion);
        fr_log(game, changed ? "Fire gutters." : "Potion splashes.");
        return;
    }
    if(potion == FR_POTION_FROST) {
        if(actor) fr_apply_effect_to_actor(actor, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 10);
        fr_extinguish_fire_area(game, game->floor, tx, ty, 1);
        fr_freeze_water_area(game, game->floor, tx, ty, 1);
        fr_mark_known_potion(game, potion);
        fr_log(game, "Frost spreads.");
        return;
    }
    fr_apply_potion_to_actor(game, actor, potion);
}
