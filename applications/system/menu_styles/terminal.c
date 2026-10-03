/**
 * @file terminal.c
 * Original Terminal style by MatthewKuKanich.
 * Historical RogueMaster Terminal menu, recovered from commit
 * 0416f25919d020f7d8b365f6704baaacc608c9e7 (before removal in 41dcc6d5edb8).
 * Adapted to the MenuStyle ABI-1 loader plugin; project license: GPL-3.0.
 */
#include "menu_style_helpers.h"
#include <furi_hal_version.h>

/** Fit a bounded UTF-8 device name while keeping the terminal suffix intact. */
static void terminal_prompt(
    Canvas* canvas,
    char* prompt,
    size_t capacity,
    const char* suffix,
    size_t width) {
    const char* device = furi_hal_version_get_name_ptr();
    if(!device) device = "";
    char name[32];
    size_t length = 0;
    while(length < sizeof(name) - 1 && device[length])
        length++;
    // Do not end the bounded copy in the middle of a UTF-8 code point.
    if(device[length]) {
        while(length && ((uint8_t)device[length] & 0xC0) == 0x80)
            length--;
    }
    memcpy(name, device, length);
    name[length] = '\0';
    while(true) {
        snprintf(prompt, capacity, "%s%s", name, suffix);
        if(canvas_string_width(canvas, prompt) <= width || !length) break;
        length--;
        while(length && ((uint8_t)name[length] & 0xC0) == 0x80)
            length--;
        name[length] = '\0';
    }
}

static void menu_style_terminal_draw(Canvas* canvas, MenuModel* model) {
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontSecondary);
    char title[48];
    terminal_prompt(canvas, title, sizeof(title), "@fz: ~/Home", 92);
    canvas_draw_str(canvas, 20, 10, title);
    canvas_draw_str(canvas, 118, 9, "x");
    canvas_draw_frame(canvas, 116, 2, 8, 9);
    canvas_draw_frame(canvas, 0, 0, 128, 13);

    canvas_set_font(canvas, FontBatteryPercent);
    char prefix[48];
    terminal_prompt(canvas, prefix, sizeof(prefix), "@fz:~$", 60);
    canvas_draw_str(canvas, 2, 56, prefix);
    // The active font determines the prompt width, not a fixed six-pixel guess.
    size_t name_x = 3 + canvas_string_width(canvas, prefix);
    size_t name_width = name_x < 127 ? 127 - name_x : 0;
    for(size_t i = 0; i < 4 && i < model->count - model->position; i++) {
        const MenuItem* item = &model->items[model->position + i];
        if(i == 0) {
            if(name_width) {
                menu_style_text_line(
                    canvas,
                    name_x,
                    56,
                    name_width,
                    menu_style_label(item, true),
                    menu_style_scroll(model, true),
                    false,
                    false);
            }
        } else {
            menu_style_text_line(
                canvas, 2, 56 - i * 12, 125, menu_style_label(item, false), 0, false, false);
        }
    }
}

static size_t menu_style_terminal_navigate(MenuModel* model, InputKey key) {
    return menu_style_navigate_list(model, key);
}

static const MenuStylePlugin style = {
    .draw = menu_style_terminal_draw,
    .navigate = menu_style_terminal_navigate,
};
MENU_STYLE_PLUGIN(style, menu_style_terminal_ep)
