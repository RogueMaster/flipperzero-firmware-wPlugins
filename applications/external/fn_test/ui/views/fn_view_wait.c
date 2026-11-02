#include "fn_view_wait.h"

#include "../fn_draw.h"
#include "../fn_ui.h"

#include <fn_test_icons.h>
#include <gui/canvas.h>

#include <furi.h>
#include <string.h>

#define FN_WAIT_TEXT_X 66
#define FN_WAIT_TEXT_W 60

typedef struct {
    IconAnimation* icon;
    FnWaitMode mode;
    char title[32];
    /* 96 байт: подзаголовок очистки МГМ «Может занять до 30 секунд, не
     * прерывайте» — 73 байта UTF-8, в 48 байт он обрезался (FR-023). */
    char subtitle[96];
    bool cancelable;
} FnWaitModel;

struct FnWaitView {
    View* view;
};

static void fn_view_wait_draw_callback(Canvas* canvas, void* context) {
    FnWaitModel* model = context;
    furi_assert(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->icon) {
        canvas_draw_icon_animation(canvas, 0, 0, model->icon);
    }

    fn_draw_primary(canvas);
    const uint16_t title_h =
        fn_draw_wrapped(canvas, FN_WAIT_TEXT_X, 2, model->title, FN_WAIT_TEXT_W, FN_UI_LINE_H, 3);

    if(model->subtitle[0] != '\0') {
        fn_draw_small(canvas);
        fn_draw_wrapped(
            canvas, FN_WAIT_TEXT_X, 2 + title_h, model->subtitle, FN_WAIT_TEXT_W, FN_UI_SMALL_H, 4);
    }
}

static void fn_view_wait_enter_callback(void* context) {
    FnWaitView* wait = context;
    with_view_model(
        wait->view, FnWaitModel * model, { icon_animation_start(model->icon); }, false);
}

static void fn_view_wait_exit_callback(void* context) {
    FnWaitView* wait = context;
    with_view_model(wait->view, FnWaitModel * model, { icon_animation_stop(model->icon); }, false);
}

FnWaitView* fn_view_wait_alloc(void) {
    FnWaitView* wait = malloc(sizeof(FnWaitView));
    wait->view = view_alloc();
    view_set_context(wait->view, wait);
    view_allocate_model(wait->view, ViewModelTypeLocking, sizeof(FnWaitModel));
    view_set_draw_callback(wait->view, fn_view_wait_draw_callback);
    view_set_enter_callback(wait->view, fn_view_wait_enter_callback);
    view_set_exit_callback(wait->view, fn_view_wait_exit_callback);

    with_view_model(
        wait->view,
        FnWaitModel * model,
        {
            model->icon = icon_animation_alloc(&A_FNLooking_64x64);
            view_tie_icon_animation(wait->view, model->icon);
            model->mode = FnWaitModeDetect;
            model->cancelable = true;
            model->title[0] = '\0';
            model->subtitle[0] = '\0';
        },
        false);

    return wait;
}

void fn_view_wait_free(FnWaitView* wait) {
    if(!wait) return;
    with_view_model(wait->view, FnWaitModel * model, { icon_animation_free(model->icon); }, false);
    view_free(wait->view);
    free(wait);
}

View* fn_view_wait_get_view(FnWaitView* wait) {
    return wait ? wait->view : NULL;
}

void fn_view_wait_set(
    FnWaitView* wait,
    FnWaitMode mode,
    const char* title,
    const char* subtitle,
    bool cancelable) {
    if(!wait) return;
    with_view_model(
        wait->view,
        FnWaitModel * model,
        {
            model->mode = mode;
            model->cancelable = cancelable;
            strncpy(model->title, title ? title : "", sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
            strncpy(model->subtitle, subtitle ? subtitle : "", sizeof(model->subtitle) - 1);
            model->subtitle[sizeof(model->subtitle) - 1] = '\0';
        },
        true);
}
