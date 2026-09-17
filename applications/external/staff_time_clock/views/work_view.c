// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "work_view.h"
#include "tc_chevron.h"
#include <furi.h>

struct WorkView {
    View* view;
    WorkViewExitCallback exit_cb;
    WorkViewNavCallback nav_cb;
    void* context;
};

typedef struct {
    char date[12];
    char time[12];
    char greeting[40];
    char footer[24];
    bool has_greeting;
} WorkViewModel;

static void work_view_draw_callback(Canvas* canvas, void* _model) {
    WorkViewModel* model = _model;
    canvas_clear(canvas);

    // Date at the top
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, model->date);

    if(model->has_greeting) {
        // Greeting takes over the center
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, model->greeting);
    } else {
        // Big clock
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, model->time);
    }

    // Footer hint (active reader technology, or the PIN/Back exit hint)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, model->footer[0] ? model->footer : "PIN to exit");

    // Left/Right chevrons, hinting the technology picker.
    tc_draw_chevron_left(canvas, 2, 58);
    tc_draw_chevron_right(canvas, 122, 58);
}

static bool work_view_input_callback(InputEvent* event, void* context) {
    WorkView* work_view = context;

    // Back triggers the PIN-protected exit; Left/Right switch the reader
    // technology; every other key is swallowed so a collaborator cannot
    // navigate away.
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            if(work_view->exit_cb) work_view->exit_cb(work_view->context);
        } else if(event->key == InputKeyLeft) {
            if(work_view->nav_cb) work_view->nav_cb(-1, work_view->context);
        } else if(event->key == InputKeyRight) {
            if(work_view->nav_cb) work_view->nav_cb(1, work_view->context);
        }
    }
    return true; // consume all input
}

WorkView* work_view_alloc(void) {
    WorkView* work_view = malloc(sizeof(WorkView));
    work_view->view = view_alloc();
    work_view->exit_cb = NULL;
    work_view->nav_cb = NULL;
    work_view->context = NULL;

    view_allocate_model(work_view->view, ViewModelTypeLocking, sizeof(WorkViewModel));
    view_set_context(work_view->view, work_view);
    view_set_draw_callback(work_view->view, work_view_draw_callback);
    view_set_input_callback(work_view->view, work_view_input_callback);
    return work_view;
}

void work_view_free(WorkView* work_view) {
    furi_assert(work_view);
    view_free(work_view->view);
    free(work_view);
}

View* work_view_get_view(WorkView* work_view) {
    furi_assert(work_view);
    return work_view->view;
}

void work_view_set_clock(WorkView* work_view, const char* date, const char* time) {
    furi_assert(work_view);
    with_view_model(
        work_view->view,
        WorkViewModel * model,
        {
            strncpy(model->date, date ? date : "", sizeof(model->date) - 1);
            model->date[sizeof(model->date) - 1] = '\0';
            strncpy(model->time, time ? time : "", sizeof(model->time) - 1);
            model->time[sizeof(model->time) - 1] = '\0';
        },
        true);
}

void work_view_set_greeting(WorkView* work_view, const char* greeting) {
    furi_assert(work_view);
    with_view_model(
        work_view->view,
        WorkViewModel * model,
        {
            if(greeting) {
                strncpy(model->greeting, greeting, sizeof(model->greeting) - 1);
                model->greeting[sizeof(model->greeting) - 1] = '\0';
                model->has_greeting = true;
            } else {
                model->greeting[0] = '\0';
                model->has_greeting = false;
            }
        },
        true);
}

void work_view_set_footer(WorkView* work_view, const char* footer) {
    furi_assert(work_view);
    with_view_model(
        work_view->view,
        WorkViewModel * model,
        {
            strncpy(model->footer, footer ? footer : "", sizeof(model->footer) - 1);
            model->footer[sizeof(model->footer) - 1] = '\0';
        },
        true);
}

void work_view_set_exit_callback(WorkView* work_view, WorkViewExitCallback cb, void* context) {
    furi_assert(work_view);
    work_view->exit_cb = cb;
    work_view->context = context;
}

void work_view_set_nav_callback(WorkView* work_view, WorkViewNavCallback cb, void* context) {
    furi_assert(work_view);
    work_view->nav_cb = cb;
    work_view->context = context;
}
