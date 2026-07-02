#ifndef DCF77_EXPERIMENTAL_TIME_INPUT_H
#define DCF77_EXPERIMENTAL_TIME_INPUT_H

#include <gui/view.h>
#include <datetime/datetime.h>

typedef struct Dcf77ExperimentalTimeInput Dcf77ExperimentalTimeInput;
typedef uint32_t (*Dcf77ExperimentalTimeInputPreviousCallback)(void* context);

Dcf77ExperimentalTimeInput* dcf77_experimental_time_input_alloc(void);
void dcf77_experimental_time_input_free(Dcf77ExperimentalTimeInput* instance);
View* dcf77_experimental_time_input_get_view(Dcf77ExperimentalTimeInput* instance);
void dcf77_experimental_time_input_set_previous_callback(
    Dcf77ExperimentalTimeInput* instance,
    Dcf77ExperimentalTimeInputPreviousCallback callback,
    void* context);
void dcf77_experimental_time_input_set(
    Dcf77ExperimentalTimeInput* instance,
    const DateTime* datetime);
bool dcf77_experimental_time_input_get(Dcf77ExperimentalTimeInput* instance, DateTime* datetime);

#endif
