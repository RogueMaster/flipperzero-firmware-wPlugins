// views/result_view.c
#include <gui/view.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/icon.h>

#include "mask_input_view.h"
#include "../network_subnet.h"
#define CHAR_WIDTH 7

static void increment_active_digit(MaskInputViewModel* m) {
    uint32_t temp_octet;
    uint8_t hundred = 100;
    uint8_t upper_lim;
    if(m->octect_digits[m->active_digit] == 9) {
        return;
    }
    if(m->active_digit < 3) {
        upper_lim = 3;
    } else if(m->active_digit < 6) {
        upper_lim = 6;
    } else if(m->active_digit < 9) {
        upper_lim = 9;
    } else {
        upper_lim = 12;
    }

    temp_octet = 0;
    for(int i = upper_lim - 3; i < upper_lim; i++) {
        if(i == m->active_digit) {
            temp_octet = temp_octet + (m->octect_digits[i] + 1) * hundred;
        } else {
            temp_octet = temp_octet + m->octect_digits[i] * hundred;
        }
        hundred = hundred / 10;
    }
    if(temp_octet < 256) {
        m->octect_digits[m->active_digit] += 1;
    }
}

static void decrement_active_digit(MaskInputViewModel* m) {
    if(m->octect_digits[m->active_digit] != 0) {
        m->octect_digits[m->active_digit] -= 1;
    }
}

static void mask_input_view_draw(Canvas* canvas, void* model) {
    MaskInputViewModel* m = (MaskInputViewModel*)model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "Mask Input");

    canvas_draw_line(canvas, 0, 12, 192, 12);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_font(canvas, FontKeyboard);

    uint8_t inc = 0;
    char buf[5];
    bool proc_active_digit = false;
    for(int i = 0; i < 12; i++) {
        switch(i) {
        case FirstOctet:
        case SecondOctet:
        case ThirdOctet:
            canvas_draw_str(canvas, 2 + (inc)*CHAR_WIDTH, 30, ".");
            inc += 1;
        }
        proc_active_digit = i == m->active_digit;
        if(proc_active_digit) {
            canvas_draw_box(canvas, 1 + (inc)*CHAR_WIDTH, 22, 7, 10);
            canvas_invert_color(canvas);
        }
        snprintf(buf, sizeof(buf), "%d", m->octect_digits[i]);
        canvas_draw_str(canvas, 2 + (inc)*CHAR_WIDTH, 30, buf);
        inc += 1;
        if(proc_active_digit) {
            canvas_invert_color(canvas);
        }
    }

    // Draw up/down arrows + label centred
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignBottom, "up/down - change");

    // Left and right corner buttons
    elements_button_left(canvas, "L");
    elements_button_right(canvas, "R");
    elements_button_center(canvas, "Next");
}

static bool mask_input_view_input(InputEvent* event, void* context) {
    bool consumed = false;
    NetworkSubnetApp* app = context;
    with_view_model(
        app->mask_input_view,
        MaskInputViewModel * m,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                switch(event->key) {
                case InputKeyUp:
                    increment_active_digit(m);
                    break;
                case InputKeyDown:
                    decrement_active_digit(m);
                    break;
                case InputKeyLeft:
                    if(m->active_digit != 0) {
                        m->active_digit = m->active_digit - 1;
                    } else if(m->active_digit == 0) {
                        m->active_digit = 11;
                    }
                    consumed = true;
                    break;
                case InputKeyRight:
                    if(m->active_digit < 11) {
                        m->active_digit = m->active_digit + 1;
                    } else if(m->active_digit == 11) {
                        m->active_digit = 0;
                    }
                    consumed = true;
                    break;
                case InputKeyOk:
                    // Don't handle OK in with_view_model — fire event after
                    break;
                default:
                    consumed = false;
                    break;
                }
            } else if(event->type == InputTypeLong) {
                switch(event->key) {
                case InputKeyDown:
                    m->octect_digits[m->active_digit] = 0;
                    break;
                default:
                    consumed = false;
                    break;
                }
            }
        },
        true // redraw
    );
    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMaskConfirmed);
    }
    return consumed;
}

View* mask_input_view_alloc(void* context) {
    View* view = view_alloc();

    // Register the draw callback
    view_set_draw_callback(view, mask_input_view_draw);

    // Register the input callback
    view_set_input_callback(view, mask_input_view_input);
    view_set_context(view, context);
    // Allocate the model that lives inside this view
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(MaskInputViewModel));

    return view;
}

void mask_input_view_free(View* view) {
    view_free(view);
}
