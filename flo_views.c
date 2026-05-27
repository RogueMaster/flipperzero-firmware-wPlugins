#include "flo_app.h"
#include <furi.h>
#include <gui/elements.h>

/* ── Status View ────────────────────────────────────────── */

static void flo_status_draw(Canvas* canvas, void* _model) {
    FloStatusModel* model = _model;
    FloData* data = model->data;
    FloDate today = flo_date_today();

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Flo Period Tracker");

    canvas_draw_line(canvas, 0, 14, 128, 14);
    canvas_set_font(canvas, FontSecondary);

    char buf[64];

    /* Current date */
    snprintf(
        buf,
        sizeof(buf),
        "Today: %s %u, %u",
        flo_month_name_short(today.month),
        today.day,
        today.year);
    canvas_draw_str(canvas, 2, 24, buf);

    if(data->period_count == 0) {
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignCenter, "No periods logged yet.");
        canvas_draw_str_aligned(
            canvas, 64, 50, AlignCenter, AlignCenter, "Use 'Log Period' to start.");
    } else {
        /* Cycle day */
        int32_t cycle_day = flo_current_cycle_day(data);
        if(cycle_day > 0) {
            snprintf(buf, sizeof(buf), "Cycle day: %ld / %u", (long)cycle_day, data->cycle_length);
            canvas_draw_str(canvas, 2, 33, buf);
        }

        /* Next period prediction */
        FloDate next = flo_predict_next_period(data);
        int32_t days_until = flo_date_diff_days(today, next);
        if(days_until > 0) {
            snprintf(
                buf,
                sizeof(buf),
                "Next period in %ld days (%s %u)",
                (long)days_until,
                flo_month_name_short(next.month),
                next.day);
        } else if(days_until == 0) {
            snprintf(buf, sizeof(buf), "Period expected today!");
        } else {
            snprintf(buf, sizeof(buf), "Period %ld days late", (long)(-days_until));
        }
        canvas_draw_str(canvas, 2, 42, buf);

        /* Fertile window */
        FloDate fw_start = flo_fertile_window_start(data);
        FloDate fw_end = flo_fertile_window_end(data);
        int32_t fw_start_in = flo_date_diff_days(today, fw_start);
        int32_t fw_end_in = flo_date_diff_days(today, fw_end);

        if(fw_start_in <= 0 && fw_end_in >= 0) {
            canvas_draw_str(canvas, 2, 51, "Fertile window: NOW");
        } else if(fw_start_in > 0) {
            snprintf(
                buf,
                sizeof(buf),
                "Fertile window in %ld days",
                (long)fw_start_in);
            canvas_draw_str(canvas, 2, 51, buf);
        }

        /* Avg cycle with variability */
        if(data->cycle_stddev > 0) {
            snprintf(
                buf, sizeof(buf), "Cycle: %u+/-%u days%s",
                data->cycle_length, data->cycle_stddev,
                flo_is_cycle_irregular(data) ? " (!)" : "");
        } else {
            snprintf(buf, sizeof(buf), "Avg cycle: %u days", data->cycle_length);
        }
        canvas_draw_str(canvas, 2, 60, buf);
    }
}

static bool flo_status_input(InputEvent* event, void* context) {
    UNUSED(context);
    if(event->key == InputKeyBack) return false;
    return false;
}

View* flo_status_view_alloc(FloData* data) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FloStatusModel));
    with_view_model(
        view,
        FloStatusModel * model,
        { model->data = data; },
        true);
    view_set_draw_callback(view, flo_status_draw);
    view_set_input_callback(view, flo_status_input);
    return view;
}

/* ── Calendar View ──────────────────────────────────────── */

static void flo_calendar_draw(Canvas* canvas, void* _model) {
    FloCalendarModel* model = _model;
    FloData* data = model->data;
    uint16_t year = model->view_year;
    uint8_t month = model->view_month;
    FloDate today = flo_date_today();

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    char header[32];
    snprintf(header, sizeof(header), "< %s %u >", flo_month_name_short(month), year);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, header);

    canvas_set_font(canvas, FontSecondary);

    /* Day of week headers */
    static const char* dow[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for(int i = 0; i < 7; i++) {
        canvas_draw_str(canvas, 2 + i * 18, 17, dow[i]);
    }

    canvas_draw_line(canvas, 0, 19, 128, 19);

    /* First day of month */
    FloDate first = {year, month, 1};
    uint8_t start_dow = flo_day_of_week(first);
    uint8_t dim = flo_days_in_month(year, month);

    int x, y_row;
    for(uint8_t d = 1; d <= dim; d++) {
        uint8_t col = (start_dow + d - 1) % 7;
        uint8_t row = (start_dow + d - 1) / 7;
        x = 2 + col * 18;
        y_row = 27 + row * 7;
        if(y_row > 62) break; /* don't render off-screen */

        char day_str[4];
        snprintf(day_str, sizeof(day_str), "%u", d);

        /* Check if this day is during a period */
        FloDate check = {year, month, d};
        bool in_period = flo_is_in_period(data, check);
        bool predicted = !in_period && flo_is_predicted_period(data, check);
        bool fertile = !in_period && !predicted && flo_is_in_fertile_window(data, check);

        /* Check if today */
        bool is_today = (year == today.year && month == today.month && d == today.day);

        if(in_period) {
            /* Draw filled box for period days */
            canvas_draw_box(canvas, x - 1, y_row - 7, 14, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y_row, day_str);
            canvas_set_color(canvas, ColorBlack);
        } else if(predicted) {
            /* Dashed frame for predicted period */
            canvas_draw_frame(canvas, x - 1, y_row - 7, 14, 9);
            canvas_draw_dot(canvas, x + 5, y_row - 7); /* top dot marker */
            canvas_draw_str(canvas, x, y_row, day_str);
        } else if(fertile) {
            /* Dotted underline for fertile window */
            canvas_draw_str(canvas, x, y_row, day_str);
            for(int dot = 0; dot < 12; dot += 3) {
                canvas_draw_dot(canvas, x + dot, y_row + 1);
            }
        } else if(is_today) {
            /* Draw frame for today */
            canvas_draw_frame(canvas, x - 1, y_row - 7, 14, 9);
            canvas_draw_str(canvas, x, y_row, day_str);
        } else {
            canvas_draw_str(canvas, x, y_row, day_str);
        }
    }
}

static bool flo_calendar_input(InputEvent* event, void* context) {
    View* view = context;
    if(event->type != InputTypePress && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false;

    with_view_model(
        view,
        FloCalendarModel * model,
        {
            if(event->key == InputKeyLeft) {
                if(model->view_month == 1) {
                    model->view_month = 12;
                    model->view_year--;
                } else {
                    model->view_month--;
                }
            } else if(event->key == InputKeyRight) {
                if(model->view_month == 12) {
                    model->view_month = 1;
                    model->view_year++;
                } else {
                    model->view_month++;
                }
            }
        },
        true);
    return true;
}

View* flo_calendar_view_alloc(FloData* data) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FloCalendarModel));
    FloDate today = flo_date_today();
    with_view_model(
        view,
        FloCalendarModel * model,
        {
            model->data = data;
            model->view_year = today.year;
            model->view_month = today.month;
            model->cursor_day = today.day;
        },
        true);
    view_set_draw_callback(view, flo_calendar_draw);
    view_set_input_callback(view, flo_calendar_input);
    view_set_context(view, view);
    return view;
}

/* ── Log Period View ────────────────────────────────────── */

static void flo_log_draw(Canvas* canvas, void* _model) {
    FloLogModel* model = _model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Log Period");
    canvas_draw_line(canvas, 0, 14, 128, 14);

    canvas_set_font(canvas, FontSecondary);

    char buf[32];
    /* Year */
    snprintf(buf, sizeof(buf), "Year:     %u", model->date.year);
    canvas_draw_str(canvas, 4, 26, buf);
    if(model->field == 0) canvas_draw_str(canvas, 0, 26, ">");

    /* Month */
    snprintf(
        buf, sizeof(buf), "Month:    %u (%s)", model->date.month,
        flo_month_name_short(model->date.month));
    canvas_draw_str(canvas, 4, 35, buf);
    if(model->field == 1) canvas_draw_str(canvas, 0, 35, ">");

    /* Day */
    snprintf(buf, sizeof(buf), "Day:      %u", model->date.day);
    canvas_draw_str(canvas, 4, 44, buf);
    if(model->field == 2) canvas_draw_str(canvas, 0, 44, ">");

    /* Duration */
    snprintf(buf, sizeof(buf), "Duration: %u days", model->duration);
    canvas_draw_str(canvas, 4, 53, buf);
    if(model->field == 3) canvas_draw_str(canvas, 0, 53, ">");

    /* Buttons */
    canvas_set_font(canvas, FontPrimary);
    if(model->field == 4) {
        canvas_draw_box(canvas, 10, 55, 45, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 14, 65, "SAVE");
    canvas_set_color(canvas, ColorBlack);

    if(model->field == 5) {
        canvas_draw_box(canvas, 73, 55, 50, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 77, 65, "CANCEL");
    canvas_set_color(canvas, ColorBlack);
}

static bool flo_log_input(InputEvent* event, void* context) {
    View* view = context;
    if(event->type != InputTypePress && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false;

    bool consumed = true;

    with_view_model(
        view,
        FloLogModel * model,
        {
            if(event->key == InputKeyOk) {
                if(model->field == 4) {
                    view_dispatcher_send_custom_event(model->view_dispatcher, FloEventLogSave);
                } else if(model->field == 5) {
                    view_dispatcher_send_custom_event(model->view_dispatcher, FloEventLogCancel);
                }
            } else if(event->key == InputKeyUp) {
                if(model->field > 0) model->field--;
            } else if(event->key == InputKeyDown) {
                if(model->field < 5) model->field++;
            } else if(event->key == InputKeyRight) {
                switch(model->field) {
                case 0:
                    model->date.year++;
                    break;
                case 1:
                    if(model->date.month < 12) model->date.month++;
                    break;
                case 2: {
                    uint8_t max =
                        flo_days_in_month(model->date.year, model->date.month);
                    if(model->date.day < max) model->date.day++;
                    break;
                }
                case 3:
                    if(model->duration < 14) model->duration++;
                    break;
                default:
                    break;
                }
            } else if(event->key == InputKeyLeft) {
                switch(model->field) {
                case 0:
                    if(model->date.year > 2020) model->date.year--;
                    break;
                case 1:
                    if(model->date.month > 1) model->date.month--;
                    break;
                case 2:
                    if(model->date.day > 1) model->date.day--;
                    break;
                case 3:
                    if(model->duration > 1) model->duration--;
                    break;
                default:
                    break;
                }
            }
            /* Clamp day to valid range after month/year change */
            uint8_t max_day =
                flo_days_in_month(model->date.year, model->date.month);
            if(model->date.day > max_day) model->date.day = max_day;
        },
        true);

    return consumed;
}

View* flo_log_view_alloc(FloData* data, ViewDispatcher* view_dispatcher) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FloLogModel));
    FloDate today = flo_date_today();
    with_view_model(
        view,
        FloLogModel * model,
        {
            model->data = data;
            model->view_dispatcher = view_dispatcher;
            model->date = today;
            model->duration = data->default_period_duration;
            model->field = 0;
        },
        true);
    view_set_draw_callback(view, flo_log_draw);
    view_set_input_callback(view, flo_log_input);
    view_set_context(view, view);
    return view;
}
