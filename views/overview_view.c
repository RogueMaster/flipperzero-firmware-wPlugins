// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "overview_view.h"
#include "tc_chevron.h"
#include <furi.h>

struct OverviewView {
    View* view;
    OverviewNavCallback nav_cb;
    void* context;
};

typedef struct {
    char name[40];
    char lines[4][24];
    char empty_msg[48];
    bool is_empty;
    size_t index;
    size_t count;
} OverviewViewModel;

static void overview_view_draw_callback(Canvas* canvas, void* _model) {
    OverviewViewModel* model = _model;
    canvas_clear(canvas);

    if(model->is_empty) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignCenter, model->empty_msg);
        return;
    }

    // Position indicator, top-right ("2/5").
    if(model->count > 1) {
        char pos[24];
        snprintf(pos, sizeof(pos), "%zu/%zu", model->index, model->count);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, pos);
    }

    // Name, centered at the top.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, model->name);

    // Left/Right hint chevrons, same shape/size as Work mode, flanking the
    // content vertically centered.
    tc_draw_chevron_left(canvas, 2, 34);
    tc_draw_chevron_right(canvas, 122, 34);

    // Stat lines.
    int y = 27;
    for(int i = 0; i < 4; i++) {
        if(model->lines[i][0] != '\0') {
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, model->lines[i]);
        }
        y += 9;
    }
}

static bool overview_view_input_callback(InputEvent* event, void* context) {
    OverviewView* view = context;
    if(event->type != InputTypeShort) return false;

    int dir = 0;
    if(event->key == InputKeyLeft) {
        dir = -1;
    } else if(event->key == InputKeyRight) {
        dir = 1;
    } else {
        return false; // let Back (and anything else) propagate to the scene
    }

    if(view->nav_cb) view->nav_cb(dir, view->context);
    return true;
}

OverviewView* overview_view_alloc(void) {
    OverviewView* view = malloc(sizeof(OverviewView));
    view->view = view_alloc();
    view->nav_cb = NULL;
    view->context = NULL;

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(OverviewViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, overview_view_draw_callback);
    view_set_input_callback(view->view, overview_view_input_callback);
    return view;
}

void overview_view_free(OverviewView* view) {
    furi_assert(view);
    view_free(view->view);
    free(view);
}

View* overview_view_get_view(OverviewView* view) {
    furi_assert(view);
    return view->view;
}

void overview_view_set_nav_callback(OverviewView* view, OverviewNavCallback cb, void* context) {
    furi_assert(view);
    view->nav_cb = cb;
    view->context = context;
}

void overview_view_set_person(
    OverviewView* view,
    const char* name,
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4,
    size_t index,
    size_t count) {
    furi_assert(view);
    const char* lines[4] = {line1, line2, line3, line4};
    with_view_model(
        view->view,
        OverviewViewModel * model,
        {
            model->is_empty = false;
            strncpy(model->name, name ? name : "", sizeof(model->name) - 1);
            model->name[sizeof(model->name) - 1] = '\0';
            for(int i = 0; i < 4; i++) {
                strncpy(model->lines[i], lines[i] ? lines[i] : "", sizeof(model->lines[i]) - 1);
                model->lines[i][sizeof(model->lines[i]) - 1] = '\0';
            }
            model->index = index;
            model->count = count;
        },
        true);
}

void overview_view_set_empty(OverviewView* view, const char* message) {
    furi_assert(view);
    with_view_model(
        view->view,
        OverviewViewModel * model,
        {
            model->is_empty = true;
            strncpy(model->empty_msg, message ? message : "", sizeof(model->empty_msg) - 1);
            model->empty_msg[sizeof(model->empty_msg) - 1] = '\0';
        },
        true);
}
