#pragma once

#include <gui/view.h>
#include "../helpers/game_custom_event.h"

typedef struct GameStartscreen GameStartscreen;

typedef void (*GameStartscreenCallback)(GameCustomEvent event, void* context);

void game_startscreen_set_callback(
    GameStartscreen* game_startscreen,
    GameStartscreenCallback callback,
    void* context);

View* game_startscreen_get_view(GameStartscreen* game_startscreen);

GameStartscreen* game_startscreen_alloc();

void game_startscreen_free(GameStartscreen* game_startscreen);
