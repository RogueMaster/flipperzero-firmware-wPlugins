// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "pin_view.h"
#include <furi.h>

#define PIN_DIGITS 4

struct PinView {
    View* view;
    PinViewCallback callback;
    void* context;
};

typedef struct {
    char title[32];
    char message[32];
    uint8_t digits[PIN_DIGITS];
    uint8_t pos;
} PinViewModel;

static void pin_view_draw_callback(Canvas* canvas, void* _model) {
    PinViewModel* model = _model;

    canvas_clear(canvas);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, model->title);

    // Digit boxes
    const int box_w = 18;
    const int box_h = 22;
    const int gap = 6;
    const int total_w = PIN_DIGITS * box_w + (PIN_DIGITS - 1) * gap;
    int x0 = (128 - total_w) / 2;
    int y0 = 20;

    canvas_set_font(canvas, FontBigNumbers);
    for(int i = 0; i < PIN_DIGITS; i++) {
        int x = x0 + i * (box_w + gap);
        canvas_draw_rframe(canvas, x, y0, box_w, box_h, 3);
        // Highlight the active box.
        if(i == model->pos) {
            canvas_draw_rframe(canvas, x - 1, y0 - 1, box_w + 2, box_h + 2, 3);
        }

        char ch[2] = {0, 0};
        if(i == model->pos) {
            ch[0] = '0' + model->digits[i]; // show the digit being edited
        } else {
            ch[0] = '*'; // mask the others
        }
        canvas_draw_str_aligned(
            canvas, x + box_w / 2, y0 + box_h / 2, AlignCenter, AlignCenter, ch);
    }

    // Message / hint
    canvas_set_font(canvas, FontSecondary);
    if(model->message[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, model->message);
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 56, AlignCenter, AlignCenter, "Up/Down: digit  OK: next");
    }
}

static bool pin_view_input_callback(InputEvent* event, void* context) {
    PinView* pin_view = context;
    bool handled = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool submit = false;

    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            switch(event->key) {
            case InputKeyUp:
                model->digits[model->pos] = (model->digits[model->pos] + 1) % 10;
                handled = true;
                break;
            case InputKeyDown:
                model->digits[model->pos] = (model->digits[model->pos] + 9) % 10;
                handled = true;
                break;
            case InputKeyLeft:
                if(model->pos > 0) model->pos--;
                handled = true;
                break;
            case InputKeyRight:
                if(model->pos < PIN_DIGITS - 1) model->pos++;
                handled = true;
                break;
            case InputKeyOk:
                if(model->pos < PIN_DIGITS - 1) {
                    model->pos++;
                } else {
                    submit = true;
                }
                handled = true;
                break;
            default:
                break;
            }
        },
        true);

    // Fire the callback outside the model lock to keep it re-entrant-safe.
    if(submit && pin_view->callback) {
        pin_view->callback(pin_view->context);
    }

    return handled;
}

PinView* pin_view_alloc(void) {
    PinView* pin_view = malloc(sizeof(PinView));
    pin_view->view = view_alloc();
    pin_view->callback = NULL;
    pin_view->context = NULL;

    view_allocate_model(pin_view->view, ViewModelTypeLocking, sizeof(PinViewModel));
    view_set_context(pin_view->view, pin_view);
    view_set_draw_callback(pin_view->view, pin_view_draw_callback);
    view_set_input_callback(pin_view->view, pin_view_input_callback);

    return pin_view;
}

void pin_view_free(PinView* pin_view) {
    furi_assert(pin_view);
    view_free(pin_view->view);
    free(pin_view);
}

View* pin_view_get_view(PinView* pin_view) {
    furi_assert(pin_view);
    return pin_view->view;
}

void pin_view_reset(PinView* pin_view, const char* title) {
    furi_assert(pin_view);
    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            strncpy(model->title, title ? title : "", sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
            model->message[0] = '\0';
            model->pos = 0;
            for(int i = 0; i < PIN_DIGITS; i++) model->digits[i] = 0;
        },
        true);
}

void pin_view_get_code(PinView* pin_view, char* out, size_t out_size) {
    furi_assert(pin_view);
    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            size_t n = 0;
            for(int i = 0; i < PIN_DIGITS && n < out_size - 1; i++) {
                out[n++] = '0' + model->digits[i];
            }
            out[n] = '\0';
        },
        false);
}

void pin_view_set_callback(PinView* pin_view, PinViewCallback callback, void* context) {
    furi_assert(pin_view);
    pin_view->callback = callback;
    pin_view->context = context;
}

void pin_view_set_message(PinView* pin_view, const char* message) {
    furi_assert(pin_view);
    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            strncpy(model->message, message ? message : "", sizeof(model->message) - 1);
            model->message[sizeof(model->message) - 1] = '\0';
        },
        true);
}
