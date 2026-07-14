#include "wendigo_hex_input.h"
#include "wendigo_text_input.h"
#include <gui/elements.h>
#include "wendigo_icons.h"
#include "wendigo_app_i.h"
#include <furi.h>

struct Wendigo_TextInput {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} Wendigo_TextInputKey;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    bool clear_default_text;

    Wendigo_TextInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;

    Wendigo_TextInputValidatorCallback validator_callback;
    void* validator_callback_context;
    FuriString* validator_text;
    bool validator_message_visible;
} Wendigo_TextInputModel;

static const uint8_t keyboard_origin_x = 5;
static const uint8_t keyboard_origin_y = 28;
static const uint8_t keyboard_row_count = 2;

#define ENTER_KEY     '\r'
#define BACKSPACE_KEY '\b'

static const Wendigo_TextInputKey keyboard_keys_row_1[] = {
    {'0', 0, 12},
    {'1', 11, 12},
    {'2', 22, 12},
    {'3', 33, 12},
    {'4', 44, 12},
    {'5', 55, 12},
    {'6', 66, 12},
    {'7', 77, 12},
    {BACKSPACE_KEY, 103, 4},
};

static const Wendigo_TextInputKey keyboard_keys_row_2[] = {
    {'8', 0, 26},
    {'9', 11, 26},
    {'A', 22, 26},
    {'B', 33, 26},
    {'C', 44, 26},
    {'D', 55, 26},
    {'E', 66, 26},
    {'F', 77, 26},
    {ENTER_KEY, 95, 17},
};

static uint8_t get_row_size(uint8_t row_index) {
    FURI_LOG_T(WENDIGO_TAG, "Start get_row_size()");
    uint8_t row_size = 0;

    switch(row_index + 1) {
    case 1:
        row_size = sizeof(keyboard_keys_row_1) / sizeof(Wendigo_TextInputKey);
        break;
    case 2:
        row_size = sizeof(keyboard_keys_row_2) / sizeof(Wendigo_TextInputKey);
        break;
    }
    FURI_LOG_T(WENDIGO_TAG, "End get_row_size()");
    return row_size;
}

static const Wendigo_TextInputKey* get_row(uint8_t row_index) {
    FURI_LOG_T(WENDIGO_TAG, "Start get_row()");
    const Wendigo_TextInputKey* row = NULL;

    switch(row_index + 1) {
    case 1:
        row = keyboard_keys_row_1;
        break;
    case 2:
        row = keyboard_keys_row_2;
        break;
    }
    FURI_LOG_T(WENDIGO_TAG, "End get_row()");
    return row;
}

static char get_selected_char(Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End get_selected_char()");
    return get_row(model->selected_row)[model->selected_column].text;
}

static void wendigo_hex_input_backspace_cb(Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_backspace_cb()");
    uint8_t text_length = model->clear_default_text ? 1 : strlen(model->text_buffer);
    if(text_length > 0) {
        model->text_buffer[text_length - 1] = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_backspace_cb()");
}

static void wendigo_hex_input_view_draw_callback(Canvas* canvas, void* _model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_view_draw_callback()");
    Wendigo_TextInputModel* model = _model;
    uint8_t needed_string_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;

    const char* text = model->text_buffer;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str(canvas, 2, 7, model->header);
    elements_slightly_rounded_frame(canvas, 1, 8, 126, 12);

    if(canvas_string_width(canvas, text) > needed_string_width) {
        canvas_draw_str(canvas, start_pos, 17, "...");
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
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 1, 18, "|");
        canvas_draw_str(canvas, start_pos + canvas_string_width(canvas, text) + 2, 18, "|");
    }
    canvas_draw_str(canvas, start_pos, 17, text);

    canvas_set_font(canvas, FontKeyboard);

    for(uint8_t row = 0; row <= keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(row);
        const Wendigo_TextInputKey* keys = get_row(row);

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
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_view_draw_callback()");
}

static void wendigo_hex_input_handle_up(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_handle_up()");
    UNUSED(wendigo_text_input);
    if(model->selected_row > 0) {
        model->selected_row--;
        if(model->selected_column > get_row_size(model->selected_row) - 6) {
            model->selected_column = model->selected_column + 1;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_handle_up()");
}

static void wendigo_hex_input_handle_down(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_handle_down()");
    UNUSED(wendigo_text_input);
    if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        if(model->selected_column > get_row_size(model->selected_row) - 4) {
            model->selected_column = model->selected_column - 1;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_handle_down()");
}

static void wendigo_hex_input_handle_left(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_handle_left()");
    UNUSED(wendigo_text_input);
    if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column = get_row_size(model->selected_row) - 1;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_handle_left()");
}

static void wendigo_hex_input_handle_right(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputModel* model) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_handle_right()");
    UNUSED(wendigo_text_input);
    if(model->selected_column < get_row_size(model->selected_row) - 1) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_handle_right()");
}

static void wendigo_hex_input_handle_ok(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputModel* model,
    bool shift) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_handle_ok()");
    UNUSED(shift);
    char selected = get_selected_char(model);
    uint8_t text_length = strlen(model->text_buffer);

    if(selected == ENTER_KEY) {
        if(model->validator_callback &&
           (!model->validator_callback(
               model->text_buffer, model->validator_text, model->validator_callback_context))) {
            model->validator_message_visible = true;
            furi_timer_start(wendigo_text_input->timer, furi_kernel_get_tick_frequency() * 4);
        } else if(model->callback != 0 && text_length > 0) {
            model->callback(model->callback_context);
        }
    } else if(selected == BACKSPACE_KEY) {
        wendigo_hex_input_backspace_cb(model);
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
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_handle_ok()");
}

static bool wendigo_hex_input_view_input_callback(InputEvent* event, void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_view_input_callback()");
    Wendigo_TextInput* wendigo_text_input = context;
    furi_assert(wendigo_text_input);

    bool consumed = false;

    // Acquire model
    Wendigo_TextInputModel* model = view_get_model(wendigo_text_input->view);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       model->validator_message_visible) {
        model->validator_message_visible = false;
        consumed = true;
    } else if(event->type == InputTypeShort) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            wendigo_hex_input_handle_up(wendigo_text_input, model);
            break;
        case InputKeyDown:
            wendigo_hex_input_handle_down(wendigo_text_input, model);
            break;
        case InputKeyLeft:
            wendigo_hex_input_handle_left(wendigo_text_input, model);
            break;
        case InputKeyRight:
            wendigo_hex_input_handle_right(wendigo_text_input, model);
            break;
        case InputKeyOk:
            wendigo_hex_input_handle_ok(wendigo_text_input, model, false);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            wendigo_hex_input_handle_up(wendigo_text_input, model);
            break;
        case InputKeyDown:
            wendigo_hex_input_handle_down(wendigo_text_input, model);
            break;
        case InputKeyLeft:
            wendigo_hex_input_handle_left(wendigo_text_input, model);
            break;
        case InputKeyRight:
            wendigo_hex_input_handle_right(wendigo_text_input, model);
            break;
        case InputKeyOk:
            wendigo_hex_input_handle_ok(wendigo_text_input, model, true);
            break;
        case InputKeyBack:
            wendigo_hex_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            wendigo_hex_input_handle_up(wendigo_text_input, model);
            break;
        case InputKeyDown:
            wendigo_hex_input_handle_down(wendigo_text_input, model);
            break;
        case InputKeyLeft:
            wendigo_hex_input_handle_left(wendigo_text_input, model);
            break;
        case InputKeyRight:
            wendigo_hex_input_handle_right(wendigo_text_input, model);
            break;
        case InputKeyBack:
            wendigo_hex_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    /* Commit model */
    view_commit_model(wendigo_text_input->view, consumed);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_view_input_callback()");
    return consumed;
}

void wendigo_hex_input_timer_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_timer_callback()");
    furi_assert(context);
    Wendigo_TextInput* wendigo_text_input = context;

    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        { model->validator_message_visible = false; },
        true);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_timer_callback()");
}

Wendigo_TextInput* wendigo_hex_input_alloc() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_alloc()");
    Wendigo_TextInput* wendigo_text_input = malloc(sizeof(Wendigo_TextInput));
    wendigo_text_input->view = view_alloc();
    view_set_context(wendigo_text_input->view, wendigo_text_input);
    view_allocate_model(
        wendigo_text_input->view, ViewModelTypeLocking, sizeof(Wendigo_TextInputModel));
    view_set_draw_callback(wendigo_text_input->view, wendigo_hex_input_view_draw_callback);
    view_set_input_callback(wendigo_text_input->view, wendigo_hex_input_view_input_callback);

    wendigo_text_input->timer =
        furi_timer_alloc(wendigo_hex_input_timer_callback, FuriTimerTypeOnce, wendigo_text_input);

    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        { model->validator_text = furi_string_alloc(); },
        false);

    wendigo_text_input_reset(wendigo_text_input);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_alloc()");
    return wendigo_text_input;
}

void wendigo_hex_input_free(Wendigo_TextInput* wendigo_text_input) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_free()");
    furi_assert(wendigo_text_input);
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        { furi_string_free(model->validator_text); },
        false);

    // Send stop command
    furi_timer_stop(wendigo_text_input->timer);
    // Release allocated memory
    furi_timer_free(wendigo_text_input->timer);

    view_free(wendigo_text_input->view);

    free(wendigo_text_input);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_free()");
}

void wendigo_hex_input_reset(Wendigo_TextInput* wendigo_text_input) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_reset()");
    furi_assert(wendigo_text_input);
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        {
            model->text_buffer_size = 0;
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->clear_default_text = false;
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
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_reset()");
}

View* wendigo_hex_input_get_view(Wendigo_TextInput* wendigo_text_input) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_get_view()");
    furi_assert(wendigo_text_input);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_get_view()");
    return wendigo_text_input->view;
}

void wendigo_hex_input_set_result_callback(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_set_result_callback()");
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            if(text_buffer && text_buffer[0] != '\0') {
                // Set focus on Save
                model->selected_row = 1;
                model->selected_column = 8;
            }
        },
        true);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_set_result_callback()");
}

void wendigo_hex_input_set_validator(
    Wendigo_TextInput* wendigo_text_input,
    Wendigo_TextInputValidatorCallback callback,
    void* callback_context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_set_validator()");
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        {
            model->validator_callback = callback;
            model->validator_callback_context = callback_context;
        },
        true);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_set_validator()");
}

Wendigo_TextInputValidatorCallback
    wendigo_hex_input_get_validator_callback(Wendigo_TextInput* wendigo_text_input) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_get_validator_callback()");
    Wendigo_TextInputValidatorCallback validator_callback = NULL;
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        { validator_callback = model->validator_callback; },
        false);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_get_validator_callback()");
    return validator_callback;
}

void* wendigo_hex_input_get_validator_callback_context(Wendigo_TextInput* wendigo_text_input) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_get_validator_callback_context()");
    void* validator_callback_context = NULL;
    with_view_model(
        wendigo_text_input->view,
        Wendigo_TextInputModel * model,
        { validator_callback_context = model->validator_callback_context; },
        false);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_get_validator_callback_context()");
    return validator_callback_context;
}

void wendigo_hex_input_set_header_text(Wendigo_TextInput* wendigo_text_input, const char* text) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_hex_input_set_header_text()");
    with_view_model(
        wendigo_text_input->view, Wendigo_TextInputModel * model, { model->header = text; }, true);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_hex_input_set_header_text()");
}
