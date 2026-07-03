// views/result_view.c
#include <gui/view.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/icon.h>

#include "ip_input_view.h"
#include "../network_subnet.h"
#define LOCAL_CHAR_WIDTH 7

static void increment_active_digit(IpInputViewModel* m) {
    uint32_t temp_octet;
    uint8_t hundred = 100;
    uint8_t upper_lim;
    if(m->octets_and_cidr_digits[m->active_digit] == 9) {
        return;
    }
    if(m->active_digit < 12) {
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
        FURI_LOG_D(NETWORK_SUBNET_TAG, "running octet from %d to %d", upper_lim - 3, upper_lim);
        for(int i = upper_lim - 3; i < upper_lim; i++) {
            if(i == m->active_digit) {
                temp_octet = temp_octet + (m->octets_and_cidr_digits[i] + 1) * hundred;
            } else {
                temp_octet = temp_octet + m->octets_and_cidr_digits[i] * hundred;
            }
            hundred = hundred / 10;
        }
        FURI_LOG_D(NETWORK_SUBNET_TAG, "octect value %ld", temp_octet);
        if(temp_octet < 256) {
            m->octets_and_cidr_digits[m->active_digit] += 1;
        }

    } else {
        temp_octet = m->octets_and_cidr_digits[12] * 10 + m->octets_and_cidr_digits[13];
        switch(m->active_digit) {
        case 12:
            if(temp_octet + 10 < 33) {
                m->octets_and_cidr_digits[m->active_digit] += 1;
            }
            break;
        case 13:
            if(temp_octet + 1 < 33 && m->octets_and_cidr_digits[m->active_digit] < 9) {
                m->octets_and_cidr_digits[m->active_digit] += 1;
            }
            break;
        }
    }
}

static void decrement_active_digit(IpInputViewModel* m) {
    if(m->octets_and_cidr_digits[m->active_digit] != 0) {
        m->octets_and_cidr_digits[m->active_digit] -= 1;
    }
}

static void ip_input_view_draw(Canvas* canvas, void* model) {
    IpInputViewModel* m = (IpInputViewModel*)model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "IP / CIDR Input");

    canvas_draw_line(canvas, 0, 12, 192, 12);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_font(canvas, FontKeyboard);

    uint8_t inc = 0;
    char buf[5];
    bool proc_active_digit = false;
    for(int i = 0; i < 14; i++) {
        switch(i) {
        case FirstOctet:
        case SecondOctet:
        case ThirdOctet:
        case CIDR:
            canvas_draw_str(canvas, 2 + (inc)*LOCAL_CHAR_WIDTH, 30, i == CIDR ? "/" : ".");
            inc += 1;
        }
        proc_active_digit = i == m->active_digit;
        if(proc_active_digit) {
            canvas_draw_box(canvas, 1 + (inc)*LOCAL_CHAR_WIDTH, 22, 7, 10);
            canvas_invert_color(canvas);
        }
        snprintf(buf, sizeof(buf), "%d", m->octets_and_cidr_digits[i]);
        canvas_draw_str(canvas, 2 + (inc)*LOCAL_CHAR_WIDTH, 30, buf);
        inc += 1;
        if(proc_active_digit) {
            canvas_invert_color(canvas);
        }
    }

    // Draw up/down arrows + label centred
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignBottom, "up/down - change");

    // Left and right corner buttons
    elements_button_left(canvas, "Move");
    elements_button_right(canvas, "Move");
    elements_button_center(canvas, "OK");
}

static bool ip_input_view_input(InputEvent* event, void* context) {
    bool consumed = false;
    NetworkSubnetApp* app = context;
    FURI_LOG_D(NETWORK_SUBNET_TAG, "procesing input on_input in ip_input");
    with_view_model(
        app->ip_input_view,
        IpInputViewModel * m,
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
                        m->active_digit = 13;
                    }
                    consumed = true;
                    break;
                case InputKeyRight:
                    if(m->active_digit < 13) {
                        m->active_digit = m->active_digit + 1;
                    } else if(m->active_digit == 13) {
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
                    m->octets_and_cidr_digits[m->active_digit] = 0;
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
        FURI_LOG_D(NETWORK_SUBNET_TAG, "calling EventIpConfirmed Event");
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIpConfirmed);
    }
    return consumed;
}

View* ip_input_view_alloc(void* context) {
    View* view = view_alloc();

    // Register the draw callback
    view_set_draw_callback(view, ip_input_view_draw);

    // Register the input callback
    view_set_input_callback(view, ip_input_view_input);
    view_set_context(view, context);
    // Allocate the model that lives inside this view
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(IpInputViewModel));

    return view;
}

void ip_input_view_free(View* view) {
    view_free(view);
}
