#pragma once

#include <gui/view.h>

typedef struct BigNumberInput BigNumberInput;

typedef void (*BigNumberInputCallback)(void* context, uint64_t number);

BigNumberInput* big_number_input_alloc(void);

void big_number_input_free(BigNumberInput* big_number_input);

View* big_number_input_get_view(BigNumberInput* big_number_input);

void big_number_input_set_result_callback(
    BigNumberInput* big_number_input,
    BigNumberInputCallback input_callback,
    void* callback_context,
    uint64_t current_number,
    uint64_t min_value,
    uint64_t max_value);

void big_number_input_set_header_text(BigNumberInput* big_number_input, const char* text);
