#include "select_screen.h"
#include <stdlib.h>
#include <string.h>

struct SelectScreen {
    int player_selection;
    int enemy_selection;
    bool selecting_enemy; // false = selecting player, true = selecting enemy
    bool selection_complete;
    int pokemon_count;
};

SelectScreen* select_screen_create(void) {
    SelectScreen* screen = malloc(sizeof(SelectScreen));
    if(!screen) return NULL;

    screen->player_selection = 0;
    screen->enemy_selection = 1;
    screen->selecting_enemy = false;
    screen->selection_complete = false;
    screen->pokemon_count = pokemon_get_species_count();

    return screen;
}

void select_screen_free(SelectScreen* screen) {
    free(screen);
}

void select_screen_draw(SelectScreen* screen, Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Title
    if(!screen->selecting_enemy) {
        canvas_draw_str(canvas, 20, 10, "Choose Your Pokemon");
    } else {
        canvas_draw_str(canvas, 20, 10, "Choose Opponent");
    }

    canvas_set_font(canvas, FontSecondary);

    // Get Pokemon list
    const PokemonSpecies* species_list = pokemon_get_species_list();

    // Draw Pokemon list (show 5 at a time)
    int current_selection = screen->selecting_enemy ? screen->enemy_selection :
                                                      screen->player_selection;
    int start_index = (current_selection / 5) * 5;
    int end_index = start_index + 5;
    if(end_index > screen->pokemon_count) end_index = screen->pokemon_count;

    for(int i = start_index; i < end_index; i++) {
        int y = 20 + ((i - start_index) * 9);

        // Highlight current selection
        if(i == current_selection) {
            canvas_draw_box(canvas, 0, y - 7, 128, 9);
            canvas_set_color(canvas, ColorWhite);
        }

        // Draw Pokemon name and type
        char display[32];
        snprintf(display, 32, "%s", species_list[i].name);
        canvas_draw_str(canvas, 2, y, display);

        // Show if already selected
        if(!screen->selecting_enemy && i == screen->player_selection) {
            canvas_draw_str(canvas, 100, y, "[YOU]");
        } else if(screen->selecting_enemy && i == screen->enemy_selection) {
            canvas_draw_str(canvas, 100, y, "[FOE]");
        }

        canvas_set_color(canvas, ColorBlack);
    }

    // Instructions
    canvas_draw_str(canvas, 2, 62, "OK to confirm, BACK to cancel");
}

SelectionResult select_screen_handle_input(SelectScreen* screen, InputKey key) {
    SelectionResult result = {
        .player_index = screen->player_selection,
        .enemy_index = screen->enemy_selection,
        .confirmed = false};

    int* current_selection = screen->selecting_enemy ? &screen->enemy_selection :
                                                       &screen->player_selection;

    switch(key) {
    case InputKeyUp:
        if(*current_selection > 0) {
            (*current_selection)--;
        }
        break;

    case InputKeyDown:
        if(*current_selection < screen->pokemon_count - 1) {
            (*current_selection)++;
        }
        break;

    case InputKeyOk:
        if(!screen->selecting_enemy) {
            // Move to enemy selection
            screen->selecting_enemy = true;
        } else {
            // Both selections made
            result.confirmed = true;
            screen->selection_complete = true;
        }
        break;

    case InputKeyBack:
        if(screen->selecting_enemy) {
            // Go back to player selection
            screen->selecting_enemy = false;
        }
        break;

    default:
        break;
    }

    return result;
}
