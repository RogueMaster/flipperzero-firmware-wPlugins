#include "ip_assigner.h"
#include "ethernet_app_icons.h"
#include "gui/elements.h"

struct ip_assigner_t {
    View* view;
};

// struct for the model
typedef struct {
    uint8_t selector_position;
    ip_assigner_callback_t callback;
    uint8_t digits_array[12];
    FuriString* header;
    void* context;
    uint8_t* ip_array;
} ip_assigner_model;

// Array for the numbers
const Icon* numbers_icons[] = {
    &I_Number_0,
    &I_Number_1,
    &I_Number_2,
    &I_Number_3,
    &I_Number_4,
    &I_Number_5,
    &I_Number_6,
    &I_Number_7,
    &I_Number_8,
    &I_Number_9,
};

// Function to get the digit from a decimal number
static uint8_t get_num(uint8_t number, uint8_t digit_pos) {
    for(uint8_t i = 0; i < digit_pos; i++) {
        number = number / 10;
    }
    return number % 10;
}

// Function to save or store data
static void update_ip_data(uint8_t* ip_array, uint8_t* digits_array) {
    ip_array[0] = 0;
    ip_array[0] += digits_array[0] * 100; // digit 3 of the 3 in the first element in the array
    ip_array[0] += digits_array[1] * 10; // digit 2 of the 3 in the first element in the array
    ip_array[0] += digits_array[2]; // digit 1 of the 3 in the first element in the array

    ip_array[1] = 0;
    ip_array[1] += digits_array[3] * 100; // digit 3 of the 3 in the first element in the array
    ip_array[1] += digits_array[4] * 10; // digit 2 of the 3 in the first element in the array
    ip_array[1] += digits_array[5]; // digit 1 of the 3 in the first element in the array

    ip_array[2] = 0;
    ip_array[2] += digits_array[6] * 100; // digit 3 of the 3 in the first element in the array
    ip_array[2] += digits_array[7] * 10; // digit 2 of the 3 in the first element in the array
    ip_array[2] += digits_array[8]; // digit 1 of the 3 in the first element in the array

    ip_array[3] = 0;
    ip_array[3] += digits_array[9] * 100; // digit 3 of the 3 in the first element in the array
    ip_array[3] += digits_array[10] * 10; // digit 2 of the 3 in the first element in the array
    ip_array[3] += digits_array[11]; // digit 1 of the 3 in the first element in the array
}

// Draw the box of the selector
static void draw_my_box(Canvas* canvas, uint8_t pos_x, uint8_t pos_y) {
    // Draw Top side
    canvas_draw_line(canvas, pos_x, pos_y, pos_x + 8, pos_y);

    // Draw Left side
    canvas_draw_line(canvas, pos_x, pos_y + 16, pos_x, pos_y);

    // Draw Bottom side
    canvas_draw_line(canvas, pos_x, pos_y + 16, pos_x + 8, pos_y + 16);

    // Draw Bottom side
    canvas_draw_line(canvas, pos_x + 8, pos_y, pos_x + 8, pos_y + 16);

    int8_t reference_top_y = pos_y - 2;
    int8_t reference_top_x = pos_x + 2;

    // draw selection reference top
    canvas_draw_dot(canvas, reference_top_x + 2, reference_top_y - 2);
    canvas_draw_line(
        canvas, reference_top_x, reference_top_y, reference_top_x + 4, reference_top_y);
    canvas_draw_line(
        canvas, reference_top_x + 1, reference_top_y - 1, reference_top_x + 3, reference_top_y - 1);

    int8_t reference_bottom_y = pos_y + 18;
    int8_t reference_bottom_x = pos_x + 2;

    // draw selection reference bottom
    canvas_draw_dot(canvas, reference_bottom_x + 2, reference_bottom_y + 2);
    canvas_draw_line(
        canvas, reference_bottom_x, reference_bottom_y, reference_bottom_x + 4, reference_bottom_y);
    canvas_draw_line(
        canvas,
        reference_bottom_x + 1,
        reference_bottom_y + 1,
        reference_bottom_x + 3,
        reference_bottom_y + 1);
}

// This function Works to draw the hello world (by the moment)
static void ip_assignament_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(_model);

    ip_assigner_model* model = (ip_assigner_model*)_model;

    if(!furi_string_empty(model->header)) {
        // Draw header
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 15, furi_string_get_cstr(model->header));
    }

    // Position of the rectangles
    uint8_t cell_pos_x = 2;
    uint8_t cell_pos_y = 25;

    // Draw positions
    canvas_draw_icon(canvas, cell_pos_x, cell_pos_y, &I_Ip_Selector_Template);

    // Y position of the digits
    uint8_t pos_y_digit_numbers = cell_pos_y + 2;

    // First digit position of the first three numbers
    uint8_t cell_one_first_digit = cell_pos_x + 4;
    uint8_t cell_one_second_digit = cell_one_first_digit + 9;
    uint8_t cell_one_third_digit = cell_one_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_two_first_digit = cell_pos_x + 35;
    uint8_t cell_two_second_digit = cell_two_first_digit + 9;
    uint8_t cell_two_third_digit = cell_two_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_three_first_digit = cell_pos_x + 66;
    uint8_t cell_three_second_digit = cell_three_first_digit + 9;
    uint8_t cell_three_third_digit = cell_three_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_fourth_first_digit = cell_pos_x + 97;
    uint8_t cell_fourth_second_digit = cell_fourth_first_digit + 9;
    uint8_t cell_fourth_third_digit = cell_fourth_second_digit + 9;

    if(model->ip_array != NULL) {
        // Draw the position numbers
        canvas_draw_icon(
            canvas,
            cell_one_first_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[0]]);
        canvas_draw_icon(
            canvas,
            cell_one_second_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[1]]);
        canvas_draw_icon(
            canvas,
            cell_one_third_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[2]]);

        // Draw position
        canvas_draw_icon(
            canvas,
            cell_two_first_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[3]]);
        canvas_draw_icon(
            canvas,
            cell_two_second_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[4]]);
        canvas_draw_icon(
            canvas,
            cell_two_third_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[5]]);

        // Draw position
        canvas_draw_icon(
            canvas,
            cell_three_first_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[6]]);
        canvas_draw_icon(
            canvas,
            cell_three_second_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[7]]);
        canvas_draw_icon(
            canvas,
            cell_three_third_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[8]]);

        // Draw position
        canvas_draw_icon(
            canvas,
            cell_fourth_first_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[9]]);
        canvas_draw_icon(
            canvas,
            cell_fourth_second_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[10]]);
        canvas_draw_icon(
            canvas,
            cell_fourth_third_digit,
            pos_y_digit_numbers,
            numbers_icons[model->digits_array[11]]);

        // Solve the offset
        uint8_t offset_position = (model->selector_position % 3) * 8 +
                                  model->selector_position % 3 +
                                  (model->selector_position / 3) * 31;

        // Selector position
        uint8_t selector_position_x = offset_position + cell_pos_x + 2;
        uint8_t selector_position_y = cell_pos_y - 2;

        // Draw an icon
        draw_my_box(canvas, selector_position_x, selector_position_y);
    }

    // set the font
    canvas_set_font(canvas, FontSecondary);

    // Add element
    elements_button_center(canvas, "Set");
}

// Functiom to solve position and add values in the array at the sum
static void validate_num_in_sum(ip_assigner_model* model) {
    // Add when the position is in third position for example
    // in the position 0, 3, 6, 9 positions
    if(model->selector_position % 3 == 0) {
        if(model->digits_array[model->selector_position] == 2) {
            model->digits_array[model->selector_position] =
                0; // If the value was 2 now it will be 2
        } else {
            model->digits_array[model->selector_position]++;
        }

        // If after add, the value is 2, we have some conditions
        if(model->digits_array[model->selector_position] == 2) {
            // To know if the number is more than 255
            if(model->digits_array[model->selector_position + 1] >= 5 &&
               model->digits_array[model->selector_position + 2] > 5) {
                model->digits_array[model->selector_position + 1] = 5;
                model->digits_array[model->selector_position + 2] = 5;
            }

            // To know if the number is more than 25x
            if(model->digits_array[model->selector_position + 1] > 5)
                model->digits_array[model->selector_position + 1] = 5;
        }
    }

    // Add when the value is in the second position
    // For example in the 1, 4, 7, 10 positions
    if(model->selector_position % 3 == 1) {
        if(model->digits_array[model->selector_position - 1] == 2 &&
           model->digits_array[model->selector_position] == 4 &&
           model->digits_array[model->selector_position + 1] > 5) {
            model->digits_array[model->selector_position]++;
            model->digits_array[model->selector_position + 1] = 5;
        } else if(
            model->digits_array[model->selector_position - 1] == 2 &&
            model->digits_array[model->selector_position] == 5) {
            model->digits_array[model->selector_position] = 0;
        } else if(model->digits_array[model->selector_position] == 9) {
            model->digits_array[model->selector_position] = 0;
        } else {
            model->digits_array[model->selector_position]++;
        }
    }

    // Add when the value is in the third position
    // For example in the 2, 5, 8, 11 positions
    if(model->selector_position % 3 == 2) {
        if(model->digits_array[model->selector_position - 2] == 2 &&
           model->digits_array[model->selector_position - 1] == 5 &&
           model->digits_array[model->selector_position] == 5) {
            model->digits_array[model->selector_position] = 0;
        } else if(model->digits_array[model->selector_position] == 9) {
            model->digits_array[model->selector_position] = 0;
        } else {
            model->digits_array[model->selector_position]++;
        }
    }

    // Update values
    update_ip_data(model->ip_array, model->digits_array);
}

// Functiom to solve position and add values in the array at the substraction
static void validate_num_in_subtraction(ip_assigner_model* model) {
    // Add when the position is in third position for example
    // in the position 0, 3, 6, 9 positions
    if(model->selector_position % 3 == 0) {
        if(model->digits_array[model->selector_position] == 0) {
            // The next value needs to be 2
            model->digits_array[model->selector_position] = 2;

            // Condition if values are higher than 8 bit
            if(model->digits_array[model->selector_position + 1] > 5 &&
               model->digits_array[model->selector_position + 2] > 5) {
                model->digits_array[model->selector_position + 1] = 5;
                model->digits_array[model->selector_position + 2] = 5;
            } else if(model->digits_array[model->selector_position + 1] > 5) {
                model->digits_array[model->selector_position + 1] = 5;
            }
        } else {
            model->digits_array[model->selector_position]--;
        }
    }

    // Add when the position is in third position for example
    // in the position 1, 4, 7, 10 positions
    if(model->selector_position % 3 == 1) {
        if(model->digits_array[model->selector_position - 1] == 2 &&
           model->digits_array[model->selector_position] == 0) {
            model->digits_array[model->selector_position] = 5;

            // If the first digit is higher
            if(model->digits_array[model->selector_position + 1] > 5) {
                model->digits_array[model->selector_position + 1] = 5;
            }
        } else if(model->digits_array[model->selector_position] == 0) {
            model->digits_array[model->selector_position] = 9;
        } else {
            model->digits_array[model->selector_position]--;
        }
    }

    // Add when the position is in third position for example
    // in the position 2, 5, 8, 11 positions
    if(model->selector_position % 3 == 2) {
        if(model->digits_array[model->selector_position - 2] == 2 &&
           model->digits_array[model->selector_position - 1] == 5 &&
           model->digits_array[model->selector_position] == 0) {
            model->digits_array[model->selector_position] = 5;
        } else if(model->digits_array[model->selector_position] == 0) {
            model->digits_array[model->selector_position] = 9;
        } else {
            model->digits_array[model->selector_position]--;
        }
    }

    // Update values
    update_ip_data(model->ip_array, model->digits_array);
}

// This function works for the inpust on the view
static bool input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);

    ip_assigner_t* instance = (ip_assigner_t*)context;

    if(input_event->type == InputTypeRelease || input_event->type == InputTypeRepeat) {
        switch(input_event->key) {
        // Update the selector and add more
        case InputKeyRight:
            with_view_model(
                instance->view,
                ip_assigner_model * model,
                {
                    model->selector_position++;
                    if(model->selector_position > 11) model->selector_position = 0;
                },
                true);
            break;

        // Update the selector and less one
        case InputKeyLeft:
            with_view_model(
                instance->view,
                ip_assigner_model * model,
                {
                    if(model->selector_position == 0)
                        model->selector_position = 11;
                    else
                        model->selector_position--;
                },
                true);
            break;

        case InputKeyUp:
            with_view_model(
                instance->view,
                ip_assigner_model * model,
                {
                    if(model->ip_array != NULL) {
                        validate_num_in_sum(model);
                    }
                },
                true);
            break;

        case InputKeyDown:
            with_view_model(
                instance->view,
                ip_assigner_model * model,
                {
                    if(model->ip_array != NULL) {
                        validate_num_in_subtraction(model);
                    }
                },
                true);
            break;

        case InputKeyOk:
            with_view_model(
                instance->view,
                ip_assigner_model * model,
                {
                    if(model->callback != 0) {
                        model->callback(model->context);
                    }
                },
                true);
            break;

        default:
            break;
        }
    }

    return false;
}

/**
 * Function to alloc the ip assigner
 */
ip_assigner_t* ip_assigner_alloc() {
    ip_assigner_t* instance = (ip_assigner_t*)malloc(sizeof(ip_assigner_t));

    // Alloc view
    View* view = view_alloc();

    // Set the orientation
    view_set_orientation(view, ViewOrientationHorizontal);

    // Set the draw callback for the view
    view_set_draw_callback(view, ip_assignament_draw_callback);

    // Set input events, to get buttons events
    view_set_input_callback(view, input_callback);

    // alloc model
    view_allocate_model(view, ViewModelTypeLocking, sizeof(ip_assigner_model));

    // Init the view model
    with_view_model(
        view,
        ip_assigner_model * model,
        {
            model->selector_position = 0;
            model->header = furi_string_alloc();
            memset(model->digits_array, 0, 12);
        },
        true);

    // Set the view
    instance->view = view;

    // Set the view context
    view_set_context(view, instance);

    return instance;
}

/**
 * Function to free the IP assigner
 */
void ip_assigner_free(ip_assigner_t* instance) {
    with_view_model(
        instance->view, ip_assigner_model * model, { furi_string_free(model->header); }, true);
    view_free_model(instance->view);
    view_free(instance->view);
    free(instance);
}

/**
 * Function to get the view
 */
View* ip_assigner_get_view(ip_assigner_t* instance) {
    return instance->view;
}

/**
 * Function set the header
 */
void ip_assigner_set_header(ip_assigner_t* instance, const char* text) {
    furi_check(instance);

    with_view_model(
        instance->view,
        ip_assigner_model * model,
        {
            if(text == NULL) {
                furi_string_reset(model->header);
            } else {
                furi_string_set_str(model->header, text);
            }
        },
        true);
}

/**
 * Set array ip in the model
 */
void ip_assigner_set_ip_array(ip_assigner_t* instance, uint8_t* ip_array) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        ip_assigner_model * model,
        {
            model->ip_array = ip_array;
            uint8_t pivot = 3;

            for(uint8_t i = 0; i < 12; i++) {
                uint8_t pos = i / 3;
                pivot--;
                model->digits_array[i] = get_num(model->ip_array[pos], pivot);

                if(pivot == 0) pivot = 3;
            }
        },
        true);
}

/**
 * Set the callback
 */
void ip_assigner_callback(ip_assigner_t* instance, ip_assigner_callback_t callback, void* context) {
    with_view_model(
        instance->view,
        ip_assigner_model * model,
        {
            model->callback = callback;
            model->context = context;
        },
        true);
}

/**
 * Reset
 */
void ip_assigner_reset(ip_assigner_t* instance) {
    with_view_model(
        instance->view,
        ip_assigner_model * model,
        {
            furi_string_reset(model->header);
            model->ip_array = NULL;
            model->context = NULL;
            model->callback = 0;
            model->selector_position = 0;

            memset(model->digits_array, 0, 12);
        },
        true);
}
