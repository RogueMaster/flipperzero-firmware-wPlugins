#include "view.h"

#include <f0_claude_remote_icons.h>

#include "view_main.h"
#include "view_options.h"

typedef struct {
    const Icon* mark;
    const Icon* no_link_mark;
    const char* no_link_message;
    const Icon* switch_mark;
} ViewLink;

static const ViewLink view_links[LinkMAX] = {
    [LinkBluetooth] =
        {
            .mark = &I_bluetooth,
            .no_link_mark = &I_bluetooth_off,
            .no_link_message = "Reconnecting...",
            .switch_mark = &I_usb,
        },
    [LinkUsb] =
        {
            .mark = &I_usb,
            .no_link_mark = &I_usb_off,
            .no_link_message = "Connect the cable",
            .switch_mark = &I_bluetooth,
        },
};

/* The title starts 3 px past its mark, whichever mark it is. */
static void view_draw_title_row(Canvas* canvas, const Icon* mark, const char* title) {
    canvas_draw_icon(canvas, 2, 1, mark);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, icon_get_width(mark) + 5, 9, title);
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void view_draw_voice(Canvas* canvas, Link link) {
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorWhite);
    view_draw_title_row(canvas, view_links[link].mark, "Connected");
    canvas_draw_icon(canvas, 13, 27, &I_microphone_large);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 39, 37, "Listening");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 39, 48, "Release OK to stop");
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_no_link(Canvas* canvas, Link link, int32_t hint_x, const char* hint) {
    view_draw_title_row(canvas, view_links[link].no_link_mark, "No link");
    canvas_draw_str_aligned(
        canvas, 64, 28, AlignCenter, AlignBottom, view_links[link].no_link_message);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, hint_x, 41, hint);
}

static void view_draw_switch_plate(Canvas* canvas, Link link) {
    canvas_draw_rbox(canvas, 32, 47, 64, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 35, 49, &I_right);
    canvas_draw_str(canvas, 42, 56, "Switch to");
    canvas_draw_icon(canvas, 86, 48, view_links[link].switch_mark);
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_hold_legend(Canvas* canvas) {
    canvas_draw_rbox(canvas, 97, 0, 30, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 100, 9, "HOLD");
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_stopped(Canvas* canvas, Link link) {
    view_draw_title_row(canvas, view_links[link].no_link_mark, "No link");
    canvas_draw_str(canvas, 17, 28, "Reconnect stopped");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 28, 41, "Any button: retry");
    canvas_draw_rframe(canvas, 48, 47, 33, 11, 2);
    canvas_draw_icon(canvas, 51, 49, &I_back);
    canvas_draw_str(canvas, 62, 56, "exit");
}

void view_draw(Canvas* canvas, const ViewState* state) {
    switch(state->mode) {
    case ModeMain:
        view_draw_title_row(
            canvas,
            view_links[state->link].mark,
            state->sent == ActionNone ? "Connected" : "Sent:");
        view_main_draw(canvas, state);
        break;
    case ModeVoice:
        view_draw_voice(canvas, state->link);
        break;
    case ModeOptions:
        view_draw_title_row(canvas, view_links[state->link].mark, "Options");
        view_options_draw(canvas, state);
        break;
    case ModeNoLink:
        view_draw_no_link(canvas, state->link, 28, "Keys are not sent");
        view_draw_switch_plate(canvas, state->link);
        view_draw_hold_legend(canvas);
        break;
    case ModeVoiceCut:
        view_draw_no_link(canvas, state->link, 11, "Voice stopped: release OK");
        break;
    case ModeOptionsNoLink:
        view_draw_title_row(canvas, view_links[state->link].no_link_mark, "Options");
        view_options_draw(canvas, state);
        break;
    case ModeStopped:
        view_draw_stopped(canvas, state->link);
        break;
    }
}
