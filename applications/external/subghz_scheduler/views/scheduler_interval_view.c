
#include "helpers/scheduler_hms.h"
#include "scheduler_interval_view.h"
#include "src/scheduler_app_i.h"
#include "subghz_scheduler_icons.h"

typedef enum {
    EditStateNone,
    EditStateActive,
    EditStateActiveEditing,
} EditState;

typedef struct {
    SchedulerHMS time;
    uint8_t col;
    bool editing;
} IntervalModel;

struct SchedulerIntervalView {
    View* view;
    SchedulerApp* app;
};

static inline EditState get_state(IntervalModel* model, uint8_t col) {
    if(model->col != col) {
        return EditStateNone;
    }
    return model->editing ? EditStateActiveEditing : EditStateActive;
}

static void draw_block(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t w,
    size_t h,
    Font font,
    EditState state,
    const char* text) {
    canvas_set_color(canvas, ColorBlack);

    if(state != EditStateNone) {
        if(state == EditStateActiveEditing) {
            canvas_draw_icon(canvas, x + w / 2 - 2, y - 4, &I_SmallArrowUp_3x5);
            canvas_draw_icon(canvas, x + w / 2 - 2, y + h + 1, &I_SmallArrowDown_3x5);
        }
        canvas_draw_rbox(canvas, x, y, w, h, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 1);
    }

    canvas_set_font(canvas, font);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, text);

    if(state != EditStateNone) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void interval_view_draw(Canvas* canvas, void* context) {
    IntervalModel* model = context;
    char buffer[4];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, GUI_DISPLAY_WIDTH / 2, 4, AlignCenter, AlignTop, "Set Interval");

    // Hours
    snprintf(buffer, sizeof(buffer), "%02u", model->time.h);
    draw_block(canvas, 14, 19, 28, 24, FontBigNumbers, get_state(model, 0), buffer);

    // Hour/Minute colon
    canvas_draw_box(canvas, 45, 34, 2, 2);
    canvas_draw_box(canvas, 45, 28, 2, 2);

    // Minutes
    snprintf(buffer, sizeof(buffer), "%02u", model->time.m);
    draw_block(canvas, 50, 19, 28, 24, FontBigNumbers, get_state(model, 1), buffer);

    // Minute/Second colon
    canvas_draw_box(canvas, 81, 34, 2, 2);
    canvas_draw_box(canvas, 81, 28, 2, 2);

    // Seconds
    snprintf(buffer, sizeof(buffer), "%02u", model->time.s);
    draw_block(canvas, 86, 19, 28, 24, FontBigNumbers, get_state(model, 2), buffer);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 58, AlignCenter, AlignCenter, model->editing ? "Editing" : "");
}

static bool interval_view_input(InputEvent* event, void* context) {
    SchedulerIntervalView* view = context;
    furi_assert(view);
    SchedulerApp* app = view->app;
    furi_assert(app);
    bool consumed = false;

    with_view_model(
        view->view,
        IntervalModel * model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                const InputKey key = event->key;

                if(key == InputKeyLeft) {
                    if(model->col > 0) {
                        model->col--;
                    }
                    consumed = true;
                } else if(key == InputKeyRight) {
                    if(model->col < 2) {
                        model->col++;
                    }
                    consumed = true;
                } else if(key == InputKeyOk) {
                    model->editing = !model->editing;
                    // Don't allow 00:00:00, clamp to 00:00:01
                    if(!model->editing) {
                        if(scheduler_hms_to_seconds(&model->time) == 0) {
                            model->time.s = 1;
                        }
                    }
                    consumed = true;
                } else if(key == InputKeyBack && model->editing) {
                    model->editing = false;
                    consumed = true;
                } else if(model->editing && (key == InputKeyUp || key == InputKeyDown)) {
                    const bool up = (key == InputKeyUp);

                    if(model->col == 0) {
                        model->time.h =
                            up ? (uint8_t)((model->time.h + 1) % 100) :
                                 (model->time.h == 0 ? 99 : (uint8_t)(model->time.h - 1));
                    } else if(model->col == 1) {
                        model->time.m =
                            up ? (uint8_t)((model->time.m + 1) % 60) :
                                 (model->time.m == 0 ? 59 : (uint8_t)(model->time.m - 1));
                    } else {
                        model->time.s =
                            up ? (uint8_t)((model->time.s + 1) % 60) :
                                 (model->time.s == 0 ? 59 : (uint8_t)(model->time.s - 1));
                    }
                    consumed = true;
                }
            }
        },
        true);

    return consumed;
}

void scheduler_interval_view_set_seconds(SchedulerIntervalView* v, uint32_t seconds) {
    furi_assert(v);

    with_view_model(
        v->view,
        IntervalModel * m,
        {
            scheduler_seconds_to_hms(seconds, &m->time);
            m->editing = false;
            m->col = 0;
        },
        true);
}

uint32_t scheduler_interval_view_get_seconds(SchedulerIntervalView* v) {
    furi_assert(v);
    uint32_t sec = 1;

    with_view_model(
        v->view, IntervalModel * m, { sec = scheduler_hms_to_seconds(&m->time); }, false);

    return sec;
}

SchedulerIntervalView* scheduler_interval_view_alloc() {
    SchedulerIntervalView* interval_view = calloc(1, sizeof(SchedulerIntervalView));
    interval_view->view = view_alloc();

    view_allocate_model(interval_view->view, ViewModelTypeLocking, sizeof(IntervalModel));
    view_set_context(interval_view->view, interval_view);
    view_set_draw_callback(interval_view->view, interval_view_draw);
    view_set_input_callback(interval_view->view, interval_view_input);

    with_view_model(
        interval_view->view,
        IntervalModel * m,
        {
            m->time.h = 0;
            m->time.m = 0;
            m->time.s = 1;
            m->col = 0;
            m->editing = false;
        },
        true);

    return interval_view;
}

View* scheduler_interval_view_get_view(SchedulerIntervalView* interval_view) {
    furi_assert(interval_view);
    return interval_view->view;
}

void scheduler_interval_view_free(SchedulerIntervalView* interval_view) {
    furi_assert(interval_view);
    view_free(interval_view->view);
    free(interval_view);
}

void scheduler_interval_view_set_app(SchedulerIntervalView* view, SchedulerApp* app) {
    furi_assert(view);
    view->app = app;
}
