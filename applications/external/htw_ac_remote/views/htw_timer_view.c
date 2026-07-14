#include "htw_timer_view.h"
#include "../htw_ir_protocol.h"
#include <furi.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <stdio.h>

// Screen dimensions for vertical mode
#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 128

// Focus items
typedef enum {
    TimerFocusOn = 0,
    TimerFocusOff,
    TimerFocusCount
} TimerFocusItem;

// Model
typedef struct {
    HtwState* state;
    TimerFocusItem focus;
} HtwTimerViewModel;

struct HtwTimerView {
    View* view;
    HtwTimerViewSendCallback send_callback;
    void* send_context;
};

// Helper: Get timer string for display
static void get_timer_string(uint8_t step, char* buf, size_t buf_size) {
    if(step == 0) {
        snprintf(buf, buf_size, "Off");
    } else {
        float hours = htw_ir_get_timer_hours(step);
        if(hours == (int)hours) {
            snprintf(buf, buf_size, "%dh", (int)hours);
        } else {
            snprintf(buf, buf_size, "%.1fh", (double)hours);
        }
    }
}

// Helper: Draw timer selector with arrows
static void draw_timer_selector(
    Canvas* canvas,
    int16_t y,
    const char* label,
    const char* value,
    bool focused) {
    // Label
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignTop, label);

    // Value box
    int16_t box_y = y + 12;
    int16_t box_w = 56;
    int16_t box_h = 20;
    int16_t box_x = (SCREEN_WIDTH - box_w) / 2;
    int16_t center_y = box_y + box_h / 2;

    if(focused) {
        // Draw filled box
        canvas_draw_rbox(canvas, box_x, box_y, box_w, box_h, 3);
        canvas_set_color(canvas, ColorWhite);

        // Draw arrows centered vertically
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, box_x + 6, center_y, AlignCenter, AlignCenter, "<");
        canvas_draw_str_aligned(
            canvas, box_x + box_w - 6, center_y, AlignCenter, AlignCenter, ">");

        // Draw value centered
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, center_y, AlignCenter, AlignCenter, value);

        canvas_set_color(canvas, ColorBlack);
    } else {
        // Draw frame
        canvas_draw_rframe(canvas, box_x, box_y, box_w, box_h, 3);

        // Draw value centered
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, center_y, AlignCenter, AlignCenter, value);
    }
}

static void htw_timer_view_draw(Canvas* canvas, void* model) {
    HtwTimerViewModel* m = model;
    if(!m->state) return;

    canvas_clear(canvas);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 2, AlignCenter, AlignTop, "Timer");

    // Separator line
    canvas_draw_line(canvas, 0, 14, SCREEN_WIDTH, 14);

    char timer_str[16];

    // Timer ON selector (y=20)
    get_timer_string(m->state->timer_on_step, timer_str, sizeof(timer_str));
    draw_timer_selector(canvas, 20, "Auto ON", timer_str, m->focus == TimerFocusOn);

    // Timer OFF selector (y=60)
    get_timer_string(m->state->timer_off_step, timer_str, sizeof(timer_str));
    draw_timer_selector(canvas, 60, "Auto OFF", timer_str, m->focus == TimerFocusOff);

    // Hint at bottom
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 32, 100, AlignCenter, AlignTop, "OK to send");
    canvas_draw_str_aligned(canvas, 32, 112, AlignCenter, AlignTop, "Back to return");
}

static bool htw_timer_view_input(InputEvent* event, void* context) {
    HtwTimerView* view = context;
    bool consumed = false;

    with_view_model(
        view->view,
        HtwTimerViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                switch(event->key) {
                case InputKeyUp:
                    if(m->focus == TimerFocusOff) {
                        m->focus = TimerFocusOn;
                    }
                    consumed = true;
                    break;

                case InputKeyDown:
                    if(m->focus == TimerFocusOn) {
                        m->focus = TimerFocusOff;
                    }
                    consumed = true;
                    break;

                case InputKeyLeft:
                    if(m->focus == TimerFocusOn) {
                        htw_state_timer_on_cycle(m->state, false);
                    } else if(m->focus == TimerFocusOff) {
                        htw_state_timer_off_cycle(m->state, false);
                    }
                    consumed = true;
                    break;

                case InputKeyRight:
                    if(m->focus == TimerFocusOn) {
                        htw_state_timer_on_cycle(m->state, true);
                    } else if(m->focus == TimerFocusOff) {
                        htw_state_timer_off_cycle(m->state, true);
                    }
                    consumed = true;
                    break;

                case InputKeyOk:
                    if(event->type != InputTypeShort) {
                        consumed = true;
                        break;
                    }
                    // Send timer command directly from selector
                    if(m->focus == TimerFocusOn && m->state->timer_on_step > 0) {
                        if(view->send_callback) {
                            view->send_callback(HtwTimerCommandOn, view->send_context);
                        }
                    } else if(m->focus == TimerFocusOff && m->state->timer_off_step > 0) {
                        if(view->send_callback) {
                            view->send_callback(HtwTimerCommandOff, view->send_context);
                        }
                    }
                    // If value is Off (0), do nothing - no cancel command in protocol
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

HtwTimerView* htw_timer_view_alloc(void) {
    HtwTimerView* view = malloc(sizeof(HtwTimerView));
    view->view = view_alloc();

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(HtwTimerViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, htw_timer_view_draw);
    view_set_input_callback(view->view, htw_timer_view_input);
    view_set_orientation(view->view, ViewOrientationVertical);

    with_view_model(
        view->view,
        HtwTimerViewModel * m,
        {
            m->state = NULL;
            m->focus = TimerFocusOn;
        },
        true);

    return view;
}

void htw_timer_view_free(HtwTimerView* view) {
    if(view) {
        view_free(view->view);
        free(view);
    }
}

View* htw_timer_view_get_view(HtwTimerView* view) {
    return view->view;
}

void htw_timer_view_set_state(HtwTimerView* view, HtwState* state) {
    with_view_model(view->view, HtwTimerViewModel * m, { m->state = state; }, true);
}

void htw_timer_view_set_send_callback(
    HtwTimerView* view,
    HtwTimerViewSendCallback callback,
    void* context) {
    view->send_callback = callback;
    view->send_context = context;
}
