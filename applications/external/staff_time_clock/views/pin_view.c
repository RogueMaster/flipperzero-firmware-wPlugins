// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "pin_view.h"
#include <furi.h>

#define PIN_STEPS 4

struct PinView {
    View* view;
    PinViewCallback callback;
    void* context;
};

typedef struct {
    char title[32];
    char message[32];
    char seq[PIN_STEPS + 1]; // 'U' 'D' 'L' 'R'
    uint8_t len;
} PinViewModel;

static void pin_view_draw_callback(Canvas* canvas, void* _model) {
    PinViewModel* model = _model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, model->title);

    // Progress boxes: filled for each entered step (masked).
    const int box = 16;
    const int gap = 8;
    const int total = PIN_STEPS * box + (PIN_STEPS - 1) * gap;
    int x0 = (128 - total) / 2;
    int y0 = 22;
    for(int i = 0; i < PIN_STEPS; i++) {
        int x = x0 + i * (box + gap);
        canvas_draw_rframe(canvas, x, y0, box, box, 3);
        if(i < model->len) {
            canvas_draw_disc(canvas, x + box / 2, y0 + box / 2, 4);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    if(model->message[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, model->message);
    } else {
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, "Arrows  |  OK=clear");
    }
}

static bool pin_view_input_callback(InputEvent* event, void* context) {
    PinView* pin_view = context;

    if(event->type != InputTypeShort) return false;

    char dir = 0;
    bool clear = false;
    switch(event->key) {
    case InputKeyUp:
        dir = 'U';
        break;
    case InputKeyDown:
        dir = 'D';
        break;
    case InputKeyLeft:
        dir = 'L';
        break;
    case InputKeyRight:
        dir = 'R';
        break;
    case InputKeyOk:
        clear = true;
        break;
    default:
        return false; // let Back propagate to the scene
    }

    bool submit = false;
    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            if(clear) {
                model->len = 0;
                model->seq[0] = '\0';
            } else if(model->len < PIN_STEPS) {
                model->seq[model->len++] = dir;
                model->seq[model->len] = '\0';
                if(model->len == PIN_STEPS) submit = true;
            }
        },
        true);

    if(submit && pin_view->callback) {
        pin_view->callback(pin_view->context);
    }
    return true;
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
            model->len = 0;
            model->seq[0] = '\0';
        },
        true);
}

void pin_view_get_code(PinView* pin_view, char* out, size_t out_size) {
    furi_assert(pin_view);
    with_view_model(
        pin_view->view,
        PinViewModel * model,
        {
            strncpy(out, model->seq, out_size - 1);
            out[out_size - 1] = '\0';
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
