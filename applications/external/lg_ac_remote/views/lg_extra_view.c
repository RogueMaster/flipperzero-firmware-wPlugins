#include "lg_extra_view.h"
#include "../lg_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>

#define SCREEN_W 64

// Vertical bands, none of which may overlap on the 128px screen:
//   0..9    title        11  rule
//   14..68  four rows    70  scroll counter
//   80      rule         83..101  "Sends" + payload
//   106..124 "State now" + payload
#define ROW_H        14
#define ROW_FIRST    14
#define ROWS_VISIBLE 4
#define COUNTER_Y    70
#define FOOT_RULE_Y  80
#define SENDS_Y      83
#define SENDS_VAL_Y  93
#define STATE_Y      106
#define STATE_VAL_Y  116

typedef struct {
    LgState* state;
    const char* last_sent; // owned by the app, read-only here
    uint8_t selected;
    uint8_t scroll; // index of the first drawn row
} LgExtraViewModel;

struct LgExtraView {
    View* view;
    LgExtraViewSendCallback send_callback;
    void* send_context;
};

static void lg_extra_view_draw(Canvas* canvas, void* model) {
    LgExtraViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "Extra");
    canvas_draw_line(canvas, 0, 11, SCREEN_W, 11);

    uint8_t total = (uint8_t)LgExtraCount;
    uint8_t shown = total < ROWS_VISIBLE ? total : ROWS_VISIBLE;

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t r = 0; r < shown; r++) {
        uint8_t i = m->scroll + r;
        if(i >= total) break;
        int16_t y = ROW_FIRST + r * ROW_H;
        bool focused = i == m->selected;

        if(focused) {
            canvas_draw_rbox(canvas, 2, y, SCREEN_W - 4, ROW_H - 3, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, y, SCREEN_W - 4, ROW_H - 3, 2);
        }
        canvas_draw_str_aligned(
            canvas, 32, y + 2, AlignCenter, AlignTop, lg_ir_get_extra_name((LgExtra)i));
        canvas_set_color(canvas, ColorBlack);
    }

    // Scroll position, only when the list does not fit
    if(total > ROWS_VISIBLE) {
        char pos[12];
        snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(m->selected + 1), (unsigned)total);
        canvas_draw_str_aligned(canvas, 32, COUNTER_Y, AlignCenter, AlignTop, pos);
    }

    // Payload of the highlighted command, and of the current main-screen state
    char buf[LG_CODE_STR_LEN];

    canvas_draw_line(canvas, 0, FOOT_RULE_Y, SCREEN_W, FOOT_RULE_Y);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, SENDS_Y, AlignCenter, AlignTop, "Sends");
    LgRequest req = lg_state_request(m->state);
    lg_ir_format_extra(&req, (LgExtra)m->selected, buf, sizeof(buf));
    canvas_draw_str_aligned(canvas, 32, SENDS_VAL_Y, AlignCenter, AlignTop, buf);

    // What actually went out last, from anywhere in the app. The main screen's
    // state word would not change when an Extra command fires, which reads as
    // the screen being stuck.
    canvas_draw_str_aligned(canvas, 32, STATE_Y, AlignCenter, AlignTop, "Last sent");
    canvas_draw_str_aligned(
        canvas, 32, STATE_VAL_Y, AlignCenter, AlignTop, m->last_sent ? m->last_sent : "--");
}

static bool lg_extra_view_input(InputEvent* event, void* context) {
    LgExtraView* view = context;
    bool consumed = false;
    bool send = false;
    LgExtra to_send = (LgExtra)0;

    with_view_model(
        view->view,
        LgExtraViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                switch(event->key) {
                case InputKeyUp:
                    if(m->selected > 0) m->selected--;
                    if(m->selected < m->scroll) m->scroll = m->selected;
                    consumed = true;
                    break;
                case InputKeyDown:
                    if(m->selected + 1 < (uint8_t)LgExtraCount) m->selected++;
                    if(m->selected >= m->scroll + ROWS_VISIBLE)
                        m->scroll = m->selected - ROWS_VISIBLE + 1;
                    consumed = true;
                    break;
                case InputKeyOk:
                    if(event->type == InputTypeShort) {
                        to_send = (LgExtra)m->selected;
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

    if(send && view->send_callback) view->send_callback(to_send, view->send_context);
    return consumed;
}

LgExtraView* lg_extra_view_alloc(void) {
    LgExtraView* view = malloc(sizeof(LgExtraView));
    view->view = view_alloc();
    view->send_callback = NULL;
    view->send_context = NULL;

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(LgExtraViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, lg_extra_view_draw);
    view_set_input_callback(view->view, lg_extra_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    with_view_model(
        view->view,
        LgExtraViewModel * m,
        {
            m->state = NULL;
            m->last_sent = NULL;
            m->selected = 0;
            m->scroll = 0;
        },
        true);

    return view;
}

void lg_extra_view_free(LgExtraView* view) {
    if(view) {
        view_free(view->view);
        free(view);
    }
}

View* lg_extra_view_get_view(LgExtraView* view) {
    return view->view;
}

void lg_extra_view_set_state(LgExtraView* view, LgState* state) {
    with_view_model(view->view, LgExtraViewModel * m, { m->state = state; }, true);
}

void lg_extra_view_set_last_sent(LgExtraView* view, const char* last_sent) {
    with_view_model(view->view, LgExtraViewModel * m, { m->last_sent = last_sent; }, true);
}

void lg_extra_view_set_send_callback(
    LgExtraView* view,
    LgExtraViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}
