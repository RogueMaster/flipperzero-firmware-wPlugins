#include "../guess_the_number.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>

struct GameStartscreen {
    View* view;
    GameStartscreenCallback callback;
    void* context;
};

typedef struct {
    int some_value;
} GameStartscreenModel;

void game_startscreen_set_callback(
    GameStartscreen* instance,
    GameStartscreenCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void game_startscreen_draw(Canvas* canvas, GameStartscreenModel* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "GUESS THE NUMBER");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "═══════════════════");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Find the secret number!");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "Range: 0-99");

    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "Press any button to start");
}

static void game_startscreen_model_init(GameStartscreenModel* const model) {
    model->some_value = 1;
}

bool game_startscreen_input(InputEvent* event, void* context) {
    furi_assert(context);
    GameStartscreen* instance = context;
    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
            with_view_model(
                instance->view,
                GameStartscreenModel * model,
                {
                    UNUSED(model);
                    instance->callback(GameCustomEventStartscreenBack, instance->context);
                },
                true);
            break;
        case InputKeyLeft:
        case InputKeyRight:
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyOk:
            with_view_model(
                instance->view,
                GameStartscreenModel * model,
                {
                    UNUSED(model);
                    instance->callback(GameCustomEventStartscreenOk, instance->context);
                },
                true);
            break;
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void game_startscreen_exit(void* context) {
    furi_assert(context);
}

void game_startscreen_enter(void* context) {
    furi_assert(context);
    GameStartscreen* instance = (GameStartscreen*)context;
    with_view_model(
        instance->view,
        GameStartscreenModel * model,
        { game_startscreen_model_init(model); },
        true);
}

GameStartscreen* game_startscreen_alloc() {
    GameStartscreen* instance = malloc(sizeof(GameStartscreen));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(GameStartscreenModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)game_startscreen_draw);
    view_set_input_callback(instance->view, game_startscreen_input);
    view_set_enter_callback(instance->view, game_startscreen_enter);
    view_set_exit_callback(instance->view, game_startscreen_exit);

    with_view_model(
        instance->view,
        GameStartscreenModel * model,
        { game_startscreen_model_init(model); },
        true);

    return instance;
}

void game_startscreen_free(GameStartscreen* instance) {
    furi_assert(instance);

    with_view_model(instance->view, GameStartscreenModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* game_startscreen_get_view(GameStartscreen* instance) {
    furi_assert(instance);
    return instance->view;
}
