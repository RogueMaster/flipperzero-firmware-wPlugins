/*
 * Barcode input view.
 */

#include "numlock_input.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

#define NUM_DIGITS  16
#define CHAR_W      6
#define GROUP_SIZE  4
#define GROUP_GAP   5
#define PREFIX_W    10

typedef struct {
    char prefix;
    uint8_t digits[NUM_DIGITS];
    uint8_t cursor;
} NumlockModel;

static uint8_t prefix_x(void) {
    return 4;
}

static uint8_t digit_x(uint8_t i) {
    uint8_t groups = i / GROUP_SIZE;
    /* Keep the full code centered across the 128 px canvas. */
    return 4 + 8 + i * CHAR_W + groups * GROUP_GAP;
}

static void numlock_draw(Canvas* canvas, void* model_v) {
    NumlockModel* m = model_v;
    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "SET BARCODE");
    canvas_set_color(canvas, ColorBlack);

    int frame_y = 19;
    int frame_h = 24;
    canvas_draw_rframe(canvas, 1, frame_y, 126, frame_h, 2);
    canvas_draw_line(canvas, 2, frame_y+1, 125, frame_y+1);

    const uint8_t baseline = 36;
    canvas_set_font(canvas, FontPrimary);

    char prefix[2] = {m->prefix, '\0'};
    if(m->cursor == 0) {
        uint8_t x = prefix_x();
        canvas_draw_box(canvas, x - 1, frame_y + 3, CHAR_W + 2, frame_h - 6);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x, baseline, prefix);
        canvas_set_color(canvas, ColorBlack);

        uint8_t cx = x + CHAR_W / 2 - 1;
        canvas_draw_line(canvas, cx, frame_y - 4, cx - 2, frame_y - 2);
        canvas_draw_line(canvas, cx, frame_y - 4, cx + 2, frame_y - 2);
        canvas_draw_line(canvas, cx, frame_y - 4, cx, frame_y - 1);

        canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx - 2, frame_y + frame_h + 1);
        canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx + 2, frame_y + frame_h + 1);
        canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx, frame_y + frame_h);
    } else {
        canvas_draw_str(canvas, prefix_x(), baseline, prefix);
    }

    for(uint8_t i = 0; i < NUM_DIGITS; i++) {
        uint8_t x = digit_x(i);
        char ch[2] = {'0' + m->digits[i], '\0'};

        if((i + 1) == m->cursor) {
            canvas_draw_box(canvas, x - 1, frame_y + 3, CHAR_W + 2, frame_h - 6);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, baseline, ch);
            canvas_set_color(canvas, ColorBlack);

            uint8_t cx = x + CHAR_W / 2 - 1;
            canvas_draw_line(canvas, cx, frame_y - 4, cx - 2, frame_y - 2);
            canvas_draw_line(canvas, cx, frame_y - 4, cx + 2, frame_y - 2);
            canvas_draw_line(canvas, cx, frame_y - 4, cx, frame_y - 1);

            canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx - 2, frame_y + frame_h + 1);
            canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx + 2, frame_y + frame_h + 1);
            canvas_draw_line(canvas, cx, frame_y + frame_h + 3, cx, frame_y + frame_h);
        } else {
            canvas_draw_str(canvas, x, baseline, ch);
        }
    }

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 2, 59, "<\x12\x13> Sel");
    canvas_draw_str(canvas, 45, 59, "^\x18\x19v Set");

    canvas_draw_rbox(canvas, 92, 48, 34, 14, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 109, 55, AlignCenter, AlignCenter, "Hold OK");
    canvas_set_color(canvas, ColorBlack);
}

static bool numlock_input(InputEvent* input, void* ctx) {
    NumlockInput* numlock = ctx;
    if(input->type != InputTypePress && input->type != InputTypeRepeat) return false;

    bool consumed = false;

    with_view_model(
        numlock->view,
        NumlockModel * m,
        {
            switch(input->key) {
            case InputKeyUp:
                if(m->cursor == 0) {
                    m->prefix = (m->prefix == 'Z') ? 'A' : (m->prefix + 1);
                } else {
                    uint8_t digit_idx = m->cursor - 1;
                    m->digits[digit_idx] = (m->digits[digit_idx] + 1) % 10;
                }
                consumed = true;
                break;
            case InputKeyDown:
                if(m->cursor == 0) {
                    m->prefix = (m->prefix == 'A') ? 'Z' : (m->prefix - 1);
                } else {
                    uint8_t digit_idx = m->cursor - 1;
                    m->digits[digit_idx] = (m->digits[digit_idx] + 9) % 10;
                }
                consumed = true;
                break;
            case InputKeyLeft:
                if(m->cursor > 0) m->cursor--;
                consumed = true;
                break;
            case InputKeyRight:
                if(m->cursor < NUM_DIGITS) m->cursor++;
                consumed = true;
                break;
            case InputKeyOk: {
                char barcode[18];
                barcode[0] = m->prefix;
                for(uint8_t i = 0; i < NUM_DIGITS; i++)
                    barcode[1 + i] = '0' + m->digits[i];
                barcode[17] = '\0';
                if(numlock->callback)
                    numlock->callback(numlock->callback_ctx, barcode);
                consumed = true;
                break;
            }
            default:
                break;
            }
        },
        consumed);

    return consumed;
}

NumlockInput* numlock_input_alloc(void) {
    NumlockInput* numlock = malloc(sizeof(NumlockInput));
    numlock->view = view_alloc();
    view_allocate_model(numlock->view, ViewModelTypeLocking, sizeof(NumlockModel));
    view_set_draw_callback(numlock->view, numlock_draw);
    view_set_input_callback(numlock->view, numlock_input);
    view_set_context(numlock->view, numlock);
    numlock->callback = NULL;
    numlock->callback_ctx = NULL;

    with_view_model(
        numlock->view,
        NumlockModel * m,
        {
            m->prefix = 'N';
            memset(m->digits, 0, NUM_DIGITS);
            m->digits[0] = 4;
            m->cursor = 1;
        },
        true);

    return numlock;
}

void numlock_input_free(NumlockInput* numlock) {
    view_free(numlock->view);
    free(numlock);
}

View* numlock_input_get_view(NumlockInput* numlock) {
    return numlock->view;
}

void numlock_input_set_callback(NumlockInput* numlock, NumlockCallback cb, void* ctx) {
    numlock->callback = cb;
    numlock->callback_ctx = ctx;
}

void numlock_input_reset(NumlockInput* numlock) {
    with_view_model(
        numlock->view,
        NumlockModel * m,
        {
            m->prefix = 'N';
            memset(m->digits, 0, NUM_DIGITS);
            m->digits[0] = 4;
            m->cursor = 1;
        },
        true);
}
