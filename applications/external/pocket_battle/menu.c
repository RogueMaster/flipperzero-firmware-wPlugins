#include "menu.h"
#include <stdlib.h>
#include <furi.h>

struct Menu {
    MenuItem selected_item;
    bool selection_made;
};

Menu* menu_create(void) {
    Menu* menu = malloc(sizeof(Menu));
    if(!menu) return NULL;

    menu->selected_item = MENU_BATTLE_1V1;
    menu->selection_made = false;

    return menu;
}

void menu_free(Menu* menu) {
    free(menu);
}

void menu_draw(Menu* menu, Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Draw title
    canvas_draw_str(canvas, 30, 12, "SHOWDOWN!");

    canvas_set_font(canvas, FontSecondary);

    // Draw menu items
    const char* items[] = {"Battle 1v1", "Battle 2v2 (Soon)", "Options (Soon)", "Quit"};

    for(int i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = 25 + (i * 10);

        // Highlight selected item
        if(i == (int)menu->selected_item) {
            canvas_draw_box(canvas, 20, y - 7, 88, 9);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 25, y, items[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

MenuItem menu_handle_input(Menu* menu, InputKey key) {
    switch(key) {
    case InputKeyUp:
        if(menu->selected_item > 0) {
            menu->selected_item--;
        }
        break;

    case InputKeyDown:
        if(menu->selected_item < MENU_ITEM_COUNT - 1) {
            menu->selected_item++;
        }
        break;

    case InputKeyOk:
        menu->selection_made = true;
        return menu->selected_item;

    default:
        break;
    }

    return -1; // No selection made
}
