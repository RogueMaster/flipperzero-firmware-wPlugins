#include "../flipcheckers.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>

struct FlipCheckersStartscreen {
    View* view;
    FlipCheckersStartscreenCallback callback;
    void* context;
};

typedef struct {
    int some_value;
} FlipCheckersStartscreenModel;

void flipcheckers_startscreen_set_callback(
    FlipCheckersStartscreen* instance,
    FlipCheckersStartscreenCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

// Draw a filled circle approximation (piece)
static void draw_piece_filled(Canvas* canvas, int cx, int cy, int r) {
    for(int dy = -r; dy <= r; dy++) {
        for(int dx = -r; dx <= r; dx++) {
            if(dx*dx + dy*dy <= r*r) {
                canvas_draw_dot(canvas, cx + dx, cy + dy);
            }
        }
    }
}

// Draw a hollow circle approximation (piece outline)
static void draw_piece_hollow(Canvas* canvas, int cx, int cy, int r) {
    for(int dy = -r; dy <= r; dy++) {
        for(int dx = -r; dx <= r; dx++) {
            int d2 = dx*dx + dy*dy;
            if(d2 <= r*r && d2 >= (r-1)*(r-1)) {
                canvas_draw_dot(canvas, cx + dx, cy + dy);
            }
        }
    }
}

void flipcheckers_startscreen_draw(Canvas* canvas, FlipCheckersStartscreenModel* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Outer border
    canvas_draw_frame(canvas, 1, 1, 126, 62);

    // === Title: "CHECKERS" bold large, version right-aligned on same baseline ===
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 14, 16, "CHECKERS");
    canvas_draw_str_aligned(canvas, 113, 16, AlignRight, AlignBottom, FLIPCHECKERS_VERSION);

    // Double underline beneath title
    canvas_draw_line(canvas, 14, 18, 113, 18);
    canvas_draw_line(canvas, 14, 20, 113, 20);

    // === Row of alternating pieces as decoration ===
    // 8 pieces evenly spaced, alternating filled (red/white) and hollow (black)
    int piece_y = 33;
    int piece_xs[] = {16, 28, 40, 52, 64, 76, 88, 100, 112};
    for(int i = 0; i < 9; i++) {
        if(i % 2 == 0) {
            draw_piece_filled(canvas, piece_xs[i], piece_y, 4);
        } else {
            draw_piece_hollow(canvas, piece_xs[i], piece_y, 4);
        }
    }

    // === Divider ===
    canvas_draw_line(canvas, 4, 41, 123, 41);

    // === Bottom: "Press OK to play" centered ===
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, "Press OK to play");
}

static void flipcheckers_startscreen_model_init(FlipCheckersStartscreenModel* const model) {
    model->some_value = 1;
}

bool flipcheckers_startscreen_input(InputEvent* event, void* context) {
    furi_assert(context);
    FlipCheckersStartscreen* instance = context;

    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
            with_view_model(
                instance->view,
                FlipCheckersStartscreenModel * model,
                {
                    UNUSED(model);
                    instance->callback(FlipCheckersCustomEventStartscreenBack, instance->context);
                },
                true);
            break;
        case InputKeyOk:
        case InputKeyRight:
        case InputKeyLeft:
        case InputKeyUp:
        case InputKeyDown:
            with_view_model(
                instance->view,
                FlipCheckersStartscreenModel * model,
                {
                    UNUSED(model);
                    instance->callback(FlipCheckersCustomEventStartscreenOk, instance->context);
                },
                true);
            break;
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void flipcheckers_startscreen_exit(void* context) {
    furi_assert(context);
}

void flipcheckers_startscreen_enter(void* context) {
    furi_assert(context);
    FlipCheckersStartscreen* instance = (FlipCheckersStartscreen*)context;
    with_view_model(
        instance->view,
        FlipCheckersStartscreenModel * model,
        { flipcheckers_startscreen_model_init(model); },
        true);
}

FlipCheckersStartscreen* flipcheckers_startscreen_alloc() {
    FlipCheckersStartscreen* instance = malloc(sizeof(FlipCheckersStartscreen));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlipCheckersStartscreenModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)flipcheckers_startscreen_draw);
    view_set_input_callback(instance->view, flipcheckers_startscreen_input);

    with_view_model(
        instance->view,
        FlipCheckersStartscreenModel * model,
        { flipcheckers_startscreen_model_init(model); },
        true);

    return instance;
}

void flipcheckers_startscreen_free(FlipCheckersStartscreen* instance) {
    furi_assert(instance);
    with_view_model(instance->view, FlipCheckersStartscreenModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}

View* flipcheckers_startscreen_get_view(FlipCheckersStartscreen* instance) {
    furi_assert(instance);
    return instance->view;
}
