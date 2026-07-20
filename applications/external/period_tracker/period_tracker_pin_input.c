#include "period_tracker_pin_input.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <string.h>

// Compact 0–9 keypad (two rows) + Save when four digits are entered.
// Original implementation for Period Tracker (MIT) — not derived from other apps.

#define PIN_KEY_ROWS 2
#define PIN_KEY_COLS 5

struct PinInput {
    View* view;
};

typedef struct {
    char header[32];
    uint8_t digits[PIN_INPUT_DIGIT_COUNT];
    uint8_t length; // 0..PIN_INPUT_DIGIT_COUNT
    uint8_t sel_row;
    uint8_t sel_col;
    bool auto_submit; // true: submit on 4th digit; false: require OK to confirm
    PinInputDoneCallback callback;
    void* callback_context;
} PinInputModel;

// Layout: 1 2 3 4 5 / 6 7 8 9 0
static const char pin_keys[PIN_KEY_ROWS][PIN_KEY_COLS] = {
    {'1', '2', '3', '4', '5'},
    {'6', '7', '8', '9', '0'},
};

static uint16_t pin_input_value_from_digits(const PinInputModel* model) {
    uint16_t value = 0;
    for(uint8_t i = 0; i < PIN_INPUT_DIGIT_COUNT; i++) {
        value = (uint16_t)(value * 10u + model->digits[i]);
    }
    return value;
}

static void pin_input_draw_callback(Canvas* canvas, void* model_ptr) {
    PinInputModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, model->header);

    // Four masked slots (empty = outline only, filled = asterisk)
    const int slot_w = 16;
    const int slot_h = 14;
    const int slot_gap = 6;
    const int total_w =
        PIN_INPUT_DIGIT_COUNT * slot_w + (PIN_INPUT_DIGIT_COUNT - 1) * slot_gap;
    int slot_x0 = (128 - total_w) / 2;
    const int slot_y = 16;

    canvas_set_font(canvas, FontPrimary);
    for(uint8_t i = 0; i < PIN_INPUT_DIGIT_COUNT; i++) {
        int x = slot_x0 + i * (slot_w + slot_gap);
        canvas_draw_frame(canvas, x, slot_y, slot_w, slot_h);
        if(i < model->length) {
            canvas_draw_str_aligned(
                canvas, x + slot_w / 2, slot_y + slot_h / 2 + 1, AlignCenter, AlignCenter, "*");
        }
    }

    // Hint
    canvas_set_font(canvas, FontSecondary);
    if(model->length < PIN_INPUT_DIGIT_COUNT) {
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "OK: digit  Back: del");
    } else if(model->auto_submit) {
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Checking...");
    } else {
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "OK: confirm  Back: del");
    }

    // Keypad (fits under y=63)
    const int key_w = 14;
    const int key_h = 9;
    const int key_gap_x = 4;
    const int key_gap_y = 1;
    const int grid_w = PIN_KEY_COLS * key_w + (PIN_KEY_COLS - 1) * key_gap_x;
    const int key_x0 = (128 - grid_w) / 2;
    const int key_y0 = 42;

    canvas_set_font(canvas, FontKeyboard);
    for(uint8_t row = 0; row < PIN_KEY_ROWS; row++) {
        for(uint8_t col = 0; col < PIN_KEY_COLS; col++) {
            int x = key_x0 + col * (key_w + key_gap_x);
            int y = key_y0 + row * (key_h + key_gap_y);
            bool selected = (model->sel_row == row && model->sel_col == col);

            if(selected) {
                canvas_draw_box(canvas, x, y, key_w, key_h);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_draw_frame(canvas, x, y, key_w, key_h);
                canvas_set_color(canvas, ColorBlack);
            }

            char label[2] = {pin_keys[row][col], '\0'};
            canvas_draw_str_aligned(
                canvas, x + key_w / 2, y + key_h / 2 + 1, AlignCenter, AlignCenter, label);
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

static void pin_input_submit(PinInputModel* model) {
    if(model->length != PIN_INPUT_DIGIT_COUNT) {
        return;
    }
    if(model->callback) {
        model->callback(model->callback_context, pin_input_value_from_digits(model));
    }
}

static void pin_input_append_digit(PinInputModel* model, uint8_t digit) {
    if(model->length >= PIN_INPUT_DIGIT_COUNT) {
        return;
    }
    model->digits[model->length++] = digit;
    // Unlock flow: submit as soon as 4 digits are in (no extra OK)
    if(model->auto_submit && model->length == PIN_INPUT_DIGIT_COUNT) {
        pin_input_submit(model);
    }
}

static void pin_input_delete_digit(PinInputModel* model) {
    if(model->length == 0) {
        return;
    }
    model->length--;
    model->digits[model->length] = 0;
}

static void pin_input_handle_ok(PinInputModel* model) {
    if(model->length >= PIN_INPUT_DIGIT_COUNT) {
        // Setup / confirm flow: OK submits after all digits entered
        if(!model->auto_submit) {
            pin_input_submit(model);
        }
        return;
    }
    char ch = pin_keys[model->sel_row][model->sel_col];
    pin_input_append_digit(model, (uint8_t)(ch - '0'));
}

static bool pin_input_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    PinInput* pin_input = context;
    bool consumed = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    with_view_model(
        pin_input->view,
        PinInputModel * model,
        {
            switch(event->key) {
            case InputKeyLeft:
                if(model->sel_col > 0) {
                    model->sel_col--;
                } else {
                    model->sel_col = PIN_KEY_COLS - 1;
                }
                consumed = true;
                break;
            case InputKeyRight:
                if(model->sel_col < PIN_KEY_COLS - 1) {
                    model->sel_col++;
                } else {
                    model->sel_col = 0;
                }
                consumed = true;
                break;
            case InputKeyUp:
                if(model->sel_row > 0) {
                    model->sel_row--;
                } else {
                    model->sel_row = PIN_KEY_ROWS - 1;
                }
                consumed = true;
                break;
            case InputKeyDown:
                if(model->sel_row < PIN_KEY_ROWS - 1) {
                    model->sel_row++;
                } else {
                    model->sel_row = 0;
                }
                consumed = true;
                break;
            case InputKeyOk:
                pin_input_handle_ok(model);
                consumed = true;
                break;
            case InputKeyBack:
                if(model->length > 0) {
                    pin_input_delete_digit(model);
                    consumed = true; // delete digit; do not leave scene
                } else {
                    consumed = false; // empty — scene handles exit
                }
                break;
            default:
                break;
            }
        },
        consumed);

    return consumed;
}

PinInput* pin_input_alloc(void) {
    PinInput* pin_input = malloc(sizeof(PinInput));
    furi_check(pin_input);

    pin_input->view = view_alloc();
    view_set_context(pin_input->view, pin_input);
    view_allocate_model(pin_input->view, ViewModelTypeLocking, sizeof(PinInputModel));
    view_set_draw_callback(pin_input->view, pin_input_draw_callback);
    view_set_input_callback(pin_input->view, pin_input_input_callback);

    with_view_model(
        pin_input->view,
        PinInputModel * model,
        {
            memset(model, 0, sizeof(PinInputModel));
            strncpy(model->header, "Enter PIN", sizeof(model->header) - 1);
            model->auto_submit = true;
        },
        true);

    return pin_input;
}

void pin_input_free(PinInput* pin_input) {
    furi_check(pin_input);
    view_free(pin_input->view);
    free(pin_input);
}

View* pin_input_get_view(PinInput* pin_input) {
    furi_check(pin_input);
    return pin_input->view;
}

void pin_input_reset(PinInput* pin_input) {
    furi_check(pin_input);
    with_view_model(
        pin_input->view,
        PinInputModel * model,
        {
            memset(model->digits, 0, sizeof(model->digits));
            model->length = 0;
            model->sel_row = 0;
            model->sel_col = 0;
            // auto_submit / callback / header preserved across reset
        },
        true);
}

void pin_input_set_header(PinInput* pin_input, const char* header) {
    furi_check(pin_input);
    furi_check(header);
    with_view_model(
        pin_input->view,
        PinInputModel * model,
        {
            strncpy(model->header, header, sizeof(model->header) - 1);
            model->header[sizeof(model->header) - 1] = '\0';
        },
        true);
}

void pin_input_set_result_callback(
    PinInput* pin_input,
    PinInputDoneCallback callback,
    void* context) {
    furi_check(pin_input);
    with_view_model(
        pin_input->view,
        PinInputModel * model,
        {
            model->callback = callback;
            model->callback_context = context;
        },
        false);
}

void pin_input_set_auto_submit(PinInput* pin_input, bool auto_submit) {
    furi_check(pin_input);
    with_view_model(
        pin_input->view, PinInputModel * model, { model->auto_submit = auto_submit; }, true);
}
