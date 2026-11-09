#include "view.h"

#include <f0_claude_remote_icons.h>

#include "view_idle.h"

static void view_draw_status_row(Canvas* canvas, Link link) {
    canvas_draw_icon(canvas, 2, 1, link == LinkUsb ? &I_usb : &I_bluetooth);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 12, 9, "Connected");
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void view_draw_voice(Canvas* canvas, Link link) {
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorWhite);
    view_draw_status_row(canvas, link);
    canvas_draw_icon(canvas, 13, 27, &I_microphone_large);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 39, 37, "Listening");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 39, 48, "Release OK to stop");
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_no_link_status_row(Canvas* canvas, Link link) {
    canvas_draw_icon(canvas, 2, 1, link == LinkUsb ? &I_usb_off : &I_bluetooth_off);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 9, "No link");
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void view_draw_no_link(Canvas* canvas, Link link, int32_t hint_x, const char* hint) {
    view_draw_no_link_status_row(canvas, link);
    if(link == LinkUsb) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, "Connect the cable");
    } else {
        canvas_draw_str(canvas, 27, 28, "Reconnecting...");
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, hint_x, 41, hint);
}

static void view_draw_switch_plate(Canvas* canvas, Link link) {
    canvas_draw_rbox(canvas, 32, 47, 64, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 35, 49, &I_right);
    canvas_draw_str(canvas, 42, 56, "Switch to");
    canvas_draw_icon(canvas, 86, 48, link == LinkUsb ? &I_bluetooth : &I_usb);
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_hold_legend(Canvas* canvas) {
    canvas_draw_rbox(canvas, 97, 0, 30, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 100, 9, "HOLD");
    canvas_set_color(canvas, ColorBlack);
}

static void view_draw_stopped(Canvas* canvas, Link link) {
    view_draw_no_link_status_row(canvas, link);
    canvas_draw_str(canvas, 17, 28, "Reconnect stopped");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 28, 41, "Any button: retry");
    canvas_draw_rframe(canvas, 48, 47, 33, 11, 2);
    canvas_draw_icon(canvas, 51, 49, &I_back);
    canvas_draw_str(canvas, 62, 56, "exit");
}

void view_draw(Canvas* canvas, Mode mode, Link link, Action flash) {
    switch(mode) {
    case ModeIdle:
        view_draw_status_row(canvas, link);
        view_idle_draw(canvas, flash);
        break;
    case ModeVoice:
        view_draw_voice(canvas, link);
        break;
    case ModeNoLink:
        view_draw_no_link(canvas, link, 28, "Keys are not sent");
        view_draw_switch_plate(canvas, link);
        view_draw_hold_legend(canvas);
        break;
    case ModeVoiceCut:
        view_draw_no_link(canvas, link, 11, "Voice stopped: release OK");
        break;
    case ModeStopped:
        view_draw_stopped(canvas, link);
        break;
    }
}
