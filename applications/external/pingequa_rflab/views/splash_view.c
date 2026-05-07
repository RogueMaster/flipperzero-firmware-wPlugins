/**
 * @file splash_view.c
 * @brief 启动屏 View 实现.绘制委托给 pq_ui_theme.
 */
#include "splash_view.h"

#include "../core/pq_ui_theme.h"

#include <furi.h>
#include <stdlib.h>

typedef struct {
    uint8_t progress; /* 0..100 */
} SplashViewModel;

struct SplashView {
    View* view;
};

static void splash_view_draw_callback(Canvas* canvas, void* model) {
    SplashViewModel* m = model;
    pq_ui_draw_splash(canvas, m->progress);
}

SplashView* splash_view_alloc(void) {
    SplashView* sv = malloc(sizeof(*sv));
    furi_check(sv);
    sv->view = view_alloc();
    view_allocate_model(sv->view, ViewModelTypeLocking, sizeof(SplashViewModel));
    view_set_context(sv->view, sv);
    view_set_draw_callback(sv->view, splash_view_draw_callback);
    /* 模型已被 view_allocate_model 零初始化,progress=0. */
    return sv;
}

void splash_view_free(SplashView* sv) {
    if(sv == NULL) return;
    view_free(sv->view);
    free(sv);
}

View* splash_view_get_view(SplashView* sv) {
    furi_check(sv);
    return sv->view;
}

void splash_view_set_progress(SplashView* sv, uint8_t progress) {
    furi_check(sv);
    if(progress > 100) progress = 100;
    with_view_model(
        sv->view, SplashViewModel * m, { m->progress = progress; }, true);
}
