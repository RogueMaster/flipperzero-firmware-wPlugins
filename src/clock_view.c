#include "clock_view.h"
#include "clock_model.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/canvas.h>
#include <locale/locale.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char beats_text[5];
    char local_time_text[16];
    int16_t utc_offset_minutes;
} ClockViewModel;

struct ClockView {
    View* view;
    ClockViewOkCallback ok_callback;
    void* ok_context;
};

static void clock_view_draw_callback(Canvas* canvas, void* model) {
    ClockViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, m->beats_text);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, m->local_time_text);
}

static bool clock_view_input_callback(InputEvent* event, void* context) {
    furi_check(context);
    ClockView* clock_view = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk) {
        if(clock_view->ok_callback) {
            clock_view->ok_callback(clock_view->ok_context);
        }
        return true;
    }

    return false;
}

static void clock_view_model_apply_snapshot(
    ClockViewModel* model,
    bool hour_format_24,
    const DateTime* dt) {
    ClockModelInput input;
    input.hour = dt->hour;
    input.minute = dt->minute;
    input.second = dt->second;
    input.utc_offset_minutes = model->utc_offset_minutes;
    input.hour_format_24 = hour_format_24;

    ClockModelSnapshot snap;
    clock_model_build_snapshot(&input, &snap);
    memcpy(model->beats_text, snap.beats_text, sizeof(model->beats_text));
    memcpy(model->local_time_text, snap.local_time_text, sizeof(model->local_time_text));
}

ClockView* clock_view_alloc(void) {
    ClockView* clock_view = malloc(sizeof(ClockView));
    furi_check(clock_view);
    memset(clock_view, 0, sizeof(ClockView));

    clock_view->view = view_alloc();
    view_set_context(clock_view->view, clock_view);
    view_allocate_model(clock_view->view, ViewModelTypeLocking, sizeof(ClockViewModel));
    view_set_draw_callback(clock_view->view, clock_view_draw_callback);
    view_set_input_callback(clock_view->view, clock_view_input_callback);

    with_view_model(
        clock_view->view,
        ClockViewModel * model,
        {
            model->utc_offset_minutes = 0;
            snprintf(model->beats_text, sizeof(model->beats_text), "@000");
            snprintf(model->local_time_text, sizeof(model->local_time_text), "00:00:00");
        },
        true);

    return clock_view;
}

void clock_view_free(ClockView* clock_view) {
    furi_check(clock_view);
    view_free(clock_view->view);
    free(clock_view);
}

View* clock_view_get_view(ClockView* clock_view) {
    furi_check(clock_view);
    return clock_view->view;
}

void clock_view_set_ok_callback(ClockView* clock_view, ClockViewOkCallback callback, void* context) {
    furi_check(clock_view);
    clock_view->ok_callback = callback;
    clock_view->ok_context = context;
}

void clock_view_set_utc_offset(ClockView* clock_view, int16_t utc_offset_minutes) {
    furi_check(clock_view);
    with_view_model(
        clock_view->view,
        ClockViewModel * model,
        { model->utc_offset_minutes = utc_offset_minutes; },
        false);
}

void clock_view_update(ClockView* clock_view) {
    furi_check(clock_view);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    bool hour_format_24 = (locale_get_time_format() == LocaleTimeFormat24h);

    with_view_model(
        clock_view->view,
        ClockViewModel * model,
        { clock_view_model_apply_snapshot(model, hour_format_24, &dt); },
        true);
}
