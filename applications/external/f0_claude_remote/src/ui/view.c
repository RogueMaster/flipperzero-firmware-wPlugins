#include "view.h"

#include <f0_claude_remote_icons.h>

#include "view_idle.h"

static void view_draw_status_row(Canvas* canvas) {
    canvas_draw_icon(canvas, 2, 1, &I_bluetooth);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 12, 9, "Connected");
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void view_draw_voice(Canvas* canvas) {
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorWhite);
    view_draw_status_row(canvas);
    canvas_draw_icon(canvas, 13, 27, &I_microphone_large);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 39, 37, "Listening");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 39, 48, "Release OK to stop");
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_no_link_status_row(Canvas* canvas) {
    canvas_draw_icon(canvas, 2, 1, &I_bluetooth_off);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 9, "No link");
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void view_draw_no_link(Canvas* canvas, int32_t hint_x, const char* hint) {
    view_draw_no_link_status_row(canvas);
    canvas_draw_str(canvas, 27, 37, "Reconnecting...");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, hint_x, 49, hint);
}

static void view_draw_stopped(Canvas* canvas) {
    view_draw_no_link_status_row(canvas);
    canvas_draw_str(canvas, 17, 28, "Reconnect stopped");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 28, 41, "Any button: retry");
    canvas_draw_rframe(canvas, 48, 47, 33, 11, 2);
    canvas_draw_icon(canvas, 51, 49, &I_back);
    canvas_draw_str(canvas, 62, 56, "exit");
}

void view_draw(Canvas* canvas, Mode mode, Action flash) {
    switch(mode) {
    case ModeIdle:
        view_draw_status_row(canvas);
        view_idle_draw(canvas, flash);
        break;
    case ModeVoice:
        view_draw_voice(canvas);
        break;
    case ModeNoLink:
        view_draw_no_link(canvas, 28, "Keys are not sent");
        break;
    case ModeVoiceCut:
        view_draw_no_link(canvas, 11, "Voice stopped: release OK");
        break;
    case ModeStopped:
        view_draw_stopped(canvas);
        break;
    }
}
