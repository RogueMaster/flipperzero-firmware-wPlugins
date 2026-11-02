#include "fn_view_confirm.h"

#include "../fn_draw.h"
#include "../fn_strings.h"
#include "../fn_ui.h"

#include <gui/canvas.h>
#include <input/input.h>

#include <furi.h>
#include <string.h>

#define FN_CONFIRM_BUTTON_H 13
#define FN_CONFIRM_BODY_W   120

typedef struct {
    char title[32];
    char body[128];
    uint8_t selected; /* 0 — отмена, 1 — подтверждение */
} FnConfirmModel;

struct FnConfirmView {
    View* view;
    FnConfirmCallback callback;
    void* callback_context;
};

static void fn_view_confirm_draw_callback(Canvas* canvas, void* context) {
    FnConfirmModel* model = context;
    furi_assert(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_box(canvas, 0, 0, 128, FN_UI_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    fn_draw_primary(canvas);
    fn_draw_wrapped(canvas, FN_UI_MARGIN_X, 1, model->title, 124, FN_UI_LINE_H, 1);
    canvas_set_color(canvas, ColorBlack);

    fn_draw_small(canvas);
    fn_draw_wrapped(canvas, FN_UI_MARGIN_X, 15, model->body, FN_CONFIRM_BODY_W, FN_UI_SMALL_H, 4);

    const int32_t button_y = 64 - FN_CONFIRM_BUTTON_H - 1;
    const int32_t button_w = 60;

    for(uint8_t i = 0; i < 2; ++i) {
        const int32_t x = (i == 0) ? 2 : (128 - button_w - 2);
        const bool selected = (model->selected == i);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, x, button_y, button_w, FN_CONFIRM_BUTTON_H);
        if(selected) {
            canvas_draw_box(canvas, x + 1, button_y + 1, button_w - 2, FN_CONFIRM_BUTTON_H - 2);
            canvas_set_color(canvas, ColorWhite);
        }
        fn_draw_small(canvas);
        canvas_draw_str_aligned(
            canvas,
            x + button_w / 2,
            button_y + FN_CONFIRM_BUTTON_H / 2 + 1,
            AlignCenter,
            AlignCenter,
            (i == 0) ? FN_STR_CONFIRM_CANCEL : FN_STR_CONFIRM_ACCEPT);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool fn_view_confirm_input_callback(InputEvent* event, void* context) {
    FnConfirmView* confirm = context;
    furi_assert(confirm);

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        with_view_model(
            confirm->view,
            FnConfirmModel * model,
            { model->selected = (event->key == InputKeyLeft) ? 0 : 1; },
            true);
        consumed = true;
    } else if(event->key == InputKeyOk) {
        uint8_t selected = 0;
        with_view_model(
            confirm->view, FnConfirmModel * model, { selected = model->selected; }, false);
        if(confirm->callback) {
            confirm->callback(confirm->callback_context, selected == 1);
        }
        consumed = true;
    }

    return consumed;
}

FnConfirmView* fn_view_confirm_alloc(void) {
    FnConfirmView* confirm = malloc(sizeof(FnConfirmView));
    confirm->view = view_alloc();
    view_set_context(confirm->view, confirm);
    view_allocate_model(confirm->view, ViewModelTypeLocking, sizeof(FnConfirmModel));
    view_set_draw_callback(confirm->view, fn_view_confirm_draw_callback);
    view_set_input_callback(confirm->view, fn_view_confirm_input_callback);

    with_view_model(
        confirm->view,
        FnConfirmModel * model,
        {
            memset(model, 0, sizeof(FnConfirmModel));
            strncpy(model->title, FN_STR_CONFIRM_TITLE, sizeof(model->title) - 1);
            strncpy(model->body, FN_STR_CONFIRM_BODY, sizeof(model->body) - 1);
            model->selected = 0;
        },
        false);
    return confirm;
}

void fn_view_confirm_free(FnConfirmView* confirm) {
    if(!confirm) return;
    view_free(confirm->view);
    free(confirm);
}

View* fn_view_confirm_get_view(FnConfirmView* confirm) {
    return confirm ? confirm->view : NULL;
}

void fn_view_confirm_set(FnConfirmView* confirm, const char* title, const char* body) {
    if(!confirm) return;
    with_view_model(
        confirm->view,
        FnConfirmModel * model,
        {
            strncpy(model->title, title ? title : "", sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
            strncpy(model->body, body ? body : "", sizeof(model->body) - 1);
            model->body[sizeof(model->body) - 1] = '\0';
            model->selected = 0;
        },
        true);
}

void fn_view_confirm_set_callback(
    FnConfirmView* confirm,
    FnConfirmCallback callback,
    void* context) {
    if(!confirm) return;
    confirm->callback = callback;
    confirm->callback_context = context;
}
