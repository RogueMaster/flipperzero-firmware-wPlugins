#include "view_main.h"

#include <f0_claude_remote_icons.h>

#include "../core/charge.h"
#include "view_plate.h"

static const uint8_t view_main_battery_cells = 20;
static const char* const view_main_effort_high = "effort high";
static const char* const view_main_effort_max = "effort max";
static const char* const view_main_clear = "clear";
static const char* const view_main_esc = "Esc";

static void view_main_draw_legend(Canvas* canvas) {
    view_plate_draw_outline(canvas, 74, 0, 53);
    view_plate_draw_fill(canvas, 74, 0, 30);
    view_plate_draw_white_str(canvas, 77, 9, "HOLD");
    canvas_draw_str(canvas, 106, 9, "TAP");
}

/* As the firmware's status bar draws it (power.c:17-48); u8g2 draws no box of zero width
 * (u8g2_box.c:40). */
static void view_main_draw_sent(Canvas* canvas, Action sent) {
    switch(sent) {
    case ActionEffortHigh:
        canvas_draw_icon(canvas, 41, 2, &I_slash);
        canvas_draw_str(canvas, 45, 9, view_main_effort_high);
        break;
    case ActionEffortMax:
        canvas_draw_icon(canvas, 41, 2, &I_slash);
        canvas_draw_str(canvas, 45, 9, view_main_effort_max);
        break;
    case ActionClear:
        canvas_draw_icon(canvas, 41, 2, &I_slash);
        canvas_draw_str(canvas, 45, 9, view_main_clear);
        break;
    case ActionEsc:
        canvas_draw_str(canvas, 41, 9, view_main_esc);
        break;
    case ActionEnter:
        canvas_draw_icon(canvas, 41, 2, &I_enter);
        break;
    case ActionArrowLeft:
        canvas_draw_icon(canvas, 41, 2, &I_left);
        break;
    case ActionArrowRight:
        canvas_draw_icon(canvas, 41, 2, &I_right);
        break;
    case ActionArrowUp:
        canvas_draw_icon(canvas, 41, 4, &I_up);
        break;
    case ActionArrowDown:
        canvas_draw_icon(canvas, 41, 4, &I_down);
        break;
    case ActionNone:
    case ActionVoiceStart:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
        break;
    }
}

static void view_main_draw_battery(Canvas* canvas, uint8_t charge, bool charging) {
    canvas_draw_icon(canvas, 99, 2, &I_battery);
    uint8_t cells = charge_cells(charge, view_main_battery_cells);
    if(cells > 0) {
        canvas_draw_box(canvas, 101, 4, cells, 4);
    }
    if(charging) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 107, 1, &I_charging_lightning_mask);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, 107, 1, &I_charging_lightning);
        canvas_set_bitmap_mode(canvas, false);
    }
}

static void view_main_draw_ring(Canvas* canvas) {
    /* Comes first: an icon paints its whole box, the clear cells too. */
    canvas_draw_icon(canvas, 38, 15, &I_recess);
    view_plate_draw_outline(canvas, 3, 14, 65);
    view_plate_draw_fill(canvas, 3, 14, 53);
    view_plate_draw_white_icon(canvas, 6, 15, &I_slash);
    view_plate_draw_white_str(canvas, 10, 22, view_main_effort_high);
    canvas_draw_icon(canvas, 58, 17, &I_up);
    view_plate_draw_outline(canvas, 2, 52, 66);
    view_plate_draw_fill(canvas, 2, 52, 54);
    view_plate_draw_white_icon(canvas, 5, 54, &I_slash);
    view_plate_draw_white_str(canvas, 9, 61, view_main_effort_max);
    canvas_draw_icon(canvas, 58, 55, &I_down);
}

/* The canvas fills no triangle, so both halves of the diagonal go row by row. */
static void view_main_draw_ok_button(Canvas* canvas) {
    canvas_draw_rframe(canvas, 46, 28, 30, 21, 2);
    for(int32_t y = 29; y <= 47; y++) {
        canvas_draw_line(canvas, 47, y, 101 - y, y);
    }
    view_plate_draw_white_icon(canvas, 49, 30, &I_microphone_9x13);
    canvas_draw_icon(canvas, 64, 39, &I_enter);
}

static void view_main_draw_side_actions(Canvas* canvas) {
    view_plate_draw_outline(canvas, 3, 33, 40);
    view_plate_draw_fill(canvas, 3, 33, 31);
    view_plate_draw_white_icon(canvas, 6, 35, &I_slash);
    view_plate_draw_white_str(canvas, 10, 42, view_main_clear);
    canvas_draw_icon(canvas, 36, 35, &I_left);
    view_plate_draw_outline(canvas, 80, 33, 30);
    view_plate_draw_fill(canvas, 89, 33, 21);
    canvas_draw_icon(canvas, 83, 35, &I_right);
    view_plate_draw_white_str(canvas, 92, 42, view_main_esc);
    view_plate_draw_outline(canvas, 78, 46, 45);
    view_plate_draw_fill(canvas, 78, 46, 33);
    view_plate_draw_white_icon(canvas, 81, 48, &I_back);
    view_plate_draw_white_str(canvas, 92, 55, "exit");
    canvas_draw_icon(canvas, 113, 48, &I_options);
}

static void view_main_draw_flash(Canvas* canvas, Action flash) {
    canvas_set_color(canvas, ColorXOR);
    switch(flash) {
    case ActionArrowUp:
        canvas_draw_box(canvas, 56, 15, 11, 9);
        break;
    case ActionArrowDown:
        canvas_draw_box(canvas, 56, 53, 11, 9);
        break;
    case ActionArrowLeft:
        canvas_draw_box(canvas, 34, 34, 8, 9);
        break;
    case ActionArrowRight:
        canvas_draw_box(canvas, 81, 34, 8, 9);
        break;
    case ActionEnter:
        for(int32_t y = 29; y <= 47; y++) {
            canvas_draw_line(canvas, 102 - y, y, 74, y);
        }
        break;
    case ActionEffortHigh:
        canvas_draw_box(canvas, 4, 15, 51, 9);
        break;
    case ActionEffortMax:
        canvas_draw_box(canvas, 3, 53, 52, 9);
        break;
    case ActionClear:
        canvas_draw_box(canvas, 4, 34, 29, 9);
        break;
    case ActionEsc:
        canvas_draw_box(canvas, 90, 34, 19, 9);
        break;
    case ActionNone:
    case ActionVoiceStart:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
        break;
    }
    canvas_set_color(canvas, ColorBlack);
}

void view_main_draw(Canvas* canvas, const ViewState* state) {
    canvas_set_font(canvas, FontSecondary);
    view_main_draw_sent(canvas, state->sent);
    if(state->legend) {
        view_main_draw_legend(canvas);
    } else {
        view_main_draw_battery(canvas, state->charge, state->charging);
    }
    view_main_draw_ring(canvas);
    view_main_draw_ok_button(canvas);
    view_main_draw_side_actions(canvas);
    view_main_draw_flash(canvas, state->flash);
}
