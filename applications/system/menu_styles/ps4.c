/**
 * @file ps4.c
 * PS4 main menu style.
 *
 * Ported from Momentum Firmware (GPL-3.0), originally by Willy-JL (@WillyJL):
 * https://github.com/Next-Flip/Momentum-Firmware/commit/cef4a004f74fecca7556ea7aeb947897a1f392b5
 */
#include "menu_style_helpers.h"
#include <dolphin/dolphin.h>
#include <furi_hal_version.h>

static uint8_t cached_level;

static void __attribute__((constructor)) menu_style_ps4_init(void) {
    Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
    cached_level = dolphin_stats(dolphin).level;
    furi_record_close(RECORD_DOLPHIN);
}

static void menu_style_ps4_draw(Canvas* canvas, MenuModel* model) {
    size_t position = model->position;
    size_t count = model->count;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 1, 1, AlignLeft, AlignTop, furi_hal_version_get_name_ptr());
    char level[12];
    snprintf(level, sizeof(level), "Level %u", cached_level);
    canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, level);

    for(int32_t i = -1; i <= 4; i++) {
        size_t item_i = position + i;
        if(item_i >= count) continue;
        const MenuItem* item = &model->items[item_i];
        int32_t width = 20;
        int32_t height = 20;
        int32_t x = 36;
        int32_t y = 27;
        if(i == 0) {
            width += 10;
            height += 10;
            y += 2;
            canvas_draw_box(canvas, x - width / 2, y + height / 2, width, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_set_font(canvas, FontBatteryPercent);
            canvas_draw_str_aligned(canvas, x, y + height / 2 + 1, AlignCenter, AlignTop, "Start");

            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontSecondary);
            menu_style_text_line(
                canvas,
                x + width / 2 + 2,
                y + height / 2 + 7,
                74,
                menu_style_label(item, true),
                menu_style_scroll(model, true),
                false,
                false);
        } else {
            x += (width + 1) * i + (i < 0 ? -6 : 6);
        }
        canvas_draw_frame(canvas, x - width / 2, y - height / 2, width, height);
        menu_style_icon_centered(canvas, item->icon, x - 7, y - 7, 14, 14);
    }

    menu_style_scrollbar_horizontal(canvas, 0, 64, 128, position, count);
}

static size_t menu_style_ps4_navigate(MenuModel* model, InputKey key) {
    return menu_style_navigate_wrap(model, key);
}

static const MenuStylePlugin menu_style_ps4 = {
    .draw = menu_style_ps4_draw,
    .navigate = menu_style_ps4_navigate,
};

MENU_STYLE_PLUGIN(menu_style_ps4, menu_style_ps4_ep)
