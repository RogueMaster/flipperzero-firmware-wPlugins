#ifndef HOLDEM_AI_H
#define HOLDEM_AI_H

#include "holdem_types.h"

// Chooses a bot action for the current betting decision.
void bot_action(
    HoldemApp* app,
    const HoldemGame* game,
    int acting_player_index,
    int32_t to_call,
    int32_t min_raise,
    int32_t current_bet,
    HoldemAction* action,
    int32_t* amount);

#endif
