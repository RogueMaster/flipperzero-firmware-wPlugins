#include "pause_menu.h"
#include "moon_lander.h"

/* Rows top to bottom. Zero tilt only shows in the tilt modes. */
typedef enum {
    PauseRowResume = 0,
    PauseRowZeroTilt,
    PauseRowQuit,
} PauseRow;

static int row_count(const PauseMenu* p) {
    return p->zero_tilt ? 3 : 2;
}

static PauseRow row_at(const PauseMenu* p, int i) {
    if(i == 0) return PauseRowResume;
    if(i == 1 && p->zero_tilt) return PauseRowZeroTilt;
    return PauseRowQuit;
}

static const char* row_label(PauseRow row) {
    switch(row) {
        case PauseRowZeroTilt: return "Zero tilt";
        case PauseRowQuit:     return "Back to menu";
        default:               return "Resume";
    }
}

void pause_menu_open(PauseMenu* p, bool zero_tilt) {
    p->open      = true;
    p->zero_tilt = zero_tilt;
    p->sel       = 0;
}

PauseAction pause_menu_input(PauseMenu* p, const InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return PauseActionNone;
    int last = row_count(p) - 1;
    if(ev->key == InputKeyUp && p->sel > 0) p->sel--;
    if(ev->key == InputKeyDown && p->sel < last) p->sel++;
    if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
        switch(row_at(p, p->sel)) {
            case PauseRowZeroTilt: return PauseActionZeroTilt;
            case PauseRowQuit:     return PauseActionQuit;
            default:               return PauseActionResume;
        }
    }
    return PauseActionNone;
}

void pause_menu_draw(Canvas* canvas, const PauseMenu* p) {
    const int count = row_count(p);
    const int row_h = 11;
    const int bw = 76;
    const int bh = 16 + count * row_h + 2;
    const int bx = (SCREEN_W - bw) / 2;
    const int by = (SCREEN_H - bh) / 2;
    /* Rounded fill, so no white corner pixels show outside the frame. */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, bx, by, bw, bh, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, 3);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 7, AlignCenter, AlignCenter, "PAUSED");
    canvas_draw_line(canvas, bx + 2, by + 13, bx + bw - 3, by + 13);

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < count; i++) {
        int ry = by + 16 + i * row_h;
        if(i == p->sel) {
            canvas_draw_rbox(canvas, bx + 4, ry, bw - 8, row_h - 1, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, ry + 5, AlignCenter, AlignCenter, row_label(row_at(p, i)));
        canvas_set_color(canvas, ColorBlack);
    }
}
