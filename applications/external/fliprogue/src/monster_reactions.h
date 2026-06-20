#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

void fr_wake_actor_toward_player(FrGame* game, FrActor* actor);
void fr_combat_wake_actor(FrGame* game, FrActor* actor);
void fr_pack_wake(FrGame* game, const FrActor* source);
void fr_pack_combat_wake(FrGame* game, const FrActor* source);
uint8_t fr_active_pack_count(const FrGame* game, uint8_t pack_id);
bool fr_maybe_split_slime(FrGame* game, FrActor* actor, uint8_t kind);
void fr_maybe_actor_breaks(FrGame* game, FrActor* actor);
bool fr_actor_evades_player_hit(FrGame* game, FrActor* actor, uint8_t kind);
void fr_maybe_actor_starts_asleep(FrGame* game, FrActor* actor);
