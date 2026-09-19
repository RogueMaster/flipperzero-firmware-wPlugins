#include "uhf_view_helpers.h"

#include <stdio.h>
#include <string.h>

void uhf_view_draw_right_text(Canvas* canvas, int y, const char* text) {
    if(!canvas || !text) return;
    int x = 128 - canvas_string_width(canvas, text);
    if(x < 0) x = 0;
    canvas_draw_str(canvas, x, y, text);
}

void uhf_view_draw_right_fraction(Canvas* canvas, int y, size_t current, size_t total) {
    if(!canvas) return;
    char current_text[12];
    char total_text[12];
    snprintf(current_text, sizeof(current_text), "%lu", (unsigned long)current);
    snprintf(total_text, sizeof(total_text), "%lu", (unsigned long)total);
    const int current_width = canvas_string_width(canvas, current_text);
    const int slash_width = canvas_string_width(canvas, "/");
    const int total_width = canvas_string_width(canvas, total_text);
    int x = 128 - current_width - 1 - slash_width - 1 - total_width;
    canvas_draw_str(canvas, x, y, current_text);
    x += current_width + 1;
    canvas_draw_str(canvas, x, y, "/");
    canvas_draw_str(canvas, x + slash_width + 1, y, total_text);
}

void uhf_view_draw_fixed_side_button(Canvas* canvas, const char* text, bool right) {
    if(!canvas || !text) return;
    const int button_x = right ? 92 : 0;
    const int text_width = canvas_string_width(canvas, text);
    const int text_x = right ? 120 - text_width : 8;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, button_x, 52, 36, 12, 3);
    canvas_draw_box(canvas, button_x, 61, 36, 3);
    canvas_draw_box(canvas, right ? 125 : 0, 52, 3, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, text_x, 61, text);
    canvas_draw_str(canvas, right ? 122 : 2, 61, right ? ">" : "<");
    canvas_set_color(canvas, ColorBlack);
}

void uhf_view_draw_fixed_center_button(Canvas* canvas, const char* text) {
    if(!canvas || !text) return;
    const int text_width = canvas_string_width(canvas, text);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 46, 52, 36, 12, 3);
    canvas_draw_box(canvas, 46, 61, 36, 3);
    canvas_set_color(canvas, ColorWhite);
    const int offset = strcmp(text, "Save") == 0 ? 0 : -1;
    canvas_draw_rframe(canvas, 49, 56 + offset, 5, 5, 1);
    canvas_draw_str(canvas, 56 + (24 - text_width) / 2, 62 + offset, text);
    canvas_set_color(canvas, ColorBlack);
}
