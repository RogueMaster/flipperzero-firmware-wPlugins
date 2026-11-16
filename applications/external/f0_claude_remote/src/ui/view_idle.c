#include "view_idle.h"

#include <f0_claude_remote_icons.h>

static void view_idle_draw_white_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, str);
    canvas_set_color(canvas, ColorBlack);
}

static void view_idle_draw_white_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, x, y, icon);
    canvas_set_color(canvas, ColorBlack);
}

/* Clears first: the ring runs under the elements that lie on it. */
static void view_idle_draw_outline(Canvas* canvas, int32_t x, int32_t y, size_t width) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, width, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, width, 11, 2);
}

static void
    view_idle_draw_split(Canvas* canvas, int32_t x, int32_t y, size_t width, int32_t tap_x) {
    view_idle_draw_outline(canvas, x, y, width);
    canvas_draw_rbox(canvas, x, y, tap_x - x, 11, 2);
    canvas_draw_box(canvas, tap_x - 2, y, 2, 11);
}

static void view_idle_draw_legend(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    view_idle_draw_split(canvas, 74, 0, 53, 104);
    view_idle_draw_white_str(canvas, 77, 9, "HOLD");
    canvas_draw_str(canvas, 106, 9, "TAP");
}

/* One outline from two shapes: each erases the other's line inside it. */
static void view_idle_draw_recess(Canvas* canvas) {
    canvas_draw_rframe(canvas, 89, 48, 38, 15, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, 73, 38, 23);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_circle(canvas, 73, 38, 23);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, 90, 49, 36, 13, 3);
    canvas_set_color(canvas, ColorBlack);
}

static void view_idle_draw_ring(Canvas* canvas) {
    view_idle_draw_recess(canvas);
    view_idle_draw_split(canvas, 15, 14, 65, 68);
    view_idle_draw_white_icon(canvas, 18, 15, &I_slash);
    view_idle_draw_white_str(canvas, 22, 22, "effort high");
    canvas_draw_icon(canvas, 70, 17, &I_up);
    view_idle_draw_split(canvas, 14, 52, 66, 68);
    view_idle_draw_white_icon(canvas, 17, 54, &I_slash);
    view_idle_draw_white_str(canvas, 21, 61, "effort max");
    canvas_draw_icon(canvas, 70, 55, &I_down);
}

/* The canvas fills no triangle, so both halves of the diagonal go row by row. */
static void view_idle_draw_ok_button(Canvas* canvas) {
    canvas_draw_rframe(canvas, 58, 28, 30, 21, 2);
    for(int32_t y = 29; y <= 47; y++) {
        canvas_draw_line(canvas, 59, y, 113 - y, y);
    }
    view_idle_draw_white_icon(canvas, 61, 30, &I_microphone_9x13);
    canvas_draw_icon(canvas, 76, 39, &I_enter);
}

static void view_idle_draw_side_actions(Canvas* canvas) {
    canvas_draw_rbox(canvas, 17, 33, 38, 11, 2);
    view_idle_draw_white_icon(canvas, 20, 35, &I_slash);
    view_idle_draw_white_str(canvas, 24, 42, "clear");
    view_idle_draw_white_icon(canvas, 48, 35, &I_left);
    view_idle_draw_outline(canvas, 92, 33, 28);
    canvas_draw_icon(canvas, 95, 35, &I_right);
    canvas_draw_str(canvas, 102, 42, "Esc");
    canvas_draw_rbox(canvas, 92, 50, 33, 11, 2);
    view_idle_draw_white_icon(canvas, 95, 52, &I_back);
    view_idle_draw_white_str(canvas, 106, 59, "exit");
}

static void view_idle_draw_flash(Canvas* canvas, Action flash) {
    canvas_set_color(canvas, ColorXOR);
    switch(flash) {
    case ActionArrowUp:
        canvas_draw_box(canvas, 68, 15, 11, 9);
        break;
    case ActionArrowDown:
        canvas_draw_box(canvas, 68, 53, 11, 9);
        break;
    case ActionEnter:
        for(int32_t y = 29; y <= 47; y++) {
            canvas_draw_line(canvas, 114 - y, y, 86, y);
        }
        break;
    case ActionEffortHigh:
        canvas_draw_box(canvas, 16, 15, 51, 9);
        break;
    case ActionEffortMax:
        canvas_draw_box(canvas, 15, 53, 52, 9);
        break;
    case ActionClear:
        canvas_draw_box(canvas, 18, 34, 36, 9);
        break;
    case ActionEsc:
        canvas_draw_box(canvas, 93, 34, 26, 9);
        break;
    default:
        break;
    }
    canvas_set_color(canvas, ColorBlack);
}

void view_idle_draw(Canvas* canvas, Action flash) {
    view_idle_draw_legend(canvas);
    view_idle_draw_ring(canvas);
    view_idle_draw_ok_button(canvas);
    view_idle_draw_side_actions(canvas);
    view_idle_draw_flash(canvas, flash);
}
