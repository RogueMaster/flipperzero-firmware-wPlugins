#pragma once

#include <gui/view.h>
#include <infrared.h>

typedef struct PauseTimerApp PauseTimerApp;
typedef struct CountdownUtils CountdownUtils;

typedef struct {
    uint16_t timer_val;
    bool has_ir_signal;
    PauseTimerApp* app;
} CountdownArgs;

CountdownUtils* countdown_utils_alloc();
void countdown_utils_free(CountdownUtils* countdown);
View* countdown_get_view(CountdownUtils* countdown);
void countdown_set_args(CountdownUtils* countdown, CountdownArgs* args);
void countdown_start(CountdownUtils* countdown);
void stop_countdown(CountdownUtils* countdown);
