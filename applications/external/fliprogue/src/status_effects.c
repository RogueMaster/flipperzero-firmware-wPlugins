#include "status_effects.h"

#include "equipment.h"
#include "game_core.h"

#include <stdio.h>

void fr_apply_effect_to_actor(FrActor* actor, uint8_t effect, uint8_t index, uint8_t turns) {
    actor->effects |= effect;
    actor->fx_timer[index] = turns;
}

void fr_apply_effect_to_player(FrPlayer* player, uint8_t effect, uint8_t index, uint8_t turns) {
    player->effects |= effect;
    player->fx_timer[index] = turns;
}

void fr_clear_effect_from_player(FrPlayer* player, uint8_t effect, uint8_t index) {
    player->effects &= (uint8_t)~effect;
    player->fx_timer[index] = 0;
}

void fr_reveal_actor(FrActor* actor) {
    if(actor) actor->flags &= (uint8_t)~FR_ACTOR_HIDDEN;
}

static void fr_tick_effect_mask(uint8_t* hp, uint8_t* effects, uint8_t timer[8]) {
    if((*effects & FR_FX_BURNING) != 0 && *hp > 0) (*hp)--;
    if((*effects & FR_FX_POISONED) != 0 && (timer[FR_FX_POISONED_INDEX] & 1u) == 0 && *hp > 0) (*hp)--;

    for(uint8_t i = 0; i < 8; i++) {
        if(timer[i] > 0) {
            timer[i]--;
            if(timer[i] == 0) *effects &= (uint8_t)~(1u << i);
        }
    }
}

void fr_tick_effects(FrGame* game) {
    fr_tick_effect_mask(&game->player.hp, &game->player.effects, game->player.fx_timer);
    if(game->player.fire_ward_timer > 0) game->player.fire_ward_timer--;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game->actors[i];
        if(!actor->active) continue;
        fr_tick_effect_mask(&actor->hp, &actor->effects, actor->fx_timer);
        if(actor->hp == 0) {
            actor->active = false;
            fr_log(game, "%s burns away.", fr_actor_log_name(actor->type));
        }
    }
}

uint8_t fr_hunger_state(const FrGame* game) {
    if(game->player.hunger == 0) return FR_HUNGER_STARVING;
    if(game->player.hunger < 128) return FR_HUNGER_HUNGRY;
    return FR_HUNGER_OK;
}

void fr_tick_hunger(FrGame* game) {
    uint8_t hunger_interval = game->player.has_orb ? 10 : 2;
    uint8_t starve_interval = game->player.has_orb ? 50 : 10;
    if(fr_has_equipped_trinket(game, FR_TRINKET_LOCKET)) hunger_interval = (uint8_t)(hunger_interval + 1);
    if(fr_has_equipped_trinket(game, FR_TRINKET_HUNGRY) && hunger_interval > 1) hunger_interval--;
    if((game->turn % hunger_interval) != 0) return;
    if(game->player.hunger > 0) game->player.hunger--;
    if(fr_hunger_state(game) == FR_HUNGER_STARVING && (game->turn % starve_interval) == 0 &&
       game->player.hp > 0) {
        game->player.hp--;
        fr_log(game, "Starving.");
    }
}

const char* fr_player_effects_label(const FrGame* game) {
    static char label[40];
    label[0] = '\0';
    if(game->player.effects == 0 && game->player.fire_ward_timer == 0) return "Fx: none";
    const struct {
        uint8_t flag;
        const char* name;
    } entries[] = {
        {FR_FX_BURNING, "Burn"},
        {FR_FX_POISONED, "Pois"},
        {FR_FX_STUNNED, "Stun"},
        {FR_FX_SLOWED, "Slow"},
        {FR_FX_AFRAID, "Fear"},
        {FR_FX_BLIND, "Blind"},
        {FR_FX_CONFUSED, "Conf"},
        {FR_FX_MARKED, "Mark"},
    };
    size_t pos = snprintf(label, sizeof(label), "Fx:");
    if(game->player.fire_ward_timer > 0) {
        int written = snprintf(label + pos, sizeof(label) - pos, " Ward");
        if(written > 0) pos = (size_t)(pos + (size_t)written);
    }
    for(uint8_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if((game->player.effects & entries[i].flag) == 0) continue;
        if(pos >= sizeof(label)) break;
        int written = snprintf(label + pos, sizeof(label) - pos, " %s", entries[i].name);
        if(written < 0) break;
        pos = (size_t)(pos + (size_t)written);
    }
    return label;
}
