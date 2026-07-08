#include "progress_view.h"

#include <furi.h>

struct ProgressView {
    View* view;
};

typedef struct {
    uint32_t level;
    uint32_t xp;
    uint32_t labs_done;
    uint32_t labs_total;
} ProgressModel;

static void progress_view_draw_card(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    const char* value,
    const char* label) {
    canvas_draw_rbox(canvas, x, y, w, h, 3);

    const uint8_t cx = x + w / 2;
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, cx, y + h / 2, AlignCenter, AlignCenter, value);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, y + h + 8, AlignCenter, AlignBottom, label);
}

static void progress_view_draw_callback(Canvas* canvas, void* context) {
    ProgressModel* model = context;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "My progress");

    char value[12];
    snprintf(value, sizeof(value), "%lu", (unsigned long)model->level);
    progress_view_draw_card(canvas, 3, 14, 58, 23, value, "LEVEL");

    snprintf(value, sizeof(value), "%lu", (unsigned long)model->xp);
    progress_view_draw_card(canvas, 67, 14, 58, 23, value, "XP");

    canvas_set_font(canvas, FontSecondary);
    char line[20];
    snprintf(
        line,
        sizeof(line),
        "Labs: %lu / %lu",
        (unsigned long)model->labs_done,
        (unsigned long)model->labs_total);
    canvas_draw_str(canvas, 4, 59, line);

    snprintf(line, sizeof(line), "Badges: %lu", (unsigned long)model->labs_done);
    canvas_draw_str_aligned(canvas, 124, 59, AlignRight, AlignBottom, line);
}

static bool progress_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

ProgressView* progress_view_alloc(void) {
    ProgressView* instance = malloc(sizeof(ProgressView));
    instance->view = view_alloc();

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ProgressModel));
    view_set_draw_callback(instance->view, progress_view_draw_callback);
    view_set_input_callback(instance->view, progress_view_input_callback);

    return instance;
}

void progress_view_free(ProgressView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* progress_view_get_view(ProgressView* instance) {
    furi_assert(instance);
    return instance->view;
}

void progress_view_configure(
    ProgressView* instance,
    uint32_t level,
    uint32_t xp,
    uint32_t labs_done,
    uint32_t labs_total) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        ProgressModel * model,
        {
            model->level = level;
            model->xp = xp;
            model->labs_done = labs_done;
            model->labs_total = labs_total;
        },
        true);
}
