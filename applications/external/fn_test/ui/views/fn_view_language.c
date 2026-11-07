#include "fn_view_language.h"

#include "../fn_draw.h"
#include "../fn_strings.h"
#include "../fn_ui.h"

#include <gui/canvas.h>
#include <input/input.h>

#include <furi.h>

#define FN_LANGUAGE_ITEMS 2

typedef struct {
    FnLang ui_lang;
    uint8_t selected; /* 0 — English, 1 — Русский */
} FnLanguageModel;

struct FnLanguageView {
    View* view;
    FnLanguageCallback callback;
    void* callback_context;
};

static const FnStrId fn_language_items[FN_LANGUAGE_ITEMS] = {
    FnStrLanguageEnglish,
    FnStrLanguageRussian,
};

static void fn_view_language_draw_callback(Canvas* canvas, void* context) {
    FnLanguageModel* model = context;
    furi_assert(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_box(canvas, 0, 0, 128, FN_UI_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    fn_draw_primary(canvas);
    canvas_draw_str(
        canvas, FN_UI_MARGIN_X, FN_UI_HEADER_H - 3, fn_str(model->ui_lang, FnStrLanguageTitle));
    canvas_set_color(canvas, ColorBlack);

    int32_t y = FN_UI_CONTENT_Y + 4;
    for(uint8_t i = 0; i < FN_LANGUAGE_ITEMS; ++i) {
        const bool selected = (model->selected == i);
        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 1, 128, FN_UI_LINE_H + 2);
            canvas_set_color(canvas, ColorWhite);
        }

        fn_draw_primary(canvas);
        canvas_draw_str(
            canvas,
            FN_UI_MARGIN_X + 4,
            y + FN_UI_LINE_H - 2,
            fn_str(model->ui_lang, fn_language_items[i]));

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
        y += FN_UI_LINE_H + 3;
    }

    fn_draw_small(canvas);
    canvas_draw_str(canvas, FN_UI_MARGIN_X, 62, fn_str(model->ui_lang, FnStrLanguageHint));
}

static bool fn_view_language_input_callback(InputEvent* event, void* context) {
    FnLanguageView* language = context;
    furi_assert(language);

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            language->view,
            FnLanguageModel * model,
            {
                if(event->key == InputKeyUp) {
                    model->selected = (model->selected == 0) ? (FN_LANGUAGE_ITEMS - 1) :
                                                               (model->selected - 1);
                } else {
                    model->selected = (model->selected + 1) % FN_LANGUAGE_ITEMS;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyOk) {
        uint8_t selected = 0;
        with_view_model(
            language->view, FnLanguageModel * model, { selected = model->selected; }, false);
        if(language->callback) {
            language->callback(
                language->callback_context, (selected == 0) ? FnLangEn : FnLangRu, true);
        }
        consumed = true;
    }

    return consumed;
}

FnLanguageView* fn_view_language_alloc(void) {
    FnLanguageView* language = malloc(sizeof(FnLanguageView));
    language->view = view_alloc();
    view_set_context(language->view, language);
    view_allocate_model(language->view, ViewModelTypeLocking, sizeof(FnLanguageModel));
    view_set_draw_callback(language->view, fn_view_language_draw_callback);
    view_set_input_callback(language->view, fn_view_language_input_callback);

    with_view_model(
        language->view,
        FnLanguageModel * model,
        {
            model->ui_lang = FN_LANG_DEFAULT;
            model->selected = 0;
        },
        false);
    return language;
}

void fn_view_language_free(FnLanguageView* language) {
    if(!language) return;
    view_free(language->view);
    free(language);
}

View* fn_view_language_get_view(FnLanguageView* language) {
    return language ? language->view : NULL;
}

void fn_view_language_set(FnLanguageView* language, FnLang ui_lang, FnLang selected) {
    if(!language) return;
    with_view_model(
        language->view,
        FnLanguageModel * model,
        {
            model->ui_lang = ui_lang;
            model->selected = (selected == FnLangRu) ? 1 : 0;
        },
        true);
}

void fn_view_language_set_callback(
    FnLanguageView* language,
    FnLanguageCallback callback,
    void* context) {
    if(!language) return;
    language->callback = callback;
    language->callback_context = context;
}
