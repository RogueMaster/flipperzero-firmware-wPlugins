#include "p1x_network_defender.h"
#include "game_state.h"
#include "ui.h"
#include "game_logic.h"

// Main app entry
int32_t system_defender_app(void* p) {
    UNUSED(p);

    // Setup event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Setup game state
    GameState* game_state = malloc(sizeof(GameState));
    game_init(game_state);

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop
    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                // Handle input based on current state
                if(game_state->current_state == GAME_STATE_TITLE) {
                    // Title screen inputs
                    if(event.key == InputKeyOk) {
                        game_state->current_state = GAME_STATE_MENU; // Go to menu instead of play
                    } else if(event.key == InputKeyBack) {
                        running = false; // Quit from title screen
                    }
                } else if(game_state->current_state == GAME_STATE_MENU) {
                    // Menu screen inputs
                    if(event.key == InputKeyUp && game_state->menu_selection > 0) {
                        game_state->menu_selection--;
                    } else if(event.key == InputKeyDown && game_state->menu_selection < 2) {
                        game_state->menu_selection++;
                    } else if(event.key == InputKeyOk) {
                        if(game_state->menu_selection == 0) {
                            game_state->current_state = GAME_STATE_PLAY;
                            reset_game_state(game_state); // Reset game when starting
                        } else if(game_state->menu_selection == 1) {
                            game_state->current_state = GAME_STATE_HELP;
                            game_state->help_page = 0; // Start at first help page
                        } else if(game_state->menu_selection == 2) {
                            running = false; // Quit
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_TITLE;
                    }
                } else if(game_state->current_state == GAME_STATE_HELP) {
                    // Help screen inputs - updated for 3 pages
                    if(event.key == InputKeyOk) {
                        if(game_state->help_page < 2) {
                            game_state->help_page++; // Go to next page
                        } else {
                            game_state->current_state =
                                GAME_STATE_MENU; // Return to menu after last page
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_MENU;
                    }
                } else if(game_state->current_state == GAME_STATE_PLAY) {
                    // Game screen inputs
                    if(game_state->game_over) {
                        if(event.key == InputKeyBack) {
                            // Check for high score before returning to title
                            if(game_state->score > game_state->high_score) {
                                game_state->high_score = game_state->score;
                                game_state->new_high_score = true;
                                save_high_score(game_state);
                            }
                            game_state->current_state = GAME_STATE_TITLE;
                            reset_game_state(game_state);
                        }
                    } else {
                        // Don't allow movement or actions if player is patching
                        if(game_state->patching) {
                            // Only allow back button to exit game while patching
                            if(event.key == InputKeyBack) {
                                game_state->current_state = GAME_STATE_TITLE;
                            }
                        } else {
                            switch(event.key) {
                            case InputKeyUp:
                                game_state->player_pos =
                                    (game_state->player_pos == 0 || game_state->player_pos == 1) ?
                                        game_state->player_pos :
                                        game_state->player_pos - 2;
                                break;
                            case InputKeyDown:
                                game_state->player_pos =
                                    (game_state->player_pos == 2 || game_state->player_pos == 3) ?
                                        game_state->player_pos :
                                        game_state->player_pos + 2;
                                break;
                            case InputKeyLeft:
                                game_state->player_pos =
                                    (game_state->player_pos == 0 || game_state->player_pos == 2) ?
                                        game_state->player_pos :
                                        game_state->player_pos - 1;
                                break;
                            case InputKeyRight:
                                game_state->player_pos =
                                    (game_state->player_pos == 1 || game_state->player_pos == 3) ?
                                        game_state->player_pos :
                                        game_state->player_pos + 1;
                                break;
                            case InputKeyOk:
                                if(game_state->hacking[game_state->player_pos] &&
                                   !game_state->patching) {
                                    game_state->patching = true;
                                    game_state->patch_start = furi_get_tick();
                                } else if(game_state->packets[game_state->player_pos] > 0) {
                                    game_state->packets
                                        [game_state->player_pos]--; // Correctly remove packet
                                    game_state->score +=
                                        10; // Increment score by 10 for each packet delivered

                                    // Start packet animation
                                    game_state->packet_animation = true;
                                    game_state->anim_start = furi_get_tick();
                                    game_state->anim_from = game_state->player_pos;

                                    furi_hal_vibro_on(true); // Vibrate for packet accept
                                    furi_delay_ms(100);
                                    furi_hal_vibro_on(false);
                                }
                                break;
                            case InputKeyBack:
                                game_state->current_state = GAME_STATE_TITLE; // Return to title
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Only update game logic if we're in play mode
        if(game_state->current_state == GAME_STATE_PLAY) {
            update_game(game_state);
        }

        view_port_update(view_port);
    }

    // Cleanup
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(game_state);

    return 0;
}
