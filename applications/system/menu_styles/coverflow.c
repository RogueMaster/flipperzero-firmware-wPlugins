/**
 * @file coverflow.c
 * CoverFlow main menu style.
 *
 * Ported from Momentum Firmware (GPL-3.0), originally by Cody Tolene (@CodyTolene),
 * later refined by Alexander Bays (@956MB):
 * https://github.com/Next-Flip/Momentum-Firmware/commit/530f3d4f227cbf9a0d50f37366b75d6081c2efd3
 * https://github.com/Next-Flip/Momentum-Firmware/commit/3b96cc47a77a82b5e095dda0d8c3ab017577b58e
 */
#include "menu_style_helpers.h"

static void menu_style_icon_centered_scaled(
    Canvas* canvas,
    const MenuItem* item,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t width_scale,
    int32_t height_scale) {
    canvas_draw_icon_animation_ex(
        canvas,
        x + (width - icon_animation_get_width(item->icon)) / 2,
        y + (height - icon_animation_get_height(item->icon)) / 2,
        width_scale,
        height_scale,
        item->icon);
}

static void menu_style_coverflow_draw(Canvas* canvas, MenuModel* model) {
    canvas_set_font(canvas, FontPrimary);

    // Draw frames
    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_rframe(canvas, 45, 4, 38, 38, 3);

    // Draw left side albums
    canvas_draw_line(canvas, 5, 36, 1, 37);
    canvas_draw_line(canvas, 4, 9, 1, 8);
    canvas_draw_line(canvas, 6, 41, 17, 36);
    canvas_draw_line(canvas, 19, 41, 30, 36);
    canvas_draw_line(canvas, 32, 41, 43, 36);
    canvas_draw_line(canvas, 6, 4, 17, 9);
    canvas_draw_line(canvas, 19, 4, 30, 9);
    canvas_draw_line(canvas, 32, 4, 43, 9);
    canvas_draw_line(canvas, 5, 5, 5, 40);
    canvas_draw_line(canvas, 18, 5, 18, 40);
    canvas_draw_line(canvas, 31, 5, 31, 40);

    // Draw right side albums
    canvas_draw_line(canvas, 95, 41, 84, 36);
    canvas_draw_line(canvas, 108, 41, 97, 36);
    canvas_draw_line(canvas, 121, 41, 110, 36);
    canvas_draw_line(canvas, 84, 9, 95, 4);
    canvas_draw_line(canvas, 97, 9, 108, 4);
    canvas_draw_line(canvas, 110, 9, 121, 4);
    canvas_draw_line(canvas, 96, 5, 96, 40);
    canvas_draw_line(canvas, 109, 5, 109, 40);
    canvas_draw_line(canvas, 122, 5, 122, 40);
    canvas_draw_line(canvas, 123, 9, 126, 8);
    canvas_draw_line(canvas, 123, 36, 126, 37);

    const int32_t pos_x_center = 128 / 2;
    const int32_t pos_y_center = (64 / 2) + 1;
    const int32_t pos_y_offset = 10;
    const int32_t icon_size = 20;
    const int32_t side_icon_width = icon_size / 2;
    const int32_t padding_center_icon = 14;
    const int32_t spacing_between_icons = 3;
    const int32_t scale_base = 100;

    const MenuItem* center_item = NULL;

    // Draw 7 icons, where index 0 is the center icon
    // [-3, -2, -1, 0, 1, 2, 3]
    for(int8_t i = -3; i <= 3; i++) {
        size_t shift_position = menu_style_position_offset(model, i);
        const MenuItem* item = &model->items[shift_position];

        int32_t pos_x = pos_x_center;
        int32_t pos_y = pos_y_center;

        int32_t scale_width = scale_base;
        int32_t scale_height = scale_base;

        if(i < 0) {
            // Left sided icons
            pos_x -= padding_center_icon;
            pos_x -= ((-i) * (side_icon_width + spacing_between_icons));
            pos_x -= (side_icon_width / 2) / 2;
            pos_y = (pos_y_center - icon_size / 2) - pos_y_offset;
            scale_width = 50;
        } else if(i > 0) {
            // Right sided icons
            pos_x += padding_center_icon;
            pos_x += (i * (side_icon_width + spacing_between_icons));
            pos_x -= side_icon_width;
            pos_y = (pos_y_center - icon_size / 2) - pos_y_offset;
            scale_width = 50;
        } else if(i == 0) {
            // Center icon
            pos_x -= icon_size / 2;
            pos_y = (pos_y_center - (icon_size / 2)) - pos_y_offset;
            // Scaling > 100% doesn't look good, keep 100% for now
            scale_width = scale_base; // TODO: 200%
            scale_height = scale_base; // TODO: 200%
            // Save center item pointer for later
            center_item = item;
        }

        // Draw the icon
        menu_style_icon_centered_scaled(
            canvas, item, pos_x, pos_y, icon_size, icon_size, scale_width, scale_height);
    }

    // Draw label for center item
    if(center_item) {
        const char* name = menu_style_label(center_item, false);
        size_t scroll_counter = menu_style_scroll(model, true);
        menu_style_text_line(
            canvas,
            pos_x_center,
            (pos_y_center + icon_size / 2) + pos_y_offset + 1,
            124,
            name,
            scroll_counter,
            false,
            true);
    }

    // Add scrollbar element
    elements_scrollbar_horizontal(canvas, 0, 60, 128, model->position, model->count);

    canvas_set_bitmap_mode(canvas, false);
}

static size_t menu_style_coverflow_navigate(MenuModel* model, InputKey key) {
    return menu_style_navigate_wrap(model, key);
}

static const MenuStylePlugin style = {
    .draw = menu_style_coverflow_draw,
    .navigate = menu_style_coverflow_navigate};
MENU_STYLE_PLUGIN(style, menu_style_coverflow_ep)
