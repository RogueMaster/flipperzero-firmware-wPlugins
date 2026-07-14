#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include "scenario.h"

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    uint8_t current_scene;
    int8_t selected_option;
    bool running;
} Game;

void draw_callback(Canvas* canvas, void* ctx) {
    Game* game = ctx;
    Scene* current_scene = &SCENARIO.scenes[game->current_scene];

    canvas_clear(canvas);

    uint8_t y = 0;

    // Scene 0 is the title screen - show title and horizontal line
    if(game->current_scene == 0) {
        // Draw scene title
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, SCENARIO.title);

        // Draw horizontal line
        canvas_draw_line(canvas, 0, 12, 128, 12);

        // Start text below the line
        y = 22;
    } else {
        // For other scenes, start from top
        y = 10;
    }

    // Draw scene text lines
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < current_scene->num_lines; i++) {
        canvas_draw_str(canvas, 2, y, current_scene->text_lines[i]);
        y += 8;
    }

    // Draw options
    y += 4; // Add some space between text and options
    for(uint8_t i = 0; i < current_scene->num_options; i++) {
        if(game->selected_option == i) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        canvas_draw_str(canvas, 10, y, current_scene->options[i].text);
        y += 8;
    }
}

void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, event, FuriWaitForever);
}

void game_init(Game* game) {
    game->current_scene = 0;
    game->selected_option = 0;
    game->running = true;

    // Initialize GUI and viewport
    game->gui = furi_record_open(RECORD_GUI);
    game->view_port = view_port_alloc();
    view_port_draw_callback_set(game->view_port, draw_callback, game);
    gui_add_view_port(game->gui, game->view_port, GuiLayerFullscreen);

    // Initialize event queue
    game->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_input_callback_set(game->view_port, input_callback, game->event_queue);
}

void game_free(Game* game) {
    // Free resources
    view_port_enabled_set(game->view_port, false);
    gui_remove_view_port(game->gui, game->view_port);
    view_port_free(game->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(game->event_queue);
}

void process_input(Game* game, InputEvent* event) {
    Scene* current_scene = &SCENARIO.scenes[game->current_scene];

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyUp:
            if(game->selected_option > 0) {
                game->selected_option--;
            }
            break;
        case InputKeyDown:
            if(game->selected_option < current_scene->num_options - 1) {
                game->selected_option++;
            }
            break;
        case InputKeyOk:
            // Navigate to the next scene based on the selected option
            if(game->selected_option >= 0 && game->selected_option < current_scene->num_options) {
                game->current_scene = current_scene->options[game->selected_option].target_scene;
                game->selected_option = 0; // Reset selection for new scene
            }
            break;
        case InputKeyBack:
            game->running = false;
            break;
        default:
            break;
        }
    }
}

int32_t p1x_your_own_adventure_app(void* p) {
    UNUSED(p);

    Game game;
    InputEvent event;

    game_init(&game);

    while(game.running) {
        if(furi_message_queue_get(game.event_queue, &event, 100) == FuriStatusOk) {
            process_input(&game, &event);
        }
        view_port_update(game.view_port);
    }

    game_free(&game);

    return 0;
}
