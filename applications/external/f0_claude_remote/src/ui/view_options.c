#include "view_options.h"

#include <f0_claude_remote_icons.h>

#include "view_plate.h"

typedef struct {
    const char* name;
    const char* other_value;
    int32_t top;
} ViewOptionsRow;

static const ViewOptionsRow view_options_rows[OptionMAX] = {
    [OptionVibration] = {.name = "Vibration", .other_value = "Off", .top = 16},
    [OptionBacklight] = {.name = "Backlight", .other_value = "Auto", .top = 31},
};

static void view_options_draw_plate(Canvas* canvas) {
    view_plate_draw_outline(canvas, 63, 0, 64);
    view_plate_draw_fill(canvas, 63, 0, 36);
    view_plate_draw_white_icon(canvas, 66, 2, &I_back);
    view_plate_draw_white_str(canvas, 77, 9, "EXIT");
    canvas_draw_str(canvas, 101, 9, "BACK");
}

/* The text sits 1 px above the frame's middle, where the g of Backlight clears its bottom. */
static void view_options_draw_row(Canvas* canvas, const ViewOptionsRow* row, bool on) {
    canvas_draw_str(canvas, 5, row->top + 9, row->name);
    canvas_draw_str_aligned(
        canvas, 102, row->top + 9, AlignCenter, AlignBottom, on ? "On" : row->other_value);
}

static void view_options_draw_cursor(Canvas* canvas, const ViewOptionsRow* row) {
    canvas_draw_rframe(canvas, 1, row->top, 125, 13, 2);
    canvas_draw_icon(canvas, 80, row->top + 2, &I_left);
    canvas_draw_icon(canvas, 120, row->top + 2, &I_right);
}

void view_options_draw(Canvas* canvas, const ViewState* state) {
    canvas_set_font(canvas, FontSecondary);
    view_options_draw_plate(canvas);
    for(Option option = OptionVibration; option < OptionMAX; option++) {
        view_options_draw_row(canvas, &view_options_rows[option], state->options[option]);
    }
    view_options_draw_cursor(canvas, &view_options_rows[state->cursor]);
}
