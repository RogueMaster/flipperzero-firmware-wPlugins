#include "coolix_extra_view.h"
#include "../coolix_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

// Screen dimensions in vertical mode
#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 128

#define ROW_H     15
#define ROW_FIRST 18

// Model
typedef struct {
    CoolixState* state;
    const char* last_sent; // owned by the app, read-only here
    uint8_t selected;
} CoolixExtraViewModel;

struct CoolixExtraView {
    View* view;
    CoolixExtraViewSendCallback send_callback;
    void* send_context;
};

static void coolix_extra_view_draw(Canvas* canvas, void* model) {
    CoolixExtraViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "Extra");
    canvas_draw_line(canvas, 0, 11, SCREEN_WIDTH, 11);

    // Command rows
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < CoolixExtraCount; i++) {
        int16_t y = ROW_FIRST + i * ROW_H;
        bool focused = i == m->selected;

        if(focused) {
            canvas_draw_rbox(canvas, 2, y, SCREEN_WIDTH - 4, ROW_H - 3, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, y, SCREEN_WIDTH - 4, ROW_H - 3, 2);
        }

        canvas_draw_str_aligned(
            canvas, 32, y + 2, AlignCenter, AlignTop, coolix_ir_get_extra_name((CoolixExtra)i));
        canvas_set_color(canvas, ColorBlack);
    }

    // Code of the highlighted command
    char buf[24];
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "0x%06lX",
        (unsigned long)coolix_ir_get_extra_code((CoolixExtra)m->selected));
    canvas_draw_str_aligned(
        canvas, 32, ROW_FIRST + CoolixExtraCount * ROW_H + 4, AlignCenter, AlignTop, buf);

    // What actually went out last, from anywhere in the app. The main screen's
    // state word would not change when an Extra command fires, which reads as
    // the screen being stuck.
    canvas_draw_line(canvas, 0, 103, SCREEN_WIDTH, 103);
    canvas_draw_str_aligned(canvas, 32, 106, AlignCenter, AlignTop, "Last sent");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 32, 116, AlignCenter, AlignTop, m->last_sent ? m->last_sent : "--");
}

static bool coolix_extra_view_input(InputEvent* event, void* context) {
    CoolixExtraView* view = context;
    bool consumed = false;
    bool send = false;
    CoolixExtra to_send = CoolixExtraSilence;

    with_view_model(
        view->view,
        CoolixExtraViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                switch(event->key) {
                case InputKeyUp:
                    if(m->selected > 0) {
                        m->selected--;
                    }
                    consumed = true;
                    break;
                case InputKeyDown:
                    if(m->selected + 1 < CoolixExtraCount) {
                        m->selected++;
                    }
                    consumed = true;
                    break;
                case InputKeyOk:
                    if(event->type == InputTypeShort) {
                        to_send = (CoolixExtra)m->selected;
                        send = true;
                    }
                    consumed = true;
                    break;
                default:
                    break;
                }
            }
        },
        consumed);

    if(send && view->send_callback) {
        view->send_callback(to_send, view->send_context);
    }

    return consumed;
}

CoolixExtraView* coolix_extra_view_alloc(void) {
    CoolixExtraView* view = malloc(sizeof(CoolixExtraView));
    view->view = view_alloc();
    view->send_callback = NULL;
    view->send_context = NULL;

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(CoolixExtraViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, coolix_extra_view_draw);
    view_set_input_callback(view->view, coolix_extra_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    with_view_model(
        view->view,
        CoolixExtraViewModel * m,
        {
            m->state = NULL;
            m->last_sent = NULL;
            m->selected = 0;
        },
        true);

    return view;
}

void coolix_extra_view_free(CoolixExtraView* view) {
    if(view) {
        view_free(view->view);
        free(view);
    }
}

View* coolix_extra_view_get_view(CoolixExtraView* view) {
    return view->view;
}

void coolix_extra_view_set_state(CoolixExtraView* view, CoolixState* state) {
    with_view_model(view->view, CoolixExtraViewModel * m, { m->state = state; }, true);
}

void coolix_extra_view_set_last_sent(CoolixExtraView* view, const char* last_sent) {
    with_view_model(view->view, CoolixExtraViewModel * m, { m->last_sent = last_sent; }, true);
}

void coolix_extra_view_set_send_callback(
    CoolixExtraView* view,
    CoolixExtraViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}
