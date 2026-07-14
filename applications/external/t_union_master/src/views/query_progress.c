#include "query_progress.h"
#include "../view_modules/elements.h"
#include <t_union_master_icons.h>

struct QureyProgressView {
    View* view;
};

typedef struct {
    size_t total_progress;
    size_t progress;
    IconAnimation* icon;
} QureyProgressViewModel;

static void query_progress_view_draw_cb(Canvas* canvas, void* _model) {
    QureyProgressViewModel* model = _model;

    canvas_draw_icon_animation(canvas, 10, 14, model->icon);
    elements_multiline_text_aligned_utf8(
        canvas, 80, 10, AlignCenter, AlignTop, "查询数据库中\n请稍等...");

    if(model->total_progress != 0) {
        float progress = (float)model->progress / model->total_progress;

        FuriString* tmp_str = furi_string_alloc_printf("%u%%", (uint8_t)(progress * 100));
        elements_progress_bar_with_text(
            canvas, 2, 48, 124, progress, furi_string_get_cstr(tmp_str));
        furi_string_free(tmp_str);
    }
}

static void query_progress_enter_cb(void* ctx) {
    QureyProgressView* instance = ctx;

    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
    with_view_model(
        instance->view,
        QureyProgressViewModel * model,
        {
            view_tie_icon_animation(instance->view, model->icon);
            icon_animation_start(model->icon);
        },
        false);
}

static void query_progress_exit_cb(void* ctx) {
    QureyProgressView* instance = ctx;

    with_view_model(
        instance->view,
        QureyProgressViewModel * model,
        { icon_animation_stop(model->icon); },
        false);
    furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
}

QureyProgressView* query_progress_alloc() {
    QureyProgressView* instance = malloc(sizeof(QureyProgressView));

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(QureyProgressViewModel));
    with_view_model(
        instance->view,
        QureyProgressViewModel * model,
        {
            model->total_progress = 0;
            model->progress = 0;
            model->icon = icon_animation_alloc(&A_Loading_24);
        },
        false);

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, query_progress_view_draw_cb);
    view_set_enter_callback(instance->view, query_progress_enter_cb);
    view_set_exit_callback(instance->view, query_progress_exit_cb);

    return instance;
}

void query_progress_free(QureyProgressView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* query_progress_get_view(QureyProgressView* instance) {
    furi_assert(instance);
    return instance->view;
}

void query_progress_update_total(QureyProgressView* instance, uint8_t total_progress) {
    with_view_model(
        instance->view,
        QureyProgressViewModel * model,
        { model->total_progress = total_progress; },
        true);
}

void query_progress_update_progress(QureyProgressView* instance, uint8_t progress) {
    with_view_model(
        instance->view, QureyProgressViewModel * model, { model->progress = progress; }, true);
}

void query_progress_clear(QureyProgressView* instance) {
    with_view_model(
        instance->view,
        QureyProgressViewModel * model,
        {
            model->total_progress = 0;
            model->progress = 0;
        },
        false);
}
