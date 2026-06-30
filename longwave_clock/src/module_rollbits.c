
#include "module_rollbits.h"
#include "longwave_clock_icons.h"

#define FRAME_HEIGHT   13
#define START_HEIGHT   10
#define MIN_X          2
#define Y_POS          11
#define ICON_WIDTH     9
#define ICON_TOP       0
#define ICON_BOTTOM    10
#define ICON_SPACING   4
#define WIDTH_DIGIT    6
#define WIDTH_CHECKSUM 2
#define WIDTH_MINUTE   10
#define WIDTH_ASYNC    13
#define WIDTH_START    2
#define MID_SCREEN     64
#define SUM_HEIGHT_1   17
#define SUM_HEIGHT_2   19
#define SUM_HEIGHT_3   21
#define STRIKE_HEIGHT  7

char* start_description[] =
    {"BBK+Weather", "DUT", "TZ", "Minute", "Hour", "Date", "Day", "Month", "Year"};

static void draw_before_start(Canvas* canvas, uint16_t top, const char* str) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, MID_SCREEN, top + FRAME_HEIGHT + START_HEIGHT, AlignCenter, AlignBottom, str);
    canvas_set_font(canvas, FontKeyboard);
}

void draw_decoded_bits(
    Canvas* canvas,
    LWCType type,
    uint16_t top,
    Bit buffer[],
    uint8_t length_buffer,
    uint8_t count_signals,
    uint8_t last_index,
    bool waiting_for_interrupt,
    bool waiting_for_sync) {
    if(waiting_for_interrupt) {
        draw_before_start(canvas, top, "Waiting first interrupt...");
    } else if(waiting_for_sync) {
        draw_before_start(canvas, top, "Waiting for sync...");
    }
    uint8_t left = canvas_width(canvas) - ICON_WIDTH;
    canvas_draw_icon(canvas, left, ICON_TOP, &I_lwc_sender);

    if(type == DCF77) {
        canvas_draw_icon(canvas, left, ICON_BOTTOM, &I_lwc_dcf);
    } else if(type == MSF) {
        canvas_draw_icon(canvas, left, ICON_BOTTOM, &I_lwc_msf);
    }

    left = left - ICON_SPACING;

    canvas_draw_line(canvas, 0, top, left, top);
    canvas_draw_line(canvas, 0, top + FRAME_HEIGHT, left, top + FRAME_HEIGHT);
    canvas_set_font(canvas, FontKeyboard);

    uint8_t index = last_index;
    uint8_t current_count = count_signals;
    bool drawn_start = false;
    bool drawn_end = false;

    while(left > MIN_X + WIDTH_DIGIT && current_count > 0) {
        Bit bit = buffer[index];

        current_count--;
        if(index == 0) {
            index = length_buffer - 1;
        } else {
            index--;
        }

        if(bit < BitChecksum) {
            left = left - WIDTH_DIGIT;
            if(bit == BitZero) {
                canvas_draw_str(canvas, left, Y_POS, "0");
            } else {
                canvas_draw_str(canvas, left, Y_POS, "1");
            }
        } else if(bit < BitEndMinute) {
            left = left - WIDTH_CHECKSUM;
            canvas_draw_line(canvas, left, top, left, top + FRAME_HEIGHT);
            if(!waiting_for_sync) {
                if(bit <= BitChecksumError) {
                    canvas_draw_line(
                        canvas, left + 2, top + SUM_HEIGHT_2, left + 6, top + SUM_HEIGHT_2);
                    canvas_draw_line(
                        canvas, left + 4, top + SUM_HEIGHT_1, left + 4, top + SUM_HEIGHT_3);
                } else {
                    canvas_draw_line(
                        canvas, left + 2, top + SUM_HEIGHT_2, left + 6, top + SUM_HEIGHT_2);
                    canvas_draw_line(
                        canvas, left + 2, top + SUM_HEIGHT_3, left + 6, top + SUM_HEIGHT_3);
                }
            }
            if(bit % 2 == 1) {
                canvas_draw_line(canvas, left, top + STRIKE_HEIGHT, left + 8, top + STRIKE_HEIGHT);
            }
        } else if(bit == BitEndMinute) {
            if(left < MIN_X + WIDTH_MINUTE) {
                break;
            }
            left = left - WIDTH_MINUTE;
            canvas_draw_box(
                canvas, left + 1, top, WIDTH_MINUTE - 2, FRAME_HEIGHT + START_HEIGHT + 1);
            drawn_end = true;
        } else if(bit == BitEndSync) {
            if(left < MIN_X + WIDTH_ASYNC) {
                break;
            }
            left = left - WIDTH_ASYNC;
            canvas_draw_line(canvas, left + 1, top, left + 4, top + 3);
            canvas_draw_line(canvas, left + WIDTH_ASYNC - 5, top, left + WIDTH_ASYNC - 2, top + 3);

            canvas_draw_line(canvas, left + 4, top + 3, left + 1, top + 6);
            canvas_draw_line(
                canvas, left + WIDTH_ASYNC - 2, top + 3, left + WIDTH_ASYNC - 5, top + 6);

            canvas_draw_line(canvas, left + 1, top + 6, left + 4, top + 9);
            canvas_draw_line(
                canvas, left + WIDTH_ASYNC - 5, top + 6, left + WIDTH_ASYNC - 2, top + 9);

            canvas_draw_line(canvas, left + 4, top + 9, left + 1, top + 12);
            canvas_draw_line(
                canvas, left + WIDTH_ASYNC - 2, top + 9, left + WIDTH_ASYNC - 5, top + 12);
            drawn_end = true;
        } else if(bit == BitUnknown) {
            left = left - WIDTH_DIGIT;
            canvas_draw_str(canvas, left, Y_POS, "?");
        } else if(bit >= BitStartEmpty) {
            left = left - WIDTH_START;
            uint8_t extra_height;
            if(bit > BitStartEmpty && !waiting_for_sync) {
                extra_height = START_HEIGHT;
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(
                    canvas,
                    left + 2,
                    top + FRAME_HEIGHT + START_HEIGHT,
                    start_description[bit - BitStartEmpty - 1]);
                canvas_set_font(canvas, FontKeyboard);
            } else {
                extra_height = 0;
            }
            canvas_draw_line(canvas, left, top, left, top + FRAME_HEIGHT + extra_height);
            drawn_start = true;
        }
    }

    while(!drawn_start && !drawn_end && !waiting_for_sync && current_count > 0) {
        Bit bit = buffer[index];

        current_count--;
        if(index == 0) {
            index = length_buffer - 1;
        } else {
            index--;
        }

        if(bit == BitEndMinute) {
            drawn_end = true;
        } else if(bit >= BitStartEmpty) {
            canvas_draw_line(canvas, 0, top, 0, top + FRAME_HEIGHT + START_HEIGHT);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(
                canvas, 2, FRAME_HEIGHT + START_HEIGHT, start_description[bit - BitStartEmpty - 1]);
            canvas_set_font(canvas, FontKeyboard);
            drawn_start = true;
        }
    }
}
