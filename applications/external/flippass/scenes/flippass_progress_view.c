#include "flippass_progress_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#define FLIPPASS_PROGRESS_TITLE_SIZE  32U
#define FLIPPASS_PROGRESS_STAGE_SIZE  48U
#define FLIPPASS_PROGRESS_DETAIL_SIZE 80U

typedef struct {
    char title[FLIPPASS_PROGRESS_TITLE_SIZE];
    char stage[FLIPPASS_PROGRESS_STAGE_SIZE];
    char detail[FLIPPASS_PROGRESS_DETAIL_SIZE];
    uint8_t progress;
} FlipPassProgressViewModel;

struct FlipPassProgressView {
    View* view;
};

static void flippass_progress_view_draw_bar(Canvas* canvas, uint8_t progress) {
    const uint8_t bar_x = 12U;
    const uint8_t bar_y = 28U;
    const uint8_t bar_w = 104U;
    const uint8_t bar_h = 9U;
    const uint8_t inner_w = bar_w - 2U;
    const uint8_t clamped = (progress <= 100U) ? progress : 100U;
    const uint8_t fill_w = (uint8_t)((inner_w * clamped) / 100U);

    elements_slightly_rounded_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    if(fill_w > 0U) {
        canvas_draw_box(canvas, bar_x + 1U, bar_y + 1U, fill_w, bar_h - 2U);
    }
}

static void flippass_progress_view_draw_callback(Canvas* canvas, void* _model) {
    FlipPassProgressViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, model->title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, model->stage);

    flippass_progress_view_draw_bar(canvas, model->progress);

    elements_multiline_text_aligned(canvas, 64, 42, AlignCenter, AlignTop, model->detail);
}

FlipPassProgressView* flippass_progress_view_alloc(void) {
    FlipPassProgressView* progress_view = malloc(sizeof(FlipPassProgressView));
    furi_check(progress_view);

    progress_view->view = view_alloc();
    view_allocate_model(
        progress_view->view, ViewModelTypeLocking, sizeof(FlipPassProgressViewModel));
    view_set_draw_callback(progress_view->view, flippass_progress_view_draw_callback);
    flippass_progress_view_reset(progress_view);
    return progress_view;
}

void flippass_progress_view_free(FlipPassProgressView* view) {
    furi_check(view);

    view_free(view->view);
    free(view);
}

View* flippass_progress_view_get_view(FlipPassProgressView* view) {
    furi_check(view);
    return view->view;
}

void flippass_progress_view_reset(FlipPassProgressView* view) {
    furi_check(view);

    with_view_model(
        view->view,
        FlipPassProgressViewModel * model,
        {
            snprintf(model->title, sizeof(model->title), "%s", "Working");
            snprintf(model->stage, sizeof(model->stage), "%s", "Preparing");
            model->detail[0] = '\0';
            model->progress = 0U;
        },
        true);
}

void flippass_progress_view_set_state(
    FlipPassProgressView* view,
    const char* title,
    const char* stage,
    const char* detail,
    uint8_t progress) {
    furi_check(view);

    with_view_model(
        view->view,
        FlipPassProgressViewModel * model,
        {
            snprintf(model->title, sizeof(model->title), "%s", title != NULL ? title : "");
            snprintf(model->stage, sizeof(model->stage), "%s", stage != NULL ? stage : "");
            snprintf(model->detail, sizeof(model->detail), "%s", detail != NULL ? detail : "");
            model->progress = progress;
        },
        true);
}
