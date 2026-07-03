#include <furi.h> // Core Flipper Zero API (types, memory, threads)
#include <gui/gui.h> // GUI system and canvas drawing functions
#include <gui/elements.h> // UI elements like button hints
#include <gui/modules/text_input.h> // Standard text input keyboard module
#include <gui/view_dispatcher.h> // View management and navigation system
#include <mitzi_tyaid_icons.h> // Auto-generated header for icons in images/ folder
#include "t9plus.h" // a T9 inspired word prediction system

// Debug tag for logging
#define TAG "TypeAid"

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================
#define TEXT_BUFFER_SIZE       256
#define LINE_SPACING           9
#define Y_VALUE_DIVIDER        8
#define SPECIAL_KEY_BACK_LINE  0
#define SPECIAL_KEY_SHIFT_LINE 3
#define SPECIAL_KEY_SPACE_LINE 4
#define T9_LINE_BACK_OFFSET    10
#define T9_LINE_SHIFT_OFFSET   30
#define T9_LINE_SPACE_OFFSET   25
static const char* t9_lines[] = {"1234567890", "qwertzuiop[]", "asdfghjkl'", "yxcvbnm,;.:-", ""};
static const char* t9_lines_upper[] =
    {"!\"§$%&@()#", "QWERTZUIOP[]", "ASDFGHJKL'", "YXCVBNM,;.:-", ""};

// ============================================================================
// TYPES AND STRUCTURES
// ============================================================================

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    TextInput* text_input;
    ViewDispatcher* view_dispatcher;
    ViewPort* t9_view_port;

    char text_buffer[TEXT_BUFFER_SIZE];
    bool in_text_input; // Track which screen we're on
    bool keyboard_used; // Track if keyboard has been opened at least once

    // Suggestion cache
    char cached_suggestions[T9PLUS_MAX_SUGGESTIONS][T9PLUS_MAX_WORD_LENGTH];
    uint8_t cached_suggestion_count;
    char last_buffer_for_suggestions[TEXT_BUFFER_SIZE]; // Track when to update suggestions

    // Suggestion selection state
    int8_t selected_suggestion; // -1 = none, 0-2 = suggestion index
    char original_word[TEXT_BUFFER_SIZE]; // Store original typed text before previewing suggestions
} TypeAidApp;

// ============================================================================
// T9-MINUS SCREEN - TYPES AND DATA
// ============================================================================
typedef struct {
    uint8_t line; // 0-3
    int8_t pos; // position within line (can be -1 for shft button)
} T9Cursor;

static T9Cursor t9_cursor = {0, 0};
static bool shift_locked = false;

// ============================================================================
// T9-MINUS SCREEN - DRAW CALLBACK
// ============================================================================

static void t9_draw_callback(Canvas* canvas, void* context) {
    TypeAidApp* app = context;
    if(!app) {
        return;
    }
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    if(strlen(app->text_buffer) > 0) { // Display current text buffer
        canvas_draw_str(canvas, 0, LINE_SPACING - 1, app->text_buffer);
    }
    // Draw blinking cursor (always, even when buffer is empty)
    if((furi_get_tick() / 500) % 2 == 0) { // Blink every 500ms
        uint8_t text_width =
            strlen(app->text_buffer) > 0 ? canvas_string_width(canvas, app->text_buffer) : 0;
        canvas_draw_str(canvas, text_width, LINE_SPACING - 1, "|");
    }
    // Draw horizontal line under text field
    canvas_draw_line(canvas, 0, Y_VALUE_DIVIDER, 128, Y_VALUE_DIVIDER);

    // Draw word suggestions or error message between buffer and divider
    const uint8_t sugg_y = Y_VALUE_DIVIDER + LINE_SPACING;

    // Check for word suggestion error message first
    const char* error_msg = t9plus_get_error_message();
    if(error_msg != NULL) {
        // Display error message
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, sugg_y, error_msg);
    } else if(app->cached_suggestion_count > 0) {
        // Display word suggestions with highlighting for selected one
        uint8_t x_pos = 2;
        for(uint8_t i = 0; i < app->cached_suggestion_count; i++) {
            // Use bold font for selected suggestion
            if(i == app->selected_suggestion) {
                canvas_set_font(canvas, FontPrimary);
            } else {
                canvas_set_font(canvas, FontSecondary);
            }

            canvas_draw_str(canvas, x_pos, sugg_y, app->cached_suggestions[i]);
            x_pos += strlen(app->cached_suggestions[i]) * 6 + 4; // Approximate spacing

            // Add space between suggestions
            if(i < app->cached_suggestion_count - 1) {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, x_pos, sugg_y, " ");
                x_pos += 4;
            }
        }
    }

    // Keyboard lines follow below
    const uint8_t start_y = sugg_y + LINE_SPACING;
    const uint8_t char_spacing = 9;
    for(uint8_t line = 0; line < 5; line++) {
        const char* line_str = shift_locked ? t9_lines_upper[line] : t9_lines[line];
        size_t line_len = strlen(line_str);
        uint8_t start_x = 2;

        if(line == SPECIAL_KEY_BACK_LINE) { // Draw backspace button
            uint8_t bksp_x = start_x + (line_len * char_spacing);
            uint8_t bksp_y = start_y + (line * LINE_SPACING);

            bool is_bksp_cursor =
                (t9_cursor.line == SPECIAL_KEY_BACK_LINE && t9_cursor.pos == (int8_t)line_len);
            if(is_bksp_cursor) {
                canvas_set_font(canvas, FontPrimary);
            } else {
                canvas_set_font(canvas, FontSecondary);
            }
            canvas_draw_str(canvas, bksp_x, bksp_y, "[<]");
        }
        if(line == SPECIAL_KEY_SHIFT_LINE) { // Draw "shft" button
            uint8_t shft_x = start_x;
            uint8_t shft_y = start_y + (line * LINE_SPACING);

            bool is_shft_cursor =
                (t9_cursor.line == SPECIAL_KEY_SHIFT_LINE && t9_cursor.pos == -1);
            if(is_shft_cursor || shift_locked) {
                canvas_set_font(canvas, FontPrimary);
            } else {
                canvas_set_font(canvas, FontSecondary);
            }
            canvas_draw_str(canvas, shft_x, shft_y, "shft");
            start_x += T9_LINE_SHIFT_OFFSET; // Space for "shft" + gap
            canvas_set_font(canvas, FontSecondary); // Reset font after shift button
        }
        if(line == SPECIAL_KEY_SPACE_LINE) { // Draw "[space]" button
            uint8_t space_x = T9_LINE_SPACE_OFFSET;
            uint8_t space_y = start_y + (line * LINE_SPACING);

            bool is_space_cursor =
                (t9_cursor.line == SPECIAL_KEY_SPACE_LINE && t9_cursor.pos == -1);
            if(is_space_cursor) {
                canvas_set_font(canvas, FontPrimary);
            } else {
                canvas_set_font(canvas, FontSecondary);
            }
            canvas_draw_str(canvas, space_x, space_y, "[space]");
            canvas_set_font(canvas, FontSecondary);
        }

        for(size_t i = 0; i < line_len; i++) {
            uint8_t x = start_x + (i * char_spacing);
            uint8_t y = start_y + (line * LINE_SPACING);

            // Set bold font for cursor position
            bool is_cursor = (line == t9_cursor.line && (int8_t)i == t9_cursor.pos);

            if(is_cursor) {
                canvas_set_font(canvas, FontPrimary);
            } else {
                canvas_set_font(canvas, FontSecondary);
            }

            char single_char[2] = {line_str[i], '\0'};
            canvas_draw_str(canvas, x, y, single_char);
        }
    }

    // Draw navigation hints
    canvas_draw_icon(canvas, 0, 55, &I_back);
    canvas_draw_str_aligned(canvas, 8, 62, AlignLeft, AlignBottom, "OK");

    canvas_draw_str_aligned(canvas, 128, 62, AlignRight, AlignBottom, "Hold > to compl.");
}

// ============================================================================
// T9-MINUS SCREEN - INPUT CALLBACK
// ============================================================================

static void t9_input_callback(InputEvent* input_event, void* context) {
    TypeAidApp* app = context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// ============================================================================
// T9-MINUS SCREEN - NAVIGATION
// ============================================================================

// Forward declaration
static void t9_update_suggestions(TypeAidApp* app);

// Helper function to get the last word position in buffer
static size_t get_last_word_start(const char* buffer) {
    size_t len = strlen(buffer);
    if(len == 0) return 0;

    // Find the start of the last word
    size_t pos = len;
    while(pos > 0 && buffer[pos - 1] != ' ') {
        pos--;
    }
    return pos;
}

// Helper function to replace last word in buffer with suggestion
static void replace_last_word_with_suggestion(TypeAidApp* app, const char* suggestion) {
    size_t last_word_pos = get_last_word_start(app->text_buffer);

    // Copy everything before the last word
    char new_buffer[TEXT_BUFFER_SIZE];
    strncpy(new_buffer, app->text_buffer, last_word_pos);
    new_buffer[last_word_pos] = '\0';

    // Manually append the suggestion (avoiding strncat)
    size_t current_len = strlen(new_buffer);
    size_t suggestion_len = strlen(suggestion);
    size_t space_left = TEXT_BUFFER_SIZE - current_len - 1;

    if(suggestion_len > space_left) {
        suggestion_len = space_left;
    }

    memcpy(new_buffer + current_len, suggestion, suggestion_len);
    new_buffer[current_len + suggestion_len] = '\0';

    // Update buffer
    strncpy(app->text_buffer, new_buffer, TEXT_BUFFER_SIZE - 1);
    app->text_buffer[TEXT_BUFFER_SIZE - 1] = '\0';
}

// Helper function to cycle through suggestions
static void t9_cycle_suggestion(TypeAidApp* app) {
    if(app->cached_suggestion_count == 0) {
        return; // No suggestions to cycle through
    }

    // Save original word on first cycle
    if(app->selected_suggestion == -1) {
        size_t last_word_pos = get_last_word_start(app->text_buffer);
        strncpy(app->original_word, app->text_buffer + last_word_pos, TEXT_BUFFER_SIZE - 1);
        app->original_word[TEXT_BUFFER_SIZE - 1] = '\0';
    }

    // Move to next suggestion
    app->selected_suggestion++;

    // Wrap around: after last suggestion, go back to original
    if(app->selected_suggestion >= (int8_t)app->cached_suggestion_count) {
        app->selected_suggestion = -1;
        // Restore original word
        replace_last_word_with_suggestion(app, app->original_word);
    } else {
        // Show the selected suggestion in buffer
        replace_last_word_with_suggestion(app, app->cached_suggestions[app->selected_suggestion]);
    }

    FURI_LOG_I(
        TAG, "Cycled to suggestion %d, buffer: '%s'", app->selected_suggestion, app->text_buffer);
}

// Helper function to accept currently selected suggestion
static void t9_accept_suggestion(TypeAidApp* app) {
    if(app->selected_suggestion >= 0 &&
       app->selected_suggestion < (int8_t)app->cached_suggestion_count) {
        // Suggestion is already in buffer, add space, and just reset selection state
        size_t current_len = strlen(app->text_buffer);
        if(current_len < TEXT_BUFFER_SIZE - 1) {
            app->text_buffer[current_len] = ' ';
            app->text_buffer[current_len + 1] = '\0';
        }

        // Update suggestions for the newly accepted word
        t9_update_suggestions(app);

        FURI_LOG_I(TAG, "Accepted suggestion, buffer: '%s'", app->text_buffer);
    }
}

// Helper function to update suggestion cache when buffer changes
static void t9_update_suggestions(TypeAidApp* app) {
    // Only update if buffer has changed
    if(strcmp(app->text_buffer, app->last_buffer_for_suggestions) == 0) {
        return; // Buffer unchanged, use cached suggestions
    }

    // Buffer changed - reset selection state
    app->selected_suggestion = -1;
    app->original_word[0] = '\0';

    // Update cache
    strncpy(app->last_buffer_for_suggestions, app->text_buffer, TEXT_BUFFER_SIZE - 1);
    app->last_buffer_for_suggestions[TEXT_BUFFER_SIZE - 1] = '\0';

    // Clear old suggestions
    memset(app->cached_suggestions, 0, sizeof(app->cached_suggestions));
    app->cached_suggestion_count = 0;

    // Get new suggestions
    if(strlen(app->text_buffer) > 0) {
        app->cached_suggestion_count = t9plus_get_suggestions(
            app->text_buffer, app->cached_suggestions, T9PLUS_MAX_SUGGESTIONS);
    }
}

static void t9_move_cursor(int8_t line_delta, int8_t pos_delta) {
    if(line_delta != 0) {
        int8_t new_line = t9_cursor.line + line_delta;
        if(new_line >= 0 && new_line < 5) {
            t9_cursor.line = new_line;
            // Clamp position to new line length
            int8_t min_pos = (t9_cursor.line == SPECIAL_KEY_SHIFT_LINE ||
                              t9_cursor.line == SPECIAL_KEY_SPACE_LINE) ?
                                 -1 :
                                 0; // Lines with special buttons at -1
            int8_t max_pos = strlen(t9_lines[t9_cursor.line]) - 1;
            // Allow backspace position on line 0
            if(t9_cursor.line == SPECIAL_KEY_BACK_LINE) max_pos++;

            if(t9_cursor.pos < min_pos) {
                t9_cursor.pos = min_pos;
            } else if(t9_cursor.pos > max_pos) {
                t9_cursor.pos = max_pos;
            }
        }
    }

    if(pos_delta != 0) {
        int8_t new_pos = t9_cursor.pos + pos_delta;
        int8_t min_pos = (t9_cursor.line == 3 || t9_cursor.line == 4) ? -1 : 0;
        int8_t max_pos = strlen(t9_lines[t9_cursor.line]) - 1;
        // Allow backspace position on line 0
        if(t9_cursor.line == SPECIAL_KEY_BACK_LINE) max_pos++;
        if(new_pos >= min_pos && new_pos <= max_pos) {
            t9_cursor.pos = new_pos;
        }
    }
}

static void t9_add_character(TypeAidApp* app) {
    // Check if we're on line with the backspace button
    if(t9_cursor.line == SPECIAL_KEY_BACK_LINE && t9_cursor.pos == (int8_t)strlen(t9_lines[0])) {
        size_t current_len = strlen(app->text_buffer);
        if(current_len > 0) {
            app->text_buffer[current_len - 1] = '\0';
            FURI_LOG_I(TAG, "Deleted character, buffer now: '%s'", app->text_buffer);
            t9_update_suggestions(app);
            app->selected_suggestion = -1;
            app->original_word[0] = '\0';
        }
        return;
    }
    if(t9_cursor.line == SPECIAL_KEY_SHIFT_LINE &&
       t9_cursor.pos == -1) { // Check if we are on the SHIFT button
        shift_locked = !shift_locked;
        FURI_LOG_I(TAG, "Shift lock toggled: %s", shift_locked ? "ON" : "OFF");
        return;
    }
    if(t9_cursor.line == SPECIAL_KEY_SPACE_LINE &&
       t9_cursor.pos == -1) { // Check if we are on the SPACE button
        size_t current_len = strlen(app->text_buffer);
        if(current_len < TEXT_BUFFER_SIZE - 1) {
            app->text_buffer[current_len] = ' ';
            app->text_buffer[current_len + 1] = '\0';
            FURI_LOG_I(TAG, "Added space, buffer now: '%s'", app->text_buffer);
            t9_update_suggestions(app); // Update suggestions after adding space
        }
        return;
    }

    size_t current_len = strlen(app->text_buffer);
    if(current_len < TEXT_BUFFER_SIZE - 1) {
        const char* line_str = shift_locked ? t9_lines_upper[t9_cursor.line] :
                                              t9_lines[t9_cursor.line];
        char ch = line_str[t9_cursor.pos];
        app->text_buffer[current_len] = ch;
        app->text_buffer[current_len + 1] = '\0';
        FURI_LOG_I(TAG, "Added char '%c', buffer now: '%s'", ch, app->text_buffer);
        t9_update_suggestions(app); // Update suggestions after adding character
    }
}

// ============================================================================
// SPLASH SCREEN - DRAW CALLBACK
// ============================================================================
static void splash_draw_callback(Canvas* canvas, void* context) {
    FURI_LOG_D(TAG, "splash_draw_callback: enter");
    TypeAidApp* app = context;

    if(!app) {
        FURI_LOG_E(TAG, "splash_draw_callback: app is NULL!");
        return;
    }

    canvas_clear(canvas);

    // Draw splash icon only on first view (before keyboard is used)
    if(!app->keyboard_used) {
        canvas_draw_icon(canvas, 46, 1, &I_splash);
    }
    // Draw icon and title at the top
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Type Aid v0.1");

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Display entered text inside the box (truncated, no scrolling)
    if(strlen(app->text_buffer) > 0) {
        canvas_draw_frame(canvas, 0, 16, 128, 35);
        elements_multiline_text(canvas, 2, 25, app->text_buffer);
    } else {
        // Show placeholder text when no input yet
        canvas_draw_str_aligned(canvas, 1, 17, AlignLeft, AlignTop, "Try different");
        canvas_draw_str_aligned(canvas, 1, 26, AlignLeft, AlignTop, "keyboards");
    }

    // Draw navigation hints at bottom
    elements_button_center(canvas, "New");
    elements_button_right(canvas, "Dflt");
    canvas_draw_icon(canvas, 1, 55, &I_back);
    canvas_draw_str_aligned(canvas, 11, 63, AlignLeft, AlignBottom, "Exit");

    FURI_LOG_D(TAG, "splash_draw_callback: exit");
}

// ============================================================================
// SPLASH SCREEN - INPUT CALLBACK
// ============================================================================

static void splash_input_callback(InputEvent* input_event, void* context) {
    TypeAidApp* app = context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// ============================================================================
// TEXT INPUT - CALLBACK
// ============================================================================

static void text_input_callback(void* context) {
    FURI_LOG_I(TAG, "text_input_callback: text entered");
    TypeAidApp* app = context;

    if(!app) {
        FURI_LOG_E(TAG, "text_input_callback: app is NULL!");
        return;
    }

    FURI_LOG_I(TAG, "Text entered: '%s'", app->text_buffer);

    // Stop the view dispatcher to return to main loop
    view_dispatcher_stop(app->view_dispatcher);
}

// ============================================================================
// APP LIFECYCLE - ALLOCATION
// ============================================================================

static TypeAidApp* type_aid_app_alloc() {
    FURI_LOG_I(TAG, "=== App allocation started ===");

    TypeAidApp* app = malloc(sizeof(TypeAidApp));
    memset(app, 0, sizeof(TypeAidApp));

    FURI_LOG_D(TAG, "Opening GUI");
    app->gui = furi_record_open(RECORD_GUI);

    FURI_LOG_D(TAG, "Creating event queue");
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    FURI_LOG_D(TAG, "Creating viewport for splash");
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, splash_draw_callback, app);
    view_port_input_callback_set(app->view_port, splash_input_callback, app);

    FURI_LOG_D(TAG, "Creating viewport for T9");
    app->t9_view_port = view_port_alloc();
    view_port_draw_callback_set(app->t9_view_port, t9_draw_callback, app);
    view_port_input_callback_set(app->t9_view_port, t9_input_callback, app);

    FURI_LOG_D(TAG, "Creating view dispatcher");
    app->view_dispatcher = view_dispatcher_alloc();

    FURI_LOG_D(TAG, "Creating text input");
    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Enter your text:");
    text_input_set_result_callback(
        app->text_input, text_input_callback, app, app->text_buffer, TEXT_BUFFER_SIZE, false);

    // Add text input to view dispatcher
    view_dispatcher_add_view(app->view_dispatcher, 1, text_input_get_view(app->text_input));
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    FURI_LOG_D(TAG, "Adding splash viewport to GUI");
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->in_text_input = false;
    app->keyboard_used = false; // Initially, keyboard hasn't been used

    // Initialize suggestion cache
    memset(app->cached_suggestions, 0, sizeof(app->cached_suggestions));
    app->cached_suggestion_count = 0;
    memset(app->last_buffer_for_suggestions, 0, sizeof(app->last_buffer_for_suggestions));

    // Initialize suggestion selection state
    app->selected_suggestion = -1;
    memset(app->original_word, 0, sizeof(app->original_word));

    t9plus_init(); // Initialize T9+ prediction system

    FURI_LOG_I(TAG, "=== App allocation complete ===");
    return app;
}

// ============================================================================
// APP LIFECYCLE - CLEANUP
// ============================================================================

static void type_aid_app_free(TypeAidApp* app) {
    FURI_LOG_I(TAG, "=== App cleanup started ===");

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    view_port_free(app->t9_view_port);

    view_dispatcher_remove_view(app->view_dispatcher, 1);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);

    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);

    t9plus_deinit(); // Clean up T9+ prediction system

    FURI_LOG_I(TAG, "=== App cleanup complete ===");
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int32_t type_aid_main(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "App TYAID starting");
    TypeAidApp* app = type_aid_app_alloc();
    InputEvent event;

    bool in_t9_mode = false;

    FURI_LOG_I(TAG, "Entering main event loop"); // --------------------------
    while(1) {
        if(app->in_text_input) {
            // We're waiting for text input to finish
            furi_delay_ms(100);
        } else {
            // Handle events based on current mode
            if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
                if(in_t9_mode) {
                    // T9 mode event handling
                    if(event.type == InputTypeShort || event.type == InputTypeLong) {
                        if(event.key == InputKeyBack) {
                            FURI_LOG_I(TAG, "Back pressed in T9, returning to splash");
                            in_t9_mode = false;
                            t9_cursor.line = 0;
                            t9_cursor.pos = 0;
                            app->keyboard_used = true; // Mark keyboard as used
                            shift_locked = false; // Reset shift lock
                            gui_remove_view_port(app->gui, app->t9_view_port);
                            gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
                        } else if(event.key == InputKeyOk) {
                            // If a suggestion is selected, accept it; otherwise add character
                            if(app->selected_suggestion >= 0) {
                                t9_accept_suggestion(app);
                            } else {
                                t9_add_character(app);
                            }
                            view_port_update(app->t9_view_port);
                        } else if(event.key == InputKeyUp) {
                            t9_move_cursor(-1, 0);
                            view_port_update(app->t9_view_port);
                        } else if(event.key == InputKeyDown) {
                            t9_move_cursor(1, 0);
                            view_port_update(app->t9_view_port);
                        } else if(event.key == InputKeyLeft) {
                            t9_move_cursor(0, -1);
                            view_port_update(app->t9_view_port);
                        } else if(event.key == InputKeyRight) {
                            // Short press: move cursor
                            // Long press: cycle through suggestions
                            if(event.type == InputTypeLong) {
                                t9_cycle_suggestion(app);
                            } else {
                                t9_move_cursor(0, 1);
                            }
                            view_port_update(app->t9_view_port);
                        }
                    }
                } else if(event.type == InputTypeShort || event.type == InputTypeLong) {
                    // Splash screen event handling
                    if(event.key == InputKeyBack) {
                        FURI_LOG_I(TAG, "Back pressed, exiting");
                        break;
                    } else if(event.key == InputKeyOk) {
                        FURI_LOG_I(TAG, "OK pressed, showing T9 input");
                        in_t9_mode = true;
                        gui_remove_view_port(app->gui, app->view_port);
                        gui_add_view_port(app->gui, app->t9_view_port, GuiLayerFullscreen);
                    } else if(event.key == InputKeyDown || event.key == InputKeyRight) {
                        FURI_LOG_I(TAG, "Down/Right pressed, showing text input");
                        app->keyboard_used =
                            true; // Flag that keyboard has been used at least once
                        gui_remove_view_port(app->gui, app->view_port); // Remove splash screen

                        // Show text input
                        app->in_text_input = true;
                        view_dispatcher_switch_to_view(app->view_dispatcher, 1);
                        view_dispatcher_run(app->view_dispatcher);

                        // Text input finished, show splash again
                        FURI_LOG_I(TAG, "Text input closed, returning to splash");
                        app->in_text_input = false;
                        gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
                        view_port_update(app->view_port);
                    }
                }
            }

            if(!in_t9_mode) {
                view_port_update(app->view_port);
            }
        }
    }
    FURI_LOG_I(TAG, "Cleaning up");
    type_aid_app_free(app);
    FURI_LOG_I(TAG, "App exiting");
    return 0;
}
