#include "../lib/bunnyconnect_keyboard.h"
#include <gui/elements.h>
#include <gui/modules/widget.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <gui/icon_i.h>

struct BunnyConnectKeyboard {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} BunnyConnectKeyboardKey;

typedef struct {
    const BunnyConnectKeyboardKey* rows[3];
    const uint8_t keyboard_index;
} Keyboard;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;

    bool cursor_select;
    size_t cursor_pos;

    BunnyConnectKeyboardCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;
    uint8_t selected_keyboard;

    BunnyConnectKeyboardValidatorCallback validator_callback;
    void* validator_callback_context;
    FuriString* validator_text;
    bool validator_message_visible;
} BunnyConnectKeyboardModel;

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 3;
static const uint8_t keyboard_count = 2;

#define ENTER_KEY           '\r'
#define BACKSPACE_KEY       '\b'
#define SWITCH_KEYBOARD_KEY 0xfe

static const BunnyConnectKeyboardKey keyboard_keys_row_1[] = {
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

static const BunnyConnectKeyboardKey keyboard_keys_row_2[] = {
    {'a', 1, 20},
    {'s', 10, 20},
    {'d', 19, 20},
    {'f', 28, 20},
    {'g', 37, 20},
    {'h', 46, 20},
    {'j', 55, 20},
    {'k', 64, 20},
    {'l', 73, 20},
    {BACKSPACE_KEY, 82, 22}, // Moved from 12 to 32 (down 20 pixels)
    {'4', 100, 20},
    {'5', 110, 20},
    {'6', 120, 20},
};

static const BunnyConnectKeyboardKey keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 1, 33}, // Moved from 23 to 43 (down 20 pixels)
    {'z', 13, 32},
    {'x', 21, 32},
    {'c', 28, 32},
    {'v', 36, 32},
    {'b', 44, 32},
    {'n', 52, 32},
    {'m', 59, 32},
    {'_', 67, 32},
    {ENTER_KEY, 74, 33}, // Moved from 23 to 43 (down 20 pixels)
    {'7', 100, 32},
    {'8', 110, 32},
    {'9', 120, 32},
};

static const BunnyConnectKeyboardKey symbol_keyboard_keys_row_1[] = {
    {'!', 2, 8},
    {'@', 12, 8},
    {'#', 22, 8},
    {'$', 32, 8},
    {'%', 42, 8},
    {'^', 52, 8},
    {'&', 62, 8},
    {'(', 71, 8},
    {')', 81, 8},
    {'0', 91, 8},
    {'1', 100, 8},
    {'2', 110, 8},
    {'3', 120, 8},
};

static const BunnyConnectKeyboardKey symbol_keyboard_keys_row_2[] = {
    {'~', 2, 20},
    {'+', 12, 20},
    {'-', 22, 20},
    {'=', 32, 20},
    {'[', 42, 20},
    {']', 52, 20},
    {'{', 62, 20},
    {'}', 72, 20},
    {BACKSPACE_KEY, 82, 22}, // Moved from 12 to 32 (down 20 pixels)
    {'4', 100, 20},
    {'5', 110, 20},
    {'6', 120, 20},
};

static const BunnyConnectKeyboardKey symbol_keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 1, 33}, // Moved from 23 to 43 (down 20 pixels)
    {'.', 15, 32},
    {',', 29, 32},
    {';', 41, 32},
    {'`', 53, 32},
    {'\'', 65, 32},
    {ENTER_KEY, 74, 33}, // Moved from 23 to 43 (down 20 pixels)
    {'7', 100, 32},
    {'8', 110, 32},
    {'9', 120, 32},
};

static const Keyboard keyboard = {
    .rows =
        {
            keyboard_keys_row_1,
            keyboard_keys_row_2,
            keyboard_keys_row_3,
        },
    .keyboard_index = 0,
};

static const Keyboard symbol_keyboard = {
    .rows =
        {
            symbol_keyboard_keys_row_1,
            symbol_keyboard_keys_row_2,
            symbol_keyboard_keys_row_3,
        },
    .keyboard_index = 1,
};

static const Keyboard* keyboards[] = {
    &keyboard,
    &symbol_keyboard,
};

static void switch_keyboard(BunnyConnectKeyboardModel* model) {
    model->selected_keyboard = (model->selected_keyboard + 1) % keyboard_count;
}

static uint8_t get_row_size(const Keyboard* keyboard, uint8_t row_index) {
    uint8_t row_size = 0;
    if(keyboard == &symbol_keyboard) {
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(symbol_keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(symbol_keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(symbol_keyboard_keys_row_3);
            break;
        default:
            furi_crash(NULL);
        }
    } else {
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(keyboard_keys_row_3);
            break;
        default:
            furi_crash(NULL);
        }
    }
    return row_size;
}

static const BunnyConnectKeyboardKey* get_row(const Keyboard* keyboard, uint8_t row_index) {
    const BunnyConnectKeyboardKey* row = NULL;
    if(row_index < 3) {
        row = keyboard->rows[row_index];
    } else {
        furi_crash(NULL);
    }
    return row;
}

static char get_selected_char(BunnyConnectKeyboardModel* model) {
    return get_row(
               keyboards[model->selected_keyboard], model->selected_row)[model->selected_column]
        .text;
}

static bool char_is_lowercase(char letter) {
    return (letter >= 0x61 && letter <= 0x7A);
}

static char char_to_uppercase(const char letter) {
    if(letter == '_') {
        return 0x20;
    } else if(char_is_lowercase(letter)) {
        return (letter - 0x20);
    } else {
        return letter;
    }
}

static void bunnyconnect_keyboard_backspace_cb(BunnyConnectKeyboardModel* model) {
    if(model == NULL || model->text_buffer == NULL) return;

    if(model->clear_default_text) {
        if(model->text_buffer_size > 0) {
            model->text_buffer[0] = 0;
        }
        model->cursor_pos = 0;
    } else if(model->cursor_pos > 0) {
        char* move = model->text_buffer + model->cursor_pos;
        if(move && strlen(move) + 1 <= model->text_buffer_size - model->cursor_pos + 1) {
            memmove(move - 1, move, strlen(move) + 1);
            model->cursor_pos--;
        }
    }
}

static void bunnyconnect_keyboard_view_draw_callback(Canvas* canvas, void* _model) {
    BunnyConnectKeyboardModel* model = _model;
    uint8_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
    uint8_t needed_string_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;

    model->cursor_pos = model->cursor_pos > text_length ? text_length : model->cursor_pos;
    size_t cursor_pos = model->cursor_pos;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    char buf[model->text_buffer_size + 1];
    if(model->text_buffer) {
        strlcpy(buf, model->text_buffer, sizeof(buf));
    }
    char* str = buf;

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14, canvas_string_width(canvas, str) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        char* move = str + cursor_pos;
        memmove(move + 1, move, strlen(move) + 1);
        str[cursor_pos] = '|';
    }

    if(cursor_pos > 0 && canvas_string_width(canvas, str) > needed_string_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos += 6;
        needed_string_width -= 8;
        for(uint32_t off = 0;
            strlen(str) && canvas_string_width(canvas, str) > needed_string_width &&
            off < cursor_pos;
            off++) {
            str++;
        }
    }

    if(canvas_string_width(canvas, str) > needed_string_width) {
        needed_string_width -= 4;
        size_t len = strlen(str);
        while(len && canvas_string_width(canvas, str) > needed_string_width) {
            str[len--] = '\0';
        }
        // Use safe string append instead of strlcat
        if(len + 3 < model->text_buffer_size) {
            memcpy(str + len, "...", 4); // Copy including null terminator
        }
    }

    canvas_draw_str(canvas, start_pos, 22, str);
    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row < keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(keyboards[model->selected_keyboard], row);
        const BunnyConnectKeyboardKey* keys = get_row(keyboards[model->selected_keyboard], row);

        for(size_t column = 0; column < column_count; column++) {
            bool selected = !model->cursor_select && model->selected_row == row &&
                            model->selected_column == column;

            canvas_set_color(canvas, ColorBlack);

            // Draw simple text labels instead of icons temporarily
            if(keys[column].text == ENTER_KEY) {
                if(selected) {
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 1,
                        keyboard_origin_y + keys[column].y - 8,
                        25,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_str(
                    canvas,
                    keyboard_origin_x + keys[column].x,
                    keyboard_origin_y + keys[column].y,
                    "OK");
            } else if(keys[column].text == SWITCH_KEYBOARD_KEY) {
                if(selected) {
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 1,
                        keyboard_origin_y + keys[column].y - 8,
                        11,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_str(
                    canvas,
                    keyboard_origin_x + keys[column].x,
                    keyboard_origin_y + keys[column].y,
                    "!?");
            } else if(keys[column].text == BACKSPACE_KEY) {
                if(selected) {
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 1,
                        keyboard_origin_y + keys[column].y - 8,
                        17,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_str(
                    canvas,
                    keyboard_origin_x + keys[column].x,
                    keyboard_origin_y + keys[column].y,
                    "<-");
            } else {
                if(selected) {
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 1,
                        keyboard_origin_y + keys[column].y - 8,
                        7,
                        10);
                    canvas_set_color(canvas, ColorWhite);
                }

                if(model->clear_default_text || text_length == 0) {
                    canvas_draw_glyph(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        char_to_uppercase(keys[column].text));
                } else {
                    canvas_draw_glyph(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        keys[column].text);
                }
            }
        }
    }

    if(model->validator_message_visible) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 8, 10, 110, 48);
        canvas_set_color(canvas, ColorBlack);
        // Remove icon and just draw error box
        canvas_draw_rframe(canvas, 8, 8, 112, 50, 3);
        canvas_draw_rframe(canvas, 9, 9, 110, 48, 2);
        // Draw a simple "!" as error indicator
        canvas_draw_str(canvas, 15, 25, "!");
        elements_multiline_text_aligned(
            canvas, 62, 20, AlignCenter, AlignCenter, furi_string_get_cstr(model->validator_text));
        canvas_set_font(canvas, FontKeyboard);
    }
}

static void bunnyconnect_keyboard_handle_up(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardModel* model) {
    UNUSED(keyboard);
    if(model == NULL) return;

    if(model->selected_row > 0) {
        model->selected_row--;
        uint8_t row_size = get_row_size(keyboards[model->selected_keyboard], model->selected_row);

        if(model->selected_row == 0 && model->selected_column > row_size - 6) {
            model->selected_column = CLAMP(model->selected_column + 1, row_size - 1, 0);
        }

        if(model->selected_row == 1 &&
           model->selected_keyboard == symbol_keyboard.keyboard_index) {
            if(model->selected_column > 5 &&
               model->selected_column <
                   get_row_size(keyboards[model->selected_keyboard], model->selected_row))
                model->selected_column = CLAMP(model->selected_column + 2, row_size - 1, 0);
            else if(model->selected_column > 1)
                model->selected_column = CLAMP(model->selected_column + 1, row_size - 1, 0);
        }

        model->selected_column = CLAMP(model->selected_column, row_size - 1, 0);
    } else {
        model->cursor_select = true;
        model->clear_default_text = false;
    }
}

static void bunnyconnect_keyboard_handle_down(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardModel* model) {
    UNUSED(keyboard);
    if(model == NULL) return;

    if(model->cursor_select) {
        model->cursor_select = false;
    } else if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        uint8_t row_size = get_row_size(keyboards[model->selected_keyboard], model->selected_row);

        if(model->selected_row == 1 && model->selected_column > row_size - 4) {
            model->selected_column = CLAMP(model->selected_column - 1, row_size - 1, 0);
        }

        if(model->selected_row == 2 &&
           model->selected_keyboard == symbol_keyboard.keyboard_index) {
            if(model->selected_column > 7 &&
               model->selected_column < get_row_size(keyboards[model->selected_keyboard], 1))
                model->selected_column = CLAMP(model->selected_column - 2, row_size - 1, 0);
            else if(model->selected_column > 1)
                model->selected_column = CLAMP(model->selected_column - 1, row_size - 1, 0);
        }

        model->selected_column = CLAMP(model->selected_column, row_size - 1, 0);
    }
}

static void bunnyconnect_keyboard_handle_left(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardModel* model) {
    UNUSED(keyboard);
    if(model == NULL) return;

    if(model->cursor_select) {
        if(model->cursor_pos > 0) {
            model->cursor_pos = CLAMP(model->cursor_pos - 1, strlen(model->text_buffer), 0u);
        }
    } else if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        uint8_t row_size = get_row_size(keyboards[model->selected_keyboard], model->selected_row);
        model->selected_column = row_size > 0 ? row_size - 1 : 0;
    }
}

static void bunnyconnect_keyboard_handle_right(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardModel* model) {
    UNUSED(keyboard);
    if(model == NULL) return;

    if(model->cursor_select) {
        if(model->text_buffer != NULL) {
            model->cursor_pos = CLAMP(model->cursor_pos + 1, strlen(model->text_buffer), 0u);
        }
    } else {
        uint8_t row_size = get_row_size(keyboards[model->selected_keyboard], model->selected_row);
        if(model->selected_column < row_size - 1) {
            model->selected_column++;
        } else {
            model->selected_column = 0;
        }
    }
}

static void bunnyconnect_keyboard_handle_ok(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardModel* model,
    InputType type) {
    if(model == NULL) return;
    if(model->cursor_select) return;

    bool shift = type == InputTypeLong;
    bool repeat = type == InputTypeRepeat;

    if(model->selected_row >= keyboard_row_count || model->selected_keyboard >= keyboard_count) {
        return;
    }

    char selected = get_selected_char(model);
    size_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;

    if(selected == ENTER_KEY) {
        if(model->validator_callback &&
           (!model->validator_callback(
               model->text_buffer, model->validator_text, model->validator_callback_context))) {
            model->validator_message_visible = true;
            if(keyboard && keyboard->timer) {
                furi_timer_start(keyboard->timer, furi_kernel_get_tick_frequency() * 4);
            }
        } else if(model->callback != 0 && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == SWITCH_KEYBOARD_KEY) {
        switch_keyboard(model);
    } else {
        if(selected == BACKSPACE_KEY) {
            bunnyconnect_keyboard_backspace_cb(model);
        } else if(!repeat && model->text_buffer != NULL) {
            if(model->clear_default_text) {
                text_length = 0;
            }
            if(text_length < (model->text_buffer_size - 1)) {
                // Apply shift/case logic
                if(shift || (text_length == 0 && char_is_lowercase(selected))) {
                    selected = char_to_uppercase(selected);
                }

                if(model->clear_default_text) {
                    model->text_buffer[0] = selected;
                    if(model->text_buffer_size > 1) {
                        model->text_buffer[1] = '\0';
                    }
                    model->cursor_pos = 1;
                } else {
                    char* move = model->text_buffer + model->cursor_pos;
                    if(move && model->cursor_pos + strlen(move) + 1 < model->text_buffer_size) {
                        memmove(move + 1, move, strlen(move) + 1);
                        model->text_buffer[model->cursor_pos] = selected;
                        model->cursor_pos++;
                    }
                }
            }
        }
        model->clear_default_text = false;
    }
}

static bool bunnyconnect_keyboard_view_input_callback(InputEvent* event, void* context) {
    BunnyConnectKeyboard* keyboard = context;
    furi_assert(keyboard);

    bool consumed = false;

    BunnyConnectKeyboardModel* model = view_get_model(keyboard->view);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       model->validator_message_visible) {
        model->validator_message_visible = false;
        consumed = true;
    } else if(event->type == InputTypeShort) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bunnyconnect_keyboard_handle_up(keyboard, model);
            break;
        case InputKeyDown:
            bunnyconnect_keyboard_handle_down(keyboard, model);
            break;
        case InputKeyLeft:
            bunnyconnect_keyboard_handle_left(keyboard, model);
            break;
        case InputKeyRight:
            bunnyconnect_keyboard_handle_right(keyboard, model);
            break;
        case InputKeyOk:
            bunnyconnect_keyboard_handle_ok(keyboard, model, event->type);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bunnyconnect_keyboard_handle_up(keyboard, model);
            break;
        case InputKeyDown:
            bunnyconnect_keyboard_handle_down(keyboard, model);
            break;
        case InputKeyLeft:
            bunnyconnect_keyboard_handle_left(keyboard, model);
            break;
        case InputKeyRight:
            bunnyconnect_keyboard_handle_right(keyboard, model);
            break;
        case InputKeyOk:
            bunnyconnect_keyboard_handle_ok(keyboard, model, event->type);
            break;
        case InputKeyBack:
            bunnyconnect_keyboard_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            bunnyconnect_keyboard_handle_up(keyboard, model);
            break;
        case InputKeyDown:
            bunnyconnect_keyboard_handle_down(keyboard, model);
            break;
        case InputKeyLeft:
            bunnyconnect_keyboard_handle_left(keyboard, model);
            break;
        case InputKeyRight:
            bunnyconnect_keyboard_handle_right(keyboard, model);
            break;
        case InputKeyOk:
            bunnyconnect_keyboard_handle_ok(keyboard, model, event->type);
            break;
        case InputKeyBack:
            bunnyconnect_keyboard_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    view_commit_model(keyboard->view, consumed);
    return consumed;
}

void bunnyconnect_keyboard_timer_callback(void* context) {
    furi_assert(context);
    BunnyConnectKeyboard* keyboard = context;

    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        { model->validator_message_visible = false; },
        true);
}

BunnyConnectKeyboard* bunnyconnect_keyboard_alloc(void) {
    BunnyConnectKeyboard* keyboard = malloc(sizeof(BunnyConnectKeyboard));
    keyboard->view = view_alloc();
    view_set_context(keyboard->view, keyboard);
    view_allocate_model(keyboard->view, ViewModelTypeLocking, sizeof(BunnyConnectKeyboardModel));
    view_set_draw_callback(keyboard->view, bunnyconnect_keyboard_view_draw_callback);
    view_set_input_callback(keyboard->view, bunnyconnect_keyboard_view_input_callback);

    keyboard->timer =
        furi_timer_alloc(bunnyconnect_keyboard_timer_callback, FuriTimerTypeOnce, keyboard);

    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        {
            model->validator_text = furi_string_alloc();
            model->minimum_length = 1;
            model->cursor_pos = 0;
            model->cursor_select = false;
        },
        false);

    bunnyconnect_keyboard_reset(keyboard);
    return keyboard;
}

void bunnyconnect_keyboard_free(BunnyConnectKeyboard* keyboard) {
    furi_assert(keyboard);
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        { furi_string_free(model->validator_text); },
        false);

    furi_timer_stop(keyboard->timer);
    furi_timer_free(keyboard->timer);
    view_free(keyboard->view);
    free(keyboard);
}

void bunnyconnect_keyboard_reset(BunnyConnectKeyboard* keyboard) {
    furi_assert(keyboard);
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        {
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->selected_keyboard = 0;
            model->minimum_length = 1;
            model->clear_default_text = false;
            model->cursor_pos = 0;
            model->cursor_select = false;
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

View* bunnyconnect_keyboard_get_view(BunnyConnectKeyboard* keyboard) {
    furi_assert(keyboard);
    return keyboard->view;
}

void bunnyconnect_keyboard_set_result_callback(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->cursor_select = false;
            if(text_buffer && text_buffer[0] != '\0') {
                model->cursor_pos = strlen(text_buffer);
                model->selected_row = 2;
                model->selected_column = 9;
                model->selected_keyboard = 0;
            } else {
                model->cursor_pos = 0;
            }
        },
        true);
}

void bunnyconnect_keyboard_set_minimum_length(
    BunnyConnectKeyboard* keyboard,
    size_t minimum_length) {
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        { model->minimum_length = minimum_length; },
        true);
}

void bunnyconnect_keyboard_set_validator_callback(
    BunnyConnectKeyboard* keyboard,
    BunnyConnectKeyboardValidatorCallback callback,
    void* callback_context) {
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        {
            model->validator_callback = callback;
            model->validator_callback_context = callback_context;
        },
        true);
}

BunnyConnectKeyboardValidatorCallback
    bunnyconnect_keyboard_get_validator_callback(BunnyConnectKeyboard* keyboard) {
    BunnyConnectKeyboardValidatorCallback validator_callback = NULL;
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        { validator_callback = model->validator_callback; },
        false);
    return validator_callback;
}

void* bunnyconnect_keyboard_get_validator_callback_context(BunnyConnectKeyboard* keyboard) {
    void* validator_callback_context = NULL;
    with_view_model(
        keyboard->view,
        BunnyConnectKeyboardModel * model,
        { validator_callback_context = model->validator_callback_context; },
        false);
    return validator_callback_context;
}

void bunnyconnect_keyboard_set_header_text(BunnyConnectKeyboard* keyboard, const char* text) {
    with_view_model(
        keyboard->view, BunnyConnectKeyboardModel * model, { model->header = text; }, true);
}

void bunnyconnect_keyboard_send_key(BunnyConnectKeyboard* keyboard, uint16_t key) {
    UNUSED(keyboard);
    if(furi_hal_hid_is_connected()) {
        furi_hal_hid_kb_press(key);
        furi_delay_ms(10);
        furi_hal_hid_kb_release(key);
    }
}

void bunnyconnect_keyboard_send_string(BunnyConnectKeyboard* keyboard, const char* string) {
    UNUSED(keyboard);
    if(!string || !furi_hal_hid_is_connected()) return;

    for(size_t i = 0; i < strlen(string); i++) {
        uint16_t key = HID_ASCII_TO_KEY(string[i]);
        if(key != HID_KEYBOARD_NONE) {
            furi_hal_hid_kb_press(key);
            furi_delay_ms(10);
            furi_hal_hid_kb_release(key);
            furi_delay_ms(10);
        }
    }
}
