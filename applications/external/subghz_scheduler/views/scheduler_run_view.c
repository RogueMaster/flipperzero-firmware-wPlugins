#include "scheduler_run_view.h"
#include "src/scheduler_app_i.h"
#include "src/scheduler_run.h"
#include "helpers/scheduler_hms.h"

#include <gui/elements.h>

#define TAG "SubGHzSchedulerRunView"

#define MAX_FILENAME_WIDTH_PX 95

typedef struct {
    FuriString* header;
    FuriString* tx_mode;
    FuriString* interval;
    FuriString* tx_count;
    FuriString* file_type;
    FuriString* file_name;
    FuriString* tx_countdown;
    FuriString* radio;
    uint8_t tick_counter;
    uint8_t list_count;
} SchedulerRunViewModel;

struct SchedulerRunView {
    View* view;
};

static uint8_t get_text_width_px(Canvas* canvas, FuriString* text) {
    uint8_t pixels = 0;
    if(text) {
        for(uint8_t i = 0; i < furi_string_size(text); i++) {
            pixels += canvas_glyph_width(canvas, furi_string_get_char(text, i));
        }
    }
    return pixels;
}

static void scheduler_run_view_draw_callback(Canvas* canvas, SchedulerRunViewModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* ============= MAIN FRAME ============= */
    canvas_draw_rframe(canvas, 0, 0, GUI_DISPLAY_WIDTH, GUI_DISPLAY_HEIGHT, 5);

    /* ============= HEADER ============= */
    canvas_draw_frame(canvas, 0, GUI_TABLE_ROW_A, GUI_DISPLAY_WIDTH, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        GUI_DISPLAY_WIDTH / 2,
        2,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(model->header));

    canvas_set_font(canvas, FontSecondary);

    /* ============= MODE ============= */
    canvas_draw_frame(canvas, 0, GUI_TABLE_ROW_A, 46, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN, (GUI_TEXT_GAP * 2) - 4, &I_mode_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 10,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        furi_string_get_cstr(model->tx_mode));

    /* ============= INTERVAL ============= */
    canvas_draw_frame(canvas, 45, GUI_TABLE_ROW_A, 54, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN + 45, (GUI_TEXT_GAP * 2) - 4, &I_time_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 54,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        furi_string_get_cstr(model->interval));

    /* ============= TX COUNT ============= */
    canvas_draw_frame(canvas, 98, GUI_TABLE_ROW_A, 33, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN + 98, (GUI_TEXT_GAP * 2) - 4, &I_rept_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 114,
        (GUI_TEXT_GAP * 2),
        AlignCenter,
        AlignCenter,
        furi_string_get_cstr(model->tx_count));

    /* ============= RADIO ============= */
    canvas_draw_frame(canvas, 98, GUI_TABLE_ROW_B, 33, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN + 98, (GUI_TEXT_GAP * 3) - 5, &I_sub2_10px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 114,
        (GUI_TEXT_GAP * 3) + 1,
        AlignCenter,
        AlignCenter,
        furi_string_get_cstr(model->radio));

    /* ============= FILE NAME ============= */
    uint8_t file_name_width_px = get_text_width_px(canvas, model->file_name);
    int8_t offset = 1;
    if(file_name_width_px > MAX_FILENAME_WIDTH_PX) {
        file_name_width_px = MAX_FILENAME_WIDTH_PX;
        offset = 0;
    }
    canvas_draw_str_aligned(canvas, 5, (GUI_TEXT_GAP * 5) - 2, AlignLeft, AlignCenter, "File: ");
    elements_scrollable_text_line(
        canvas,
        GUI_DISPLAY_WIDTH - 5 - file_name_width_px + offset,
        (GUI_TEXT_GAP * 5) + 1,
        file_name_width_px,
        model->file_name,
        model->tick_counter,
        false);

    /* ============= NEXT TX COUNTDOWN ============= */
    canvas_draw_str_aligned(
        canvas, 5, GUI_DISPLAY_HEIGHT - 6, AlignLeft, AlignCenter, "Next TX: ");
    canvas_draw_str_aligned(
        canvas,
        GUI_DISPLAY_WIDTH - 5,
        GUI_DISPLAY_HEIGHT - 6,
        AlignRight,
        AlignCenter,
        furi_string_get_cstr(model->tx_countdown));
}

SchedulerRunView* scheduler_run_view_alloc() {
    SchedulerRunView* run_view = calloc(1, sizeof(SchedulerRunView));
    run_view->view = view_alloc();
    view_allocate_model(run_view->view, ViewModelTypeLocking, sizeof(SchedulerRunViewModel));
    view_set_context(run_view->view, run_view);
    view_set_draw_callback(run_view->view, (ViewDrawCallback)scheduler_run_view_draw_callback);

    with_view_model(
        run_view->view,
        SchedulerRunViewModel * model,
        {
            model->header = furi_string_alloc_set("Schedule Running");
            model->tx_mode = furi_string_alloc();
            model->interval = furi_string_alloc();
            model->tx_count = furi_string_alloc();
            model->file_type = furi_string_alloc();
            model->file_name = furi_string_alloc();
            model->tx_countdown = furi_string_alloc();
            model->radio = furi_string_alloc();
            model->tick_counter = 0;
            model->list_count = 0;
        },
        true);
    return run_view;
}

void scheduler_run_view_free(SchedulerRunView* run_view) {
    furi_assert(run_view);
    with_view_model(
        run_view->view,
        SchedulerRunViewModel * model,
        {
            furi_string_free(model->header);
            furi_string_free(model->tx_mode);
            furi_string_free(model->interval);
            furi_string_free(model->tx_count);
            furi_string_free(model->file_type);
            furi_string_free(model->file_name);
            furi_string_free(model->tx_countdown);
            furi_string_free(model->radio);
        },
        true);
    view_free(run_view->view);
    free(run_view);
}

View* scheduler_run_view_get_view(SchedulerRunView* run_view) {
    furi_assert(run_view);
    return run_view->view;
}

void scheduler_run_view_set_static_fields(SchedulerRunView* run_view, Scheduler* scheduler) {
    furi_assert(run_view);
    furi_assert(scheduler);

    char hms[9];
    const uint32_t interval_sec = scheduler_get_interval_seconds(scheduler);
    scheduler_seconds_to_hms_string(interval_sec, hms, sizeof(hms));

    with_view_model(
        run_view->view,
        SchedulerRunViewModel * model,
        {
            furi_string_set(model->tx_mode, tx_mode_text[scheduler_get_tx_mode(scheduler)]);
            furi_string_set(model->interval, hms);
            furi_string_set(model->tx_count, tx_count_text[scheduler_get_tx_count(scheduler)]);
            furi_string_set(model->file_type, file_type_text[scheduler_get_file_type(scheduler)]);
            furi_string_set(model->file_name, scheduler_get_file_name(scheduler));
            furi_string_set(model->radio, scheduler_get_radio(scheduler) ? "Ext" : "Int");
            model->list_count = scheduler_get_list_count(scheduler);
        },
        true);
}

void scheduler_run_view_update_countdown(SchedulerRunView* run_view, Scheduler* scheduler) {
    furi_assert(run_view);
    furi_assert(scheduler);

    char countdown[9];
    const uint32_t seconds = scheduler_get_countdown_seconds(scheduler);
    scheduler_seconds_to_hms_string(seconds, countdown, sizeof(countdown));

    with_view_model(
        run_view->view,
        SchedulerRunViewModel * model,
        { furi_string_set(model->tx_countdown, countdown); },
        true);
}

uint8_t scheduler_run_view_get_tick_counter(SchedulerRunView* run_view) {
    uint8_t tick = 0;
    with_view_model(
        run_view->view, SchedulerRunViewModel * model, { tick = model->tick_counter; }, false);
    return tick;
}

void scheduler_run_view_inc_tick_counter(SchedulerRunView* run_view) {
    with_view_model(
        run_view->view, SchedulerRunViewModel * model, { model->tick_counter++; }, false);
}
