#pragma once

#include "game_logic.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t hp;
    uint8_t damage;
    bool hidden;
} FrMonsterDef;

const FrMonsterDef* fr_monster_def(uint8_t type);
uint8_t fr_monster_tier_for_floor(uint8_t floor);
uint8_t fr_monster_from_tier(uint8_t tier, uint8_t roll);
uint8_t fr_monster_chase_chance(uint8_t type);
bool fr_monster_can_pack(uint8_t type, uint8_t floor);
uint8_t fr_monster_pack_roll(uint8_t type);
uint8_t fr_monster_pack_extra_count(uint8_t type, uint8_t roll_a, uint8_t roll_b);
