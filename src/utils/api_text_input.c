/*
 * Extended text input keyboard for Flipper API Caller.
 * Based on the custom keyboard of "UART Terminal" by Malik cool4uma
 * (https://github.com/cool4uma/UART_Terminal), MIT License.
 * Copyright (c) 2023 Malik cool4uma
 *
 * Changes vs the original: renamed to api_text_input, removed the AT-command
 * mode, and the typed text is shown on two lines (URLs and headers wrap).
 */

#include "api_text_input.h"

#include <furi.h>
#include <gui/elements.h>

#include <api_caller_icons.h>

struct ApiTextInput {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} ApiTextInputKey;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    bool clear_default_text;

    ApiTextInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;

    bool edit_mode; // Cursor editing inside the typed text (UP from top row)
    size_t cursor_pos; // Insert/delete position in text_buffer
    size_t window_start; // First visible character while editing

    ApiTextInputValidatorCallback validator_callback;
    void* validator_callback_context;
    FuriString* validator_text;
    bool validator_message_visible;
} ApiTextInputModel;

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 4;

#define ENTER_KEY     '\r'
#define BACKSPACE_KEY '\b'

static const ApiTextInputKey keyboard_keys_row_1[] = {
    {'{', 1, 0},
    {'(', 9, 0},
    {'[', 17, 0},
    {'|', 25, 0},
    {'@', 33, 0},
    {'&', 41, 0},
    {'#', 49, 0},
    {';', 57, 0},
    {'^', 65, 0},
    {'*', 73, 0},
    {'`', 81, 0},
    {'"', 89, 0},
    {'~', 97, 0},
    {'\'', 105, 0},
    {'.', 113, 0},
    {'/', 120, 0},
};

static const ApiTextInputKey keyboard_keys_row_2[] = {
    {'q', 1, 10},
    {'w', 9, 10},
    {'e', 17, 10},
    {'r', 25, 10},
    {'t', 33, 10},
    {'y', 41, 10},
    {'u', 49, 10},
    {'i', 57, 10},
    {'o', 65, 10},
    {'p', 73, 10},
    {'0', 81, 10},
    {'1', 89, 10},
    {'2', 97, 10},
    {'3', 105, 10},
    {'=', 113, 10},
    {'-', 120, 10},
};

static const ApiTextInputKey keyboard_keys_row_3[] = {
    {'a', 1, 21},
    {'s', 9, 21},
    {'d', 18, 21},
    {'f', 25, 21},
    {'g', 33, 21},
    {'h', 41, 21},
    {'j', 49, 21},
    {'k', 57, 21},
    {'l', 65, 21},
    {BACKSPACE_KEY, 72, 13},
    {'4', 89, 21},
    {'5', 97, 21},
    {'6', 105, 21},
    {'$', 113, 21},
    {'%', 120, 21},

};

static const ApiTextInputKey keyboard_keys_row_4[] = {
    {'z', 1, 33},
    {'x', 9, 33},
    {'c', 18, 33},
    {'v', 25, 33},
    {'b', 33, 33},
    {'n', 41, 33},
    {'m', 49, 33},
    {'_', 57, 33},
    {ENTER_KEY, 64, 24},
    {'7', 89, 33},
    {'8', 97, 33},
    {'9', 105, 33},
    {'!', 113, 33},
    {'+', 120, 33},
};

static uint8_t get_row_size(uint8_t row_index) {
    uint8_t row_size = 0;

    switch(row_index + 1) {
    case 1:
        row_size = sizeof(keyboard_keys_row_1) / sizeof(ApiTextInputKey);
        break;
    case 2:
        row_size = sizeof(keyboard_keys_row_2) / sizeof(ApiTextInputKey);
        break;
    case 3:
        row_size = sizeof(keyboard_keys_row_3) / sizeof(ApiTextInputKey);
        break;
    case 4:
        row_size = sizeof(keyboard_keys_row_4) / sizeof(ApiTextInputKey);
        break;
    }

    return row_size;
}

static const ApiTextInputKey* get_row(uint8_t row_index) {
    const ApiTextInputKey* row = NULL;

    switch(row_index + 1) {
    case 1:
        row = keyboard_keys_row_1;
        break;
    case 2:
        row = keyboard_keys_row_2;
        break;
    case 3:
        row = keyboard_keys_row_3;
        break;
    case 4:
        row = keyboard_keys_row_4;
        break;
    }

    return row;
}

static char get_selected_char(ApiTextInputModel* model) {
    return get_row(model->selected_row)[model->selected_column].text;
}

static bool char_is_lowercase(char letter) {
    return (letter >= 0x61 && letter <= 0x7A);
}

static char char_to_uppercase(const char letter) {
    switch(letter) {
    case '_':
        return 0x20;
        break;
    case '(':
        return 0x29;
        break;
    case '{':
        return 0x7d;
        break;
    case '[':
        return 0x5d;
        break;
    case '/':
        return 0x5c;
        break;
    case ';':
        return 0x3a;
        break;
    case '.':
        return 0x2c;
        break;
    case '!':
        return 0x3f;
        break;
    case '<':
        return 0x3e;
        break;
    }
    if(char_is_lowercase(letter)) {
        return (letter - 0x20);
    } else {
        return letter;
    }
}

static void api_text_input_backspace_cb(ApiTextInputModel* model) {
    if(model->clear_default_text) {
        // Backspace clears the whole prefilled text
        model->text_buffer[0] = '\0';
        model->cursor_pos = 0;
    } else if(model->cursor_pos > 0) {
        size_t text_length = strlen(model->text_buffer);
        if(model->cursor_pos > text_length) {
            model->cursor_pos = text_length;
        }
        if(text_length > 0) {
            // Delete the character before the cursor
            memmove(
                model->text_buffer + model->cursor_pos - 1,
                model->text_buffer + model->cursor_pos,
                text_length - model->cursor_pos + 1);
            model->cursor_pos--;
        }
    }
}

static void api_text_input_view_draw_callback(Canvas* canvas, void* _model) {
    ApiTextInputModel* model = _model;
    uint8_t needed_string_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;

    const char* text = model->text_buffer ? model->text_buffer : "";
    size_t text_length = strlen(text);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Two-line input frame at the top of the screen (no title row)
    elements_slightly_rounded_frame(canvas, 1, 0, 126, 20);

    if(model->cursor_pos > text_length) {
        model->cursor_pos = text_length;
    }

    char line1[64];
    char line2[64];
    const char* tail = text;
    uint8_t tail_offset = 0;
    bool line1_ellipsis = false;
    uint16_t line1_width = 0;
    uint16_t line2_width = 0;
    bool edit_mode = model->edit_mode;
    bool edit_cursor_line2 = false;
    uint16_t edit_cursor_x = 0;

    if(edit_mode) {
        // Edit mode: the cursor moves inside the text; the window only
        // scrolls when the cursor leaves it (left or right edge).
        size_t line1_end;
        size_t line2_end;
        bool remeasure = true;
        while(remeasure) {
            remeasure = false;
            if(model->cursor_pos < model->window_start) {
                model->window_start = model->cursor_pos;
            }

            line1_end = model->window_start;
            line1_width = 0;
            while(line1_end < text_length &&
                  line1_width + canvas_glyph_width(canvas, (uint16_t)text[line1_end]) <=
                      needed_string_width) {
                line1_width += canvas_glyph_width(canvas, (uint16_t)text[line1_end]);
                line1_end++;
            }

            line2_end = line1_end;
            line2_width = 0;
            while(line2_end < text_length &&
                  line2_width + canvas_glyph_width(canvas, (uint16_t)text[line2_end]) <=
                      needed_string_width) {
                line2_width += canvas_glyph_width(canvas, (uint16_t)text[line2_end]);
                line2_end++;
            }

            if(model->cursor_pos > line2_end) {
                // Keep the cursor at the right edge of the window
                model->window_start += model->cursor_pos - line2_end;
                remeasure = true;
            }
        }

        line1_ellipsis = (model->window_start > 0);

        size_t line1_len = line1_end - model->window_start;
        if(line1_len >= sizeof(line1)) {
            line1_len = sizeof(line1) - 1;
        }
        memcpy(line1, text + model->window_start, line1_len);
        line1[line1_len] = '\0';

        size_t line2_len = line2_end - line1_end;
        if(line2_len >= sizeof(line2)) {
            line2_len = sizeof(line2) - 1;
        }
        memcpy(line2, text + line1_end, line2_len);
        line2[line2_len] = '\0';
        tail = line2;

        // Cursor marker position inside the visible text
        uint16_t prefix_width = 0;
        size_t pos;
        if(model->cursor_pos <= line1_end) {
            for(pos = model->window_start; pos < model->cursor_pos; pos++) {
                prefix_width += canvas_glyph_width(canvas, (uint16_t)text[pos]);
            }
            edit_cursor_x = start_pos + (line1_ellipsis ? 10 : 0) + prefix_width;
        } else {
            for(pos = line1_end; pos < model->cursor_pos; pos++) {
                prefix_width += canvas_glyph_width(canvas, (uint16_t)text[pos]);
            }
            edit_cursor_x = start_pos + prefix_width;
            edit_cursor_line2 = true;
        }
    } else {
        // Line 1: the buffer prefix that fits
        while(*tail != '\0' &&
              line1_width + canvas_glyph_width(canvas, (uint16_t)*tail) <= needed_string_width) {
            line1_width += canvas_glyph_width(canvas, (uint16_t)*tail);
            tail++;
        }
        size_t line1_len = (size_t)(tail - text);
        if(line1_len >= sizeof(line1)) {
            line1_len = sizeof(line1) - 1;
        }
        memcpy(line1, text, line1_len);
        line1[line1_len] = '\0';

        // Keep the end of the second line visible with a leading ellipsis
        if(canvas_string_width(canvas, tail) > needed_string_width) {
            tail_offset = 10;
            while(*tail != '\0' &&
                  tail_offset + canvas_string_width(canvas, tail) > needed_string_width) {
                tail++;
            }
        }
    }

    if(model->clear_default_text && !edit_mode) {
        // Highlight the text that will be replaced by the first key press
        elements_slightly_rounded_box(canvas, start_pos - 1, 4, line1_width + 2, 8);
        if(*tail != '\0') {
            uint16_t tail_width = canvas_string_width(canvas, tail);
            elements_slightly_rounded_box(
                canvas, start_pos - 1, 12, tail_offset + tail_width + 2, 8);
        }
        canvas_set_color(canvas, ColorWhite);
    }

    if(line1_ellipsis) {
        canvas_draw_str(canvas, start_pos, 9, "...");
        canvas_draw_str(canvas, start_pos + 10, 9, line1);
    } else {
        canvas_draw_str(canvas, start_pos, 9, line1);
    }

    if(tail_offset > 0) {
        canvas_draw_str(canvas, start_pos, 18, "...");
    }
    canvas_draw_str(canvas, start_pos + tail_offset, 18, tail);

    if(model->clear_default_text && !edit_mode) {
        canvas_set_color(canvas, ColorBlack);
    } else if(edit_mode) {
        // Cursor at its position inside the visible text
        if(edit_cursor_line2) {
            canvas_draw_str(canvas, edit_cursor_x, 18, "|");
            canvas_draw_str(canvas, edit_cursor_x + 1, 18, "|");
        } else {
            canvas_draw_str(canvas, edit_cursor_x, 9, "|");
            canvas_draw_str(canvas, edit_cursor_x + 1, 9, "|");
        }
    } else if(*tail != '\0') {
        // Cursor after the last visible character of the second line
        uint16_t cursor_x = start_pos + tail_offset + canvas_string_width(canvas, tail);
        canvas_draw_str(canvas, cursor_x + 1, 18, "|");
        canvas_draw_str(canvas, cursor_x + 2, 18, "|");
    } else {
        // Everything fits on the first line
        canvas_draw_str(canvas, start_pos + line1_width + 1, 9, "|");
        canvas_draw_str(canvas, start_pos + line1_width + 2, 9, "|");
    }

    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row <= keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(row);
        const ApiTextInputKey* keys = get_row(row);

        for(size_t column = 0; column < column_count; column++) {
            if(keys[column].text == ENTER_KEY) {
                canvas_set_color(canvas, ColorBlack);
                if(model->selected_row == row && model->selected_column == column) {
                    canvas_draw_icon(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        &I_KeySaveSelected_24x11);
                } else {
                    canvas_draw_icon(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        &I_KeySave_24x11);
                }
            } else if(keys[column].text == BACKSPACE_KEY) {
                canvas_set_color(canvas, ColorBlack);
                if(model->selected_row == row && model->selected_column == column) {
                    canvas_draw_icon(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        &I_KeyBackspaceSelected_16x9);
                } else {
                    canvas_draw_icon(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        &I_KeyBackspace_16x9);
                }
            } else {
                if(model->selected_row == row && model->selected_column == column) {
                    canvas_set_color(canvas, ColorBlack);
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 1,
                        keyboard_origin_y + keys[column].y - 8,
                        7,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }
                canvas_draw_glyph(
                    canvas,
                    keyboard_origin_x + keys[column].x,
                    keyboard_origin_y + keys[column].y,
                    keys[column].text);
            }
        }
    }
    if(model->validator_message_visible) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 8, 10, 110, 48);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, 10, 14, &I_WarningDolphin_45x42);
        canvas_draw_rframe(canvas, 8, 8, 112, 50, 3);
        canvas_draw_rframe(canvas, 9, 9, 110, 48, 2);
        elements_multiline_text(canvas, 62, 20, furi_string_get_cstr(model->validator_text));
        canvas_set_font(canvas, FontKeyboard);
    }
}

static void api_text_input_handle_up(ApiTextInput* api_text_input, ApiTextInputModel* model) {
    UNUSED(api_text_input);
    if(model->selected_row > 0) {
        model->selected_row--;
        if(model->selected_column > get_row_size(model->selected_row) - 6) {
            model->selected_column = model->selected_column + 1;
        }
    }
}

static void api_text_input_handle_down(ApiTextInput* api_text_input, ApiTextInputModel* model) {
    UNUSED(api_text_input);
    if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        if(model->selected_column > get_row_size(model->selected_row) - 4) {
            model->selected_column = model->selected_column - 1;
        }
    }
}

static void api_text_input_handle_left(ApiTextInput* api_text_input, ApiTextInputModel* model) {
    UNUSED(api_text_input);
    if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column = get_row_size(model->selected_row) - 1;
    }
}

static void api_text_input_handle_right(ApiTextInput* api_text_input, ApiTextInputModel* model) {
    UNUSED(api_text_input);
    if(model->selected_column < get_row_size(model->selected_row) - 1) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
}

static void
    api_text_input_handle_ok(ApiTextInput* api_text_input, ApiTextInputModel* model, bool shift) {
    char selected = get_selected_char(model);
    uint8_t text_length = strlen(model->text_buffer);

    // Long-press OK gives the shifted (uppercase) variant of the key
    if(shift) {
        selected = char_to_uppercase(selected);
    }

    if(selected == ENTER_KEY) {
        if(model->validator_callback &&
           (!model->validator_callback(
               model->text_buffer, model->validator_text, model->validator_callback_context))) {
            model->validator_message_visible = true;
            furi_timer_start(api_text_input->timer, furi_kernel_get_tick_frequency() * 4);
        } else if(model->callback != 0 && text_length > 0) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        api_text_input_backspace_cb(model);
    } else {
        if(model->clear_default_text) {
            model->text_buffer[0] = '\0';
            text_length = 0;
            model->cursor_pos = 0;
        }
        if(text_length < (model->text_buffer_size - 1)) {
            if(model->cursor_pos > text_length) {
                model->cursor_pos = text_length;
            }
            // Insert the character at the cursor position
            memmove(
                model->text_buffer + model->cursor_pos + 1,
                model->text_buffer + model->cursor_pos,
                text_length - model->cursor_pos + 1);
            model->text_buffer[model->cursor_pos] = selected;
            model->cursor_pos++;
        }
    }
    model->clear_default_text = false;
}

static bool api_text_input_view_input_callback(InputEvent* event, void* context) {
    ApiTextInput* api_text_input = context;
    furi_assert(api_text_input);

    bool consumed = false;

    // Acquire model
    ApiTextInputModel* model = view_get_model(api_text_input->view);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       model->validator_message_visible) {
        model->validator_message_visible = false;
        consumed = true;
    } else if(model->edit_mode) {
        // Cursor editing: LEFT/RIGHT move the cursor, OK returns to the keys
        consumed = true;
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            size_t text_length = strlen(model->text_buffer);
            if(event->key == InputKeyLeft) {
                if(model->cursor_pos > 0) {
                    model->cursor_pos--;
                }
            } else if(event->key == InputKeyRight) {
                if(model->cursor_pos < text_length) {
                    model->cursor_pos++;
                }
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                model->edit_mode = false;
            } else {
                consumed = false;
            }
        } else {
            consumed = false;
        }
    } else if(event->type == InputTypeShort) {
        consumed = true;
        if(event->key == InputKeyUp && model->selected_row == 0) {
            // From the top key row, UP enters cursor editing of the typed text
            model->edit_mode = true;
            model->cursor_pos = strlen(model->text_buffer);
            model->window_start = 0;
        } else {
            switch(event->key) {
            case InputKeyUp:
                api_text_input_handle_up(api_text_input, model);
                break;
            case InputKeyDown:
                api_text_input_handle_down(api_text_input, model);
                break;
            case InputKeyLeft:
                api_text_input_handle_left(api_text_input, model);
                break;
            case InputKeyRight:
                api_text_input_handle_right(api_text_input, model);
                break;
            case InputKeyOk:
                api_text_input_handle_ok(api_text_input, model, false);
                break;
            default:
                consumed = false;
                break;
            }
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            api_text_input_handle_up(api_text_input, model);
            break;
        case InputKeyDown:
            api_text_input_handle_down(api_text_input, model);
            break;
        case InputKeyLeft:
            api_text_input_handle_left(api_text_input, model);
            break;
        case InputKeyRight:
            api_text_input_handle_right(api_text_input, model);
            break;
        case InputKeyOk:
            api_text_input_handle_ok(api_text_input, model, true);
            break;
        case InputKeyBack:
            api_text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            api_text_input_handle_up(api_text_input, model);
            break;
        case InputKeyDown:
            api_text_input_handle_down(api_text_input, model);
            break;
        case InputKeyLeft:
            api_text_input_handle_left(api_text_input, model);
            break;
        case InputKeyRight:
            api_text_input_handle_right(api_text_input, model);
            break;
        case InputKeyBack:
            api_text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    // Commit model
    view_commit_model(api_text_input->view, consumed);

    return consumed;
}

void api_text_input_timer_callback(void* context) {
    furi_assert(context);
    ApiTextInput* api_text_input = context;

    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        { model->validator_message_visible = false; },
        true);
}

ApiTextInput* api_text_input_alloc() {
    ApiTextInput* api_text_input = malloc(sizeof(ApiTextInput));
    api_text_input->view = view_alloc();
    view_set_context(api_text_input->view, api_text_input);
    view_allocate_model(api_text_input->view, ViewModelTypeLocking, sizeof(ApiTextInputModel));
    view_set_draw_callback(api_text_input->view, api_text_input_view_draw_callback);
    view_set_input_callback(api_text_input->view, api_text_input_view_input_callback);

    api_text_input->timer =
        furi_timer_alloc(api_text_input_timer_callback, FuriTimerTypeOnce, api_text_input);

    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        { model->validator_text = furi_string_alloc(); },
        false);

    api_text_input_reset(api_text_input);

    return api_text_input;
}

void api_text_input_free(ApiTextInput* api_text_input) {
    furi_assert(api_text_input);
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        { furi_string_free(model->validator_text); },
        false);

    // Send stop command
    furi_timer_stop(api_text_input->timer);
    // Release allocated memory
    furi_timer_free(api_text_input->timer);

    view_free(api_text_input->view);

    free(api_text_input);
}

void api_text_input_reset(ApiTextInput* api_text_input) {
    furi_assert(api_text_input);
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        {
            model->text_buffer_size = 0;
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->clear_default_text = false;
            model->edit_mode = false;
            model->cursor_pos = 0;
            model->window_start = 0;
            model->text_buffer = NULL;
            model->text_buffer_size = 0;
            model->callback = NULL;
            model->callback_context = NULL;
            model->validator_callback = NULL;
            model->validator_callback_context = NULL;
            furi_string_reset(model->validator_text);
            model->validator_message_visible = false;
        },
        true);
}

View* api_text_input_get_view(ApiTextInput* api_text_input) {
    furi_assert(api_text_input);
    return api_text_input->view;
}

void api_text_input_set_result_callback(
    ApiTextInput* api_text_input,
    ApiTextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            if(text_buffer && text_buffer[0] != '\0') {
                // Set focus on Save
                model->selected_row = 2;
                model->selected_column = 8;
                model->cursor_pos = strlen(text_buffer);
            }
        },
        true);
}

void api_text_input_set_validator(
    ApiTextInput* api_text_input,
    ApiTextInputValidatorCallback callback,
    void* callback_context) {
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        {
            model->validator_callback = callback;
            model->validator_callback_context = callback_context;
        },
        true);
}

ApiTextInputValidatorCallback api_text_input_get_validator_callback(ApiTextInput* api_text_input) {
    ApiTextInputValidatorCallback validator_callback = NULL;
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        { validator_callback = model->validator_callback; },
        false);
    return validator_callback;
}

void* api_text_input_get_validator_callback_context(ApiTextInput* api_text_input) {
    void* validator_callback_context = NULL;
    with_view_model(
        api_text_input->view,
        ApiTextInputModel * model,
        { validator_callback_context = model->validator_callback_context; },
        false);
    return validator_callback_context;
}

void api_text_input_set_header_text(ApiTextInput* api_text_input, const char* text) {
    with_view_model(
        api_text_input->view, ApiTextInputModel * model, { model->header = text; }, true);
}
