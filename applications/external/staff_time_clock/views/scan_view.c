// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "scan_view.h"
#include "tc_chevron.h"
#include <furi.h>

struct ScanView {
    View* view;
    ScanViewNavCallback nav_cb;
    void* context;
};

typedef struct {
    char header[24];
    char text[64];
    char tech[8];
} ScanViewModel;

// model->text may contain '\n' (the purpose-specific hints are two lines);
// canvas_draw_str_aligned does not wrap or break on it, so split it here.
static void draw_multiline(Canvas* canvas, int y, int line_height, const char* text) {
    char buf[64];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* line = buf;
    while(line) {
        char* nl = strchr(line, '\n');
        if(nl) *nl = '\0';
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, line);
        y += line_height;
        line = nl ? nl + 1 : NULL;
    }
}

static void scan_view_draw_callback(Canvas* canvas, void* _model) {
    ScanViewModel* model = _model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, model->header);

    canvas_set_font(canvas, FontSecondary);
    draw_multiline(canvas, 20, 9, model->text);
    // Same spot as Work mode's footer, so the technology indicator is always
    // in the same place regardless of which screen you're on.
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, model->tech);

    tc_draw_chevron_left(canvas, 2, 58);
    tc_draw_chevron_right(canvas, 122, 58);
}

static bool scan_view_input_callback(InputEvent* event, void* context) {
    ScanView* scan_view = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyLeft) {
        if(scan_view->nav_cb) scan_view->nav_cb(-1, scan_view->context);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(scan_view->nav_cb) scan_view->nav_cb(1, scan_view->context);
        return true;
    }
    return false; // Back and anything else propagate to the scene
}

ScanView* scan_view_alloc(void) {
    ScanView* scan_view = malloc(sizeof(ScanView));
    scan_view->view = view_alloc();
    scan_view->nav_cb = NULL;
    scan_view->context = NULL;

    view_allocate_model(scan_view->view, ViewModelTypeLocking, sizeof(ScanViewModel));
    view_set_context(scan_view->view, scan_view);
    view_set_draw_callback(scan_view->view, scan_view_draw_callback);
    view_set_input_callback(scan_view->view, scan_view_input_callback);
    return scan_view;
}

void scan_view_free(ScanView* scan_view) {
    furi_assert(scan_view);
    view_free(scan_view->view);
    free(scan_view);
}

View* scan_view_get_view(ScanView* scan_view) {
    furi_assert(scan_view);
    return scan_view->view;
}

void scan_view_set_content(
    ScanView* scan_view,
    const char* header,
    const char* text,
    const char* tech) {
    furi_assert(scan_view);
    with_view_model(
        scan_view->view,
        ScanViewModel * model,
        {
            strncpy(model->header, header ? header : "", sizeof(model->header) - 1);
            model->header[sizeof(model->header) - 1] = '\0';
            strncpy(model->text, text ? text : "", sizeof(model->text) - 1);
            model->text[sizeof(model->text) - 1] = '\0';
            strncpy(model->tech, tech ? tech : "", sizeof(model->tech) - 1);
            model->tech[sizeof(model->tech) - 1] = '\0';
        },
        true);
}

void scan_view_set_nav_callback(ScanView* scan_view, ScanViewNavCallback cb, void* context) {
    furi_assert(scan_view);
    scan_view->nav_cb = cb;
    scan_view->context = context;
}
