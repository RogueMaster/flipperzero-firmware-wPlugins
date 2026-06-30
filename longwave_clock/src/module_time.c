#include "module_time.h"

#define TIME_DIGIT_HALFW  6
#define TIME_DIGIT_HEIGHT 15
#define LEFT_TIME         2
#define WIDTH_TIME        90
#define MID_TIME_S1       28
#define MID_TIME_S2       60
#define MID_TIME          46
#define MID_HOURS         14
#define MID_HOURS_10s     8
#define MID_HOURS_1s      20
#define MID_MINUTES       46
#define MID_MINUTES_10s   40
#define MID_MINUTES_1s    52
#define MID_SECONDS       78
#define RIGHT_TZ          126
#define TIMEZONE_W        32

static char* timezone_before_repr[] = {"- : - ", "+00:01", "+01:01", "+02:01"};
static char* timezone_exact_repr[] = {"- : - ", "+00:00", "+01:00", "+02:00"};

static void draw_selection_digit(Canvas* canvas, uint16_t top, uint16_t mid_left) {
    canvas_draw_frame(
        canvas, mid_left - TIME_DIGIT_HALFW, top + TIME_DIGIT_HEIGHT, TIME_DIGIT_HALFW * 2, 2);
}

static void draw_selection_two_digits(Canvas* canvas, uint16_t top, uint16_t mid_left) {
    canvas_draw_frame(
        canvas, mid_left - TIME_DIGIT_HALFW * 2, top + TIME_DIGIT_HEIGHT, TIME_DIGIT_HALFW * 4, 2);
}

static void draw_selection_timezone(Canvas* canvas, uint16_t top, uint16_t right) {
    canvas_draw_frame(canvas, right - TIMEZONE_W, top + TIME_DIGIT_HEIGHT, TIMEZONE_W, 2);
}

static void draw_selection_time(Canvas* canvas, uint16_t top, uint16_t right) {
    canvas_draw_frame(canvas, right, top + TIME_DIGIT_HEIGHT, WIDTH_TIME, 2);
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

static void
    draw_two_digits_centered(Canvas* canvas, uint16_t top, uint16_t mid_left, int8_t digits) {
    if(digits < 0) {
        canvas_draw_str_aligned(canvas, mid_left, top, AlignCenter, AlignTop, "--");
    } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02d", digits);
        canvas_draw_str_aligned(canvas, mid_left, top, AlignCenter, AlignTop, buf);
    }
}

static void draw_timezone(
    Canvas* canvas,
    uint16_t top,
    uint16_t right,
    Timezone timezone,
    bool for_next_minute) {
    canvas_set_font(canvas, FontPrimary);

    if(for_next_minute) {
        canvas_draw_str_aligned(
            canvas, right, top, AlignRight, AlignBottom, timezone_before_repr[timezone]);
    } else {
        canvas_draw_str_aligned(
            canvas, right, top, AlignRight, AlignBottom, timezone_exact_repr[timezone]);
    }
}

void draw_decoded_time(
    Canvas* canvas,
    uint16_t top,
    DecodingTimePhase selection,
    int8_t hours_10s,
    int8_t hours_1s,
    int8_t minutes_10s,
    int8_t minutes_1s,
    int8_t seconds,
    Timezone timezone,
    bool for_next_minute) {
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, MID_TIME_S1, top, AlignCenter, AlignTop, ":");
    canvas_draw_str_aligned(canvas, MID_TIME_S2, top, AlignCenter, AlignTop, ":");

    draw_digit_centered(canvas, top, MID_HOURS_10s, hours_10s);
    draw_digit_centered(canvas, top, MID_HOURS_1s, hours_1s);

    draw_digit_centered(canvas, top, MID_MINUTES_10s, minutes_10s);
    draw_digit_centered(canvas, top, MID_MINUTES_1s, minutes_1s);

    draw_two_digits_centered(canvas, top, MID_SECONDS, seconds);

    draw_timezone(canvas, top + TIME_DIGIT_HEIGHT - 1, RIGHT_TZ, timezone, for_next_minute);

    switch(selection) {
    case DecodingTimeTimezone:
        draw_selection_timezone(canvas, top, RIGHT_TZ);
        break;
    case DecodingTimeHours:
        draw_selection_two_digits(canvas, top, MID_HOURS);
        break;
    case DecodingTimeMinutes:
        draw_selection_two_digits(canvas, top, MID_MINUTES);
        break;
    case DecodingTimeSeconds:
        draw_selection_two_digits(canvas, top, MID_SECONDS);
        break;
    case DecodingTimeHours10s:
        draw_selection_digit(canvas, top, MID_HOURS_10s);
        break;
    case DecodingTimeHours1s:
        draw_selection_digit(canvas, top, MID_HOURS_1s);
        break;
    case DecodingTimeMinutes10s:
        draw_selection_digit(canvas, top, MID_MINUTES_10s);
        break;
    case DecodingTimeMinutes1s:
        draw_selection_digit(canvas, top, MID_MINUTES_1s);
        break;
    case DecodingTimeChecksum:
        draw_selection_time(canvas, top, LEFT_TIME);
        break;
    default:
        break;
    }
}
