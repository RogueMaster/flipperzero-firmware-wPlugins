#include "fdxb_temperature_input.h"

#include <gui/elements.h>
#include <furi.h>
#include "fdxb_maker_icons.h"
#include <lib/toolbox/strint.h>

struct FdxbTemperatureInput {
    View* view;
};

typedef struct {
    const char text;
    const size_t x;
    const size_t y;
} FdxbTemperatureInputKey;

/*
    Available keys vary based on state

    FirstNum:
    1       <-
        7 8 9

    BigTen:
    0 1 2     <-

    // BigOne:
    // 0 1 2 3 4 <-
    // 5 6 7 8 9

    Ones:
    0 1 2 3 4 <-
    5 6 7 8 9

    Decimal:
            <-
            . ->

    Tenths:
    0   2   4 <-
    6   8     ->

    PostTenths:
            <-
                ->
*/

typedef enum {
    FdxbTemperatureInputStateFirstNum = 0,
    FdxbTemperatureInputStateBigTen,
    FdxbTemperatureInputStateOnes,
    FdxbTemperatureInputStateDecimal,
    FdxbTemperatureInputStateTenths,
    FdxbTemperatureInputStatePostTenths,
} FdxbTemperatureInputState;

typedef struct {
    FuriString* header;
    FuriString* text_buffer;

    FdxbTemperatureInputCallback callback;
    void* callback_context;

    size_t selected_row;
    size_t selected_column;

    FdxbTemperatureInputState state;
} FdxbTemperatureInputModel;

static const size_t keyboard_origin_x = 7;
static const size_t keyboard_origin_y = 31;
static const size_t keyboard_row_count = 2;
static const char enter_symbol = '\r';
static const char backspace_symbol = '\b';
static const char decimal_symbol = '.';

static const float FDXB_TEMP_MAX_VALUE = 74 + (0.2 * 0xFF);
static const float FDXB_TEMP_MIN_VALUE = 74 + (0.2 * 0x00);

static const FdxbTemperatureInputKey keyboard_keys_row_1[] = {
    {'0', 0, 12},
    {'1', 11, 12},
    {'2', 22, 12},
    {'3', 33, 12},
    {'4', 44, 12},
    {backspace_symbol, 103, 4},
};

static const FdxbTemperatureInputKey keyboard_keys_row_2[] = {
    {'5', 0, 26},
    {'6', 11, 26},
    {'7', 22, 26},
    {'8', 33, 26},
    {'9', 44, 26},
    {decimal_symbol, 55, 17},
    {enter_symbol, 95, 17},
};

static size_t fdxb_temperature_input_get_row_size(size_t row_index) {
    size_t row_size = 0;

    switch(row_index + 1) {
    case 1:
        row_size = COUNT_OF(keyboard_keys_row_1);
        break;
    case 2:
        row_size = COUNT_OF(keyboard_keys_row_2);
        break;
    default:
        furi_crash();
    }

    return row_size;
}

static const FdxbTemperatureInputKey* fdxb_temperature_input_get_row(size_t row_index) {
    const FdxbTemperatureInputKey* row = NULL;

    switch(row_index + 1) {
    case 1:
        row = keyboard_keys_row_1;
        break;
    case 2:
        row = keyboard_keys_row_2;
        break;
    default:
        furi_crash();
    }

    return row;
}

static bool is_number_too_large(FdxbTemperatureInputModel* model) {
    float value = strtof(furi_string_get_cstr(model->text_buffer), NULL);
    return value > FDXB_TEMP_MAX_VALUE;
}

static bool is_number_too_small(FdxbTemperatureInputModel* model) {
    float value = strtof(furi_string_get_cstr(model->text_buffer), NULL);
    return value < FDXB_TEMP_MIN_VALUE;
}

static bool fdxb_temperature_is_valid_input(FdxbTemperatureInputModel* model, char key_text) {
    FdxbTemperatureInputState state = model->state;
    switch(key_text) {
    case '0':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateBigTen ||
               state == FdxbTemperatureInputStateTenths;
    case '1':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateFirstNum ||
               state == FdxbTemperatureInputStateBigTen;
    case '2':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateBigTen ||
               state == FdxbTemperatureInputStateTenths;
    case '3':
        return state == FdxbTemperatureInputStateOnes;
    case '4':
        return state == FdxbTemperatureInputStateOnes || state == FdxbTemperatureInputStateTenths;
    case '5':
        return state == FdxbTemperatureInputStateOnes;
    case '6':
        return state == FdxbTemperatureInputStateOnes || state == FdxbTemperatureInputStateTenths;
    case '7':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateFirstNum;
    case '8':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateFirstNum ||
               state == FdxbTemperatureInputStateTenths;
    case '9':
        return state == FdxbTemperatureInputStateOnes ||
               state == FdxbTemperatureInputStateFirstNum;
    case backspace_symbol:
        return true;
    case decimal_symbol:
        return state == FdxbTemperatureInputStateDecimal;
    case enter_symbol:
        return !is_number_too_large(model) && !is_number_too_small(model) &&
               (state == FdxbTemperatureInputStateDecimal ||
                state == FdxbTemperatureInputStateTenths ||
                state == FdxbTemperatureInputStatePostTenths);
    default:
        return false;
    }
}

static void fdxb_temperature_input_draw_input(Canvas* canvas, FdxbTemperatureInputModel* model) {
    const size_t text_x = 8;
    const size_t text_y = 25;

    elements_slightly_rounded_frame(canvas, 6, 14, 116, 15);

    canvas_draw_str(canvas, text_x, text_y, furi_string_get_cstr(model->text_buffer));
}

static void fdxb_temperature_input_backspace_cb(FdxbTemperatureInputModel* model) {
    size_t text_length = furi_string_utf8_length(model->text_buffer);
    if(text_length < 1) {
        return;
    }
    furi_string_set_strn(
        model->text_buffer, furi_string_get_cstr(model->text_buffer), text_length - 1);

    if(model->state == FdxbTemperatureInputStateOnes) {
        if(text_length == 2) {
            model->state = FdxbTemperatureInputStateBigTen;
        } else {
            model->state = FdxbTemperatureInputStateFirstNum;
        }
    } else {
        model->state--;
    }
}

static void fdxb_temperature_input_handle_up(FdxbTemperatureInputModel* model) {
    if(model->selected_row > 0) {
        model->selected_row--;
        if(model->selected_column > fdxb_temperature_input_get_row_size(model->selected_row) - 1) {
            model->selected_column = fdxb_temperature_input_get_row_size(model->selected_row) - 1;
        }
    }
}

static void fdxb_temperature_input_handle_down(FdxbTemperatureInputModel* model) {
    if(model->selected_row < keyboard_row_count - 1) {
        if(model->selected_column >=
           fdxb_temperature_input_get_row_size(model->selected_row) - 1) {
            model->selected_column =
                fdxb_temperature_input_get_row_size(model->selected_row + 1) - 1;
        }
        model->selected_row += 1;
    }
}

static void fdxb_temperature_input_handle_left(FdxbTemperatureInputModel* model) {
    if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column = fdxb_temperature_input_get_row_size(model->selected_row) - 1;
    }
}

static void fdxb_temperature_input_handle_right(FdxbTemperatureInputModel* model) {
    if(model->selected_column < fdxb_temperature_input_get_row_size(model->selected_row) - 1) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
}

static void fdxb_temperature_input_decimal(FdxbTemperatureInputModel* model) {
    char temp_str[2] = {decimal_symbol, '\0'};
    furi_string_cat_str(model->text_buffer, temp_str);
    model->state = FdxbTemperatureInputStateTenths;
}

static void fdxb_temperature_input_add_digit(FdxbTemperatureInputModel* model, char* newChar) {
    furi_string_cat_str(model->text_buffer, newChar);
    if(model->state == FdxbTemperatureInputStateFirstNum) {
        if(newChar[0] == '1') {
            model->state = FdxbTemperatureInputStateBigTen;
        } else {
            model->state = FdxbTemperatureInputStateOnes;
        }
    } else {
        model->state++;
    }
}

static void fdxb_temperature_input_handle_ok(FdxbTemperatureInputModel* model) {
    char selected =
        fdxb_temperature_input_get_row(model->selected_row)[model->selected_column].text;
    char temp_str[2] = {selected, '\0'};
    if(selected == enter_symbol && fdxb_temperature_is_valid_input(model, enter_symbol)) {
        model->callback(
            model->callback_context, strtof(furi_string_get_cstr(model->text_buffer), NULL));
    } else if(selected == backspace_symbol) {
        fdxb_temperature_input_backspace_cb(model);
    } else if(selected == decimal_symbol && fdxb_temperature_is_valid_input(model, decimal_symbol)) {
        fdxb_temperature_input_decimal(model);
    } else if(fdxb_temperature_is_valid_input(model, selected)) {
        fdxb_temperature_input_add_digit(model, temp_str);
    }
}

static void fdxb_temperature_input_view_draw_callback(Canvas* canvas, void* _model) {
    FdxbTemperatureInputModel* model = _model;

    fdxb_temperature_input_draw_input(canvas, model);

    if(!furi_string_empty(model->header)) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 9, furi_string_get_cstr(model->header));
    }
    canvas_set_font(canvas, FontKeyboard);
    // Draw keyboard
    for(size_t row = 0; row < keyboard_row_count; row++) {
        const size_t column_count = fdxb_temperature_input_get_row_size(row);
        const FdxbTemperatureInputKey* keys = fdxb_temperature_input_get_row(row);

        for(size_t column = 0; column < column_count; column++) {
            if(keys[column].text == enter_symbol) {
                if(!fdxb_temperature_is_valid_input(model, enter_symbol)) {
                    if(model->selected_row == row && model->selected_column == column) {
                        canvas_draw_icon(
                            canvas,
                            keyboard_origin_x + keys[column].x,
                            keyboard_origin_y + keys[column].y,
                            &I_KeySaveBlockedSelected_24x11);
                    } else {
                        canvas_draw_icon(
                            canvas,
                            keyboard_origin_x + keys[column].x,
                            keyboard_origin_y + keys[column].y,
                            &I_KeySaveBlocked_24x11);
                    }
                } else {
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
                }
            } else if(keys[column].text == backspace_symbol) {
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
                    canvas_draw_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 3,
                        keyboard_origin_y + keys[column].y - 10,
                        11,
                        13);
                    canvas_set_color(canvas, ColorWhite);
                }
                if(fdxb_temperature_is_valid_input(model, keys[column].text)) {
                    canvas_draw_glyph(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        keys[column].text);
                }
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }
}

static bool fdxb_temperature_input_view_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    FdxbTemperatureInput* fdxb_temperature_input = context;

    bool consumed = false;

    // Fetch the model
    FdxbTemperatureInputModel* model = view_get_model(fdxb_temperature_input->view);

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyLeft:
            fdxb_temperature_input_handle_left(model);
            break;
        case InputKeyRight:
            fdxb_temperature_input_handle_right(model);
            break;
        case InputKeyUp:
            fdxb_temperature_input_handle_up(model);
            break;
        case InputKeyDown:
            fdxb_temperature_input_handle_down(model);
            break;
        case InputKeyOk:
            fdxb_temperature_input_handle_ok(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    // commit view
    view_commit_model(fdxb_temperature_input->view, consumed);

    return consumed;
}

FdxbTemperatureInput* fdxb_temperature_input_alloc(void) {
    FdxbTemperatureInput* fdxb_temperature_input = malloc(sizeof(FdxbTemperatureInput));
    fdxb_temperature_input->view = view_alloc();
    view_set_context(fdxb_temperature_input->view, fdxb_temperature_input);
    view_allocate_model(
        fdxb_temperature_input->view, ViewModelTypeLocking, sizeof(FdxbTemperatureInputModel));
    view_set_draw_callback(
        fdxb_temperature_input->view, fdxb_temperature_input_view_draw_callback);
    view_set_input_callback(
        fdxb_temperature_input->view, fdxb_temperature_input_view_input_callback);

    with_view_model(
        fdxb_temperature_input->view,
        FdxbTemperatureInputModel * model,
        {
            model->header = furi_string_alloc();
            model->text_buffer = furi_string_alloc();
        },
        true);

    return fdxb_temperature_input;
}

void fdxb_temperature_input_free(FdxbTemperatureInput* fdxb_temperature_input) {
    furi_check(fdxb_temperature_input);
    with_view_model(
        fdxb_temperature_input->view,
        FdxbTemperatureInputModel * model,
        {
            furi_string_free(model->header);
            furi_string_free(model->text_buffer);
        },
        true);
    view_free(fdxb_temperature_input->view);
    free(fdxb_temperature_input);
}

View* fdxb_temperature_input_get_view(FdxbTemperatureInput* fdxb_temperature_input) {
    furi_check(fdxb_temperature_input);
    return fdxb_temperature_input->view;
}

void fdxb_temperature_input_set_result_callback(
    FdxbTemperatureInput* fdxb_temperature_input,
    FdxbTemperatureInputCallback callback,
    void* callback_context,
    float current_number) {
    furi_check(fdxb_temperature_input);

    bool zero = fabs(current_number) < (double)__FLT_EPSILON__;

    current_number = CLAMP(current_number, FDXB_TEMP_MAX_VALUE, FDXB_TEMP_MIN_VALUE);

    with_view_model(
        fdxb_temperature_input->view,
        FdxbTemperatureInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            if(zero) {
                furi_string_reset(model->text_buffer);
                model->state = FdxbTemperatureInputStateFirstNum;
            } else {
                if(fabs(round(current_number) - (double)current_number) <
                   (double)__FLT_EPSILON__) {
                    furi_string_printf(model->text_buffer, "%lu", (uint32_t)current_number);
                } else {
                    furi_string_printf(model->text_buffer, "%.1f", (double)current_number);
                }
                size_t text_length = furi_string_utf8_length(model->text_buffer);
                if(text_length >= 4)
                    model->state = FdxbTemperatureInputStatePostTenths;
                else
                    model->state = FdxbTemperatureInputStateDecimal;
            }
        },
        true);
}

void fdxb_temperature_input_set_header_text(
    FdxbTemperatureInput* fdxb_temperature_input,
    const char* text) {
    furi_check(fdxb_temperature_input);
    with_view_model(
        fdxb_temperature_input->view,
        FdxbTemperatureInputModel * model,
        { furi_string_set(model->header, text); },
        true);
}

void fdxb_temperature_input_reset(FdxbTemperatureInput* fdxb_temperature_input) {
    with_view_model(
        fdxb_temperature_input->view,
        FdxbTemperatureInputModel * model,
        {
            furi_string_reset(model->header);
            model->state = FdxbTemperatureInputStateFirstNum;
        },
        true);
}
