#include "turns.h"

#include "combat.h"
#include "floor_actors.h"
#include "monster_ai.h"
#include "perception.h"
#include "status_effects.h"
#include "terrain_effects.h"
#include "trinket_effects.h"

static void fr_actor_phase(FrGame* game) {
    if(game->player.cube_hp > 0) return;
    fr_actor_turns(game);
    if(game->mode == FR_MODE_PLAYING && (game->player.effects & FR_FX_SLOWED) != 0) fr_actor_turns(game);
    if(game->mode == FR_MODE_PLAYING) fr_warden_global_turn(game);
}

void fr_finish_turn(FrGame* game) {
    if(game->mode != FR_MODE_PLAYING) return;
    game->turn++;
    fr_tick_hunger(game);
    fr_tick_effects(game);
    fr_tick_terrain_effects(game);
    fr_tick_trinkets(game);
    fr_cube_tick(game);
    if(game->player.hp == 0) {
        uint8_t cause = FR_DEATH_KILLED;
        if(fr_hunger_state(game) == FR_HUNGER_STARVING) cause = FR_DEATH_STARVED;
        else if((game->player.effects & FR_FX_BURNING) != 0) cause = FR_DEATH_BURNED;
        else if((game->player.effects & FR_FX_POISONED) != 0) cause = FR_DEATH_POISONED;
        fr_set_game_over(game, cause, "You died.");
        return;
    }
    fr_actor_phase(game);
    fr_auto_close_doors(game);
    fr_update_fov(game);
}

void fr_finish_item_turn(FrGame* game) {
    fr_finish_turn(game);
}

void fr_finish_world_turns(FrGame* game, uint8_t count) {
    for(uint8_t i = 0; i < count && game->mode == FR_MODE_PLAYING; i++) {
        fr_finish_turn(game);
    }
}
