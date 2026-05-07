#include "nfc_tools_email_input.h"
#include <gui/elements.h>
#include <furi.h>

// ── Internal structures ────────────────────────────────────────────────────

struct EmailInput {
    View* view;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} EmailInputKey;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;

    EmailInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;
} EmailInputModel;

// ── Keyboard layout ────────────────────────────────────────────────────────
// Identical to standard TextInput, EXCEPT the 3rd row:
//   7  8  9  →  @  .  -   (more useful for an email address)

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 3;

#define ENTER_KEY     '\r'
#define BACKSPACE_KEY '\b'

static const EmailInputKey keyboard_keys_row_1[] = {
    {'q', 1, 8},
    {'w', 10, 8},
    {'e', 19, 8},
    {'r', 28, 8},
    {'t', 37, 8},
    {'y', 46, 8},
    {'u', 55, 8},
    {'i', 64, 8},
    {'o', 73, 8},
    {'p', 82, 8},
    {'0', 91, 8},
    {'1', 100, 8},
    {'2', 110, 8},
    {'3', 120, 8},
};

static const EmailInputKey keyboard_keys_row_2[] = {
    {'a', 1, 20},
    {'s', 10, 20},
    {'d', 19, 20},
    {'f', 28, 20},
    {'g', 37, 20},
    {'h', 46, 20},
    {'j', 55, 20},
    {'k', 64, 20},
    {'l', 73, 20},
    {BACKSPACE_KEY, 82, 12},
    {'4', 100, 20},
    {'5', 110, 20},
    {'6', 120, 20},
};

static const EmailInputKey keyboard_keys_row_3[] = {
    {'z', 1, 32},
    {'x', 10, 32},
    {'c', 19, 32},
    {'v', 28, 32},
    {'b', 37, 32},
    {'n', 46, 32},
    {'m', 55, 32},
    {'_', 64, 32},
    {ENTER_KEY, 74, 23},
    {'@', 100, 32},
    {'.', 110, 32},
    {'-', 120, 32},
};

// ── Helpers ────────────────────────────────────────────────────────────────
static uint8_t get_row_size(uint8_t row_index) {
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

static const EmailInputKey* get_row(uint8_t row_index) {
    switch(row_index + 1) {
    case 1:
        return keyboard_keys_row_1;
    case 2:
        return keyboard_keys_row_2;
    case 3:
        return keyboard_keys_row_3;
    default:
        furi_crash();
    }
}

static char get_selected_char(EmailInputModel* model) {
    return get_row(model->selected_row)[model->selected_column].text;
}

static bool char_is_lowercase(char letter) {
    return letter >= 'a' && letter <= 'z';
}

static char char_to_uppercase(char letter) {
    if(letter == '_') return ' ';
    if(char_is_lowercase(letter)) return letter - 0x20;
    return letter; // @  .  -  digits: unchanged
}

// ── Special keys (Backspace and Enter) ────────────────────────────────────
// System icons are not exported to FAPs; we draw them manually.

static void draw_backspace_key(Canvas* canvas, uint8_t x, uint8_t y, bool selected) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, 16, 9);
        canvas_set_color(canvas, ColorWhite);
    }
    // Horizontal bar ←
    canvas_draw_line(canvas, x + 2, y + 4, x + 12, y + 4);
    // Left arrow tip
    canvas_draw_line(canvas, x + 2, y + 4, x + 5, y + 2);
    canvas_draw_line(canvas, x + 2, y + 4, x + 5, y + 6);
    // Vertical "delete" bar
    canvas_draw_line(canvas, x + 12, y + 2, x + 12, y + 6);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_enter_key(Canvas* canvas, uint8_t x, uint8_t y, bool selected) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, 24, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, 24, 11, 1);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + 4, y + 8, "OK");
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorBlack);
}

// ── Rendering ──────────────────────────────────────────────────────────────

static void email_input_view_draw_callback(Canvas* canvas, void* _model) {
    EmailInputModel* model = _model;
    uint8_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
    uint8_t needed_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;
    const char* text = model->text_buffer;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    if(canvas_string_width(canvas, text) > needed_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos += 6;
        needed_width -= 8;
    }
    while(text != NULL && canvas_string_width(canvas, text) > needed_width) {
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
        const uint8_t col_count = get_row_size(row);
        const EmailInputKey* keys = get_row(row);

        for(uint8_t col = 0; col < col_count; col++) {
            char key_char = keys[col].text;

            if(key_char == ENTER_KEY) {
                bool sel = (model->selected_row == row && model->selected_column == col);
                draw_enter_key(
                    canvas, keyboard_origin_x + keys[col].x, keyboard_origin_y + keys[col].y, sel);
            } else if(key_char == BACKSPACE_KEY) {
                bool sel = (model->selected_row == row && model->selected_column == col);
                draw_backspace_key(
                    canvas, keyboard_origin_x + keys[col].x, keyboard_origin_y + keys[col].y, sel);
            } else {
                bool selected = (model->selected_row == row && model->selected_column == col);
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[col].x - 1,
                        keyboard_origin_y + keys[col].y - 8,
                        7,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }

                char glyph;
                if(model->clear_default_text ||
                   (text_length == 0 && char_is_lowercase(key_char))) {
                    glyph = char_to_uppercase(key_char);
                } else {
                    glyph = key_char;
                }
                canvas_draw_glyph(
                    canvas,
                    keyboard_origin_x + keys[col].x,
                    keyboard_origin_y + keys[col].y,
                    glyph);
            }
        }
    }
}

// ── Input handling ─────────────────────────────────────────────────────────

static void email_input_backspace(EmailInputModel* model) {
    uint8_t len = model->clear_default_text ? 1 : (uint8_t)strlen(model->text_buffer);
    if(len > 0) model->text_buffer[len - 1] = '\0';
}

static void email_input_handle_ok(EmailInputModel* model, bool shift) {
    char selected = get_selected_char(model);
    size_t text_length = strlen(model->text_buffer);

    bool toggle_case = (text_length == 0 || model->clear_default_text);
    if(shift) toggle_case = !toggle_case;
    if(toggle_case) selected = char_to_uppercase(selected);

    if(selected == ENTER_KEY) {
        if(model->callback && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        email_input_backspace(model);
    } else {
        if(model->clear_default_text) text_length = 0;
        if(text_length < model->text_buffer_size - 1) {
            model->text_buffer[text_length] = selected;
            model->text_buffer[text_length + 1] = '\0';
        }
    }
    model->clear_default_text = false;
}

static bool email_input_view_input_callback(InputEvent* event, void* context) {
    EmailInput* ei = context;
    bool consumed = false;

    EmailInputModel* model = view_get_model(ei->view);

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            if(model->selected_row > 0) {
                model->selected_row--;
                if(model->selected_column > get_row_size(model->selected_row) - 6)
                    model->selected_column++;
            }
            break;
        case InputKeyDown:
            if(model->selected_row < keyboard_row_count - 1) {
                model->selected_row++;
                if(model->selected_column > get_row_size(model->selected_row) - 4)
                    model->selected_column--;
            }
            break;
        case InputKeyLeft:
            if(model->selected_column > 0) {
                model->selected_column--;
            } else {
                model->selected_column = get_row_size(model->selected_row) - 1;
            }
            break;
        case InputKeyRight:
            if(model->selected_column < get_row_size(model->selected_row) - 1) {
                model->selected_column++;
            } else {
                model->selected_column = 0;
            }
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat)
                email_input_handle_ok(model, event->type == InputTypeLong);
            break;
        case InputKeyBack:
            if(event->type == InputTypeLong || event->type == InputTypeRepeat)
                email_input_backspace(model);
            else
                consumed = false; // let the SceneManager handle the back event
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(ei->view, consumed);
    return consumed;
}

// ── Public API ─────────────────────────────────────────────────────────────

EmailInput* email_input_alloc(void) {
    EmailInput* ei = malloc(sizeof(EmailInput));
    ei->view = view_alloc();
    view_set_context(ei->view, ei);
    view_allocate_model(ei->view, ViewModelTypeLocking, sizeof(EmailInputModel));
    view_set_draw_callback(ei->view, email_input_view_draw_callback);
    view_set_input_callback(ei->view, email_input_view_input_callback);
    email_input_reset(ei);
    return ei;
}

void email_input_free(EmailInput* ei) {
    furi_check(ei);
    view_free(ei->view);
    free(ei);
}

void email_input_reset(EmailInput* ei) {
    furi_check(ei);
    with_view_model(
        ei->view,
        EmailInputModel * model,
        {
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->minimum_length = 1;
            model->clear_default_text = false;
            model->text_buffer = NULL;
            model->text_buffer_size = 0;
            model->callback = NULL;
            model->callback_context = NULL;
        },
        true);
}

View* email_input_get_view(EmailInput* ei) {
    furi_check(ei);
    return ei->view;
}

void email_input_set_header_text(EmailInput* ei, const char* text) {
    furi_check(ei);
    with_view_model(ei->view, EmailInputModel * model, { model->header = text; }, true);
}

void email_input_set_result_callback(
    EmailInput* ei,
    EmailInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    furi_check(ei);
    with_view_model(
        ei->view,
        EmailInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            if(text_buffer && text_buffer[0] != '\0') {
                // Position cursor on "Save"
                model->selected_row = 2;
                model->selected_column = 8;
            }
        },
        true);
}

void email_input_set_minimum_length(EmailInput* ei, size_t minimum_length) {
    with_view_model(
        ei->view, EmailInputModel * model, { model->minimum_length = minimum_length; }, true);
}
