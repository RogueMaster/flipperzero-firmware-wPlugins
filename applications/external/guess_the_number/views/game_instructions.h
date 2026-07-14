#pragma once

#include <gui/view.h>
#include "../helpers/game_custom_event.h"

typedef struct GameInstructions GameInstructions;

typedef void (*GameInstructionsCallback)(GameCustomEvent event, void* context);

void game_instructions_set_callback(
    GameInstructions* game_instructions,
    GameInstructionsCallback callback,
    void* context);

View* game_instructions_get_view(GameInstructions* game_instructions);

GameInstructions* game_instructions_alloc();

void game_instructions_free(GameInstructions* game_instructions);
