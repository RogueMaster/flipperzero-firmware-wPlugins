
#include "module_date.h"

#define RIGHT_DOW     44
#define LEFT_YEAR     46
#define WIDTH_YEAR    26
#define MID_CENTURY   53
#define MID_YEAR_10s  62
#define MID_YEAR_1s   69
#define MID_DATE_S1   74
#define LEFT_MONTH    76
#define WIDTH_INYEAR  30
#define MID_MONTH_10s 79
#define MID_MONTH_1s  86
#define MID_DATE_S2   91
#define MID_DAY_10s   96
#define MID_DAY_1s    103
#define DIGIT_HALFW   3
#define DIGIT_HEIGHT  9
#define WIDTH_DOW     25
#define WIDTH_DATE    88

char* day_name[] = {"-   ", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

static void draw_dow_right(Canvas* canvas, uint16_t top, uint16_t right, int8_t day_in_week) {
    if(day_in_week == -1) {
        day_in_week = 0;
    }

    char buf[5];
    snprintf(buf, sizeof(buf), "%s,", day_name[day_in_week]);
    canvas_draw_str_aligned(canvas, right, top, AlignRight, AlignTop, buf);
}

static void draw_digit_centered(Canvas* canvas, uint16_t top, uint16_t mid_left, int8_t digit) {
    if(digit < 0) {
        canvas_draw_str_aligned(canvas, mid_left, top, AlignCenter, AlignTop, "-");
    } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", digit);
        canvas_draw_str_aligned(canvas, mid_left, top, AlignCenter, AlignTop, buf);
    }
}

static void draw_selection_digit(Canvas* canvas, uint16_t top, uint16_t mid_left) {
    canvas_draw_frame(canvas, mid_left - DIGIT_HALFW, top + DIGIT_HEIGHT, DIGIT_HALFW * 2 + 1, 2);
}

void draw_decoded_date(
    Canvas* canvas,
    uint16_t top,
    DecodingDatePhase selection,
    int8_t century,
    int8_t year_10s,
    int8_t year_1s,
    int8_t month_10s,
    int8_t month_1s,
    int8_t day_in_month_10s,
    int8_t day_in_month_1s,
    int8_t day_in_week) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, MID_DATE_S1, top, AlignCenter, AlignTop, ".");
    canvas_draw_str_aligned(canvas, MID_DATE_S2, top, AlignCenter, AlignTop, ".");
    draw_dow_right(canvas, top, RIGHT_DOW, day_in_week);
    draw_digit_centered(canvas, top, MID_CENTURY, century);
    draw_digit_centered(canvas, top, MID_YEAR_10s, year_10s);
    draw_digit_centered(canvas, top, MID_YEAR_1s, year_1s);
    draw_digit_centered(canvas, top, MID_MONTH_10s, month_10s);
    draw_digit_centered(canvas, top, MID_MONTH_1s, month_1s);
    draw_digit_centered(canvas, top, MID_DAY_10s, day_in_month_10s);
    draw_digit_centered(canvas, top, MID_DAY_1s, day_in_month_1s);

    switch(selection) {
    case DecodingDateYear10s:
        draw_selection_digit(canvas, top, MID_YEAR_10s);
        break;
    case DecodingDateYear1s:
        draw_selection_digit(canvas, top, MID_YEAR_1s);
        break;
    case DecodingDateMonth10s:
        draw_selection_digit(canvas, top, MID_MONTH_10s);
        break;
    case DecodingDateMonth1s:
        draw_selection_digit(canvas, top, MID_MONTH_1s);
        break;
    case DecodingDateDayOfMonth10s:
        draw_selection_digit(canvas, top, MID_DAY_10s);
        break;
    case DecodingDateDayOfMonth1s:
        draw_selection_digit(canvas, top, MID_DAY_1s);
        break;
    case DecodingDateDayOfWeek:
        canvas_draw_frame(canvas, RIGHT_DOW - WIDTH_DOW, top + DIGIT_HEIGHT, WIDTH_DOW, 2);
        break;
    case DecodingDateDayOfWeekChecksum:
        canvas_draw_frame(canvas, RIGHT_DOW - WIDTH_DOW, top + DIGIT_HEIGHT, WIDTH_DOW, 2);
        break;
    case DecodingDateDate:
        canvas_draw_frame(canvas, RIGHT_DOW - WIDTH_DOW, top + DIGIT_HEIGHT, WIDTH_DATE, 2);
        break;
    case DecodingDateYearChecksum:
        canvas_draw_frame(canvas, LEFT_YEAR, top + DIGIT_HEIGHT, WIDTH_YEAR, 2);
        break;
    case DecodingDateInYearChecksum:
        canvas_draw_frame(canvas, LEFT_MONTH, top + DIGIT_HEIGHT, WIDTH_INYEAR, 2);
        break;
    default:
        break;
    }
}
