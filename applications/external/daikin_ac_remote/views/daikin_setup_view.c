#include "daikin_setup_view.h"
#include "../daikin_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

#define SCREEN_W 64

#ifndef FAP_VERSION
#define FAP_VERSION "?"
#endif

// Rows. The variant picker only exists when the protocol declares variants,
// in which case the save-state row shrinks to make room.
#define ROW_SAVE 0
#define ROW_OPT  1

typedef struct {
    DaikinState* state;
    uint8_t focus;
} DaikinSetupViewModel;

struct DaikinSetupView {
    View* view;
};

static uint8_t row_count(void) {
    return daikin_ir_get_option_count() > 0 ? 2 : 1;
}

static void draw_toggle_switch(Canvas* canvas, int16_t x, int16_t y, bool value) {
    int16_t sw_w = 36;
    int16_t sw_h = 16;
    canvas_draw_rframe(canvas, x, y, sw_w, sw_h, sw_h / 2);
    if(value) {
        canvas_draw_disc(canvas, x + sw_w - sw_h / 2 - 2, y + sw_h / 2, 5);
    } else {
        canvas_draw_circle(canvas, x + sw_h / 2 + 2, y + sw_h / 2, 5);
    }
}

static void daikin_setup_view_draw(Canvas* canvas, void* model) {
    DaikinSetupViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "Settings");
    canvas_draw_line(canvas, 0, 11, SCREEN_W, 11);

    bool has_opt = daikin_ir_get_option_count() > 0;

    // ---- save state ----
    int16_t y = 14;
    int16_t h = has_opt ? 46 : 56;
    if(row_count() > 1 && m->focus == ROW_SAVE) {
        canvas_draw_rframe(canvas, 1, y, SCREEN_W - 2, h, 3);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, y + 3, AlignCenter, AlignTop, "Save state");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 32, y + 13, AlignCenter, AlignTop, m->state->save_state ? "ON" : "OFF");
    draw_toggle_switch(canvas, 14, y + 25, m->state->save_state);

    // ---- protocol variant ----
    if(has_opt) {
        int16_t oy = 64;
        int16_t oh = 40;
        if(m->focus == ROW_OPT) {
            canvas_draw_rframe(canvas, 1, oy, SCREEN_W - 2, oh, 3);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 32, oy + 3, AlignCenter, AlignTop, daikin_ir_get_option_label());

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            32,
            oy + 15,
            AlignCenter,
            AlignTop,
            daikin_ir_get_option_name(m->state->option));

        // Arrows, so it reads as adjustable. The focus frame runs x=1..62, so
        // these sit far enough in to clear it: a glyph centred at 5 covers
        // 3..6 and one centred at 59 covers 57..60, leaving a pixel of air
        // against each border. The longest variant name spans 11..53, so
        // there is still room between them.
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 5, oy + 16, AlignCenter, AlignTop, "<");
        canvas_draw_str_aligned(canvas, 59, oy + 16, AlignCenter, AlignTop, ">");

        char pos[12];
        snprintf(
            pos,
            sizeof(pos),
            "%u/%u",
            (unsigned)(m->state->option + 1),
            (unsigned)daikin_ir_get_option_count());
        canvas_draw_str_aligned(canvas, 32, oy + 28, AlignCenter, AlignTop, pos);
    } else {
        canvas_draw_rframe(canvas, 2, 74, 60, 34, 3);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 78, AlignCenter, AlignTop, "When ON:");
        canvas_draw_str_aligned(canvas, 32, 88, AlignCenter, AlignTop, "Settings are");
        canvas_draw_str_aligned(canvas, 32, 98, AlignCenter, AlignTop, "saved to SD");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 116, AlignCenter, AlignTop, "v" FAP_VERSION);
}

static bool daikin_setup_view_input(InputEvent* event, void* context) {
    DaikinSetupView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        DaikinSetupViewModel * m,
        {
            if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
                // nothing
            } else {
                uint8_t rows = row_count();
                uint8_t opts = daikin_ir_get_option_count();
                switch(event->key) {
                case InputKeyUp:
                    if(m->focus > 0) m->focus--;
                    consumed = true;
                    break;
                case InputKeyDown:
                    if(m->focus + 1 < rows) m->focus++;
                    consumed = true;
                    break;
                case InputKeyLeft:
                    if(m->focus == ROW_OPT && opts) {
                        m->state->option = (uint8_t)((m->state->option + opts - 1) % opts);
                    } else if(m->focus == ROW_SAVE) {
                        m->state->save_state = !m->state->save_state;
                    }
                    consumed = true;
                    break;
                case InputKeyRight:
                    if(m->focus == ROW_OPT && opts) {
                        m->state->option = (uint8_t)((m->state->option + 1) % opts);
                    } else if(m->focus == ROW_SAVE) {
                        m->state->save_state = !m->state->save_state;
                    }
                    consumed = true;
                    break;
                case InputKeyOk:
                    if(m->focus == ROW_OPT && opts) {
                        m->state->option = (uint8_t)((m->state->option + 1) % opts);
                    } else {
                        m->state->save_state = !m->state->save_state;
                    }
                    consumed = true;
                    break;
                default:
                    break;
                }
            }
        },
        consumed);

    return consumed;
}

DaikinSetupView* daikin_setup_view_alloc(void) {
    DaikinSetupView* view = malloc(sizeof(DaikinSetupView));
    view->view = view_alloc();

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(DaikinSetupViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, daikin_setup_view_draw);
    view_set_input_callback(view->view, daikin_setup_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    with_view_model(
        view->view,
        DaikinSetupViewModel * m,
        {
            m->state = NULL;
            m->focus = ROW_SAVE;
        },
        true);

    return view;
}

void daikin_setup_view_free(DaikinSetupView* view) {
    if(view) {
        view_free(view->view);
        free(view);
    }
}

View* daikin_setup_view_get_view(DaikinSetupView* view) {
    return view->view;
}

void daikin_setup_view_set_state(DaikinSetupView* view, DaikinState* state) {
    with_view_model(view->view, DaikinSetupViewModel * m, { m->state = state; }, true);
}
