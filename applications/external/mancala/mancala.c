#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/gui.h> // GUI system
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <gui/elements.h> // Button drawing functions
#include <input/input.h> // Input handling (buttons)
#include <mancala_icons.h>

#define TAG "Mancala" // Tag for logging purposes

// =============================================================================
// GAME STATE DEFINITIONS
// =============================================================================

// Represents whose turn it is
typedef enum {
    TURN_USER = 0,
    TURN_AI = 1
} Turn;

// Represents board sections for stone sowing
typedef enum {
    SECTION_USER_PITS,
    SECTION_USER_STORE,
    SECTION_AI_PITS,
    SECTION_AI_STORE
} BoardSection;

// Main application state structure
typedef struct {
    ViewPort* view_port; // ViewPort for rendering UI
    Gui* gui; // GUI instance
    int pits_user[6]; // Lower row (user's pits, left to right, indices 0-5)
    int pits_ai[6]; // Upper row (AI's pits, left to right, indices 0-5)
    int store_user; // Right store (user's collection)
    int store_ai; // Left store (AI's collection)
    Turn turn; // Current player's turn
    int cursor; // Selected pit index for user (0..5, left->right)
    bool game_over; // Game completion flag
    char status_text[32]; // Status message displayed at bottom
    bool redraw; // Flag to trigger screen redraw
    bool ai_ready; // Flag indicating AI's turn is ready to execute (needs user to press OK)
    bool should_exit; // Flag to exit application
} MancalaState;

// Game constants
#define INIT_STONES 4 // Starting stones per pit

// Layout constants for rendering
// Y-positions for pit number display
static const int ai_pit_y = 26;
static const int user_pit_y = 45;
// X-positions and spacing for pits
static const int pit_start_x = 24; // X position of first pit
static const int pit_spacing = 16; // Distance between pits
// Number position for stores (=mancalas)
static const int store_ai_x = 3;
static const int store_ai_y = 36;
static const int store_user_x = 115;
static const int store_user_y = 36;
// Status message position
static const int status_x = 57;
static const int status_y = 2;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================
// Game logic functions --------------------------------------------------------
static void init_game(MancalaState* s);
static void end_game_if_needed(MancalaState* s);
static void collect_remaining_to_stores(MancalaState* s);
static void do_move_from(MancalaState* s, int pit_index, bool by_user);
static int ai_choose_move(MancalaState* s);
// Rendering functions ---------------------------------------------------------
static void render_callback(Canvas* canvas, void* ctx);
static void draw_board_background(Canvas* canvas, MancalaState* state);
static void draw_numbers(Canvas* canvas, MancalaState* state);
static void draw_cursor(Canvas* canvas, MancalaState* state);
// Input handling functions ----------------------------------------------------
static void input_handler(InputEvent* evt, void* ctx);
static void perform_user_action_ok(MancalaState* state);
static void perform_ai_turn(MancalaState* state);

// =============================================================================
// GAME LOGIC IMPLEMENTATION (Kalah rules)
// =============================================================================
static void init_game(MancalaState* s) {
    // Initialize all pits with equal number of starting stones
    for(int i = 0; i < 6; i++) {
        s->pits_user[i] = INIT_STONES;
        s->pits_ai[i] = INIT_STONES;
    }

    // Empty stores
    s->store_user = 0;
    s->store_ai = 0;
    s->turn = TURN_USER;
    s->cursor = 0; // Default cursor position (left pit)
    s->game_over = false;
    s->ai_ready = false;
    strcpy(s->status_text, ""); // Empty status line
    s->redraw = true;
}

/**
 * Collect all remaining stones from both sides into respective stores
 * Called when game ends
 */
static void collect_remaining_to_stores(MancalaState* s) {
    // Collect user's remaining stones
    int sum = 0;
    for(int i = 0; i < 6; i++) {
        sum += s->pits_user[i];
        s->pits_user[i] = 0;
    }
    s->store_user += sum;

    // Collect AI's remaining stones
    sum = 0;
    for(int i = 0; i < 6; i++) {
        sum += s->pits_ai[i];
        s->pits_ai[i] = 0;
    }
    s->store_ai += sum;
}

/**
 * Check if game should end (one side has no stones)
 * If so, collect remaining stones and determine winner
 */
static void end_game_if_needed(MancalaState* s) {
    bool user_empty = true;
    bool ai_empty = true;

    // Check if either side has no stones left
    for(int i = 0; i < 6; i++) {
        if(s->pits_user[i] != 0) user_empty = false;
        if(s->pits_ai[i] != 0) ai_empty = false;
    }

    // If either side is empty, game ends
    if(user_empty || ai_empty) {
        collect_remaining_to_stores(s);
        s->game_over = true;

        // Determine winner
        if(s->store_user > s->store_ai) {
            strcpy(s->status_text, "YOU WIN");
        } else if(s->store_user < s->store_ai) {
            strcpy(s->status_text, "YOU LOSE");
        } else {
            strcpy(s->status_text, "DRAW");
        }
    }
}

/**
 * Perform a move from a selected pit
 * 
 * @param s Game state
 * @param pit_index Index (0..5) of the pit to move from (left to right)
 * @param by_user true if user is moving, false if AI is moving
 * 
 * Implements standard Kalah rules:
 * - Sow stones counter-clockwise around the board
 * - Skip opponent's store when sowing
 * - Extra turn if last stone lands in own store
 * - Capture if last stone lands in own empty pit (opposite pit must have stones)
 * 
 * Board layout for counter-clockwise sowing:
 *     AI pits: [5] [4] [3] [2] [1] [0]
 * AI Store [  ]                      [  ] User Store
 *    User pits: [0] [1] [2] [3] [4] [5]
 * 
 * Sowing direction: User pits increase left-to-right (0→5), 
 *                   then user store, then AI pits decrease right-to-left (5→0),
 *                   then AI store, then wrap back to user pits
 */
static void do_move_from(MancalaState* s, int pit_index, bool by_user) {
    // Validate game state and pit selection
    if(s->game_over) return;
    if(pit_index < 0 || pit_index > 5) return;

    // Get stones from the selected pit
    int stones = by_user ? s->pits_user[pit_index] : s->pits_ai[pit_index];
    if(stones == 0) return;

    // Clear the pit
    if(by_user) {
        s->pits_user[pit_index] = 0;
    } else {
        s->pits_ai[pit_index] = 0;
    }

    // Track current position as we sow stones counter-clockwise
    // We'll use a state machine approach: which side and which position
    BoardSection current_section;
    int current_pit_index;

    // Initialize starting position based on who is moving
    if(by_user) {
        current_section = SECTION_USER_PITS;
        current_pit_index = pit_index;
    } else {
        current_section = SECTION_AI_PITS;
        current_pit_index = pit_index;
    }

    // Variables to track where the last stone lands
    BoardSection last_section = SECTION_USER_PITS;
    int last_pit_index = -1;

    // Sow stones counter-clockwise
    while(stones > 0) {
        // Move to next position counter-clockwise
        if(current_section == SECTION_USER_PITS) {
            current_pit_index++;
            if(current_pit_index > 5) {
                // Reached end of user pits, move to user store
                current_section = SECTION_USER_STORE;
            }
        } else if(current_section == SECTION_USER_STORE) {
            // Move from user store to AI pits (starting from rightmost = index 5)
            current_section = SECTION_AI_PITS;
            current_pit_index = 5;
        } else if(current_section == SECTION_AI_PITS) {
            current_pit_index--;
            if(current_pit_index < 0) {
                // Reached end of AI pits, move to AI store
                current_section = SECTION_AI_STORE;
            }
        } else { // SECTION_AI_STORE
            // Move from AI store back to user pits (starting from leftmost = index 0)
            current_section = SECTION_USER_PITS;
            current_pit_index = 0;
        }

        // Skip opponent's store (standard Kalah rule)
        if(by_user && current_section == SECTION_AI_STORE) {
            // User skips AI store, continue to user pits
            current_section = SECTION_USER_PITS;
            current_pit_index = 0;
        }
        if(!by_user && current_section == SECTION_USER_STORE) {
            // AI skips user store, continue to AI pits
            current_section = SECTION_AI_PITS;
            current_pit_index = 5;
        }

        // Place one stone in current position
        if(current_section == SECTION_USER_PITS) {
            s->pits_user[current_pit_index]++;
        } else if(current_section == SECTION_USER_STORE) {
            s->store_user++;
        } else if(current_section == SECTION_AI_PITS) {
            s->pits_ai[current_pit_index]++;
        } else { // SECTION_AI_STORE
            s->store_ai++;
        }

        // Record where this stone landed
        last_section = current_section;
        last_pit_index = current_pit_index;

        stones--;
    }

    // Check for extra turn: last stone landed in own store
    bool extra_turn = false;
    if(by_user && last_section == SECTION_USER_STORE) {
        extra_turn = true;
    }
    if(!by_user && last_section == SECTION_AI_STORE) {
        extra_turn = true;
    }

    // Check for capture (only if no extra turn occurred)
    // Rule: If last stone landed in an empty pit on own side,
    //       capture that stone plus all stones from the opposite pit
    if(!extra_turn) {
        if(by_user && last_section == SECTION_USER_PITS) {
            // User's last stone landed in a user pit
            // Check if it was empty before (now has exactly 1 stone)
            if(s->pits_user[last_pit_index] == 1) {
                // Calculate opposite pit: user pit i corresponds to AI pit (5-i)
                int opposite_idx = 5 - last_pit_index;
                int captured = s->pits_ai[opposite_idx];
                if(captured > 0) {
                    // Capture: take opposite pit stones and the landing stone
                    s->pits_ai[opposite_idx] = 0;
                    s->pits_user[last_pit_index] = 0;
                    s->store_user += (1 + captured);
                }
            }
        } else if(!by_user && last_section == SECTION_AI_PITS) {
            // AI's last stone landed in an AI pit
            // Check if it was empty before (now has exactly 1 stone)
            if(s->pits_ai[last_pit_index] == 1) {
                // Calculate opposite pit: AI pit i corresponds to user pit (5-i)
                int opposite_idx = 5 - last_pit_index;
                int captured = s->pits_user[opposite_idx];
                if(captured > 0) {
                    // Capture: take opposite pit stones and the landing stone
                    s->pits_user[opposite_idx] = 0;
                    s->pits_ai[last_pit_index] = 0;
                    s->store_ai += (1 + captured);
                }
            }
        }
    }

    // Set next turn based on extra turn rule
    if(extra_turn) {
        // Same player goes again
        s->turn = by_user ? TURN_USER : TURN_AI;
        if(by_user) {
            strcpy(s->status_text, "Extra turn!");
            s->ai_ready = false;
        } else {
            strcpy(s->status_text, "AI extra turn");
            // AI gets another turn immediately (auto-execute)
            s->ai_ready = false;
        }
    } else {
        // Switch to other player
        s->turn = by_user ? TURN_AI : TURN_USER;
        if(s->turn == TURN_AI) {
            // AI's turn - set ready flag so user can trigger it
            s->ai_ready = true;
            strcpy(s->status_text, "");
        } else {
            s->ai_ready = false;
            strcpy(s->status_text, "");
        }
    }

    // Check if game has ended
    end_game_if_needed(s);
    s->redraw = true;
}

// =============================================================================
// AI IMPLEMENTATION (Heuristic-based move selection)
// =============================================================================
/**
 * AI Strategy - prioritized evaluation:
 * 1. Extra turns (highest priority) - score +100
 * 2. Captures (medium priority) - score +50 plus captured stones
 * 3. Points gained - score +10 per stone added to store
 * 
 * @param s Current game state
 * @return Index (0..5) of best pit to move from, or -1 if no valid moves
 */
static int ai_choose_move(MancalaState* s) {
    int best_move = -1;
    int best_score = -1000;

    // Evaluate each possible move
    for(int i = 0; i < 6; i++) {
        if(s->pits_ai[i] == 0) continue; // Skip empty pits

        // Simulate move on temporary state copy
        MancalaState tmp = *s;

        // Perform simulated move using the same logic as do_move_from
        int stones = tmp.pits_ai[i];
        tmp.pits_ai[i] = 0;

        // Track position during sowing
        BoardSection current_section = SECTION_AI_PITS;
        int current_pit_index = i;

        BoardSection last_section = SECTION_AI_PITS;
        int last_pit_index = -1;

        // Sow stones counter-clockwise
        while(stones > 0) {
            // Move to next position
            if(current_section == SECTION_USER_PITS) {
                current_pit_index++;
                if(current_pit_index > 5) {
                    current_section = SECTION_USER_STORE;
                }
            } else if(current_section == SECTION_USER_STORE) {
                current_section = SECTION_AI_PITS;
                current_pit_index = 5;
            } else if(current_section == SECTION_AI_PITS) {
                current_pit_index--;
                if(current_pit_index < 0) {
                    current_section = SECTION_AI_STORE;
                }
            } else { // SECTION_AI_STORE
                current_section = SECTION_USER_PITS;
                current_pit_index = 0;
            }

            // AI skips user store
            if(current_section == SECTION_USER_STORE) {
                current_section = SECTION_AI_PITS;
                current_pit_index = 5;
            }

            // Place stone
            if(current_section == SECTION_USER_PITS) {
                tmp.pits_user[current_pit_index]++;
            } else if(current_section == SECTION_USER_STORE) {
                tmp.store_user++;
            } else if(current_section == SECTION_AI_PITS) {
                tmp.pits_ai[current_pit_index]++;
            } else { // SECTION_AI_STORE
                tmp.store_ai++;
            }

            last_section = current_section;
            last_pit_index = current_pit_index;
            stones--;
        }

        // Calculate score for this move
        int score = 0;

        // Check for extra turn (highest priority)
        bool extra_turn = (last_section == SECTION_AI_STORE);
        if(extra_turn) score += 100;

        // Check for capture (medium priority)
        if(!extra_turn && last_section == SECTION_AI_PITS) {
            if(tmp.pits_ai[last_pit_index] == 1) { // Was empty before
                int opposite_idx = 5 - last_pit_index;
                if(tmp.pits_user[opposite_idx] > 0) {
                    // Value capture by amount that would be captured
                    score += 50 + tmp.pits_user[opposite_idx];
                }
            }
        }

        // Favor moves that increase our score (base priority)
        score += (tmp.store_ai - s->store_ai) * 10;

        // Update best move if this is better
        if(score > best_score) {
            best_score = score;
            best_move = i;
        }
    }

    // Fallback: leftmost non-empty pit (should rarely be reached)
    if(best_move < 0) {
        for(int i = 0; i < 6; i++) {
            if(s->pits_ai[i] > 0) return i;
        }
    }

    return best_move;
}

// =============================================================================
// RENDERING FUNCTIONS
// =============================================================================
static void draw_board_background(Canvas* canvas, MancalaState* state) {
    canvas_draw_icon(canvas, 1, 1, &I_board);
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // App icon comes on top of the board
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "Mancala");
    canvas_set_font(canvas, FontSecondary);

    // Draw navigation button hints (only when available)
    if(state->game_over) {
        elements_button_left(canvas, "Exit");
    }
    if(state->turn == TURN_USER && !state->game_over) {
        if(state->cursor < 5) elements_button_right(canvas, "");
        if(state->cursor > 0) elements_button_left(canvas, "");
        elements_button_center(canvas, "Pick pit"); // Action button hint
    }
}

static void draw_numbers(Canvas* canvas, MancalaState* state) {
    char buf[8];
    // Write numbers for AI pits (top row, left to right)
    for(int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%d", state->pits_ai[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3); // Center text
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, ai_pit_y, buf);
    }
    // Write numbers for user pits (bottom row, left to right)
    for(int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%d", state->pits_user[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3); // Center text
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, user_pit_y, buf);
    }

    // Draw store values
    snprintf(buf, sizeof(buf), "%d", state->store_ai);
    canvas_draw_str(canvas, store_ai_x, store_ai_y, buf);

    snprintf(buf, sizeof(buf), "%d", state->store_user);
    canvas_draw_str(canvas, store_user_x, store_user_y, buf);
}

static void draw_cursor(Canvas* canvas, MancalaState* state) {
    // Draw cursor frame around the selected pit
    int i = state->cursor;
    int x = pit_start_x + (i * pit_spacing) - 7;
    int y = user_pit_y - 11;
    // Draw frame around selected pit
    canvas_draw_frame(canvas, x, y, 14, 14);
}

static void render_callback(Canvas* canvas, void* ctx) {
    MancalaState* state = (MancalaState*)ctx;
    canvas_clear(canvas);
    draw_board_background(canvas, state);
    draw_numbers(canvas, state);
    if(state->turn == TURN_USER && !state->game_over) {
        draw_cursor(canvas, state);
    }
    // Draw status message
    canvas_draw_str_aligned(canvas, status_x, status_y, AlignLeft, AlignTop, state->status_text);

    // Draw game over overlay if applicable
    if(state->game_over) {
        canvas_draw_str_aligned(
            canvas, status_x, status_y, AlignLeft, AlignTop, state->status_text);
        elements_button_center(canvas, "Play again");
    } else if(state->ai_ready) {
        elements_button_center(canvas, "Let Flipper calculate");
    }
}

// =============================================================================
// INPUT HANDLING FUNCTIONS
// =============================================================================
static void perform_user_action_ok(MancalaState* state) {
    // Restart game if it's over
    if(state->game_over) {
        init_game(state);
        return;
    }
    // Execute AI turn if ready and waiting for user confirmation
    if(state->ai_ready) {
        perform_ai_turn(state);
        state->ai_ready = false;
        return;
    }
    // Only allow moves on user's turn
    if(state->turn != TURN_USER) return;
    // Check if selected pit has stones
    int idx = state->cursor;
    if(state->pits_user[idx] == 0) {
        strcpy(state->status_text, "Empty pit!");
        state->redraw = true;
        return;
    }
    // Perform the move
    do_move_from(state, idx, true);
    state->redraw = true;
}

static void perform_ai_turn(MancalaState* state) {
    FURI_LOG_I(TAG, "perform_ai_turn() called.");
    if(state->game_over) return;

    // Choose best move using AI heuristics
    int pick = ai_choose_move(state);
    if(pick < 0) {
        // No valid moves available - end game
        end_game_if_needed(state);
        return;
    }

    // Perform AI move
    do_move_from(state, pick, false);
}

static void input_handler(InputEvent* evt, void* ctx) {
    MancalaState* state = (MancalaState*)ctx;

    // Long back press exits application
    if(evt->type == InputTypeLong && evt->key == InputKeyBack) {
        state->should_exit = true;
        return;
    }

    // Handle short button presses
    if(evt->type == InputTypeShort) {
        // Exit on left button when game is over
        if(evt->key == InputKeyLeft && state->game_over) {
            state->should_exit = true;
            return;
        }
        if(evt->key == InputKeyRight) {
            // Move cursor right (no wrap-around)
            if(state->cursor < 5) {
                state->cursor++;
                state->redraw = true;
            }
        } else if(evt->key == InputKeyLeft) {
            // Move cursor left (no wrap-around)
            if(state->cursor > 0) {
                state->cursor--;
                state->redraw = true;
            }
        } else if(evt->key == InputKeyOk) {
            // Select pit / restart game / execute AI turn
            perform_user_action_ok(state);
        }
    }
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================
int32_t mancala_main(void* p) {
    UNUSED(p);
    // Allocate and initialize game state
    MancalaState* state = malloc(sizeof(MancalaState));
    state->should_exit = false;
    init_game(state);

    // Create and configure view port
    state->view_port = view_port_alloc();
    view_port_draw_callback_set(state->view_port, render_callback, state);
    view_port_input_callback_set(state->view_port, input_handler, state);

    // Register with GUI system
    state->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);

    FURI_LOG_I(TAG, "Mancala game initialized and ready.");

    // Main game loop ----------------------------------------------------------
    while(!state->should_exit) {
        // Auto-execute AI extra turns without user interaction
        if(state->turn == TURN_AI && !state->game_over && !state->ai_ready) {
            // Small delay makes AI moves visible to user
            furi_delay_ms(750);
            perform_ai_turn(state);
        }

        // Update display if redraw flag is set
        if(state->redraw) {
            view_port_update(state->view_port);
            state->redraw = false;
        }

        // Small delay to prevent busy-waiting
        furi_delay_ms(50);
    }

    // Cleanup: Free all allocated resources
    FURI_LOG_I(TAG, "Exiting Mancala game.");
    view_port_enabled_set(state->view_port, false);
    gui_remove_view_port(state->gui, state->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(state->view_port);
    free(state);

    return 0;
}
