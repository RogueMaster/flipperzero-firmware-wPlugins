#pragma once

#include <gui/gui.h>
#include <stdint.h>

typedef struct Menu Menu;

typedef enum {
    MENU_BATTLE_1V1,
    MENU_BATTLE_2V2, // For future
    MENU_OPTIONS, // For future
    MENU_QUIT,
    MENU_ITEM_COUNT
} MenuItem;

Menu* menu_create(void);
void menu_free(Menu* menu);
void menu_draw(Menu* menu, Canvas* canvas);
MenuItem menu_handle_input(Menu* menu, InputKey key);
