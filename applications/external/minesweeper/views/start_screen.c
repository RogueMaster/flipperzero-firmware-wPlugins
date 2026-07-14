#include "start_screen.h"
#include <gui/elements.h>
#include <gui/icon_animation.h>
#include <input/input.h>

#include <furi.h>

struct StartScreen {
    View* view;
    void* context;
    StartScreenInputCallback input_callback;
};

typedef struct {
    const char* text;
    Font font;
    uint8_t x, y;
    Align horizontal, vertical;
} TextElement;

typedef struct {
    IconAnimation* animation;
    uint8_t x, y;
} IconElement;

typedef struct {
    TextElement text1;
    TextElement text2;
    TextElement text3;
    IconElement icon;
    StartScreenDrawCallback secondary_draw_callback;
} StartScreenModel;

static void start_screen_text_element_set_default(TextElement* text) {
    furi_assert(text);
    text->x = 0;
    text->y = 0;
    text->text = NULL;
    text->font = FontSecondary;
    text->horizontal = AlignLeft;
    text->vertical = AlignBottom;
}

static void start_screen_icon_animation_cleanup(IconElement* icon) {
    furi_assert(icon);
    if(icon->animation != NULL) {
        icon_animation_stop(icon->animation);
        icon_animation_free(icon->animation);
        icon->animation = NULL;
    }
}

static void start_screen_model_set_default(StartScreenModel* model) {
    furi_assert(model);
    start_screen_text_element_set_default(&model->text1);
    start_screen_text_element_set_default(&model->text2);
    start_screen_text_element_set_default(&model->text3);

    model->icon.x = 0;
    model->icon.y = 0;
    model->icon.animation = NULL;
    model->secondary_draw_callback = NULL;
}

void start_screen_view_enter(void* context) {
    furi_assert(context);
    StartScreen* start_screen = context;

    with_view_model(
        start_screen->view,
        StartScreenModel * model,
        {
            if(model->icon.animation != NULL) icon_animation_start(model->icon.animation);
        },
        true);
}

void start_screen_view_exit(void* context) {
    furi_assert(context);
    StartScreen* start_screen = context;

    with_view_model(
        start_screen->view,
        StartScreenModel * model,
        {
            if(model->icon.animation != NULL) icon_animation_stop(model->icon.animation);
        },
        true);
}

void start_screen_view_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);
    StartScreenModel* model = _model;

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), canvas_height(canvas));
    canvas_set_color(canvas, ColorBlack);

    if(model->icon.animation != NULL) {
        canvas_draw_icon_animation(canvas, model->icon.x, model->icon.y, model->icon.animation);
    }

    if(model->text1.text != NULL) {
        canvas_set_font(canvas, model->text1.font);
        elements_multiline_text_aligned(
            canvas,
            model->text1.x,
            model->text1.y,
            model->text1.horizontal,
            model->text1.vertical,
            model->text1.text);
    }

    if(model->text2.text != NULL) {
        canvas_set_font(canvas, model->text2.font);
        elements_multiline_text_aligned(
            canvas,
            model->text2.x,
            model->text2.y,
            model->text2.horizontal,
            model->text2.vertical,
            model->text2.text);
    }

    if(model->text3.text != NULL) {
        canvas_set_font(canvas, model->text3.font);
        elements_multiline_text_aligned(
            canvas,
            model->text3.x,
            model->text3.y,
            model->text3.horizontal,
            model->text3.vertical,
            model->text3.text);
    }

    if(model->secondary_draw_callback != NULL) {
        model->secondary_draw_callback(canvas, model);
    }
}

bool start_screen_view_input_callback(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    StartScreen* start_screen = context;
    bool consumed = false;

    // If custom input callback is defined pass event to it
    if(start_screen->input_callback != NULL) {
        consumed = start_screen->input_callback(event, start_screen->context);
    }

    return consumed;
}

StartScreen* start_screen_alloc(void) {
    StartScreen* start_screen = (StartScreen*)malloc(sizeof(StartScreen));
    if(start_screen == NULL) return NULL;

    start_screen->view = view_alloc();
    if(start_screen->view == NULL) return NULL;

    start_screen->context = NULL;
    start_screen->input_callback = NULL;

    view_set_context(start_screen->view, start_screen);
    view_allocate_model(start_screen->view, ViewModelTypeLocking, sizeof(StartScreenModel));

    with_view_model(
        start_screen->view,
        StartScreenModel * model,
        { start_screen_model_set_default(model); },
        true);

    view_set_draw_callback(start_screen->view, start_screen_view_draw_callback);
    view_set_input_callback(start_screen->view, start_screen_view_input_callback);

    // Right now these enter/exit callbacks are being used to start/stop animations
    view_set_enter_callback(start_screen->view, start_screen_view_enter);
    view_set_exit_callback(start_screen->view, start_screen_view_exit);

    return start_screen;
}

void start_screen_free(StartScreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        StartScreenModel * model,
        { start_screen_icon_animation_cleanup(&model->icon); },
        false);

    instance->context = NULL;
    instance->input_callback = NULL;

    if(instance->view) view_free(instance->view);
    if(instance) free(instance);
}

void start_screen_reset(StartScreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        StartScreenModel * model,
        {
            start_screen_icon_animation_cleanup(&model->icon);
            start_screen_model_set_default(model);
        },
        false);
    instance->input_callback = NULL;
    instance->context = NULL;
}

View* start_screen_get_view(StartScreen* instance) {
    furi_assert(instance);
    return instance->view;
}

void start_screen_set_input_callback(StartScreen* instance, StartScreenInputCallback callback) {
    furi_assert(instance);
    instance->input_callback = callback;
}

void start_screen_set_secondary_draw_callback(
    StartScreen* instance,
    StartScreenDrawCallback callback) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        StartScreenModel * model,
        { model->secondary_draw_callback = callback; },
        true);
}

void start_screen_set_context(StartScreen* instance, void* context) {
    furi_assert(instance);
    instance->context = context;
}

void start_screen_set_text1(
    StartScreen* instance,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        StartScreenModel * model,
        {
            model->text1.x = x;
            model->text1.y = y;
            model->text1.horizontal = horizontal;
            model->text1.vertical = vertical;
            model->text1.font = font;
            model->text1.text = text;
        },
        true);
}

void start_screen_set_text2(
    StartScreen* instance,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        StartScreenModel * model,
        {
            model->text2.x = x;
            model->text2.y = y;
            model->text2.horizontal = horizontal;
            model->text2.vertical = vertical;
            model->text2.font = font;
            model->text2.text = text;
        },
        true);
}

void start_screen_set_text3(
    StartScreen* instance,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        StartScreenModel * model,
        {
            model->text3.x = x;
            model->text3.y = y;
            model->text3.horizontal = horizontal;
            model->text3.vertical = vertical;
            model->text3.font = font;
            model->text3.text = text;
        },
        true);
}

void start_screen_set_icon_animation(
    StartScreen* instance,
    uint8_t x,
    uint8_t y,
    const Icon* animation) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        StartScreenModel * model,
        {
            start_screen_icon_animation_cleanup(&model->icon);

            model->icon.x = x;
            model->icon.y = y;

            if(animation != NULL) {
                model->icon.animation = icon_animation_alloc(animation);
                view_tie_icon_animation(instance->view, model->icon.animation);
            }
        },
        true);
}
