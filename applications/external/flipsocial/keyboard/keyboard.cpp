#include "keyboard.hpp"
#include "font/font.h"
#include <furi_hal.h>
#include <string.h>

#define BLINK_INTERVAL_MS 1000

Keyboard::Keyboard() {
    reset();
    resetText();
    text_edit_mode = false;
    last_word[0] = '\0';

    if(!auto_complete_init(&autoComplete)) {
        FURI_LOG_E("Keyboard", "Failed to initialize autocomplete");
    }
}

Keyboard::~Keyboard() {
    auto_complete_free(&autoComplete);
}

bool Keyboard::addDictionary(const char* filename) {
    if(!auto_complete_add_dictionary(&autoComplete, filename)) {
        FURI_LOG_E("Keyboard", "Failed to add dictionary: %s", filename);
        return false;
    }
    return true;
}

bool Keyboard::addSuggestion(const char* word) {
    if(!auto_complete_add_word(&autoComplete, word)) {
        FURI_LOG_E("Keyboard", "Failed to add word to autocomplete: %s", word);
        return false;
    }
    return true;
}

void Keyboard::clampCursorToValidPosition() {
    const char(*keyboard)[11] = getCurrentKeyboard();

    if(cursor_y < KEYBOARD_ROWS) {
        // Clamp to valid position in keyboard grid
        if(cursor_x > KEYBOARD_COLS - 1) cursor_x = KEYBOARD_COLS - 1;

        while(cursor_x > 0 && keyboard[cursor_y][cursor_x] == '\0') {
            cursor_x--;
        }
    }
}

void Keyboard::clearText() {
    text_buffer[0] = '\0';
    text_cursor = 0;
    text_edit_mode = false;
}

void Keyboard::draw(Canvas* canvas, const char* title) {
    draw(canvas, title, text_buffer);
}

void Keyboard::draw(Canvas* canvas, const char* title, const char* current_text) {
    canvas_clear(canvas);
    const char(*keyboard)[11] = getCurrentKeyboard();

    // Calculate text display parameters
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    size_t text_len = strlen(current_text);
    const int char_width = 6; // Width of each character in pixels
    const int display_width = 128; // Screen width in pixels
    const int max_visible_chars =
        (display_width - char_width) / char_width; // Reserve space for cursor

    // Calculate scroll offset to show the end of the text
    int scroll_offset = 0;
    if(text_len > max_visible_chars) {
        scroll_offset = text_len - max_visible_chars;
    }

    // Create a substring to display (scrolled portion)
    char display_text[MAX_TEXT_SIZE];
    if(scroll_offset > 0) {
        snprintf(display_text, MAX_TEXT_SIZE, "%s", current_text + scroll_offset);
    } else {
        snprintf(display_text, MAX_TEXT_SIZE, "%s", current_text);
    }

    // Show scroll indicator if text is scrolled (draw first, before text)
    int text_start_x = 0;
    if(scroll_offset > 0) {
        canvas_draw_str(canvas, 0, 8, "<"); // Left arrow to indicate scrolled content
        text_start_x = char_width + 2; // Offset text to make room for indicator
    }

    // Draw the visible portion of the text
    canvas_draw_str(canvas, text_start_x, 8, display_text);

    // Show autocomplete suggestion on the right side
    if(autoComplete.suggestion_count > 0) {
        const char* suggestion = autoComplete.suggestions[0];
        canvas_set_color(canvas, ColorBlack);

        // Calculate position to right-align the suggestion
        uint16_t suggestion_width = canvas_string_width_custom(canvas, suggestion);
        int suggestion_x = display_width - suggestion_width - 2; // 2 pixel padding from right edge

        // Draw suggestion on the right side of the text line
        canvas_draw_str(canvas, suggestion_x, 8, suggestion);
    }

    // Draw blinking text cursor
    uint32_t tick = furi_get_tick();
    bool cursor_visible = (tick / BLINK_INTERVAL_MS) % 2 == 0; // Blink every BLINK_INTERVAL_MS

    if(cursor_visible) {
        int cursor_display_pos = text_cursor - scroll_offset;
        if(cursor_display_pos >= 0 && cursor_display_pos <= max_visible_chars) {
            char text_before_cursor[MAX_TEXT_SIZE];
            snprintf(text_before_cursor, MAX_TEXT_SIZE, "%.*s", cursor_display_pos, display_text);
            uint16_t text_width = canvas_string_width_custom(canvas, text_before_cursor);
            int cursor_x = text_start_x + text_width;
            canvas_draw_box(canvas, cursor_x, 1, 1, 8);
        }
    }

    // Draw compact 3x10 virtual keyboard
    for(int row = 0; row < KEYBOARD_ROWS; row++) {
        for(int col = 0; col < KEYBOARD_COLS; col++) {
            char ch = keyboard[row][col];
            if(ch == '\0') break;

            int x = 3 + col * 12;
            int y = 22 + row * 10;

            // Highlight current cursor position
            if(row == cursor_y && col == cursor_x) {
                canvas_draw_rbox(canvas, x - 1, y - 7, 11, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }

            // Draw character
            char str[2] = {ch, '\0'};
            canvas_draw_str(canvas, x + 1, y, str);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    // Draw function keys below the main keyboard
    int func_y = 55;

    // Space bar (wide button)
    bool space_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_SPACE);
    if(space_selected) {
        canvas_draw_rbox(canvas, 3, func_y - 7, 30, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 10, func_y, "SPACE");
    canvas_set_color(canvas, ColorBlack);

    // Backspace
    bool backspace_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_BACKSPACE);
    if(backspace_selected) {
        canvas_draw_rbox(canvas, 35, func_y - 7, 20, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 38, func_y, "DEL");
    canvas_set_color(canvas, ColorBlack);

    // Shift/Mode
    bool shift_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_MODE_SWITCH);
    if(shift_selected) {
        canvas_draw_rbox(canvas, 57, func_y - 7, 20, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    if(mode == KEYBOARD_NUMBERS) {
        canvas_draw_str(canvas, 60, func_y, "ABC");
    } else {
        canvas_draw_str(canvas, 60, func_y, "123");
    }
    canvas_set_color(canvas, ColorBlack);

    // Caps Lock (only show in letter modes)
    if(mode != KEYBOARD_NUMBERS) {
        bool caps_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_CAPS_LOCK);
        if(caps_selected) {
            canvas_draw_rbox(canvas, 79, func_y - 7, 20, 9, 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 82, func_y, "CAPS");
            canvas_set_color(canvas, ColorBlack);
        } else if(caps_lock) {
            // When caps lock is on but not selected, draw with inverted colors
            canvas_draw_rbox(canvas, 79, func_y - 7, 20, 9, 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 82, func_y, "CAPS");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 82, func_y, "CAPS");
        }
    }

    // Done/Enter
    bool done_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_DONE);
    if(done_selected) {
        canvas_draw_rbox(canvas, 101, func_y - 7, 25, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 105, func_y, "DONE");
    canvas_set_color(canvas, ColorBlack);

    // Draw title at the bottom centered
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    uint16_t title_width = canvas_string_width_custom(canvas, title);
    int title_x = (display_width - title_width) / 2;
    canvas_draw_str(canvas, title_x, 64, title);
}

char Keyboard::getCurrentChar(bool long_press) {
    const char(*keyboard)[11] = getCurrentKeyboard();
    char ch = keyboard[cursor_y][cursor_x];
    /* if long press,
      - return uppercase version if in lowercase mode
      - return lowercase version if in uppercase mode
    */
    if(long_press) {
        switch(mode) {
        case KEYBOARD_NUMBERS:
            // No change in number mode
            return ch;
        case KEYBOARD_LOWERCASE:
            if(ch >= 'a' && ch <= 'z') {
                return ch - ('a' - 'A');
            }
            switch(ch) {
            case '.':
                return ',';
            case '_':
                return '-';
            case '/':
                return '\\';
            case ':':
                return ';';
            default:
                break;
            };
            break;
        case KEYBOARD_UPPERCASE:
            if(ch >= 'A' && ch <= 'Z') {
                return ch + ('a' - 'A');
            }
            switch(ch) {
            case ',':
                return '.';
            case '-':
                return '_';
            case '\\':
                return '/';
            case ';':
                return ':';
            default:
                break;
            };
            break;
        default:
            break;
        };
    }
    return ch;
}

const char (*Keyboard::getCurrentKeyboard())[11] {
    switch(mode) {
    case KEYBOARD_UPPERCASE:
        return keyboard_uppercase;
    case KEYBOARD_NUMBERS:
        return keyboard_numbers;
    default:
        return keyboard_lowercase;
    }
}

const char* Keyboard::getCurrentWord() {
    // Find the start of the current word
    int pos = text_cursor - 1;
    while(pos >= 0 && text_buffer[pos] != ' ') {
        pos--;
    }
    return &text_buffer[pos + 1];
}

void Keyboard::updateAutoComplete() {
    // Only update if text has changed
    if(strcmp(text_buffer, last_word) != 0) {
        snprintf(last_word, MAX_TEXT_SIZE, "%s", text_buffer);

        if(strlen(text_buffer) > 0) {
            // First try to find a phrase match with the entire text
            if(auto_complete_search(&autoComplete, text_buffer)) {
                // Found phrase suggestions
                return;
            }

            // No phrase match, fall back to searching just the current word
            const char* current_word = getCurrentWord();

            // Extract only the current word
            char word_buffer[MAX_TEXT_SIZE];
            size_t word_len = 0;
            while(current_word[word_len] != '\0' && current_word[word_len] != ' ' &&
                  word_len < MAX_TEXT_SIZE - 1) {
                word_buffer[word_len] = current_word[word_len];
                word_len++;
            }
            word_buffer[word_len] = '\0';

            // Search with just the current word
            if(word_len > 0) {
                auto_complete_search(&autoComplete, word_buffer);
            }
        }
    }
}

const char* Keyboard::getModeName() {
    switch(mode) {
    case KEYBOARD_UPPERCASE:
        return caps_lock ? "CAPS" : "ABC";
    case KEYBOARD_NUMBERS:
        return "123";
    default:
        return "abc";
    }
}

size_t Keyboard::getStringLength(const char* str, size_t max_size) {
    size_t len = 0;
    while(len < max_size - 1 && str[len] != '\0') {
        len++;
    }
    return len;
}

const char* Keyboard::getText() const {
    return text_buffer;
}

size_t Keyboard::getTextLength() const {
    return strlen(text_buffer);
}

const char* Keyboard::getSuggestion(uint8_t index) {
    if(autoComplete.suggestion_count > 0 && index < autoComplete.suggestion_count) {
        return autoComplete.suggestions[index];
    }
    return nullptr;
}

const char Keyboard::keyboard_lowercase[3][11] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '\0'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\0'},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', '.', '_', '/', '\0'}};

const char Keyboard::keyboard_uppercase[3][11] = {
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '\0'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\0'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '-', '\\', '\0'}};

const char Keyboard::keyboard_numbers[3][11] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\0'},
    {'@', '#', '$', '%', '&', '*', '+', '=', '?', '!', '\0'},
    {'(', ')', '[', ']', '{', '}', '<', '>', '|', '~', '\0'}};

bool Keyboard::handleInput(InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }
    return handleInput(event, text_buffer, MAX_TEXT_SIZE);
}

bool Keyboard::handleInput(InputEvent* event, char* target_buffer, size_t target_size) {
    const char(*keyboard)[11] = getCurrentKeyboard();
    const uint8_t key = event->key;

    if(key == InputKeyBack && event->type == InputTypeShort) {
        // Delete character to the left of cursor
        size_t text_len = getStringLength(target_buffer, target_size);
        if(text_len > 0 && text_cursor > 0) {
            // Shift characters left to remove character before cursor
            for(size_t i = text_cursor - 1; i < text_len; i++) {
                target_buffer[i] = target_buffer[i + 1];
            }
            text_cursor--;
        }
        return false; // keyboard not done yet
    }

    // Handle text edit mode
    if(text_edit_mode) {
        if(event->type == InputTypeLong && key == InputKeyOk) {
            // Add the suggested phrase/word on long press OK in text edit mode
            if(autoComplete.suggestion_count > 0) {
                const char* suggestion = autoComplete.suggestions[0];
                size_t suggestion_len = strlen(suggestion);
                size_t text_len = getStringLength(target_buffer, target_size);

                // Check if this is a phrase completion (suggestion starts with entire buffer)
                bool is_phrase_match = (text_len <= suggestion_len);
                for(size_t i = 0; i < text_len && is_phrase_match; i++) {
                    if(target_buffer[i] != suggestion[i]) {
                        is_phrase_match = false;
                    }
                }

                if(is_phrase_match) {
                    // Phrase completion: append the remaining part
                    size_t remaining_len = suggestion_len - text_len;
                    if(text_len + remaining_len < target_size - 1) {
                        for(size_t i = 0; i < remaining_len; i++) {
                            target_buffer[text_len + i] = suggestion[text_len + i];
                        }
                        target_buffer[text_len + remaining_len] = '\0';
                        text_cursor = text_len + remaining_len;
                    }
                } else {
                    // Word completion: replace current word with suggestion
                    // Find start of current word
                    int word_start = text_cursor - 1;
                    while(word_start >= 0 && target_buffer[word_start] != ' ') {
                        word_start--;
                    }
                    word_start++;

                    // Calculate lengths
                    int current_word_len = text_cursor - word_start;
                    int shift_amount = suggestion_len - current_word_len;

                    // Check if we have space
                    if(text_len + shift_amount < target_size - 1) {
                        // Shift text after current word
                        if(shift_amount > 0) {
                            // Need to make room
                            for(int i = text_len; i >= text_cursor; i--) {
                                target_buffer[i + shift_amount] = target_buffer[i];
                            }
                        } else if(shift_amount < 0) {
                            // Need to close gap
                            for(size_t i = text_cursor; i <= text_len; i++) {
                                target_buffer[i + shift_amount] = target_buffer[i];
                            }
                        }

                        // Copy suggestion to replace current word
                        for(size_t i = 0; i < suggestion_len; i++) {
                            target_buffer[word_start + i] = suggestion[i];
                        }

                        // Update cursor position
                        text_cursor = word_start + suggestion_len;
                    }
                }

                // Clear suggestions after inserting
                last_word[0] = '\0';
                autoComplete.suggestion_count = 0;
            }
            return false;
        }
        size_t text_len = getStringLength(target_buffer, target_size);

        switch(key) {
        case InputKeyLeft:
            if(text_cursor > 0) {
                text_cursor--;
            }
            break;

        case InputKeyRight:
            if(text_cursor < text_len) {
                text_cursor++;
            }
            break;

        case InputKeyUp:
            // Stay in text edit mode
            break;

        case InputKeyDown:
            // Exit text edit mode and return to keyboard
            text_edit_mode = false;
            break;

        case InputKeyOk:
            // Backspace at cursor position
            if(text_cursor > 0 && text_len > 0) {
                // Shift characters left to remove character before cursor
                for(size_t i = text_cursor - 1; i < text_len; i++) {
                    target_buffer[i] = target_buffer[i + 1];
                }
                text_cursor--;
            }
            break;

        default:
            break;
        }

        return false;
    }

    // Handle keyboard navigation mode
    switch(key) {
    case InputKeyLeft:
        if(cursor_y < KEYBOARD_ROWS) { // Main keyboard area
            if(cursor_x > 0) {
                cursor_x--;
            } else {
                // Wrap to end of row
                cursor_x = KEYBOARD_COLS - 1;
                while(cursor_x > 0 && keyboard[cursor_y][cursor_x] == '\0') {
                    cursor_x--;
                }
            }
        } else { // Function key row
            if(cursor_x > 0) {
                cursor_x--;
            } else {
                // Wrap to last function key
                cursor_x = (mode == KEYBOARD_NUMBERS) ? 3 : 4; // No caps in number mode
            }
        }
        break;

    case InputKeyRight:
        if(cursor_y < KEYBOARD_ROWS) { // Main keyboard area
            if(cursor_x < KEYBOARD_COLS - 1 && keyboard[cursor_y][cursor_x + 1] != '\0') {
                cursor_x++;
            } else {
                // Wrap to beginning of row
                cursor_x = 0;
            }
        } else { // Function key row
            int max_func_key = (mode == KEYBOARD_NUMBERS) ? 3 : 4; // No caps in number mode
            if(cursor_x < max_func_key) {
                cursor_x++;
            } else {
                cursor_x = 0;
            }
        }
        break;

    case InputKeyUp:
        if(cursor_y > 0) {
            // Map function keys to bottom row keys when moving up
            if(cursor_y == FUNCTION_ROW) {
                switch(cursor_x) {
                case FUNC_KEY_SPACE:
                    cursor_x = 1; // → x (middle of z,x,c)
                    break;
                case FUNC_KEY_BACKSPACE:
                    cursor_x = 3; // → v
                    break;
                case FUNC_KEY_MODE_SWITCH:
                    cursor_x = 5; // → n (middle of b,n)
                    break;
                case FUNC_KEY_CAPS_LOCK:
                    cursor_x = 6; // → m (middle of m,.)
                    break;
                case FUNC_KEY_DONE:
                    cursor_x = 8; // → _ (middle of _,/)
                    break;
                }
            }
            cursor_y--;
            if(cursor_y < KEYBOARD_ROWS) { // Moving to main keyboard
                clampCursorToValidPosition();
            }
        } else {
            // Enter text edit mode when pressing up from top keyboard row
            text_edit_mode = true;
            updateAutoComplete();
        }
        break;

    case InputKeyDown:
        if(cursor_y < KEYBOARD_ROWS) {
            if(cursor_y < KEYBOARD_ROWS - 1) { // Can move down within keyboard
                cursor_y++;
                clampCursorToValidPosition();
            } else { // Move to function keys
                cursor_y = FUNCTION_ROW;
                // Map bottom row keys to corresponding function keys
                if(cursor_x <= 2) { // z, x, c → SPACE
                    cursor_x = FUNC_KEY_SPACE;
                } else if(cursor_x == 3) { // v → DEL
                    cursor_x = FUNC_KEY_BACKSPACE;
                } else if(cursor_x <= 5) { // b, n → 123
                    cursor_x = FUNC_KEY_MODE_SWITCH;
                } else if(cursor_x <= 7) { // m, . → CAPS
                    cursor_x = FUNC_KEY_CAPS_LOCK;
                } else { // _, / → DONE
                    cursor_x = FUNC_KEY_DONE;
                }
            }
        } else {
            // Wrap to top row
            cursor_y = 0;
            if(cursor_x > KEYBOARD_COLS - 1) cursor_x = KEYBOARD_COLS - 1;
        }
        break;

    case InputKeyOk:
        if(cursor_y == FUNCTION_ROW) { // Function key row
            switch(cursor_x) {
            case FUNC_KEY_SPACE: {
                size_t len = getStringLength(target_buffer, target_size);
                if(len < target_size - 1) {
                    // Insert space at cursor position
                    // Shift characters right
                    for(size_t i = len; i > text_cursor; i--) {
                        target_buffer[i] = target_buffer[i - 1];
                    }
                    target_buffer[text_cursor] = ' ';
                    target_buffer[len + 1] = '\0';
                    text_cursor++;
                    // Clear autocomplete after space
                    last_word[0] = '\0';
                    autoComplete.suggestion_count = 0;
                }
                break;
            }

            case FUNC_KEY_BACKSPACE: {
                size_t len = getStringLength(target_buffer, target_size);
                if(len > 0) {
                    // Delete at cursor position (before cursor)
                    if(text_cursor > 0) {
                        // Shift characters left
                        for(size_t i = text_cursor - 1; i < len; i++) {
                            target_buffer[i] = target_buffer[i + 1];
                        }
                        text_cursor--;
                        // Update autocomplete after backspace
                        updateAutoComplete();
                    }
                }
                break;
            }

            case FUNC_KEY_MODE_SWITCH:
                if(mode == KEYBOARD_NUMBERS) {
                    mode = caps_lock ? KEYBOARD_UPPERCASE : KEYBOARD_LOWERCASE;
                } else {
                    mode = KEYBOARD_NUMBERS;
                }
                break;

            case FUNC_KEY_CAPS_LOCK:
                if(mode != KEYBOARD_NUMBERS) {
                    caps_lock = !caps_lock;
                    mode = caps_lock ? KEYBOARD_UPPERCASE : KEYBOARD_LOWERCASE;
                }
                break;

            case FUNC_KEY_DONE:
                return true; // Signal that input is complete
            }
        } else {
            // Main keyboard area
            char ch = getCurrentChar(event->type == InputTypeLong);
            if(ch != '\0') {
                size_t len = getStringLength(target_buffer, target_size);
                if(len < target_size - 1) {
                    // Insert character at cursor position
                    // Shift characters right
                    for(size_t i = len; i > text_cursor; i--) {
                        target_buffer[i] = target_buffer[i - 1];
                    }
                    target_buffer[text_cursor] = ch;
                    target_buffer[len + 1] = '\0';
                    text_cursor++;

                    // Update autocomplete after character insertion
                    updateAutoComplete();

                    // Auto-switch to lowercase after typing a letter in caps mode (not caps lock)
                    if(mode == KEYBOARD_UPPERCASE && !caps_lock && ((ch >= 'A' && ch <= 'Z'))) {
                        mode = KEYBOARD_LOWERCASE;
                    }
                }
            }
        }
        break;

    default:
        break;
    }

    return false; // Input not complete
}

void Keyboard::reset() {
    cursor_x = 0;
    cursor_y = 0;
    mode = KEYBOARD_LOWERCASE;
    caps_lock = false;
    text_edit_mode = false;
    auto_complete_remove_suggestions(&autoComplete);
    auto_complete_remove_words(&autoComplete);
}

void Keyboard::resetText() {
    text_buffer[0] = '\0';
    text_cursor = 0;
    text_edit_mode = false;
}

void Keyboard::setText(const char* text) {
    if(text != nullptr) {
        snprintf(text_buffer, MAX_TEXT_SIZE, "%s", text);
        text_cursor = strlen(text_buffer);
    } else {
        text_buffer[0] = '\0';
        text_cursor = 0;
    }
    text_edit_mode = false;
}
