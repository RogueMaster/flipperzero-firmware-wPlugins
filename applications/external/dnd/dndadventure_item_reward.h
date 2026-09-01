#pragma once

#include "dnd_storage.h"

#include <stdbool.h>
#include <stdint.h>

/* Adventure reward helper: add/increment one item without owning Currency,
   starting inventory, or Inventory Resources metadata. */
bool dndadventure_item_reward_grant_reward(
    Storage* storage,
    uint32_t profile,
    PocketCharacter* character,
    const char* name,
    const char* detail);
