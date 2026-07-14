#include <furi.h> // Core Flipper Zero system library
#include <gui/gui.h> // GUI system for display rendering
#include <input/input.h> // Input handling for button events
#include <gui/elements.h> // GUI elements library for button hints and UI components
#include <stdlib.h> // Standard library for rand(), malloc(), etc.
#include <math.h>
#include "mitzi_hirn_icons.h"

#define TAG "Hirn" // Tag for logging

#define COLOR_REPEAT    false // Whether colors can repeat in the secret code
#define NUM_COLORS      6 // Number of available colors
#define NUM_PEGS        4 // Number of pegs in the code
#define PEG_Y_POSITION  22 // Vertical position for current guessing pegs
#define PEG_X_POSITION  10 // Horizontal position
#define FEEDBACK_RADIUS 3
#define MAX_ATTEMPTS    20 // Maximum number of guessing attempts
#define CURSOR_SIZE     20 // Size of cursor box (width and height)
#define HUD_X_POSITION  65 // X position for HUD (timer and attempts counter)
#define MAX_TIME_MS     (20 * 60 * 1000) // Maximum time in milliseconds

// ============================================================================
// Enumerations
// ============================================================================

// Color patterns (fill styles)
typedef enum {
    COLOR_NONE = 0, // Empty/unfilled
    COLOR_RED, // Solid fill
    COLOR_GREEN, // Horizontal lines
    COLOR_BLUE, // Vertical lines
    COLOR_YELLOW, // Diagonal lines (/)
    COLOR_PURPLE, // Diagonal lines (\)
    COLOR_ORANGE // Cross-hatch
} PegColor;

// Feedback peg types
typedef enum {
    FEEDBACK_NONE = 0, // No feedback
    FEEDBACK_BLACK, // Correct color, correct position
    FEEDBACK_WHITE // Correct color, wrong position
} FeedbackType;

// Game states
typedef enum {
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WON,
    STATE_LOST,
    STATE_REVEAL
} GameState;

// ============================================================================
// Data Structures
// ============================================================================

// Application state
typedef struct {
    GameState state;
    PegColor secret_code[NUM_PEGS];
    PegColor current_guess[NUM_PEGS];
    int cursor_position;
    int attempts_used;
    uint32_t start_time;
    uint32_t elapsed_time;

    // History of previous guesses and feedback
    PegColor guess_history[MAX_ATTEMPTS][NUM_PEGS];
    FeedbackType feedback_history[MAX_ATTEMPTS][NUM_PEGS];
} CodeBreakerState;

// ============================================================================
// Game Logic Functions
// ============================================================================

// Generate random secret code
static void generate_secret_code(CodeBreakerState* state) {
    FURI_LOG_I(TAG, "Generating secret code (COLOR_REPEAT=%d)", COLOR_REPEAT);

    if(COLOR_REPEAT) {
        // Colors can repeat
        for(int i = 0; i < NUM_PEGS; i++) {
            state->secret_code[i] = (rand() % NUM_COLORS) + 1;
        }
    } else {
        // No color repetition
        bool used[NUM_COLORS + 1] = {false};
        for(int i = 0; i < NUM_PEGS; i++) {
            PegColor color;
            do {
                color = (rand() % NUM_COLORS) + 1;
            } while(used[color]);
            used[color] = true;
            state->secret_code[i] = color;
        }
    }
    FURI_LOG_I(
        TAG,
        "Secret code: [%d, %d, %d, %d]",
        state->secret_code[0],
        state->secret_code[1],
        state->secret_code[2],
        state->secret_code[3]);
}

// Check if all pegs in current guess have been selected
static bool is_guess_complete(CodeBreakerState* state) {
    for(int i = 0; i < NUM_PEGS; i++) {
        if(state->current_guess[i] == COLOR_NONE) {
            return false;
        }
    }
    return true;
}

static void reset_game_state(CodeBreakerState* state) {
    state->state = STATE_PLAYING;
    state->cursor_position = 0;
    state->attempts_used = 0;
    state->start_time = furi_get_tick();
    state->elapsed_time = 0;

    for(int i = 0; i < NUM_PEGS; i++) {
        state->current_guess[i] = COLOR_NONE;
    }

    generate_secret_code(state);
}

// Check if current guess is different from previous guess
static bool is_guess_different(CodeBreakerState* state) {
    if(state->attempts_used == 0) {
        return true; // First guess is always different
    }

    for(int i = 0; i < NUM_PEGS; i++) {
        if(state->current_guess[i] != state->guess_history[state->attempts_used - 1][i]) {
            return true;
        }
    }
    return false;
}

// Evaluate the current guess and provide feedback
static void evaluate_guess(CodeBreakerState* state) {
    FURI_LOG_I(
        TAG,
        "Evaluating guess #%d: [%d, %d, %d, %d]",
        state->attempts_used + 1,
        state->current_guess[0],
        state->current_guess[1],
        state->current_guess[2],
        state->current_guess[3]);

    bool secret_used[NUM_PEGS] = {false};
    bool guess_used[NUM_PEGS] = {false};
    int feedback_idx = 0;

    // Initialize feedback to none
    for(int i = 0; i < NUM_PEGS; i++) {
        state->feedback_history[state->attempts_used][i] = FEEDBACK_NONE;
    }

    // First pass: check for exact matches (black pegs)
    for(int i = 0; i < NUM_PEGS; i++) {
        if(state->current_guess[i] == state->secret_code[i]) {
            state->feedback_history[state->attempts_used][feedback_idx++] = FEEDBACK_BLACK;
            secret_used[i] = true;
            guess_used[i] = true;
        }
    }

    // Second pass: check for color matches in wrong position (white pegs)
    for(int i = 0; i < NUM_PEGS; i++) {
        if(!guess_used[i]) {
            for(int j = 0; j < NUM_PEGS; j++) {
                if(!secret_used[j] && state->current_guess[i] == state->secret_code[j]) {
                    state->feedback_history[state->attempts_used][feedback_idx++] = FEEDBACK_WHITE;
                    secret_used[j] = true;
                    break;
                }
            }
        }
    }

    FURI_LOG_D(
        TAG,
        "Feedback: Black=%d, White=%d",
        feedback_idx > 0 && state->feedback_history[state->attempts_used][0] == FEEDBACK_BLACK ?
            1 :
            0,
        feedback_idx);

    // Check for win condition (all black pegs)
    bool won = true;
    for(int i = 0; i < NUM_PEGS; i++) {
        if(state->current_guess[i] != state->secret_code[i]) {
            won = false;
            break;
        }
    }

    // Save guess to history
    for(int i = 0; i < NUM_PEGS; i++) {
        state->guess_history[state->attempts_used][i] = state->current_guess[i];
    }

    state->attempts_used++;

    if(won) {
        state->state = STATE_WON;
        state->elapsed_time += furi_get_tick() - state->start_time;
        FURI_LOG_I(
            TAG, "Game won! Attempts: %d, Time: %lu ms", state->attempts_used, state->elapsed_time);
    } else if(state->attempts_used >= MAX_ATTEMPTS) {
        state->state = STATE_LOST;
        state->elapsed_time += furi_get_tick() - state->start_time;
        FURI_LOG_I(TAG, "Game lost! Max attempts reached.");
    }
    // Don't reset guess - keep previous colors for next attempt
}

// ============================================================================
// Drawing Functions
// ============================================================================

// Draw modal dialog box with text
static void draw_simple_modal(Canvas* canvas, const char* text) {
    canvas_set_font(canvas, FontPrimary);
    const int box_w = canvas_string_width(canvas, text) + 6;
    const int box_h = 20;
    const int box_x = 2;
    const int box_y = 20;
    // White filled rectangle
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
    // Black border
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);
    // Text inside
    canvas_draw_str(canvas, box_x + 2, box_y + 14, text);
    canvas_set_font(canvas, FontSecondary);
}

// Draw a filled circle with pattern
static void draw_peg(Canvas* canvas, int x, int y, int radius, PegColor color) {
    switch(color) {
    case COLOR_NONE:
        // Empty circle
        canvas_draw_circle(canvas, x, y, radius);
        break;
    case COLOR_RED:
        // Solid fill
        canvas_draw_disc(canvas, x, y, radius);
        break;
    case COLOR_GREEN:
        // Horizontal lines
        canvas_draw_circle(canvas, x, y, radius);
        for(int i = -radius; i <= radius; i += 3) {
            if(y + i >= 0) {
                int half_width = (int)sqrt(radius * radius - i * i);
                canvas_draw_line(canvas, x - half_width, y + i, x + half_width, y + i);
            }
        }
        break;
    case COLOR_BLUE:
        // Vertical lines
        canvas_draw_circle(canvas, x, y, radius);
        for(int i = -radius; i <= radius; i += 3) {
            if(x + i >= 0) {
                int half_height = (int)sqrt(radius * radius - i * i);
                canvas_draw_line(canvas, x + i, y - half_height, x + i, y + half_height);
            }
        }
        break;
    case COLOR_YELLOW:
        // Diagonal lines (/)
        canvas_draw_circle(canvas, x, y, radius);
        for(int offset = -radius * 2; offset <= radius * 2; offset += 4) {
            for(int i = -radius; i <= radius; i++) {
                int j = i + offset;
                if(i * i + j * j <= radius * radius) {
                    if(x + i >= 0 && y - j >= 0) {
                        canvas_draw_dot(canvas, x + i, y - j);
                    }
                }
            }
        }
        break;
    case COLOR_PURPLE:
        // Diagonal lines (\)
        canvas_draw_circle(canvas, x, y, radius);
        for(int offset = -radius * 2; offset <= radius * 2; offset += 4) {
            for(int i = -radius; i <= radius; i++) {
                int j = -i + offset;
                if(i * i + j * j <= radius * radius) {
                    if(x + i >= 0 && y - j >= 0) {
                        canvas_draw_dot(canvas, x + i, y - j);
                    }
                }
            }
        }
        break;
    case COLOR_ORANGE:
        // Cross-hatch
        canvas_draw_circle(canvas, x, y, radius);
        for(int i = -radius; i <= radius; i += 3) {
            if(y + i >= 0) {
                int half_width = (int)sqrt(radius * radius - i * i);
                canvas_draw_line(canvas, x - half_width, y + i, x + half_width, y + i);
            }
            if(x + i >= 0) {
                int half_height = (int)sqrt(radius * radius - i * i);
                canvas_draw_line(canvas, x + i, y - half_height, x + i, y + half_height);
            }
        }
        break;
    }
}

// Draw feedback pegs (2x2 arrangement)
static void
    draw_feedback(Canvas* canvas, int x, int y, FeedbackType feedback[NUM_PEGS], int radius) {
    int spacing = radius * 2 + 2; // Space between feedback pegs
    int positions[4][2] = {{0, 0}, {spacing, 0}, {0, spacing}, {spacing, spacing}};

    for(int i = 0; i < NUM_PEGS; i++) {
        int px = x + positions[i][0];
        int py = y + positions[i][1];
        canvas_draw_circle(canvas, px, py, radius); // draw circle outline
        if(feedback[i] == FEEDBACK_BLACK) {
            canvas_draw_disc(canvas, px, py, radius);
        } else if(feedback[i] == FEEDBACK_WHITE) { // grey dot pattern fill
            for(int dy = -radius; dy <= radius; dy += 2) {
                for(int dx = -radius; dx <= radius; dx += 2) {
                    if(dx * dx + dy * dy <= radius * radius) {
                        canvas_draw_dot(canvas, px + dx, py + dy);
                    }
                }
            }
        }
    }
}

// ============================================================================
// GUI Callback Functions
// ============================================================================

// Draw callback
static void draw_callback(Canvas* canvas, void* ctx) {
    CodeBreakerState* state = (CodeBreakerState*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Draw header with icon and title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "HIRN");
    canvas_set_font(canvas, FontSecondary);

    // Draw HUD (top right)
    char time_str[16];
    uint32_t total_time;
    if(state->state == STATE_PLAYING) {
        total_time = state->elapsed_time + (furi_get_tick() - state->start_time);
    } else {
        total_time = state->elapsed_time;
    }
    if(total_time > MAX_TIME_MS) total_time = MAX_TIME_MS;
    uint32_t seconds = total_time / 1000;
    uint32_t minutes = seconds / 60;
    seconds = seconds % 60;
    snprintf(
        time_str,
        sizeof(time_str),
        "A: %d(%d) %02lu:%02lu",
        state->attempts_used,
        MAX_ATTEMPTS,
        minutes,
        seconds);
    canvas_draw_str(canvas, HUD_X_POSITION, 7, time_str);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignTop, "f418.eu");
    canvas_draw_str_aligned(canvas, 127, 16, AlignRight, AlignTop, "v0.2");

    // Draw current guess area
    int peg_radius = CURSOR_SIZE / 2 - 2; // Peg radius is slightly smaller than half cursor
    int peg_spacing = CURSOR_SIZE; // Pegs touch when cursors would touch
    int feedback_radius = (CURSOR_SIZE / 8 > 2) ? CURSOR_SIZE / 8 :
                                                  3; // Feedback pegs scale with cursor, min 3
    int guess_y = PEG_Y_POSITION; // Vertical position

    for(int i = 0; i < NUM_PEGS; i++) {
        int x = PEG_X_POSITION + i * peg_spacing;

        // Draw cursor rectangle
        if(i == state->cursor_position && state->state == STATE_PLAYING) {
            canvas_draw_rframe(
                canvas,
                x - CURSOR_SIZE / 2,
                guess_y - CURSOR_SIZE / 2,
                CURSOR_SIZE + 1,
                CURSOR_SIZE + 1,
                2);
        }

        draw_peg(canvas, x, guess_y, peg_radius, state->current_guess[i]);
    }

    // Draw last guess from history (directly below current guess)
    if(state->attempts_used > 0) {
        int history_y = guess_y + peg_spacing; // Below current guess with spacing
        for(int i = 0; i < NUM_PEGS; i++) {
            int x = PEG_X_POSITION + i * peg_spacing;
            draw_peg(
                canvas,
                x,
                history_y,
                peg_radius - 2,
                state->guess_history[state->attempts_used - 1][i]);
        }
        draw_feedback(
            canvas,
            PEG_X_POSITION + NUM_PEGS * peg_spacing + 5,
            history_y - 5,
            state->feedback_history[state->attempts_used - 1],
            feedback_radius);
    }

    // Construct status message
    const char* modal_text = NULL;
    if(state->state == STATE_PAUSED) {
        modal_text = "Paused.";
    } else if(state->state == STATE_WON) {
        modal_text = "You won!";
    } else if(state->state == STATE_LOST) {
        if(total_time >= MAX_TIME_MS) {
            modal_text = "Time out :-(";
        } else {
            modal_text = "No attempts left :-(";
        }
    }

    // Draw secret code if revealed or game ended
    if(state->state == STATE_REVEAL || state->state == STATE_WON || state->state == STATE_LOST) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 120, "Code:");
        for(int i = 0; i < NUM_PEGS; i++) {
            draw_peg(canvas, 45 + i * 20, 120, 8, state->secret_code[i]);
        }
    }

    if(modal_text) {
        draw_simple_modal(canvas, modal_text);
        // When modal is shown, only show exit hint
        canvas_draw_icon(canvas, 121, 57, &I_back);
        canvas_draw_str_aligned(canvas, 120, 63, AlignRight, AlignBottom, "Exit");
    } else {
        // Normal hints
        canvas_draw_icon(canvas, 1, 55, &I_arrows);
        canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Navigate");
        canvas_draw_icon(canvas, 121, 57, &I_back);
        canvas_draw_str_aligned(canvas, 120, 63, AlignRight, AlignBottom, "Pause");
    }

    if(state->state == STATE_PLAYING && is_guess_complete(state) && is_guess_different(state)) {
        elements_button_center(canvas, "OK");
    } else if(state->state == STATE_PAUSED) {
        elements_button_center(canvas, "Resume");
    } else if(state->state == STATE_WON || state->state == STATE_LOST) {
        elements_button_center(canvas, "Play again");
    }
}

// Input callback
static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// ============================================================================
// Main Application Entry Point
// ============================================================================

// Main application entry point
int32_t hirn_main(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Starting HIRN game");
    srand(furi_get_tick());
    FURI_LOG_D(TAG, "Random seed initialized with tick: %lu", furi_get_tick());
    CodeBreakerState* state = malloc(sizeof(CodeBreakerState));
    memset(state, 0, sizeof(CodeBreakerState));
    FURI_LOG_D(TAG, "State allocated and initialized"); // --------------------
    reset_game_state(state);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FURI_LOG_D(TAG, "Event queue created");

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    FURI_LOG_I(TAG, "GUI initialized and view port added");

    // Main loop
    InputEvent event;
    bool running = true;

    FURI_LOG_I(TAG, "Entering main game loop");

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(event.key == InputKeyBack) {
                    if(event.type == InputTypePress) {
                        // Short press - pause (when playing) or exit (when paused)
                        if(state->state == STATE_PAUSED) {
                            // Exit when paused
                            FURI_LOG_I(TAG, "User exiting from pause menu");
                            running = false;
                        } else if(state->state == STATE_PLAYING) {
                            // Pause when playing
                            FURI_LOG_I(TAG, "Game paused");
                            state->state = STATE_PAUSED;
                            state->elapsed_time += furi_get_tick() - state->start_time;
                        }
                    }
                } else if(event.key == InputKeyLeft && state->state == STATE_PLAYING) {
                    if(state->cursor_position > 0) {
                        state->cursor_position--;
                        FURI_LOG_D(
                            TAG, "Cursor moved left to position %d", state->cursor_position);
                    }
                } else if(event.key == InputKeyRight && state->state == STATE_PLAYING) {
                    if(state->cursor_position < NUM_PEGS - 1) {
                        state->cursor_position++;
                        FURI_LOG_D(
                            TAG, "Cursor moved right to position %d", state->cursor_position);
                    }
                } else if(event.key == InputKeyUp && state->state == STATE_PLAYING) {
                    int current = state->current_guess[state->cursor_position];
                    current++;
                    if(current > NUM_COLORS) current = COLOR_NONE;
                    state->current_guess[state->cursor_position] = current;
                    FURI_LOG_D(
                        TAG, "Color changed to %d at position %d", current, state->cursor_position);
                } else if(event.key == InputKeyDown && state->state == STATE_PLAYING) {
                    int current = state->current_guess[state->cursor_position];
                    current--;
                    if(current < COLOR_NONE) current = NUM_COLORS;
                    state->current_guess[state->cursor_position] = current;
                    FURI_LOG_D(
                        TAG, "Color changed to %d at position %d", current, state->cursor_position);
                } else if(event.key == InputKeyOk) {
                    if(state->state == STATE_PLAYING && is_guess_complete(state) &&
                       is_guess_different(state)) {
                        FURI_LOG_I(TAG, "Submitting guess");
                        evaluate_guess(state);
                    } else if(state->state == STATE_PAUSED) {
                        FURI_LOG_I(TAG, "Game resumed");
                        state->state = STATE_PLAYING;
                        state->start_time = furi_get_tick();
                    } else if(state->state == STATE_REVEAL) {
                        FURI_LOG_I(TAG, "Returning to game from reveal");
                        state->state = STATE_PLAYING;
                        state->start_time = furi_get_tick();
                    } else if(state->state == STATE_WON || state->state == STATE_LOST) {
                        // Reset game
                        FURI_LOG_I(TAG, "Resetting game for new round");
                        reset_game_state(state);
                    }
                }
            } else if(event.type == InputTypeLong) {
                if(event.key == InputKeyBack) {
                    // Long press - exit
                    FURI_LOG_I(TAG, "User exiting via long press");
                    running = false;
                } else if(event.key == InputKeyOk) {
                    // Long press - reveal combination
                    if(state->state == STATE_PLAYING) {
                        FURI_LOG_W(TAG, "Secret code revealed by user");
                        state->elapsed_time += furi_get_tick() - state->start_time;
                        state->state = STATE_REVEAL;
                    } else if(state->state == STATE_REVEAL) {
                        FURI_LOG_I(TAG, "Hiding secret code");
                        state->state = STATE_PLAYING;
                        state->start_time = furi_get_tick();
                    }
                }
            }

            view_port_update(view_port);
        }

        // Update display for timer
        if(state->state == STATE_PLAYING) {
            view_port_update(view_port);
        }
        // Check time limit
        if(state->state == STATE_PLAYING) {
            uint32_t current_time = state->elapsed_time + (furi_get_tick() - state->start_time);
            if(current_time >= MAX_TIME_MS) {
                FURI_LOG_W(TAG, "Time limit reached - game lost");
                state->state = STATE_LOST;
                state->elapsed_time = MAX_TIME_MS;
                view_port_update(view_port);
            }
        }
    }
    FURI_LOG_I(TAG, "Cleaning up and exiting");
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(event_queue);
    free(state);
    FURI_LOG_I(TAG, "HIRN game stopped");

    return 0;
}
