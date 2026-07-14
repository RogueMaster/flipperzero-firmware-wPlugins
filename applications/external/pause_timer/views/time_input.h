#pragma once

#include <gui/view.h>

typedef struct PauseTimerApp PauseTimerApp;
typedef struct PTTimeInput PTTimeInput;

typedef void (*TimeInputStartCallback)(void* context, uint16_t timer_val);
typedef void (*TimeInputLearnCallback)(void* context);

PTTimeInput* time_input_alloc(PauseTimerApp* pt_app);

void time_input_free(PTTimeInput* time_input);

View* time_input_get_view(PTTimeInput* time_input);

void time_input_set_start_callback(
    PTTimeInput* time_input,
    TimeInputStartCallback callback,
    void* context);
void time_input_set_learn_callback(
    PTTimeInput* time_input,
    TimeInputLearnCallback callback,
    void* context);
