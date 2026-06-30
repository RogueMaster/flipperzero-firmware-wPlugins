#pragma once
#include <gui/gui.h>

enum KeyboardMode
{
    KEYBOARD_LOWERCASE = 0,
    KEYBOARD_UPPERCASE = 1,
    KEYBOARD_NUMBERS = 2
};

enum FunctionKey
{
    FUNC_KEY_SPACE = 0,
    FUNC_KEY_BACKSPACE = 1,
    FUNC_KEY_MODE_SWITCH = 2,
    FUNC_KEY_CAPS_LOCK = 3,
    FUNC_KEY_DONE = 4
};

class Keyboard
{
private:
    bool caps_lock;                              // Caps lock state
    uint8_t cursor_x;                            // Current cursor position in the keyboard grid
    uint8_t cursor_y;                            // Current cursor position in the keyboard grid
    static const char keyboard_lowercase[3][11]; // keyboard layout for lowercase letters
    static const char keyboard_uppercase[3][11]; // keyboard layout for uppercase letters
    static const char keyboard_numbers[3][11];   // keyboard layout for numbers
    static const size_t MAX_TEXT_SIZE = 256;     // Maximum size of text input buffer
    KeyboardMode mode;                           // Current keyboard mode
    char text_buffer[MAX_TEXT_SIZE];             // internal buffer for text input
    //
    void clampCursorToValidPosition();                        // Ensure cursor is within valid bounds of the current keyboard layout
    const char (*getCurrentKeyboard())[11];                   // Get the current keyboard layout based on mode
    const char *getModeName();                                // Get the name of the current mode (e.g., "ABC", "123", "CAPS")
    size_t getStringLength(const char *str, size_t max_size); // Get the length of a string, respecting max size

public:
    Keyboard();
    ~Keyboard();
    //
    static const int KEYBOARD_ROWS = 3;
    static const int KEYBOARD_COLS = 10;
    static const int FUNCTION_ROW = 3;
    //
    void clearText();                                                       // clear text
    void draw(Canvas *canvas, const char *title);                           // draw keyboard with title
    void draw(Canvas *canvas, const char *title, const char *current_text); // draw keyboard with title and current text
    bool getCapsLock() const { return caps_lock; }                          // check if caps lock is enabled
    uint8_t getCursorX() const { return cursor_x; }                         // get current cursor X position
    uint8_t getCursorY() const { return cursor_y; }                         // get current cursor Y position
    KeyboardMode getMode() const { return mode; }                           // get current keyboard mode
    const char *getText() const;                                            // get current text input
    size_t getTextLength() const;                                           // get length of current text input
    bool handleInput(uint8_t key);                                          // handle input from the user (pass input key here)
    bool handleInput(uint8_t key, char *target_buffer, size_t target_size); // handle input and store result in target buffer
    void reset();                                                           // reset keyboard state
    void resetText();                                                       // reset text input buffer
    void setText(const char *text);                                         // set text input buffer to a specific string
};