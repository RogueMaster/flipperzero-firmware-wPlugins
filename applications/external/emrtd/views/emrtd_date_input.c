/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The DD MM YY date field.
 *
 * A mistyped date is indistinguishable from a wrong document number: the chip
 * answers both with the same refusal. Three separate fields that can only hold
 * a real date remove one of the three ways to get this wrong, and showing the
 * resolved four digit year removes the fourth, which is entering 1974 as 74
 * and meaning 2074.
 */
#include "emrtd_date_input.h"

#include <furi.h>
#include <furi_hal.h>
#include <datetime/datetime.h>
#include <gui/elements.h>

#include "../protocol/emrtd_mrz.h"

/** Which of the three fields the arrows act on. */
typedef enum {
    EmrtdDatePartDay,
    EmrtdDatePartMonth,
    EmrtdDatePartYear,
    EmrtdDatePartCount,
} EmrtdDatePart;

typedef struct {
    char header[32];
    uint8_t day; /**< 1..31, before the month is taken into account. */
    uint8_t month; /**< 1..12. */
    uint8_t year; /**< The two digits the MRZ carries, 0..99. */
    uint8_t focus;
    bool future;
    uint16_t current_year; /**< Four digits, read from the clock once. */
} EmrtdDateInputModel;

struct EmrtdDateInput {
    View* view;
    EmrtdDateInputCallback callback;
    void* context;
};

/* Geometry of the three fields, in screen coordinates. */
#define EMRTD_DATE_BOX_WIDTH  32
#define EMRTD_DATE_BOX_HEIGHT 20
#define EMRTD_DATE_BOX_TOP    18

static const uint8_t emrtd_date_input_centre[EmrtdDatePartCount] = {24, 64, 104};
static const char* const emrtd_date_input_caption[EmrtdDatePartCount] = {"DD", "MM", "YY"};

/**
 * Turn the two digit year into four.
 *
 * Two digits cannot say which century they belong to, so the direction of the
 * field decides: an expiry lies ahead, a date of birth behind. This is the
 * convention every eMRTD reader uses and it is display only - the value handed
 * back is still the six digits the MRZ carries.
 */
static uint16_t emrtd_date_input_resolve_year(const EmrtdDateInputModel* model) {
    uint16_t year = (uint16_t)(2000 + model->year);
    if(!model->future && year > model->current_year) {
        year -= 100;
    }
    return year;
}

/** True when the three fields name a day that exists. */
static bool emrtd_date_input_is_valid(const EmrtdDateInputModel* model) {
    if(model->month < 1 || model->month > 12 || model->day < 1) {
        return false;
    }
    const uint16_t year = emrtd_date_input_resolve_year(model);
    return model->day <= datetime_get_days_per_month(datetime_is_leap_year(year), model->month);
}

/**
 * The six digits the MRZ carries, in the order it carries them.
 *
 * The masks say nothing the fields do not already guarantee; they are there
 * so that the compiler can see two digits is all any of them can ever be.
 */
static void emrtd_date_input_to_mrz(const EmrtdDateInputModel* model, char* out, size_t out_size) {
    snprintf(
        out, out_size, "%02u%02u%02u", model->year % 100u, model->month % 100u, model->day % 100u);
}

static void emrtd_date_input_draw_field(
    Canvas* canvas,
    const EmrtdDateInputModel* model,
    EmrtdDatePart field,
    uint8_t value) {
    const uint8_t centre = emrtd_date_input_centre[field];
    const uint8_t left = (uint8_t)(centre - EMRTD_DATE_BOX_WIDTH / 2);

    if(model->focus == field) {
        /* Two frames one inside the other read as a bold box on this screen. */
        canvas_draw_rframe(
            canvas, left, EMRTD_DATE_BOX_TOP, EMRTD_DATE_BOX_WIDTH, EMRTD_DATE_BOX_HEIGHT, 3);
        canvas_draw_rframe(
            canvas,
            left + 1,
            EMRTD_DATE_BOX_TOP + 1,
            EMRTD_DATE_BOX_WIDTH - 2,
            EMRTD_DATE_BOX_HEIGHT - 2,
            2);

        /* The arrows say which field the up and down keys are pointing at. */
        canvas_draw_triangle(
            canvas, centre, EMRTD_DATE_BOX_TOP - 2, 7, 4, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas,
            centre,
            EMRTD_DATE_BOX_TOP + EMRTD_DATE_BOX_HEIGHT + 2,
            7,
            4,
            CanvasDirectionTopToBottom);
    }

    char text[4];
    snprintf(text, sizeof(text), "%02u", value);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        centre,
        EMRTD_DATE_BOX_TOP + EMRTD_DATE_BOX_HEIGHT / 2,
        AlignCenter,
        AlignCenter,
        text);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, centre, 51, AlignCenter, AlignBottom, emrtd_date_input_caption[field]);
}

static void emrtd_date_input_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    const EmrtdDateInputModel* model = context;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, model->header);

    emrtd_date_input_draw_field(canvas, model, EmrtdDatePartDay, model->day);
    emrtd_date_input_draw_field(canvas, model, EmrtdDatePartMonth, model->month);
    emrtd_date_input_draw_field(canvas, model, EmrtdDatePartYear, model->year);

    canvas_set_font(canvas, FontSecondary);
    if(emrtd_date_input_is_valid(model)) {
        char yymmdd[EMRTD_DATE_LEN + 1];
        char pretty[16];
        emrtd_date_input_to_mrz(model, yymmdd, sizeof(yymmdd));
        emrtd_mrz_format_date(yymmdd, model->future, pretty, sizeof(pretty));
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, pretty);
    } else {
        /*
         * Inverted, because an impossible date is the one thing on this screen
         * that has to be noticed rather than read.
         */
        const char* message = "No such date";
        const uint16_t width = canvas_string_width(canvas, message);
        const uint8_t left = (uint8_t)((128 - width) / 2 - 2);
        canvas_draw_box(canvas, left, 53, (size_t)(width + 4), 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, message);
        canvas_set_color(canvas, ColorBlack);
    }
}

/** Step one field, wrapping at its own limits. */
static void emrtd_date_input_step(EmrtdDateInputModel* model, int8_t delta) {
    if(model->focus == EmrtdDatePartDay) {
        int16_t day = (int16_t)(model->day + delta);
        if(day < 1) {
            day = 31;
        } else if(day > 31) {
            day = 1;
        }
        model->day = (uint8_t)day;
    } else if(model->focus == EmrtdDatePartMonth) {
        int16_t month = (int16_t)(model->month + delta);
        if(month < 1) {
            month = 12;
        } else if(month > 12) {
            month = 1;
        }
        model->month = (uint8_t)month;
    } else {
        int16_t year = (int16_t)(model->year + delta);
        if(year < 0) {
            year = 99;
        } else if(year > 99) {
            year = 0;
        }
        model->year = (uint8_t)year;
    }
}

static bool emrtd_date_input_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    EmrtdDateInput* instance = context;

    /* Press and release are the halves of a short press; acting on them too
     * would move the focus twice per key. Repeat is kept so that holding up
     * runs through the years. */
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool accepted = false;
    char yymmdd[EMRTD_DATE_LEN + 1] = {0};

    switch(event->key) {
    case InputKeyLeft:
        with_view_model(
            instance->view,
            EmrtdDateInputModel * model,
            {
                model->focus =
                    (uint8_t)((model->focus + EmrtdDatePartCount - 1) % EmrtdDatePartCount);
            },
            true);
        consumed = true;
        break;

    case InputKeyRight:
        with_view_model(
            instance->view,
            EmrtdDateInputModel * model,
            { model->focus = (uint8_t)((model->focus + 1) % EmrtdDatePartCount); },
            true);
        consumed = true;
        break;

    case InputKeyUp:
        with_view_model(
            instance->view,
            EmrtdDateInputModel * model,
            { emrtd_date_input_step(model, 1); },
            true);
        consumed = true;
        break;

    case InputKeyDown:
        with_view_model(
            instance->view,
            EmrtdDateInputModel * model,
            { emrtd_date_input_step(model, -1); },
            true);
        consumed = true;
        break;

    case InputKeyOk:
        if(event->type == InputTypeShort) {
            with_view_model(
                instance->view,
                EmrtdDateInputModel * model,
                {
                    if(emrtd_date_input_is_valid(model)) {
                        /* The MRZ orders the digits YYMMDD, which is the one
                         * thing this view exists to hide from the user. */
                        emrtd_date_input_to_mrz(model, yymmdd, sizeof(yymmdd));
                        accepted = true;
                    }
                },
                false);
            consumed = true;
        }
        break;

    default:
        /* Back is left alone so that the dispatcher's navigation callback runs
         * and the scene decides where the user goes. */
        break;
    }

    /* Outside the model block: the callback runs scene code, which is free to
     * switch views, and that must not happen with the model locked. */
    if(accepted && instance->callback) {
        instance->callback(instance->context, yymmdd);
    }

    return consumed;
}

EmrtdDateInput* emrtd_date_input_alloc(void) {
    EmrtdDateInput* instance = malloc(sizeof(EmrtdDateInput));
    memset(instance, 0, sizeof(EmrtdDateInput));

    instance->view = view_alloc();
    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(EmrtdDateInputModel));
    view_set_draw_callback(instance->view, emrtd_date_input_draw_callback);
    view_set_input_callback(instance->view, emrtd_date_input_input_callback);

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    with_view_model(
        instance->view,
        EmrtdDateInputModel * model,
        {
            model->day = 1;
            model->month = 1;
            model->year = (uint8_t)(now.year % 100);
            model->current_year = now.year;
            model->focus = EmrtdDatePartDay;
            strlcpy(model->header, "Date", sizeof(model->header));
        },
        false);

    return instance;
}

void emrtd_date_input_free(EmrtdDateInput* instance) {
    furi_assert(instance);

    /* The fields are part of the key to a document, so they are cleared
     * before the model behind them is handed back to the heap. */
    with_view_model(
        instance->view, EmrtdDateInputModel * model, { memset(model, 0, sizeof(*model)); }, false);

    /* view_free releases the model with the view. */
    view_free(instance->view);
    free(instance);
}

View* emrtd_date_input_get_view(EmrtdDateInput* instance) {
    furi_assert(instance);
    return instance->view;
}

void emrtd_date_input_set_callback(
    EmrtdDateInput* instance,
    EmrtdDateInputCallback callback,
    void* context) {
    furi_assert(instance);

    instance->callback = callback;
    instance->context = context;
}

void emrtd_date_input_set_header(EmrtdDateInput* instance, const char* header) {
    furi_assert(instance);
    furi_assert(header);

    with_view_model(
        instance->view,
        EmrtdDateInputModel * model,
        { strlcpy(model->header, header, sizeof(model->header)); },
        true);
}

void emrtd_date_input_set_value(EmrtdDateInput* instance, const char* yymmdd) {
    furi_assert(instance);
    furi_assert(yymmdd);

    /*
     * Anything that is not six digits means nothing is stored yet, and the
     * fields are put back to the first of the current year. Leaving them as
     * they were would show the date of birth on the expiry screen, where one
     * press of OK would store it as the expiry.
     */
    bool usable = strlen(yymmdd) == EMRTD_DATE_LEN;
    for(size_t i = 0; usable && i < EMRTD_DATE_LEN; i++) {
        usable = yymmdd[i] >= '0' && yymmdd[i] <= '9';
    }
    if(!usable) {
        with_view_model(
            instance->view,
            EmrtdDateInputModel * model,
            {
                model->day = 1;
                model->month = 1;
                model->year = (uint8_t)(model->current_year % 100);
                model->focus = EmrtdDatePartDay;
            },
            true);
        return;
    }

    const uint8_t year = (uint8_t)((yymmdd[0] - '0') * 10 + (yymmdd[1] - '0'));
    const uint8_t month = (uint8_t)((yymmdd[2] - '0') * 10 + (yymmdd[3] - '0'));
    const uint8_t day = (uint8_t)((yymmdd[4] - '0') * 10 + (yymmdd[5] - '0'));

    with_view_model(
        instance->view,
        EmrtdDateInputModel * model,
        {
            model->year = year;
            model->month = month;
            model->day = day;
            model->focus = EmrtdDatePartDay;
        },
        true);
}

void emrtd_date_input_set_future(EmrtdDateInput* instance, bool future) {
    furi_assert(instance);

    with_view_model(
        instance->view, EmrtdDateInputModel * model, { model->future = future; }, true);
}
