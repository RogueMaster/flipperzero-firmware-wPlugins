#include "progress_view.h"

#include <furi.h>
#include <input/input.h>

#define PROGRESS_ANIM_MS 90

struct ProgressView {
    View* view;
    FuriTimer* timer;
    ProgressViewCallback callback;
    void* context;
};

typedef struct {
    uint32_t level;
    uint32_t xp;
    uint32_t labs_done;
    uint32_t labs_total;
    uint32_t streak;
    const char* level_title;
    uint32_t anim;
} ProgressModel;

// A stat card: the value on a filled black panel, the label in an outlined
// panel below it, both sharing one rounded card.
static void progress_view_draw_card(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t num_h,
    const char* value,
    const char* label) {
    const uint8_t cx = x + w / 2;
    const uint8_t lbl_h = 12;
    const uint8_t r = 3;

    // Value: filled black panel, rounded only on the TOP corners.
    canvas_draw_rbox(canvas, x, y, w, num_h, r);
    canvas_draw_box(canvas, x, y + num_h - r, w, r); // square off the bottom
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, cx, y + num_h / 2, AlignCenter, AlignCenter, value);
    canvas_set_color(canvas, ColorBlack);

    // Label: outlined panel butted underneath, rounded only on the BOTTOM corners.
    const uint8_t ly = y + num_h;
    canvas_draw_rframe(canvas, x, ly, w, lbl_h, r);
    canvas_draw_line(canvas, x, ly, x + w - 1, ly); // straight top edge
    canvas_draw_line(canvas, x, ly, x, ly + r); // square top-left corner
    canvas_draw_line(canvas, x + w - 1, ly, x + w - 1, ly + r); // square top-right corner
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, ly + lbl_h / 2, AlignCenter, AlignCenter, label);
}

static void progress_view_draw_callback(Canvas* canvas, void* context) {
    ProgressModel* model = context;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Profile");

    if(model->streak > 0) {
        char streak[24];
        snprintf(streak, sizeof(streak), "Streak: %lu", (unsigned long)model->streak);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 124, 9, AlignRight, AlignBottom, streak);
    }

    char value[12];
    snprintf(value, sizeof(value), "%lu", (unsigned long)model->level);
    progress_view_draw_card(canvas, 3, 13, 58, 22, value, model->level_title);

    snprintf(value, sizeof(value), "%lu", (unsigned long)model->xp);
    progress_view_draw_card(canvas, 67, 13, 58, 22, value, "XP");

    canvas_set_font(canvas, FontSecondary);
    char line[20];
    snprintf(
        line,
        sizeof(line),
        "Labs: %lu/%lu",
        (unsigned long)model->labs_done,
        (unsigned long)model->labs_total);
    canvas_draw_str(canvas, 4, 59, line);

    // Blinking hint that Right opens the badge gallery.
    if((model->anim / 6) % 2 == 0) {
        canvas_draw_str_aligned(canvas, 124, 59, AlignRight, AlignBottom, "Badges >");
    }
}

static bool progress_view_input_callback(InputEvent* event, void* context) {
    ProgressView* instance = context;
    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        if(instance->callback) {
            instance->callback(instance->context);
        }
        return true;
    }
    return false;
}

static void progress_view_timer_callback(void* context) {
    ProgressView* instance = context;
    with_view_model(instance->view, ProgressModel * model, { model->anim++; }, true);
}

static void progress_view_enter_callback(void* context) {
    ProgressView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(PROGRESS_ANIM_MS));
}

static void progress_view_exit_callback(void* context) {
    ProgressView* instance = context;
    furi_timer_stop(instance->timer);
}

ProgressView* progress_view_alloc(void) {
    ProgressView* instance = malloc(sizeof(ProgressView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ProgressModel));
    view_set_draw_callback(instance->view, progress_view_draw_callback);
    view_set_input_callback(instance->view, progress_view_input_callback);
    view_set_enter_callback(instance->view, progress_view_enter_callback);
    view_set_exit_callback(instance->view, progress_view_exit_callback);

    instance->timer =
        furi_timer_alloc(progress_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void progress_view_free(ProgressView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* progress_view_get_view(ProgressView* instance) {
    furi_assert(instance);
    return instance->view;
}

void progress_view_set_callback(
    ProgressView* instance,
    ProgressViewCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void progress_view_configure(
    ProgressView* instance,
    uint32_t level,
    uint32_t xp,
    uint32_t labs_done,
    uint32_t labs_total,
    uint32_t streak,
    const char* level_title) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        ProgressModel * model,
        {
            model->level = level;
            model->xp = xp;
            model->labs_done = labs_done;
            model->labs_total = labs_total;
            model->streak = streak;
            model->level_title = level_title;
        },
        true);
}
