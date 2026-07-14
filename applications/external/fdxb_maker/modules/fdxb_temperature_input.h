#pragma once

#include <gui/view.h>

typedef struct FdxbTemperatureInput FdxbTemperatureInput;

typedef void (*FdxbTemperatureInputCallback)(void* context, float number);

FdxbTemperatureInput* fdxb_temperature_input_alloc(void);

void fdxb_temperature_input_free(FdxbTemperatureInput* fdxb_temperature_input);

View* fdxb_temperature_input_get_view(FdxbTemperatureInput* fdxb_temperature_input);

void fdxb_temperature_input_set_result_callback(
    FdxbTemperatureInput* fdxb_temperature_input,
    FdxbTemperatureInputCallback input_callback,
    void* callback_context,
    float current_number);

void fdxb_temperature_input_set_header_text(
    FdxbTemperatureInput* fdxb_temperature_input,
    const char* text);

void fdxb_temperature_input_reset(FdxbTemperatureInput* fdxb_temperature_input);
