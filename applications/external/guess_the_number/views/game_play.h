#pragma once

#include <gui/view.h>
#include "../helpers/game_custom_event.h"

typedef struct GamePlay GamePlay;

typedef void (*GamePlayCallback)(GameCustomEvent event, void* context);

void game_play_set_callback(GamePlay* instance, GamePlayCallback callback, void* context);

GamePlay* game_play_alloc();

void game_play_free(GamePlay* game_play);

View* game_play_get_view(GamePlay* game_play);
