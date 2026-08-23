#include "bv_text_input.h"

#include <gui/elements.h>
#include "biovault_icons.h" // generated from images/ (fap_icon_assets)
#include <furi.h>

struct BvTextInput {
    View* view;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} TextInputKey;

typedef struct {
    const TextInputKey* rows[3];
} Keyboard;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;

    BvTextInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;
    uint8_t selected_keyboard; // 0 = letters, 1 = symbols
} TextInputModel;

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 3;
static const uint8_t keyboard_count = 2;

#define ENTER_KEY           '\r'
#define BACKSPACE_KEY       '\b'
#define SWITCH_KEYBOARD_KEY 0xfe

// --- Letters keyboard ---
static const TextInputKey keyboard_keys_row_1[] = {
    {'q', 1, 8},   {'w', 10, 8},  {'e', 19, 8},  {'r', 28, 8}, {'t', 37, 8},
    {'y', 46, 8},  {'u', 55, 8},  {'i', 64, 8},  {'o', 73, 8}, {'p', 82, 8},
    {'0', 91, 8},  {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};
static const TextInputKey keyboard_keys_row_2[] = {
    {'a', 1, 20},  {'s', 10, 20}, {'d', 19, 20}, {'f', 28, 20},
    {'g', 37, 20}, {'h', 46, 20}, {'j', 55, 20}, {'k', 64, 20},
    {'l', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};
static const TextInputKey keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 1, 23},
    {'z', 18, 32}, {'x', 25, 32}, {'c', 32, 32}, {'v', 39, 32}, {'b', 46, 32},
    {'n', 53, 32}, {'m', 60, 32}, {'_', 67, 32}, {ENTER_KEY, 74, 23},
    {'7', 100, 32}, {'8', 110, 32}, {'9', 120, 32},
};

// --- Symbols keyboard (same grid as letters) ---
static const TextInputKey symbol_keyboard_keys_row_1[] = {
    {'!', 1, 8},  {'@', 10, 8},  {'#', 19, 8},  {'$', 28, 8}, {'%', 37, 8},
    {'^', 46, 8}, {'&', 55, 8},  {'*', 64, 8},  {'(', 73, 8}, {')', 82, 8},
    {'0', 91, 8}, {'1', 100, 8}, {'2', 110, 8}, {'3', 120, 8},
};
static const TextInputKey symbol_keyboard_keys_row_2[] = {
    {'/', 1, 20},  {'\\', 10, 20}, {'=', 19, 20}, {'+', 28, 20},
    {'{', 37, 20}, {'}', 46, 20},  {'[', 55, 20}, {']', 64, 20},
    {'@', 73, 20}, {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20}, {'5', 110, 20}, {'6', 120, 20},
};
static const TextInputKey symbol_keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 1, 23},
    {'-', 18, 32}, {':', 25, 32}, {';', 32, 32}, {'?', 39, 32}, {'<', 46, 32},
    {'>', 53, 32}, {',', 60, 32}, {'.', 67, 32}, {ENTER_KEY, 74, 23},
    {'7', 100, 32}, {'8', 110, 32}, {'9', 120, 32},
};

static const Keyboard keyboards[] = {
    {.rows = {keyboard_keys_row_1, keyboard_keys_row_2, keyboard_keys_row_3}},
    {.rows =
         {symbol_keyboard_keys_row_1,
          symbol_keyboard_keys_row_2,
          symbol_keyboard_keys_row_3}},
};

static uint8_t get_row_size(uint8_t row_index) {
    // Both keyboards share the same per-row key counts.
    switch(row_index + 1) {
    case 1:
        return COUNT_OF(keyboard_keys_row_1);
    case 2:
        return COUNT_OF(keyboard_keys_row_2);
    case 3:
        return COUNT_OF(keyboard_keys_row_3);
    default:
        furi_crash();
    }
}

static const TextInputKey* get_row(uint8_t keyboard_index, uint8_t row_index) {
    furi_check(keyboard_index < keyboard_count);
    furi_check(row_index < keyboard_row_count);
    return keyboards[keyboard_index].rows[row_index];
}

static char get_selected_char(TextInputModel* model) {
    return get_row(model->selected_keyboard, model->selected_row)[model->selected_column].text;
}

static bool char_is_lowercase(char letter) {
    return letter >= 0x61 && letter <= 0x7A;
}

static char char_to_uppercase(const char letter) {
    if(letter == '_') {
        return 0x20; // space
    } else if(islower((int)letter)) {
        return letter - 0x20;
    } else {
        return letter;
    }
}

static void switch_keyboard(TextInputModel* model) {
    model->selected_keyboard = (model->selected_keyboard + 1) % keyboard_count;
}

static void bv_text_input_backspace_cb(TextInputModel* model) {
    uint8_t text_length = model->clear_default_text ? 1 : strlen(model->text_buffer);
    if(text_length > 0) {
        model->text_buffer[text_length - 1] = 0;
    }
}

static void bv_text_input_view_draw_callback(Canvas* canvas, void* _model) {
    TextInputModel* model = _model;
    uint8_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
    uint8_t needed_string_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;
    const char* text = model->text_buffer;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    if(canvas_string_width(canvas, text) > needed_string_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos += 6;
        needed_string_width -= 8;
    }
    while(text != 0 && canvas_string_width(canvas, text) > needed_string_width) {
        text++;
    }

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14, canvas_string_width(canvas, text) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 1, 22, "|");
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 2, 22, "|");
    }
    canvas_draw_str(canvas, start_pos, 22, text);

    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row < keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(row);
        const TextInputKey* keys = get_row(model->selected_keyboard, row);

        for(size_t column = 0; column < column_count; column++) {
            bool selected = (model->selected_row == row) && (model->selected_column == column);
            uint8_t kx = keyboard_origin_x + keys[column].x;
            uint8_t ky = keyboard_origin_y + keys[column].y;

            if(keys[column].text == ENTER_KEY) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_icon(
                    canvas, kx, ky, selected ? &I_KeySaveSelected_24x11 : &I_KeySave_24x11);
            } else if(keys[column].text == BACKSPACE_KEY) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_icon(
                    canvas,
                    kx,
                    ky,
                    selected ? &I_KeyBackspaceSelected_16x9 : &I_KeyBackspace_16x9);
            } else if(keys[column].text == SWITCH_KEYBOARD_KEY) {
                // Drawn (no icon in 1.4.3): a small box labelled with the OTHER
                // keyboard ('#' from letters, 'ab' from symbols).
                canvas_set_color(canvas, ColorBlack);
                if(selected) {
                    canvas_draw_box(canvas, kx - 1, ky, 16, 9);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_draw_rframe(canvas, kx - 1, ky, 16, 9, 1);
                }
                const char* label = (model->selected_keyboard == 0) ? "#" : "ab";
                canvas_draw_str(canvas, kx + 2, ky + 7, label);
                canvas_set_color(canvas, ColorBlack);
            } else {
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_box(canvas, kx - 1, ky - 8, 7, 10);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }

                if(model->clear_default_text ||
                   (text_length == 0 && char_is_lowercase(keys[column].text))) {
                    canvas_draw_glyph(canvas, kx, ky, char_to_uppercase(keys[column].text));
                } else {
                    canvas_draw_glyph(canvas, kx, ky, keys[column].text);
                }
            }
        }
    }
}

static void bv_text_input_handle_up(TextInputModel* model) {
    if(model->selected_row > 0) {
        model->selected_row--;
        if(model->selected_column > get_row_size(model->selected_row) - 6) {
            model->selected_column = model->selected_column + 1;
        }
    }
}

static void bv_text_input_handle_down(TextInputModel* model) {
    if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        if(model->selected_column > get_row_size(model->selected_row) - 4) {
            model->selected_column = model->selected_column - 1;
        }
    }
}

static void bv_text_input_handle_left(TextInputModel* model) {
    if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column = get_row_size(model->selected_row) - 1;
    }
}

static void bv_text_input_handle_right(TextInputModel* model) {
    if(model->selected_column < get_row_size(model->selected_row) - 1) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
}

static void bv_text_input_handle_ok(TextInputModel* model, bool shift) {
    char selected = get_selected_char(model);

    if(selected == SWITCH_KEYBOARD_KEY) {
        switch_keyboard(model);
        return;
    }

    size_t text_length = strlen(model->text_buffer);
    bool toggle_case = text_length == 0 || model->clear_default_text;
    if(shift) toggle_case = !toggle_case;
    if(toggle_case) {
        selected = char_to_uppercase(selected);
    }

    if(selected == ENTER_KEY) {
        if(model->callback != 0 && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        bv_text_input_backspace_cb(model);
    } else {
        if(model->clear_default_text) {
            text_length = 0;
        }
        if(text_length < (model->text_buffer_size - 1)) {
            model->text_buffer[text_length] = selected;
            model->text_buffer[text_length + 1] = 0;
        }
    }
    model->clear_default_text = false;
}

static bool bv_text_input_view_input_callback(InputEvent* event, void* context) {
    BvTextInput* text_input = context;
    furi_assert(text_input);
    bool consumed = false;

    TextInputModel* model = view_get_model(text_input->view);

    if(event->type == InputTypeShort) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bv_text_input_handle_up(model);
            break;
        case InputKeyDown:
            bv_text_input_handle_down(model);
            break;
        case InputKeyLeft:
            bv_text_input_handle_left(model);
            break;
        case InputKeyRight:
            bv_text_input_handle_right(model);
            break;
        case InputKeyOk:
            bv_text_input_handle_ok(model, false);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bv_text_input_handle_up(model);
            break;
        case InputKeyDown:
            bv_text_input_handle_down(model);
            break;
        case InputKeyLeft:
            bv_text_input_handle_left(model);
            break;
        case InputKeyRight:
            bv_text_input_handle_right(model);
            break;
        case InputKeyOk:
            bv_text_input_handle_ok(model, true);
            break;
        case InputKeyBack:
            bv_text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bv_text_input_handle_up(model);
            break;
        case InputKeyDown:
            bv_text_input_handle_down(model);
            break;
        case InputKeyLeft:
            bv_text_input_handle_left(model);
            break;
        case InputKeyRight:
            bv_text_input_handle_right(model);
            break;
        case InputKeyBack:
            bv_text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(text_input->view, consumed);
    return consumed;
}

BvTextInput* bv_text_input_alloc(void) {
    BvTextInput* text_input = malloc(sizeof(BvTextInput));
    text_input->view = view_alloc();
    view_set_context(text_input->view, text_input);
    view_allocate_model(text_input->view, ViewModelTypeLocking, sizeof(TextInputModel));
    view_set_draw_callback(text_input->view, bv_text_input_view_draw_callback);
    view_set_input_callback(text_input->view, bv_text_input_view_input_callback);

    bv_text_input_reset(text_input);
    return text_input;
}

void bv_text_input_free(BvTextInput* text_input) {
    furi_check(text_input);
    view_free(text_input->view);
    free(text_input);
}

void bv_text_input_reset(BvTextInput* text_input) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->selected_keyboard = 0;
            model->minimum_length = 1;
            model->clear_default_text = false;
            model->text_buffer = NULL;
            model->text_buffer_size = 0;
            model->callback = NULL;
            model->callback_context = NULL;
        },
        true);
}

View* bv_text_input_get_view(BvTextInput* text_input) {
    furi_check(text_input);
    return text_input->view;
}

void bv_text_input_set_result_callback(
    BvTextInput* text_input,
    BvTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->selected_keyboard = 0;
            if(text_buffer && text_buffer[0] != '\0') {
                // Focus on the Save (enter) key of the letters keyboard.
                model->selected_row = 2;
                model->selected_column = 9;
            } else {
                model->selected_row = 0;
                model->selected_column = 0;
            }
        },
        true);
}

void bv_text_input_set_minimum_length(BvTextInput* text_input, size_t minimum_length) {
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { model->minimum_length = minimum_length; },
        true);
}

void bv_text_input_set_header_text(BvTextInput* text_input, const char* text) {
    furi_check(text_input);
    with_view_model(text_input->view, TextInputModel * model, { model->header = text; }, true);
}
