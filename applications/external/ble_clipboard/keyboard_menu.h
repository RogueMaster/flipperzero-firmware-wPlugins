#pragma once

#include <input/input.h>
#include "clipboard_screen.h"
#include "keyboard_settings.h"

typedef struct {
    ClipboardKeyboardSettings draft;
    unsigned row;
    uint8_t selected_layout;
    bool layouts;
} ClipboardKeyboardMenu;

typedef enum { ClipboardKeyboardMenuNone, ClipboardKeyboardMenuBack, ClipboardKeyboardMenuSave } ClipboardKeyboardMenuAction;

void clipboard_keyboard_menu_init(ClipboardKeyboardMenu* menu, ClipboardKeyboardSettings settings);
ClipboardKeyboardMenuAction clipboard_keyboard_menu_input(ClipboardKeyboardMenu* menu, InputKey key);
void clipboard_keyboard_menu_screen(const ClipboardKeyboardMenu* menu, ClipboardScreen* screen);
