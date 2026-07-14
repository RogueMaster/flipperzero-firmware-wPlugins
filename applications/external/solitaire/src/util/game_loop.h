#pragma once
#include <furi.h>

struct GameState {
    bool (*update)(void* inputState);
};
