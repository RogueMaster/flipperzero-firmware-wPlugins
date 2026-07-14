#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include "menu.h"
#include "select_screen.h"
#include "battle.h"

typedef enum {
    APP_STATE_MENU,
    APP_STATE_SELECT,
    APP_STATE_BATTLE,
    APP_STATE_GAME_OVER
} AppState;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    AppState state;
    Menu* menu;
    Battle* battle;
    SelectScreen* select_screen;
    int player_selection;
    int enemy_selection;
} ShowdownApp;

// This function is called by Flipper to draw the screen
// Commented out unused functions
/*
static void pocket_battle_draw_callback(Canvas *canvas, void *ctx)
{
    ShowdownApp *app = (ShowdownApp *)ctx;

    switch (app->state)
    {
    case APP_STATE_MENU:
        menu_draw(app->menu, canvas);
        break;
    case APP_STATE_SELECT:
        select_screen_draw(app->select_screen, canvas);
        break;
    case APP_STATE_BATTLE:
        battle_draw(app->battle, canvas);
        break;
    default:
        break;
    }
}
*/

// This function is called by Flipper when buttons are pressed
/*
static void pocket_battle_input_callback(InputEvent *input_event, void *ctx)
{
    ShowdownApp *app = (ShowdownApp *)ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}
*/

// Main entry point - COMMENTED OUT TO AVOID CONFLICTS
// Use pokemon_main_enhanced.c instead
/*
int32_t pocket_battle_main(void *p)
{
    UNUSED(p);

    // Create our app
    ShowdownApp *app = malloc(sizeof(ShowdownApp));

    // Set up the screen
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Initialize app state
    app->state = APP_STATE_MENU;
    app->menu = menu_create();
    app->select_screen = select_screen_create();
    app->battle = NULL; // No battle yet

    // Tell Flipper to use our draw and input functions
    view_port_draw_callback_set(app->view_port, pocket_battle_draw_callback, app);
    view_port_input_callback_set(app->view_port, pocket_battle_input_callback, app);

    // Add to screen
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Main game loop
    InputEvent event;
    bool running = true;

    while (running)
    {
        // Check for button presses
        if (furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk)
        {
            if (event.type == InputTypePress)
            {
                if (event.key == InputKeyBack)
                {
                    running = false; // Exit on Back button
                }
                else
                {
                    // Handle input based on current state
                    if (app->state == APP_STATE_MENU)
                    {
                        MenuItem selected = menu_handle_input(app->menu, event.key);
                        if (selected == MENU_BATTLE_1V1)
                        {
                            app->state = APP_STATE_SELECT; // Go to select screen
                        }
                        else if (selected == MENU_QUIT)
                        {
                            running = false; // Exit on Quit
                        }
                    }
                    else if (app->state == APP_STATE_SELECT)
                    {
                        SelectionResult result =
                            select_screen_handle_input(app->select_screen, event.key);
                        if (result.confirmed)
                        {
                            // Store selections
                            app->player_selection = result.player_index;
                            app->enemy_selection = result.enemy_index;

                            // Create a new battle with selected Pokemon
                            battle_free(app->battle); // Free old battle if exists
                            app->battle = battle_create_with_selection(
                                app->player_selection, app->enemy_selection);
                            app->state = APP_STATE_BATTLE;
                        }
                    }
                    else if (app->state == APP_STATE_BATTLE)
                    {
                        battle_handle_input(app->battle, event.key);

                        // Check if battle is over
                        if (battle_is_over(app->battle))
                        {
                            // Return to menu
                            battle_free(app->battle);
                            app->battle = NULL;
                            app->state = APP_STATE_MENU;
                        }
                    }
                }
            }
        }

        // Update the screen
        view_port_update(app->view_port);
    }

    // Clean up
    battle_free(app->battle);
    menu_free(app->menu);
    select_screen_free(app->select_screen);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
*/
