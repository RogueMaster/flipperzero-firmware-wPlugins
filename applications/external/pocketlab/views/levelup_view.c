#include "levelup_view.h"

#include <furi.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_fonts.h"

#define LEVELUP_PERIOD_MS 50
#define LEVELUP_FRAMES    75 // ~3750 ms, 50% longer than the old popup timeout
#define CONFETTI_COUNT    18

// A falling confetti fleck: fixed column, fall speed, vertical offset and shape.
typedef struct {
    uint8_t x;
    uint8_t speed;
    uint8_t phase;
    uint8_t kind;
} Confetti;

// Deterministic scatter so every celebration looks lively without an RNG.
static const Confetti confetti[CONFETTI_COUNT] = {
    {6, 3, 10, 0},
    {18, 4, 44, 1},
    {30, 2, 6, 2},
    {42, 5, 60, 3},
    {54, 3, 30, 0},
    {66, 4, 74, 1},
    {78, 2, 18, 2},
    {90, 5, 52, 3},
    {102, 3, 4, 0},
    {114, 4, 66, 1},
    {12, 5, 38, 2},
    {36, 2, 80, 3},
    {60, 4, 14, 0},
    {84, 3, 48, 1},
    {108, 5, 26, 2},
    {24, 3, 70, 3},
    {48, 4, 2, 0},
    {96, 2, 58, 1},
};

struct LevelUpView {
    View* view;
    FuriTimer* timer;
    LevelUpViewDoneCallback callback;
    void* context;
};

typedef struct {
    uint32_t frame;
    uint16_t total_frames;
    char header[16];
    char title[24];
} LevelUpModel;

static void levelup_view_draw_callback(Canvas* canvas, void* model_) {
    LevelUpModel* model = model_;
    const uint32_t f = model->frame;

    // Confetti raining down behind the card.
    for(size_t i = 0; i < CONFETTI_COUNT; i++) {
        const Confetti* c = &confetti[i];
        const int y = (int)((f * c->speed + c->phase) % 88) - 12;
        if(y < -4 || y > 63) continue;
        const int sway = (int)(((f / 3) + c->phase) % 3) - 1;
        const int x = c->x + sway;
        switch(c->kind) {
        case 0:
            canvas_draw_dot(canvas, x, y);
            break;
        case 1:
            canvas_draw_box(canvas, x, y, 2, 2);
            break;
        case 2:
            canvas_draw_circle(canvas, x, y, 1);
            break;
        default:
            canvas_draw_line(canvas, x, y, x + 2, y);
            break;
        }
    }

    // Card behind the text so it stays readable over the confetti.
    const int card_x = 12;
    const int card_y = 18;
    const int card_w = 104;
    const int card_h = 30;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, card_x, card_y, card_w, card_h);
    canvas_set_color(canvas, ColorBlack);
    elements_slightly_rounded_frame(canvas, card_x, card_y, card_w, card_h);

    pocketlab_font_apply(canvas, true);
    canvas_draw_str_aligned(canvas, 64, card_y + 10, AlignCenter, AlignCenter, model->header);
    pocketlab_font_apply_small(canvas);
    canvas_draw_str_aligned(canvas, 64, card_y + 22, AlignCenter, AlignCenter, model->title);
}

static void levelup_view_finish(LevelUpView* instance) {
    furi_timer_stop(instance->timer);
    if(instance->callback) {
        instance->callback(instance->context);
    }
}

static bool levelup_view_input_callback(InputEvent* event, void* context) {
    LevelUpView* instance = context;
    if(event->type == InputTypeShort && (event->key == InputKeyOk || event->key == InputKeyBack)) {
        levelup_view_finish(instance);
        return true;
    }
    return false;
}

static void levelup_view_timer_callback(void* context) {
    LevelUpView* instance = context;
    bool done = false;
    with_view_model(
        instance->view,
        LevelUpModel * model,
        {
            model->frame++;
            done = model->frame >= model->total_frames;
        },
        true);
    if(done) {
        levelup_view_finish(instance);
    }
}

static void levelup_view_enter_callback(void* context) {
    LevelUpView* instance = context;
    with_view_model(instance->view, LevelUpModel * model, { model->frame = 0; }, true);
    furi_timer_start(instance->timer, furi_ms_to_ticks(LEVELUP_PERIOD_MS));
}

static void levelup_view_exit_callback(void* context) {
    LevelUpView* instance = context;
    furi_timer_stop(instance->timer);
}

LevelUpView* levelup_view_alloc(void) {
    LevelUpView* instance = malloc(sizeof(LevelUpView));

    instance->view = view_alloc();
    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(LevelUpModel));
    view_set_draw_callback(instance->view, levelup_view_draw_callback);
    view_set_input_callback(instance->view, levelup_view_input_callback);
    view_set_enter_callback(instance->view, levelup_view_enter_callback);
    view_set_exit_callback(instance->view, levelup_view_exit_callback);

    instance->timer =
        furi_timer_alloc(levelup_view_timer_callback, FuriTimerTypePeriodic, instance);
    instance->callback = NULL;
    instance->context = NULL;

    return instance;
}

void levelup_view_free(LevelUpView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* levelup_view_get_view(LevelUpView* instance) {
    furi_assert(instance);
    return instance->view;
}

void levelup_view_set_done_callback(
    LevelUpView* instance,
    LevelUpViewDoneCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void levelup_view_configure(
    LevelUpView* instance,
    const char* header,
    const char* title,
    uint16_t frames) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        LevelUpModel * model,
        {
            model->frame = 0;
            model->total_frames = frames;
            strncpy(model->header, header, sizeof(model->header) - 1);
            model->header[sizeof(model->header) - 1] = '\0';
            strncpy(model->title, title, sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
        },
        true);
}
