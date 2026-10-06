#include "numpad.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>

#define NUMPAD_ROWS 3

#define NUMPAD_MARGIN_X 4
#define NUMPAD_WIDTH    120
#define NUMPAD_KEY_H    11
#define NUMPAD_KEYS_Y   28
#define NUMPAD_ROW_STEP 12

typedef enum {
    NumPadKeyDigit,
    NumPadKeyDelete,
    NumPadKeyClear,
    NumPadKeyEnter,
} NumPadKeyType;

typedef struct {
    NumPadKeyType type;
    const char* label;
    uint8_t digit;
} NumPadKey;

static const NumPadKey numpad_row_0[] = {
    {NumPadKeyDigit, "1", 1},
    {NumPadKeyDigit, "2", 2},
    {NumPadKeyDigit, "3", 3},
    {NumPadKeyDigit, "4", 4},
    {NumPadKeyDigit, "5", 5},
};

static const NumPadKey numpad_row_1[] = {
    {NumPadKeyDigit, "6", 6},
    {NumPadKeyDigit, "7", 7},
    {NumPadKeyDigit, "8", 8},
    {NumPadKeyDigit, "9", 9},
    {NumPadKeyDigit, "0", 0},
};

static const NumPadKey numpad_row_2[] = {
    {NumPadKeyDelete, "DEL", 0},
    {NumPadKeyClear, "CLR", 0},
    {NumPadKeyEnter, "OK", 0},
};

static const NumPadKey* const numpad_rows[NUMPAD_ROWS] = {
    numpad_row_0,
    numpad_row_1,
    numpad_row_2,
};

static const uint8_t numpad_row_sizes[NUMPAD_ROWS] = {
    COUNT_OF(numpad_row_0),
    COUNT_OF(numpad_row_1),
    COUNT_OF(numpad_row_2),
};

typedef struct {
    const char* header;
    uint32_t value;
    uint32_t max_value;
    bool fresh; // The value has not been touched yet, so the next digit replaces it
    uint8_t row;
    uint8_t col;
} NumPadModel;

struct NumPad {
    View* view;
    NumPadCallback callback;
    void* context;
};

// Keeps the cursor roughly in the same place when moving between rows of different length
static uint8_t numpad_map_column(uint8_t col, uint8_t from_size, uint8_t to_size) {
    uint8_t mapped = ((col * 2 + 1) * to_size) / (from_size * 2);
    return mapped < to_size ? mapped : to_size - 1;
}

static void numpad_draw_callback(Canvas* canvas, void* _model) {
    NumPadModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, NUMPAD_MARGIN_X, 8, model->header ? model->header : "");

    // Value, highlighted while a typed digit would still replace it
    char text[16];
    snprintf(text, sizeof(text), "%lu", model->value);
    if(model->fresh) {
        canvas_draw_box(canvas, NUMPAD_MARGIN_X, 11, NUMPAD_WIDTH, 14);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, NUMPAD_MARGIN_X, 11, NUMPAD_WIDTH, 14);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, NUMPAD_MARGIN_X + NUMPAD_WIDTH - 4, 18, AlignRight, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);

    // Keys
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t row = 0; row < NUMPAD_ROWS; row++) {
        uint8_t size = numpad_row_sizes[row];
        uint8_t key_w = NUMPAD_WIDTH / size;
        uint8_t y = NUMPAD_KEYS_Y + row * NUMPAD_ROW_STEP;

        for(uint8_t col = 0; col < size; col++) {
            uint8_t x = NUMPAD_MARGIN_X + col * key_w;
            bool selected = (row == model->row && col == model->col);

            if(selected) {
                canvas_draw_box(canvas, x, y, key_w - 1, NUMPAD_KEY_H);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_draw_frame(canvas, x, y, key_w - 1, NUMPAD_KEY_H);
            }
            canvas_draw_str_aligned(
                canvas,
                x + (key_w - 1) / 2,
                y + NUMPAD_KEY_H / 2 + 1,
                AlignCenter,
                AlignCenter,
                numpad_rows[row][col].label);
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

// Applies the selected key to the model. Returns true if OK was pressed.
static bool numpad_press_key(NumPadModel* model) {
    const NumPadKey* key = &numpad_rows[model->row][model->col];

    switch(key->type) {
    case NumPadKeyDigit: {
        uint64_t base = model->fresh ? 0 : model->value;
        uint64_t next = base * 10 + key->digit;
        if(next <= model->max_value) {
            model->value = (uint32_t)next;
            model->fresh = false;
        }
        break;
    }
    case NumPadKeyDelete:
        // On the untouched value DEL clears it, afterwards it removes the last digit
        model->value = model->fresh ? 0 : model->value / 10;
        model->fresh = false;
        break;
    case NumPadKeyClear:
        model->value = 0;
        model->fresh = false;
        break;
    case NumPadKeyEnter:
        return true;
    }

    return false;
}

static bool numpad_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    NumPad* numpad = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;
    bool done = false;
    uint32_t result = 0;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
    case InputKeyRight:
        with_view_model(
            numpad->view,
            NumPadModel * model,
            {
                if(event->key == InputKeyUp && model->row > 0) {
                    uint8_t from = numpad_row_sizes[model->row];
                    model->row--;
                    model->col = numpad_map_column(model->col, from, numpad_row_sizes[model->row]);
                } else if(event->key == InputKeyDown && model->row < NUMPAD_ROWS - 1) {
                    uint8_t from = numpad_row_sizes[model->row];
                    model->row++;
                    model->col = numpad_map_column(model->col, from, numpad_row_sizes[model->row]);
                } else if(event->key == InputKeyLeft) {
                    uint8_t size = numpad_row_sizes[model->row];
                    model->col = model->col > 0 ? model->col - 1 : size - 1;
                } else if(event->key == InputKeyRight) {
                    uint8_t size = numpad_row_sizes[model->row];
                    model->col = model->col + 1 < size ? model->col + 1 : 0;
                }
            },
            true);
        break;

    case InputKeyOk:
        if(event->type != InputTypeShort) break;
        with_view_model(
            numpad->view,
            NumPadModel * model,
            {
                done = numpad_press_key(model);
                result = model->value;
            },
            true);
        break;

    default:
        // BACK and anything else: leave it to the view dispatcher
        consumed = false;
        break;
    }

    if(done && numpad->callback) {
        numpad->callback(numpad->context, result);
    }

    return consumed;
}

NumPad* numpad_alloc(void) {
    NumPad* numpad = malloc(sizeof(NumPad));
    numpad->callback = NULL;
    numpad->context = NULL;

    numpad->view = view_alloc();
    view_set_context(numpad->view, numpad);
    view_allocate_model(numpad->view, ViewModelTypeLocking, sizeof(NumPadModel));
    view_set_draw_callback(numpad->view, numpad_draw_callback);
    view_set_input_callback(numpad->view, numpad_input_callback);

    with_view_model(
        numpad->view,
        NumPadModel * model,
        {
            model->header = NULL;
            model->value = 0;
            model->max_value = 0;
            model->fresh = false;
            model->row = 0;
            model->col = 0;
        },
        false);

    return numpad;
}

void numpad_free(NumPad* numpad) {
    furi_check(numpad);
    view_free_model(numpad->view);
    view_free(numpad->view);
    free(numpad);
}

View* numpad_get_view(NumPad* numpad) {
    furi_check(numpad);
    return numpad->view;
}

void numpad_setup(
    NumPad* numpad,
    const char* header,
    uint32_t value,
    uint32_t max_value,
    NumPadCallback callback,
    void* context) {
    furi_check(numpad);

    numpad->callback = callback;
    numpad->context = context;

    with_view_model(
        numpad->view,
        NumPadModel * model,
        {
            model->header = header;
            model->max_value = max_value;
            model->value = value > max_value ? max_value : value;
            model->fresh = true;
            // Start on the first key
            model->row = 0;
            model->col = 0;
        },
        true);
}
