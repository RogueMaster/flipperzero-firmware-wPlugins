#include "../guess_the_number.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>
#include <dolphin/dolphin.h>

struct GameInstructions {
    View* view;
    GameInstructionsCallback callback;
    void* context;
};

typedef struct {
    int some_value;
} GameInstructionsModel;

void game_instructions_set_callback(
    GameInstructions* instance,
    GameInstructionsCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void game_instructions_draw(Canvas* canvas, GameInstructionsModel* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "HOW TO PLAY");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 4, 16, AlignLeft, AlignTop, "🎮 CONTROLS:");
    canvas_draw_str_aligned(canvas, 4, 26, AlignLeft, AlignTop, "  OK - Submit your guess");
    canvas_draw_str_aligned(canvas, 4, 36, AlignLeft, AlignTop, "  ↑↓ - Fine tune (+/-1)");
    canvas_draw_str_aligned(canvas, 4, 46, AlignLeft, AlignTop, "  ←→ - Quick adjust (+/-10)");
    canvas_draw_str_aligned(canvas, 4, 56, AlignLeft, AlignTop, "  BACK - Tap:Reset | Hold:Exit");
}

static void game_instructions_model_init(GameInstructionsModel* const model) {
    model->some_value = 1;
}

bool game_instructions_input(InputEvent* event, void* context) {
    furi_assert(context);
    GameInstructions* instance = context;
    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
            with_view_model(
                instance->view,
                GameInstructionsModel * model,
                {
                    UNUSED(model);
                    instance->callback(GameCustomEventInstructionsBack, instance->context);
                },
                true);
            break;
        case InputKeyLeft:
        case InputKeyRight:
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyOk:
            with_view_model(
                instance->view, GameInstructionsModel * model, { UNUSED(model); }, true);
            break;
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void game_instructions_exit(void* context) {
    furi_assert(context);
}

void game_instructions_enter(void* context) {
    furi_assert(context);
    GameInstructions* instance = (GameInstructions*)context;
    with_view_model(
        instance->view,
        GameInstructionsModel * model,
        { game_instructions_model_init(model); },
        true);
}

GameInstructions* game_instructions_alloc() {
    GameInstructions* instance = malloc(sizeof(GameInstructions));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(GameInstructionsModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)game_instructions_draw);
    view_set_input_callback(instance->view, game_instructions_input);
    view_set_enter_callback(instance->view, game_instructions_enter);
    view_set_exit_callback(instance->view, game_instructions_exit);

    with_view_model(
        instance->view,
        GameInstructionsModel * model,
        { game_instructions_model_init(model); },
        true);

    return instance;
}

void game_instructions_free(GameInstructions* instance) {
    furi_assert(instance);

    with_view_model(instance->view, GameInstructionsModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* game_instructions_get_view(GameInstructions* instance) {
    furi_assert(instance);
    return instance->view;
}
