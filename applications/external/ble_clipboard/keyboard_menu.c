#include "keyboard_menu.h"
#include "keyboard_layout.h"
#include <stdio.h>
#include <string.h>

void clipboard_keyboard_menu_init(ClipboardKeyboardMenu* menu, ClipboardKeyboardSettings settings) {
    *menu = (ClipboardKeyboardMenu){.draft = settings, .selected_layout = settings.layout};
}

ClipboardKeyboardMenuAction clipboard_keyboard_menu_input(ClipboardKeyboardMenu* menu, InputKey key) {
    if(key == InputKeyLeft || key == InputKeyBack) {
        if(menu->layouts) { menu->layouts = false; return ClipboardKeyboardMenuNone; }
        return ClipboardKeyboardMenuBack;
    }
    if(menu->layouts) {
        if(key == InputKeyUp && menu->selected_layout) menu->selected_layout--;
        if(key == InputKeyDown && (size_t)menu->selected_layout + 1U < CLIPBOARD_LAYOUT_COUNT) menu->selected_layout++;
        if(key == InputKeyOk) {
            menu->draft.layout = menu->selected_layout;
            menu->draft.override_saved = true;
            menu->layouts = false;
        }
    } else {
        if(key == InputKeyUp && menu->row) menu->row--;
        if(key == InputKeyDown && menu->row < 3) menu->row++;
        if(key == InputKeyOk) {
            if(menu->row == 0) {
                menu->draft.target = (menu->draft.target + 1) % 3;
                menu->draft.override_saved = true;
            } else if(menu->row == 1) {
                menu->layouts = true; menu->selected_layout = menu->draft.layout;
            } else if(menu->row == 2) menu->draft.override_saved = !menu->draft.override_saved;
            else return ClipboardKeyboardMenuSave;
        }
    }
    return ClipboardKeyboardMenuNone;
}

void clipboard_keyboard_menu_screen(const ClipboardKeyboardMenu* menu, ClipboardScreen* screen) {
    memset(screen, 0, sizeof(*screen));
    snprintf(screen->left, sizeof(screen->left), "Back");
    if(menu->layouts) {
        snprintf(screen->lines[0], sizeof(screen->lines[0]), "Layout %u/%u", menu->selected_layout + 1, (unsigned)CLIPBOARD_LAYOUT_COUNT);
        unsigned start = menu->selected_layout / 3 * 3;
        for(unsigned i = 0; i < 3 && start + i < CLIPBOARD_LAYOUT_COUNT; i++) {
            snprintf(screen->lines[i + 1], sizeof(screen->lines[i + 1]), "%s", clipboard_layout_titles[start + i]);
            if(start + i == menu->selected_layout) screen->selected = (int8_t)(i + 1);
        }
        snprintf(screen->center, sizeof(screen->center), "Choose");
    } else {
        snprintf(screen->lines[0], sizeof(screen->lines[0]), "Target: %s", clipboard_target_titles[menu->draft.target]);
        snprintf(screen->lines[1], sizeof(screen->lines[1]), "Layout: %s", clipboard_layout_titles[menu->draft.layout]);
        snprintf(screen->lines[2], sizeof(screen->lines[2]), "Saved runs: %s", menu->draft.override_saved ? "Device" : "Original");
        snprintf(screen->lines[3], sizeof(screen->lines[3]), "Save keyboard");
        screen->selected = (int8_t)menu->row;
        snprintf(screen->center, sizeof(screen->center), "%s", menu->row == 3 ? "Save" : "Change");
    }
}
