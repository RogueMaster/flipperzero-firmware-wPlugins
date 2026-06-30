#include "keyboard.hpp"
#include "font/font.h"
#include <furi_hal.h>
#include <string.h>

#define BLINK_INTERVAL_MS 1000

Keyboard::Keyboard()
{
    reset();
    resetText();
    text_edit_mode = false;
}

Keyboard::~Keyboard()
{
    // Destructor - text buffer is automatically cleaned up
}

void Keyboard::clampCursorToValidPosition()
{
    const char (*keyboard)[11] = getCurrentKeyboard();

    if (cursor_y < KEYBOARD_ROWS)
    {
        // Clamp to valid position in keyboard grid
        if (cursor_x > KEYBOARD_COLS - 1)
            cursor_x = KEYBOARD_COLS - 1;

        while (cursor_x > 0 && keyboard[cursor_y][cursor_x] == '\0')
        {
            cursor_x--;
        }
    }
}

void Keyboard::clearText()
{
    text_buffer[0] = '\0';
    text_cursor = 0;
    text_edit_mode = false;
}

void Keyboard::draw(Canvas *canvas, const char *title)
{
    draw(canvas, title, text_buffer);
}

void Keyboard::draw(Canvas *canvas, const char *title, const char *current_text)
{
    canvas_clear(canvas);
    const char (*keyboard)[11] = getCurrentKeyboard();

    // Calculate text display parameters
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    size_t text_len = strlen(current_text);
    const int char_width = 6;                                                // Width of each character in pixels
    const int display_width = 128;                                           // Screen width in pixels
    const int max_visible_chars = (display_width - char_width) / char_width; // Reserve space for cursor

    // Calculate scroll offset to show the end of the text
    int scroll_offset = 0;
    if (text_len > max_visible_chars)
    {
        scroll_offset = text_len - max_visible_chars;
    }

    // Create a substring to display (scrolled portion)
    char display_text[MAX_TEXT_SIZE];
    if (scroll_offset > 0)
    {
        strncpy(display_text, current_text + scroll_offset, MAX_TEXT_SIZE - 1);
        display_text[MAX_TEXT_SIZE - 1] = '\0';
    }
    else
    {
        strncpy(display_text, current_text, MAX_TEXT_SIZE - 1);
        display_text[MAX_TEXT_SIZE - 1] = '\0';
    }

    // Show scroll indicator if text is scrolled (draw first, before text)
    int text_start_x = 0;
    if (scroll_offset > 0)
    {
        canvas_draw_str(canvas, 0, 8, "<"); // Left arrow to indicate scrolled content
        text_start_x = char_width + 2;      // Offset text to make room for indicator
    }

    // Draw the visible portion of the text
    canvas_draw_str(canvas, text_start_x, 8, display_text);

    // Draw blinking text cursor
    uint32_t tick = furi_get_tick();
    bool cursor_visible = (tick / BLINK_INTERVAL_MS) % 2 == 0; // Blink every BLINK_INTERVAL_MS

    if (cursor_visible)
    {
        int cursor_display_pos = text_cursor - scroll_offset;
        if (cursor_display_pos >= 0 && cursor_display_pos <= max_visible_chars)
        {
            char text_before_cursor[MAX_TEXT_SIZE];
            strncpy(text_before_cursor, display_text, cursor_display_pos);
            text_before_cursor[cursor_display_pos] = '\0';
            uint16_t text_width = canvas_string_width(canvas, text_before_cursor);
            int cursor_x = text_start_x + text_width;
            canvas_draw_box(canvas, cursor_x, 1, 1, 8);
        }
    }

    // Draw compact 3x10 virtual keyboard
    for (int row = 0; row < KEYBOARD_ROWS; row++)
    {
        for (int col = 0; col < KEYBOARD_COLS; col++)
        {
            char ch = keyboard[row][col];
            if (ch == '\0')
                break;

            int x = 3 + col * 12;
            int y = 22 + row * 10;

            // Highlight current cursor position
            if (row == cursor_y && col == cursor_x)
            {
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
    if (space_selected)
    {
        canvas_draw_rbox(canvas, 3, func_y - 7, 30, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 10, func_y, "SPACE");
    canvas_set_color(canvas, ColorBlack);

    // Backspace
    bool backspace_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_BACKSPACE);
    if (backspace_selected)
    {
        canvas_draw_rbox(canvas, 35, func_y - 7, 20, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 38, func_y, "DEL");
    canvas_set_color(canvas, ColorBlack);

    // Shift/Mode
    bool shift_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_MODE_SWITCH);
    if (shift_selected)
    {
        canvas_draw_rbox(canvas, 57, func_y - 7, 20, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    if (mode == KEYBOARD_NUMBERS)
    {
        canvas_draw_str(canvas, 60, func_y, "ABC");
    }
    else
    {
        canvas_draw_str(canvas, 60, func_y, "123");
    }
    canvas_set_color(canvas, ColorBlack);

    // Caps Lock (only show in letter modes)
    if (mode != KEYBOARD_NUMBERS)
    {
        bool caps_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_CAPS_LOCK);
        if (caps_selected)
        {
            canvas_draw_rbox(canvas, 79, func_y - 7, 20, 9, 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 82, func_y, "CAPS");
            canvas_set_color(canvas, ColorBlack);
        }
        else if (caps_lock)
        {
            // When caps lock is on but not selected, draw with inverted colors
            canvas_draw_rbox(canvas, 79, func_y - 7, 20, 9, 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 82, func_y, "CAPS");
            canvas_set_color(canvas, ColorBlack);
        }
        else
        {
            canvas_draw_str(canvas, 82, func_y, "CAPS");
        }
    }

    // Done/Enter
    bool done_selected = (cursor_y == FUNCTION_ROW && cursor_x == FUNC_KEY_DONE);
    if (done_selected)
    {
        canvas_draw_rbox(canvas, 101, func_y - 7, 25, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 105, func_y, "DONE");
    canvas_set_color(canvas, ColorBlack);

    // Draw title at the bottom
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, 50, 64, title);
}

char Keyboard::getCurrentChar(bool long_press)
{
    const char (*keyboard)[11] = getCurrentKeyboard();
    char ch = keyboard[cursor_y][cursor_x];
    /* if long press,
      - return uppercase version if in lowercase mode
      - return lowercase version if in uppercase mode
    */
    if (long_press)
    {
        if (mode == KEYBOARD_LOWERCASE && ch >= 'a' && ch <= 'z')
        {
            ch = ch - ('a' - 'A');
        }
        else if (mode == KEYBOARD_UPPERCASE && ch >= 'A' && ch <= 'Z')
        {
            ch = ch + ('a' - 'A');
        }
    }
    return ch;
}

const char (*Keyboard::getCurrentKeyboard())[11]
{
    switch (mode)
    {
    case KEYBOARD_UPPERCASE:
        return keyboard_uppercase;
    case KEYBOARD_NUMBERS:
        return keyboard_numbers;
    default:
        return keyboard_lowercase;
    }
}

const char *Keyboard::getModeName()
{
    switch (mode)
    {
    case KEYBOARD_UPPERCASE:
        return caps_lock ? "CAPS" : "ABC";
    case KEYBOARD_NUMBERS:
        return "123";
    default:
        return "abc";
    }
}

size_t Keyboard::getStringLength(const char *str, size_t max_size)
{
    size_t len = 0;
    while (len < max_size - 1 && str[len] != '\0')
    {
        len++;
    }
    return len;
}

const char *Keyboard::getText() const
{
    return text_buffer;
}

size_t Keyboard::getTextLength() const
{
    return strlen(text_buffer);
}

const char Keyboard::keyboard_lowercase[3][11] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '\0'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '-', '\0'},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', '.', '_', '/', '\0'}};

const char Keyboard::keyboard_uppercase[3][11] = {
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '\0'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '-', '\0'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', '_', '/', '\0'}};

const char Keyboard::keyboard_numbers[3][11] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\0'},
    {'@', '#', '$', '%', '&', '*', '+', '=', '?', '!', '\0'},
    {'(', ')', '[', ']', '{', '}', '<', '>', '|', '\\', '\0'}};

bool Keyboard::handleInput(InputEvent *event)
{
    return handleInput(event, text_buffer, MAX_TEXT_SIZE);
}

bool Keyboard::handleInput(InputEvent *event, char *target_buffer, size_t target_size)
{
    const char (*keyboard)[11] = getCurrentKeyboard();
    const uint8_t key = event->key;

    // Handle text edit mode
    if (text_edit_mode)
    {
        size_t text_len = getStringLength(target_buffer, target_size);

        switch (key)
        {
        case InputKeyLeft:
            if (text_cursor > 0)
            {
                text_cursor--;
            }
            break;

        case InputKeyRight:
            if (text_cursor < text_len)
            {
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
            if (text_cursor > 0 && text_len > 0)
            {
                // Shift characters left to remove character before cursor
                for (size_t i = text_cursor - 1; i < text_len; i++)
                {
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
    switch (key)
    {
    case InputKeyLeft:
        if (cursor_y < KEYBOARD_ROWS)
        { // Main keyboard area
            if (cursor_x > 0)
            {
                cursor_x--;
            }
            else
            {
                // Wrap to end of row
                cursor_x = KEYBOARD_COLS - 1;
                while (cursor_x > 0 && keyboard[cursor_y][cursor_x] == '\0')
                {
                    cursor_x--;
                }
            }
        }
        else
        { // Function key row
            if (cursor_x > 0)
            {
                cursor_x--;
            }
            else
            {
                // Wrap to last function key
                cursor_x = (mode == KEYBOARD_NUMBERS) ? 3 : 4; // No caps in number mode
            }
        }
        break;

    case InputKeyRight:
        if (cursor_y < KEYBOARD_ROWS)
        { // Main keyboard area
            if (cursor_x < KEYBOARD_COLS - 1 && keyboard[cursor_y][cursor_x + 1] != '\0')
            {
                cursor_x++;
            }
            else
            {
                // Wrap to beginning of row
                cursor_x = 0;
            }
        }
        else
        {                                                          // Function key row
            int max_func_key = (mode == KEYBOARD_NUMBERS) ? 3 : 4; // No caps in number mode
            if (cursor_x < max_func_key)
            {
                cursor_x++;
            }
            else
            {
                cursor_x = 0;
            }
        }
        break;

    case InputKeyUp:
        if (cursor_y > 0)
        {
            cursor_y--;
            if (cursor_y < KEYBOARD_ROWS)
            { // Moving to main keyboard
                clampCursorToValidPosition();
            }
        }
        else
        {
            // Enter text edit mode when pressing up from top keyboard row
            text_edit_mode = true;
        }
        break;

    case InputKeyDown:
        if (cursor_y < KEYBOARD_ROWS)
        {
            if (cursor_y < KEYBOARD_ROWS - 1)
            { // Can move down within keyboard
                cursor_y++;
                clampCursorToValidPosition();
            }
            else
            { // Move to function keys
                cursor_y = FUNCTION_ROW;
                if (cursor_x > 4)
                    cursor_x = 4;
            }
        }
        else
        {
            // Wrap to top row
            cursor_y = 0;
            if (cursor_x > KEYBOARD_COLS - 1)
                cursor_x = KEYBOARD_COLS - 1;
        }
        break;

    case InputKeyOk:
        if (cursor_y == FUNCTION_ROW)
        { // Function key row
            switch (cursor_x)
            {
            case FUNC_KEY_SPACE:
            {
                size_t len = getStringLength(target_buffer, target_size);
                if (len < target_size - 1)
                {
                    // Insert space at cursor position
                    // Shift characters right
                    for (size_t i = len; i > text_cursor; i--)
                    {
                        target_buffer[i] = target_buffer[i - 1];
                    }
                    target_buffer[text_cursor] = ' ';
                    target_buffer[len + 1] = '\0';
                    text_cursor++;
                }
                break;
            }

            case FUNC_KEY_BACKSPACE:
            {
                size_t len = getStringLength(target_buffer, target_size);
                if (len > 0)
                {
                    // Delete at cursor position (before cursor)
                    if (text_cursor > 0)
                    {
                        // Shift characters left
                        for (size_t i = text_cursor - 1; i < len; i++)
                        {
                            target_buffer[i] = target_buffer[i + 1];
                        }
                        text_cursor--;
                    }
                }
                break;
            }

            case FUNC_KEY_MODE_SWITCH:
                if (mode == KEYBOARD_NUMBERS)
                {
                    mode = caps_lock ? KEYBOARD_UPPERCASE : KEYBOARD_LOWERCASE;
                }
                else
                {
                    mode = KEYBOARD_NUMBERS;
                }
                break;

            case FUNC_KEY_CAPS_LOCK:
                if (mode != KEYBOARD_NUMBERS)
                {
                    caps_lock = !caps_lock;
                    mode = caps_lock ? KEYBOARD_UPPERCASE : KEYBOARD_LOWERCASE;
                }
                break;

            case FUNC_KEY_DONE:
                return true; // Signal that input is complete
            }
        }
        else
        {
            // Main keyboard area
            char ch = getCurrentChar(event->type == InputTypeLong);
            if (ch != '\0')
            {
                size_t len = getStringLength(target_buffer, target_size);
                if (len < target_size - 1)
                {
                    // Insert character at cursor position
                    // Shift characters right
                    for (size_t i = len; i > text_cursor; i--)
                    {
                        target_buffer[i] = target_buffer[i - 1];
                    }
                    target_buffer[text_cursor] = ch;
                    target_buffer[len + 1] = '\0';
                    text_cursor++;

                    // Auto-switch to lowercase after typing a letter in caps mode (not caps lock)
                    if (mode == KEYBOARD_UPPERCASE && !caps_lock &&
                        ((ch >= 'A' && ch <= 'Z')))
                    {
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

void Keyboard::reset()
{
    cursor_x = 0;
    cursor_y = 0;
    mode = KEYBOARD_LOWERCASE;
    caps_lock = false;
    text_edit_mode = false;
}

void Keyboard::resetText()
{
    text_buffer[0] = '\0';
    text_cursor = 0;
    text_edit_mode = false;
}

void Keyboard::setText(const char *text)
{
    if (text != nullptr)
    {
        strncpy(text_buffer, text, MAX_TEXT_SIZE - 1);
        text_buffer[MAX_TEXT_SIZE - 1] = '\0';
        text_cursor = strlen(text_buffer);
    }
    else
    {
        text_buffer[0] = '\0';
        text_cursor = 0;
    }
    text_edit_mode = false;
}