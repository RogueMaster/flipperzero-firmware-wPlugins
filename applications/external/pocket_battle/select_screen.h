#pragma once
#include <gui/gui.h>
#include "pokemon.h"

typedef struct SelectScreen SelectScreen;

typedef struct {
    int player_index;
    int enemy_index;
    bool confirmed;
} SelectionResult;

SelectScreen* select_screen_create(void);
void select_screen_free(SelectScreen* screen);
void select_screen_draw(SelectScreen* screen, Canvas* canvas);
SelectionResult select_screen_handle_input(SelectScreen* screen, InputKey key);
