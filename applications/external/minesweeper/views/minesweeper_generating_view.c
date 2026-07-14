#include "minesweeper_generating_view.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>

struct MineSweeperGeneratingView {
    View* view;
    void* context;
    MineSweeperGeneratingInputCallback input_callback;
};

typedef struct {
    uint32_t attempts_total;
    uint32_t elapsed_seconds;
} MineSweeperGeneratingViewModel;

static void minesweeper_generating_view_draw(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);

    MineSweeperGeneratingViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Generating solvable");
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, "board...");

    canvas_set_font(canvas, FontSecondary);
    char line[32];

    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Solvable from 0,0");

    snprintf(line, sizeof(line), "Attempts: %lu", (unsigned long)model->attempts_total);
    canvas_draw_str_aligned(canvas, 2, 34, AlignLeft, AlignTop, line);

    snprintf(line, sizeof(line), "Elapsed:  %lus", (unsigned long)model->elapsed_seconds);
    canvas_draw_str_aligned(canvas, 2, 44, AlignLeft, AlignTop, line);

    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Force start now");
}

static bool minesweeper_generating_view_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    MineSweeperGeneratingView* instance = context;

    if(event->key == InputKeyBack) {
        // Generating flow is non-interruptible via Back.
        return true;
    }

    if(event->key == InputKeyOk &&
       (event->type == InputTypePress || event->type == InputTypeShort ||
        event->type == InputTypeLong) &&
       instance->input_callback) {
        instance->input_callback(MineSweeperGeneratingEventStartNow, instance->context);
        return true;
    }

    return false;
}

MineSweeperGeneratingView* minesweeper_generating_view_alloc(void) {
    MineSweeperGeneratingView* instance = malloc(sizeof(MineSweeperGeneratingView));
    if(!instance) {
        return NULL;
    }

    instance->view = view_alloc();
    if(instance->view == NULL) return NULL;
    instance->context = NULL;
    instance->input_callback = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(MineSweeperGeneratingViewModel));
    view_set_draw_callback(instance->view, minesweeper_generating_view_draw);
    view_set_input_callback(instance->view, minesweeper_generating_view_input);

    with_view_model(
        instance->view,
        MineSweeperGeneratingViewModel * model,
        {
            model->attempts_total = 0;
            model->elapsed_seconds = 0;
        },
        true);

    return instance;
}

void minesweeper_generating_view_free(MineSweeperGeneratingView* instance) {
    furi_assert(instance);

    instance->context = NULL;
    instance->input_callback = NULL;

    if(instance->view) view_free(instance->view);
    if(instance) free(instance);
}

View* minesweeper_generating_view_get_view(MineSweeperGeneratingView* instance) {
    furi_assert(instance);
    return instance->view;
}

void minesweeper_generating_view_set_context(MineSweeperGeneratingView* instance, void* context) {
    furi_assert(instance);
    instance->context = context;
}

void minesweeper_generating_view_set_input_callback(
    MineSweeperGeneratingView* instance,
    MineSweeperGeneratingInputCallback callback) {
    furi_assert(instance);
    instance->input_callback = callback;
}

void minesweeper_generating_view_set_stats(
    MineSweeperGeneratingView* instance,
    uint32_t attempts_total,
    uint32_t elapsed_seconds) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        MineSweeperGeneratingViewModel * model,
        {
            model->attempts_total = attempts_total;
            model->elapsed_seconds = elapsed_seconds;
        },
        true);
}

void minesweeper_generating_view_reset(MineSweeperGeneratingView* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        MineSweeperGeneratingViewModel * model,
        {
            model->attempts_total = 0;
            model->elapsed_seconds = 0;
        },
        true);

    instance->context = NULL;
    instance->input_callback = NULL;
}
