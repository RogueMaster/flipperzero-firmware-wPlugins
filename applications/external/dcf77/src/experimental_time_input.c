#include "experimental_time_input.h"

#include <gui/elements.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Dcf77ExperimentalTimeInput {
    View* view;
    Dcf77ExperimentalTimeInputPreviousCallback previous_callback;
    void* previous_callback_context;
};

typedef struct {
    DateTime datetime;
    uint8_t field;
} Dcf77ExperimentalTimeInputModel;

#define DCF77_EXPERIMENTAL_TIME_INPUT_FIELD_COUNT    6U
#define DCF77_EXPERIMENTAL_TIME_INPUT_VALUE_HEIGHT   18U
#define DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y         5U
#define DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y         39U
#define DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH 28U

static uint8_t dcf77_experimental_time_input_days_per_month(uint8_t month, uint16_t year) {
    if(month < 1U || month > 12U) {
        return 31U;
    }

    return datetime_get_days_per_month(datetime_is_leap_year(year), month);
}

static void dcf77_experimental_time_input_normalize_date(DateTime* datetime) {
    if(datetime->year < 2000U) {
        datetime->year = 2000U;
    } else if(datetime->year > 2069U) {
        datetime->year = 2069U;
    }

    if(datetime->month < 1U) {
        datetime->month = 1U;
    } else if(datetime->month > 12U) {
        datetime->month = 12U;
    }

    const uint8_t max_day =
        dcf77_experimental_time_input_days_per_month(datetime->month, datetime->year);
    if(datetime->day < 1U) {
        datetime->day = 1U;
    } else if(datetime->day > max_day) {
        datetime->day = max_day;
    }
}

static uint8_t dcf77_experimental_time_input_weekday(const DateTime* datetime) {
    DateTime normalized = *datetime;
    const uint32_t timestamp = datetime_datetime_to_timestamp(&normalized);
    datetime_timestamp_to_datetime(timestamp, &normalized);
    return normalized.weekday;
}

static void dcf77_experimental_time_input_draw_value(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    const char* text,
    bool active) {
    canvas_set_color(canvas, ColorBlack);

    if(active) {
        canvas_draw_box(canvas, x, y, width, 18);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, x, y, width, 18);
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, x + (int32_t)(width / 2U), y + 16, AlignCenter, AlignBottom, text);

    if(active) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void dcf77_experimental_time_input_draw_triangle_up(
    Canvas* canvas,
    int32_t center_x,
    int32_t base_y) {
    canvas_draw_line(canvas, center_x, base_y - 2, center_x, base_y - 2);
    canvas_draw_line(canvas, center_x - 1, base_y - 1, center_x + 1, base_y - 1);
    canvas_draw_line(canvas, center_x - 2, base_y, center_x + 2, base_y);
}

static void dcf77_experimental_time_input_draw_triangle_down(
    Canvas* canvas,
    int32_t center_x,
    int32_t top_y) {
    canvas_draw_line(canvas, center_x - 2, top_y, center_x + 2, top_y);
    canvas_draw_line(canvas, center_x - 1, top_y + 1, center_x + 1, top_y + 1);
    canvas_draw_line(canvas, center_x, top_y + 2, center_x, top_y + 2);
}

static void dcf77_experimental_time_input_draw_focus(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    bool active) {
    if(!active) {
        return;
    }

    const int32_t center_x = x + (int32_t)(width / 2U);
    dcf77_experimental_time_input_draw_triangle_up(canvas, center_x, y - 2);
    dcf77_experimental_time_input_draw_triangle_down(
        canvas, center_x, y + (int32_t)DCF77_EXPERIMENTAL_TIME_INPUT_VALUE_HEIGHT + 1);
}

static void dcf77_experimental_time_input_draw_callback(Canvas* canvas, void* context) {
    static const char* const weekday_labels[] = {
        "?", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    static const char* const month_labels[] = {
        "?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    Dcf77ExperimentalTimeInputModel* model = context;
    char day_text[4];
    char month_text[4];
    char year_text[6];
    char hour_text[4];
    char minute_text[4];
    char second_text[4];
    char date_text[16];
    const uint8_t weekday = dcf77_experimental_time_input_weekday(&model->datetime);

    snprintf(day_text, sizeof(day_text), "%02u", model->datetime.day);
    snprintf(month_text, sizeof(month_text), "%02u", model->datetime.month);
    snprintf(year_text, sizeof(year_text), "%04u", model->datetime.year);
    snprintf(hour_text, sizeof(hour_text), "%02u", model->datetime.hour);
    snprintf(minute_text, sizeof(minute_text), "%02u", model->datetime.minute);
    snprintf(second_text, sizeof(second_text), "%02u", model->datetime.second);
    snprintf(
        date_text,
        sizeof(date_text),
        "%s, %u %s",
        weekday_labels[(weekday <= 7U) ? weekday : 0U],
        model->datetime.day,
        month_labels[(model->datetime.month <= 12U) ? model->datetime.month : 0U]);

    dcf77_experimental_time_input_draw_focus(
        canvas, 4, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 28U, model->field == 0U);
    dcf77_experimental_time_input_draw_value(
        canvas, 4, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 28U, day_text, model->field == 0U);
    canvas_draw_box(canvas, 35, 17, 2, 2);
    dcf77_experimental_time_input_draw_focus(
        canvas, 40, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 28U, model->field == 1U);
    dcf77_experimental_time_input_draw_value(
        canvas, 40, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 28U, month_text, model->field == 1U);
    canvas_draw_box(canvas, 70, 17, 2, 2);
    dcf77_experimental_time_input_draw_focus(
        canvas, 74, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 52U, model->field == 2U);
    dcf77_experimental_time_input_draw_value(
        canvas, 74, DCF77_EXPERIMENTAL_TIME_INPUT_DATE_Y, 52U, year_text, model->field == 2U);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, date_text);

    dcf77_experimental_time_input_draw_focus(
        canvas,
        8,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        model->field == 3U);
    dcf77_experimental_time_input_draw_value(
        canvas,
        8,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        hour_text,
        model->field == 3U);
    canvas_draw_box(canvas, 40, 51, 2, 2);
    dcf77_experimental_time_input_draw_focus(
        canvas,
        50,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        model->field == 4U);
    dcf77_experimental_time_input_draw_value(
        canvas,
        50,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        minute_text,
        model->field == 4U);
    canvas_draw_box(canvas, 82, 51, 2, 2);
    dcf77_experimental_time_input_draw_focus(
        canvas,
        92,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        model->field == 5U);
    dcf77_experimental_time_input_draw_value(
        canvas,
        92,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_Y,
        DCF77_EXPERIMENTAL_TIME_INPUT_TIME_BOX_WIDTH,
        second_text,
        model->field == 5U);
}

static bool dcf77_experimental_time_input_step_field(
    Dcf77ExperimentalTimeInputModel* model,
    bool increment) {
    if(model->field == 0U) {
        const uint8_t max_day = dcf77_experimental_time_input_days_per_month(
            model->datetime.month, model->datetime.year);
        if(increment) {
            model->datetime.day = (model->datetime.day >= max_day) ? 1U : model->datetime.day + 1U;
        } else {
            model->datetime.day = (model->datetime.day <= 1U) ? max_day : model->datetime.day - 1U;
        }
        return true;
    }

    if(model->field == 1U) {
        if(increment) {
            model->datetime.month = (model->datetime.month >= 12U) ? 1U :
                                                                     model->datetime.month + 1U;
        } else {
            model->datetime.month = (model->datetime.month <= 1U) ? 12U :
                                                                    model->datetime.month - 1U;
        }
        dcf77_experimental_time_input_normalize_date(&model->datetime);
        return true;
    }

    if(model->field == 2U) {
        if(increment) {
            model->datetime.year = (model->datetime.year >= 2069U) ? 2000U :
                                                                     model->datetime.year + 1U;
        } else {
            model->datetime.year = (model->datetime.year <= 2000U) ? 2069U :
                                                                     model->datetime.year - 1U;
        }
        dcf77_experimental_time_input_normalize_date(&model->datetime);
        return true;
    }

    if(model->field == 3U) {
        if(increment) {
            model->datetime.hour = (model->datetime.hour >= 23U) ? 0U : model->datetime.hour + 1U;
        } else {
            model->datetime.hour = (model->datetime.hour == 0U) ? 23U : model->datetime.hour - 1U;
        }
        return true;
    }

    if(model->field == 4U) {
        if(increment) {
            model->datetime.minute = (model->datetime.minute >= 59U) ? 0U :
                                                                       model->datetime.minute + 1U;
        } else {
            model->datetime.minute = (model->datetime.minute == 0U) ? 59U :
                                                                      model->datetime.minute - 1U;
        }
        return true;
    }

    if(model->field == 5U) {
        if(increment) {
            model->datetime.second = (model->datetime.second >= 59U) ? 0U :
                                                                       model->datetime.second + 1U;
        } else {
            model->datetime.second = (model->datetime.second == 0U) ? 59U :
                                                                      model->datetime.second - 1U;
        }
        return true;
    }

    return false;
}

static bool dcf77_experimental_time_input_input_callback(InputEvent* event, void* context) {
    Dcf77ExperimentalTimeInput* instance = context;
    bool consumed = false;

    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(event->key == InputKeyLeft) {
                    if(model->field > 0U) {
                        model->field--;
                    } else {
                        model->field = DCF77_EXPERIMENTAL_TIME_INPUT_FIELD_COUNT - 1U;
                    }
                    consumed = true;
                } else if(event->key == InputKeyRight) {
                    if(model->field + 1U < DCF77_EXPERIMENTAL_TIME_INPUT_FIELD_COUNT) {
                        model->field++;
                    } else {
                        model->field = 0U;
                    }
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    consumed = dcf77_experimental_time_input_step_field(model, true);
                } else if(event->key == InputKeyDown) {
                    consumed = dcf77_experimental_time_input_step_field(model, false);
                }
            }
        },
        consumed);

    return consumed;
}

static uint32_t dcf77_experimental_time_input_previous_callback(void* context) {
    Dcf77ExperimentalTimeInput* instance = context;

    if(instance == NULL || instance->previous_callback == NULL) {
        return VIEW_NONE;
    }

    return instance->previous_callback(instance->previous_callback_context);
}

Dcf77ExperimentalTimeInput* dcf77_experimental_time_input_alloc(void) {
    Dcf77ExperimentalTimeInput* instance = malloc(sizeof(Dcf77ExperimentalTimeInput));
    const DateTime default_datetime = {
        .hour = 0,
        .minute = 0,
        .second = 0,
        .day = 1,
        .month = 1,
        .year = 2000,
        .weekday = 6,
    };

    instance->view = view_alloc();
    instance->previous_callback = NULL;
    instance->previous_callback_context = NULL;
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(Dcf77ExperimentalTimeInputModel));
    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            model->datetime = default_datetime;
            model->field = 0U;
        },
        false);
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, dcf77_experimental_time_input_draw_callback);
    view_set_input_callback(instance->view, dcf77_experimental_time_input_input_callback);
    view_set_previous_callback(instance->view, dcf77_experimental_time_input_previous_callback);

    return instance;
}

void dcf77_experimental_time_input_free(Dcf77ExperimentalTimeInput* instance) {
    if(instance == NULL) {
        return;
    }

    view_free(instance->view);
    free(instance);
}

View* dcf77_experimental_time_input_get_view(Dcf77ExperimentalTimeInput* instance) {
    return instance->view;
}

void dcf77_experimental_time_input_set_previous_callback(
    Dcf77ExperimentalTimeInput* instance,
    Dcf77ExperimentalTimeInputPreviousCallback callback,
    void* context) {
    if(instance == NULL) {
        return;
    }

    instance->previous_callback = callback;
    instance->previous_callback_context = context;
}

void dcf77_experimental_time_input_set(
    Dcf77ExperimentalTimeInput* instance,
    const DateTime* datetime) {
    if(instance == NULL || datetime == NULL) {
        return;
    }

    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            model->datetime = *datetime;
            dcf77_experimental_time_input_normalize_date(&model->datetime);
            model->field = 0U;
        },
        true);
}

bool dcf77_experimental_time_input_get(Dcf77ExperimentalTimeInput* instance, DateTime* datetime) {
    if(instance == NULL || datetime == NULL) {
        return false;
    }

    bool copied = false;
    with_view_model(
        instance->view,
        Dcf77ExperimentalTimeInputModel * model,
        {
            *datetime = model->datetime;
            copied = true;
        },
        false);

    return copied;
}
