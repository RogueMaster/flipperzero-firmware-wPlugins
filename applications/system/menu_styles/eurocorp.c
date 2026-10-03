/**
 * @file eurocorp.c
 * Original Eurocorp style by xtruan (Struan Clark).
 * Historical RogueMaster Eurocorp menu, recovered from commit
 * bc5d0ab308a3f064ecf570caa45f8f3c60896e05 (before removal in 9d0ae7f20d95).
 * Adapted to the MenuStyle ABI-1 loader plugin; project license: GPL-3.0.
 */
#include "menu_style_helpers.h"
#include "font.h"

static void menu_style_eurocorp_draw(Canvas* canvas, MenuModel* model) {
    canvas_set_custom_u8g2_font(canvas, menu_style_eurocorp_font);
    FuriString* name = furi_string_alloc();
    for(int32_t i = 0; i < 3; i++) {
        canvas_set_color(canvas, ColorBlack);
        size_t position = menu_style_position_offset(model, i - 1);
        furi_string_set(name, menu_style_label(&model->items[position], true));
        // Only ASCII case changes; leave UTF-8 code-point bytes intact.
        for(size_t j = 0; j < furi_string_size(name); j++) {
            char c = furi_string_get_char(name, j);
            if(c >= 'a' && c <= 'z') furi_string_set_char(name, j, c - 'a' + 'A');
        }
        bool selected = i == 1;
        if(selected) {
            canvas_draw_box(canvas, 0, 22, 128, 22);
            canvas_set_color(canvas, ColorWhite);
            // The selected band keeps its clipped upper-right corner inside the screen.
            for(int32_t x = 0; x < 6; x++) {
                for(int32_t y = x; y < 6; y++) {
                    canvas_draw_dot(canvas, 127 - x, 22 + y - x);
                }
            }
        }
        elements_scrollable_text_line_centered(
            canvas, 2, 19 + 22 * i, 125, name, menu_style_scroll(model, selected), false, false);
    }
    furi_string_free(name);
    // The shared canvas must not retain a pointer into an unloadable plugin image.
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
}

static size_t menu_style_eurocorp_navigate(MenuModel* model, InputKey key) {
    return menu_style_navigate_list(model, key);
}

static const MenuStylePlugin style = {
    .draw = menu_style_eurocorp_draw,
    .navigate = menu_style_eurocorp_navigate,
};
MENU_STYLE_PLUGIN(style, menu_style_eurocorp_ep)
